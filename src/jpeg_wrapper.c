#include "jpeg_wrapper.h"

#include <limits.h>
#include <turbojpeg.h>

/* NULL・空データ・非対応の出力形式をライブラリ呼び出し前に排除します。 */
static int valid_input(const void *data, size_t size, jpegw_mode mode)
{
    return data != NULL && size != 0 &&
           (mode == JPEGW_RGB || mode == JPEGW_RGBA);
}

/* 問い合わせと正式な展開で同じ寸法・容量チェックを使用します。 */
static jpegw_status inspect(tjhandle decoder, const void *data, size_t size,
                            jpegw_mode mode, jpegw_info *info)
{
    int width, height, subsamp, colorspace;
    size_t channels = (size_t)mode;
    /* TurboJPEG 2.x 互換 API の入力長は unsigned long です。 */
    if (size > ULONG_MAX)
        return JPEGW_SIZE_OVERFLOW;
    if (tjDecompressHeader3(decoder, (const unsigned char *)data,
                            (unsigned long)size, &width, &height,
                            &subsamp, &colorspace) != 0)
        return JPEGW_INVALID_JPEG;
    /* CMYK/YCCK は単純な RGB 変換で色が保証できないため拒否します。 */
    if (colorspace != TJCS_RGB && colorspace != TJCS_YCbCr &&
        colorspace != TJCS_GRAY)
        return JPEGW_UNSUPPORTED;
    if (width <= 0 || height <= 0)
        return JPEGW_INVALID_JPEG;
    /* 乗算の前に size_t と TurboJPEG の int 型 stride の上限を確認します。 */
    if ((size_t)width > SIZE_MAX / channels ||
        (size_t)width > (size_t)INT_MAX / channels)
        return JPEGW_SIZE_OVERFLOW;
    info->stride = (size_t)width * channels;
    if ((size_t)height > SIZE_MAX / info->stride)
        return JPEGW_SIZE_OVERFLOW;
    info->width = (uint32_t)width;
    info->height = (uint32_t)height;
    info->mode = mode;
    info->pixels_size = info->stride * (size_t)height;
    return JPEGW_OK;
}

jpegw_status jpegw_get_info(const void *jpeg_data, size_t jpeg_size,
                            jpegw_mode mode, jpegw_info *info)
{
    tjhandle decoder;
    jpegw_info result;
    jpegw_status status;
    if (!info || !valid_input(jpeg_data, jpeg_size, mode))
        return JPEGW_INVALID_ARGUMENT;
    if (jpeg_size > ULONG_MAX)
        return JPEGW_SIZE_OVERFLOW;
    /* デコーダは呼び出しごとに生成し、共有状態を持たせません。 */
    decoder = tjInitDecompress();
    if (!decoder)
        return JPEGW_DECODER_ERROR;
    status = inspect(decoder, jpeg_data, jpeg_size, mode, &result);
    /* エラー経路でも内部リソースを必ず解放します。出力バッファは解放しません。 */
    tjDestroy(decoder);
    /* 成功した場合に限り、呼び出し元の構造体を更新します。 */
    if (status == JPEGW_OK)
        *info = result;
    return status;
}

jpegw_status jpegw_read(jpegw_image *image, const void *jpeg_data,
                        size_t jpeg_size, jpegw_mode mode,
                        void *pixels, size_t pixels_capacity)
{
    tjhandle decoder;
    jpegw_info info;
    jpegw_status status;
    if (!image || !pixels || !valid_input(jpeg_data, jpeg_size, mode))
        return JPEGW_INVALID_ARGUMENT;
    if (jpeg_size > ULONG_MAX)
        return JPEGW_SIZE_OVERFLOW;
    /* デコーダは呼び出しごとに生成し、共有状態を持たせません。 */
    decoder = tjInitDecompress();
    if (!decoder)
        return JPEGW_DECODER_ERROR;
    status = inspect(decoder, jpeg_data, jpeg_size, mode, &info);
    /* 出力先の容量は、現在の入力から再計算した値で検証します。 */
    if (status == JPEGW_OK && pixels_capacity < info.pixels_size)
        status = JPEGW_BUFFER_TOO_SMALL;
    /* 切り詰めなどの警告も失敗とし、不完全な画像を成功扱いしません。 */
    if (status == JPEGW_OK &&
        tjDecompress2(decoder, (const unsigned char *)jpeg_data,
                      (unsigned long)jpeg_size, (unsigned char *)pixels,
                      (int)info.width, (int)info.stride, (int)info.height,
                      mode == JPEGW_RGB ? TJPF_RGB : TJPF_RGBA,
                      TJFLAG_STOPONWARNING) != 0)
        status = JPEGW_INVALID_JPEG;
    /* エラー経路でも内部リソースを必ず解放します。出力バッファは解放しません。 */
    tjDestroy(decoder);
    /* 展開に成功した場合に限り、画像メタデータと出力ポインタを設定します。 */
    if (status == JPEGW_OK) {
        image->width = info.width;
        image->height = info.height;
        image->pixels = (unsigned char *)pixels;
        image->mode = mode;
    }
    return status;
}

const char *jpegw_status_string(jpegw_status status)
{
    switch (status) {
    case JPEGW_OK: return "success";
    case JPEGW_INVALID_ARGUMENT: return "invalid argument";
    case JPEGW_INVALID_JPEG: return "invalid or damaged JPEG";
    case JPEGW_UNSUPPORTED: return "unsupported JPEG colorspace";
    case JPEGW_SIZE_OVERFLOW: return "pixel size exceeds platform limits";
    case JPEGW_BUFFER_TOO_SMALL: return "pixel buffer too small";
    case JPEGW_DECODER_ERROR: return "cannot create JPEG decoder";
    default: return "unknown status";
    }
}
