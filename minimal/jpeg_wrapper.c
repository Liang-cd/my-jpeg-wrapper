#include "jpeg_wrapper.h"

#include <limits.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <jpeglib.h>
#include <jerror.h>

/* setjmp 後に変更されるデコーダはヒープに置き、longjmp 後も有効に保ちます。 */
typedef struct {
    struct jpeg_decompress_struct decoder;
    struct jpeg_error_mgr error;
    struct jpeg_source_mgr source;
    jmp_buf jump;
    jpegw_status failure;
} jpegw_context;

/* libjpeg の exit() を使わず、すべての致命的エラーを呼び出し元へ返します。 */
static void fail(j_common_ptr decoder)
{
    jpegw_context *context = (jpegw_context *)decoder->client_data;
    context->failure = decoder->err->msg_code == JERR_OUT_OF_MEMORY ?
                       JPEGW_DECODER_ERROR : JPEGW_INVALID_JPEG;
    longjmp(context->jump, 1);
}

/* 途中で切れた JPEG などの警告も失敗とします。診断は標準出力へ出しません。 */
static void emit(j_common_ptr decoder, int level)
{
    if (level < 0)
        fail(decoder);
}

static void quiet(j_common_ptr decoder)
{
    (void)decoder;
}

/* 文字列テーブルや fprintf/exit に依存しないエラーマネージャです。 */
static void format(j_common_ptr decoder, char *buffer)
{
    (void)decoder;
    buffer[0] = '\0';
}

static void reset(j_common_ptr decoder)
{
    decoder->err->num_warnings = 0;
    decoder->err->msg_code = 0;
}

/* メモリ入力専用です。stdio 入力モジュールをリンクする必要はありません。 */
static void source_noop(j_decompress_ptr decoder)
{
    (void)decoder;
}

static boolean source_empty(j_decompress_ptr decoder)
{
    /* 終端の補完はせず、入力不足を必ずエラーとして返します。 */
    decoder->err->msg_code = JERR_INPUT_EOF;
    fail((j_common_ptr)decoder);
    return FALSE;
}

static void source_skip(j_decompress_ptr decoder, long count)
{
    if (count > 0) {
        if ((unsigned long)count > decoder->src->bytes_in_buffer)
            (void)source_empty(decoder);
        decoder->src->next_input_byte += (size_t)count;
        decoder->src->bytes_in_buffer -= (size_t)count;
    }
}

/* 問い合わせと展開で共通のヘッダ解析・容量チェックを行います。 */
static jpegw_status process(const void *data, size_t length, jpegw_mode mode,
                            jpegw_info *info, void *pixels, size_t capacity)
{
    jpegw_context *context;
    struct jpeg_decompress_struct *decoder;
    jpegw_info result;
    jpegw_status status = JPEGW_OK;
    if (!data || !length || (mode != JPEGW_RGB && mode != JPEGW_RGBA))
        return JPEGW_INVALID_ARGUMENT;
    if (length > ULONG_MAX)
        return JPEGW_SIZE_OVERFLOW;
    context = (jpegw_context *)calloc(1, sizeof(*context));
    if (!context)
        return JPEGW_DECODER_ERROR;
    decoder = &context->decoder;
    decoder->err = &context->error;
    context->error.error_exit = fail;
    context->error.emit_message = emit;
    context->error.output_message = quiet;
    context->error.format_message = format;
    context->error.reset_error_mgr = reset;
    decoder->client_data = context;

    /* 生成途中の失敗も、初期化済みの mem ポインタから安全に解放できます。 */
    if (setjmp(context->jump)) {
        jpegw_status failure = context->failure;
        jpeg_destroy_decompress(&context->decoder);
        free(context);
        return failure;
    }
    jpeg_create_decompress(decoder);
    context->source.init_source = source_noop;
    context->source.fill_input_buffer = source_empty;
    context->source.skip_input_data = source_skip;
    context->source.resync_to_restart = jpeg_resync_to_restart;
    context->source.term_source = source_noop;
    context->source.next_input_byte = (const JOCTET *)data;
    context->source.bytes_in_buffer = length;
    decoder->src = &context->source;
    if (jpeg_read_header(decoder, TRUE) != JPEG_HEADER_OK) {
        status = JPEGW_INVALID_JPEG;
        goto done;
    }
    /* 公開 API は 8 ビット RGB/RGBA 専用です。CMYK の暗黙変換はしません。 */
    if (decoder->data_precision != 8 ||
        (decoder->jpeg_color_space != JCS_RGB &&
         decoder->jpeg_color_space != JCS_YCbCr &&
         decoder->jpeg_color_space != JCS_GRAYSCALE)) {
        status = JPEGW_UNSUPPORTED;
        goto done;
    }
    if (!decoder->image_width || !decoder->image_height) {
        status = JPEGW_INVALID_JPEG;
        goto done;
    }
    /* 乗算より前にサイズを検証し、元の wrapper と同じ範囲を許可します。 */
    if ((size_t)decoder->image_width > SIZE_MAX / (size_t)mode ||
        (size_t)decoder->image_width > (size_t)INT_MAX / (size_t)mode) {
        status = JPEGW_SIZE_OVERFLOW;
        goto done;
    }
    result.width = decoder->image_width;
    result.height = decoder->image_height;
    result.mode = mode;
    result.stride = (size_t)result.width * (size_t)mode;
    if ((size_t)result.height > SIZE_MAX / result.stride) {
        status = JPEGW_SIZE_OVERFLOW;
        goto done;
    }
    result.pixels_size = result.stride * (size_t)result.height;
    if (pixels) {
        if (capacity < result.pixels_size) {
            status = JPEGW_BUFFER_TOO_SMALL;
            goto done;
        }
        /* 整数 IDCT を明示します。SIMD は構成ヘッダでも無効化しています。 */
        decoder->dct_method = JDCT_ISLOW;
        decoder->out_color_space = mode == JPEGW_RGB ? JCS_EXT_RGB : JCS_EXT_RGBA;
        if (!jpeg_start_decompress(decoder) ||
            decoder->output_width != result.width ||
            decoder->output_height != result.height ||
            decoder->output_components != (int)mode) {
            status = JPEGW_INVALID_JPEG;
            goto done;
        }
        /* 追加のピクセルバッファは作らず、呼び出し元の領域へ 1 行ずつ展開します。 */
        while (decoder->output_scanline < decoder->output_height) {
            JSAMPROW row = (unsigned char *)pixels +
                          (size_t)decoder->output_scanline * result.stride;
            if (jpeg_read_scanlines(decoder, &row, 1) != 1) {
                status = JPEGW_INVALID_JPEG;
                goto done;
            }
        }
        if (!jpeg_finish_decompress(decoder)) {
            status = JPEGW_INVALID_JPEG;
            goto done;
        }
    }
    /* ヘッダ問い合わせでも展開でも、成功した場合に限り結果を更新します。 */
    *info = result;
done:
    jpeg_destroy_decompress(decoder);
    free(context);
    return status;
}

jpegw_status jpegw_get_info(const void *jpeg_data, size_t jpeg_size,
                            jpegw_mode mode, jpegw_info *info)
{
    if (!info)
        return JPEGW_INVALID_ARGUMENT;
    return process(jpeg_data, jpeg_size, mode, info, NULL, 0);
}

jpegw_status jpegw_read(jpegw_image *image, const void *jpeg_data,
                        size_t jpeg_size, jpegw_mode mode,
                        void *pixels, size_t pixels_capacity)
{
    jpegw_info info;
    jpegw_status status;
    if (!image || !pixels)
        return JPEGW_INVALID_ARGUMENT;
    status = process(jpeg_data, jpeg_size, mode, &info, pixels, pixels_capacity);
    if (status == JPEGW_OK) {
        image->width = info.width;
        image->height = info.height;
        image->mode = info.mode;
        image->pixels = (unsigned char *)pixels;
    }
    return status;
}

const char *jpegw_status_string(jpegw_status status)
{
    switch (status) {
    case JPEGW_OK: return "success";
    case JPEGW_INVALID_ARGUMENT: return "invalid argument";
    case JPEGW_INVALID_JPEG: return "invalid or damaged JPEG";
    case JPEGW_UNSUPPORTED: return "unsupported JPEG colorspace or precision";
    case JPEGW_SIZE_OVERFLOW: return "pixel size exceeds platform limits";
    case JPEGW_BUFFER_TOO_SMALL: return "pixel buffer too small";
    case JPEGW_DECODER_ERROR: return "cannot create JPEG decoder";
    default: return "unknown status";
    }
}
