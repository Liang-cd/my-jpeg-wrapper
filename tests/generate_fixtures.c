/* ホスト側の完全な IJG libjpeg で、テスト用 JPEG 配列を再生成します。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jpeglib.h>

typedef enum { RGB_BASELINE, RGB_PROGRESSIVE, GRAYSCALE, CMYK } fixture_kind;

static void generate(const char *name, fixture_kind kind)
{
    enum { W = 17, H = 13 };
    unsigned char input[W * H * 4];
    unsigned char *jpeg = NULL;
    unsigned long length = 0;
    struct jpeg_compress_struct encoder;
    struct jpeg_error_mgr error;
    int channels = kind == GRAYSCALE ? 1 : kind == CMYK ? 4 : 3;
    size_t i;
    memset(input, 100, sizeof(input));
    for (i = 0; i < W * H; ++i) {
        if (kind == RGB_BASELINE || kind == RGB_PROGRESSIVE) {
            input[i * 3] = (unsigned char)(20 + (i % W) * 4);
            input[i * 3 + 1] = (unsigned char)(50 + (i / W) * 6);
            input[i * 3 + 2] = 180;
        } else if (kind == GRAYSCALE) {
            input[i] = (unsigned char)(40 + (i % W) * 5 + (i / W) * 4);
        }
    }
    encoder.err = jpeg_std_error(&error);
    jpeg_create_compress(&encoder);
    jpeg_mem_dest(&encoder, &jpeg, &length);
    encoder.image_width = W;
    encoder.image_height = H;
    encoder.input_components = channels;
    encoder.in_color_space = kind == GRAYSCALE ? JCS_GRAYSCALE :
                             kind == CMYK ? JCS_CMYK : JCS_RGB;
    jpeg_set_defaults(&encoder);
    jpeg_set_quality(&encoder, 95, TRUE);
    if (kind == RGB_PROGRESSIVE) {
        encoder.comp_info[0].h_samp_factor = 1;
        encoder.comp_info[0].v_samp_factor = 1;
        jpeg_simple_progression(&encoder);
    }
    jpeg_start_compress(&encoder, TRUE);
    while (encoder.next_scanline < encoder.image_height) {
        JSAMPROW row = input + (size_t)encoder.next_scanline * W * (size_t)channels;
        jpeg_write_scanlines(&encoder, &row, 1);
    }
    jpeg_finish_compress(&encoder);
    printf("static const unsigned char %s[] = {\n", name);
    for (i = 0; i < length; ++i)
        printf("0x%02x,%s", jpeg[i], i % 12 == 11 ? "\n" : " ");
    puts("\n};");
    free(jpeg);
    jpeg_destroy_compress(&encoder);
}

int main(void)
{
    puts("/* 自動生成したテストデータ。ライブラリ本体には含めません。 */");
    generate("fixture_rgb", RGB_BASELINE);
    generate("fixture_progressive", RGB_PROGRESSIVE);
    generate("fixture_gray", GRAYSCALE);
    generate("fixture_cmyk", CMYK);
    return 0;
}
