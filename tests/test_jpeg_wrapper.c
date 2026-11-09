#include "jpeg_wrapper.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <turbojpeg.h>
#ifdef JPEGW_TEST_FIXTURES
#include "fixtures/jpeg_fixtures.h"
#endif

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); exit(1); \
} } while (0)

static void exercise(int format, int subsamp, int flags)
{
    enum { W = 17, H = 13 };
    unsigned char input[W * H * 4];
#ifdef JPEGW_TEST_FIXTURES
    const unsigned char *jpeg = NULL;
#else
    unsigned char *jpeg = NULL;
#endif
    unsigned long jpeg_size = 0;
#ifndef JPEGW_TEST_FIXTURES
    tjhandle compressor = tjInitCompress();
#endif
    jpegw_info info;
    jpegw_image image, sentinel;
    unsigned char *pixels, *rgb;
    size_t i;
    int mode;
    memset(input, 100, sizeof(input));
    /* 座標で色を変え、行順・チャンネル順の入れ違いも検出します。 */
    for (i = 0; i < W * H; ++i) {
        if (format == TJPF_RGB) {
            input[i * 3] = (unsigned char)(20 + (i % W) * 4);
            input[i * 3 + 1] = (unsigned char)(50 + (i / W) * 6);
            input[i * 3 + 2] = 180;
        } else if (format == TJPF_GRAY) {
            input[i] = (unsigned char)(40 + (i % W) * 5 + (i / W) * 4);
        }
    }
#ifdef JPEGW_TEST_FIXTURES
    (void)subsamp;
    if (format == TJPF_CMYK) {
        jpeg = fixture_cmyk; jpeg_size = sizeof(fixture_cmyk);
    } else if (format == TJPF_GRAY) {
        jpeg = fixture_gray; jpeg_size = sizeof(fixture_gray);
    } else if (flags & TJFLAG_PROGRESSIVE) {
        jpeg = fixture_progressive; jpeg_size = sizeof(fixture_progressive);
    } else {
        jpeg = fixture_rgb; jpeg_size = sizeof(fixture_rgb);
    }
#else
    CHECK(compressor != NULL);
    CHECK(tjCompress2(compressor, input, W, 0, H, format, &jpeg, &jpeg_size,
                      subsamp, 95, flags) == 0);
    tjDestroy(compressor);
#endif
    if (format == TJPF_CMYK) {
        CHECK(jpegw_get_info(jpeg, jpeg_size, JPEGW_RGB, &info) == JPEGW_UNSUPPORTED);
#ifndef JPEGW_TEST_FIXTURES
        tjFree(jpeg);
#endif
        return;
    }
    rgb = malloc(W * H * 3);
    CHECK(rgb != NULL);
    memset(&sentinel, 0, sizeof(sentinel));
    sentinel.width = 999;
    for (mode = JPEGW_RGB; mode <= JPEGW_RGBA; ++mode) {
        CHECK(jpegw_get_info(jpeg, jpeg_size, (jpegw_mode)mode, &info) == JPEGW_OK);
        CHECK(info.width == W && info.height == H && info.mode == (jpegw_mode)mode);
        CHECK(info.stride == W * (size_t)mode);
        CHECK(info.pixels_size == W * H * (size_t)mode);
        pixels = malloc(info.pixels_size + 16);
        CHECK(pixels != NULL);
        memset(pixels, 0xA5, info.pixels_size + 16);
        image = sentinel;
        CHECK(jpegw_read(&image, jpeg, jpeg_size, (jpegw_mode)mode,
                         pixels, info.pixels_size - 1) == JPEGW_BUFFER_TOO_SMALL);
        CHECK(memcmp(&image, &sentinel, sizeof(image)) == 0);
        for (i = 0; i < info.pixels_size + 16; ++i) CHECK(pixels[i] == 0xA5);
        CHECK(jpegw_read(&image, jpeg, jpeg_size, (jpegw_mode)mode,
                         pixels, info.pixels_size) == JPEGW_OK);
        CHECK(image.width == W && image.height == H && image.pixels == pixels);
        CHECK(image.mode == (jpegw_mode)mode);
        for (i = info.pixels_size; i < info.pixels_size + 16; ++i)
            CHECK(pixels[i] == 0xA5);
        for (i = 0; i < W * H; ++i) {
            if (mode == JPEGW_RGB) {
                memcpy(rgb + i * 3, pixels + i * 3, 3);
                int channel;
                for (channel = 0; channel < 3; ++channel) {
                    int expected = format == TJPF_GRAY ? input[i] :
                                   input[i * 3 + (size_t)channel];
                    CHECK(abs((int)pixels[i * 3 + (size_t)channel] - expected) <= 6);
                }
            } else {
                CHECK(memcmp(rgb + i * 3, pixels + i * 4, 3) == 0);
                CHECK(pixels[i * 4 + 3] == 255);
            }
        }
        /* JPEG を各位置で切り詰め、正式な展開が必ず失敗することを確認します。 */
        for (i = 1; i < jpeg_size; ++i) {
            image = sentinel;
            CHECK(jpegw_read(&image, jpeg, i, (jpegw_mode)mode,
                             pixels, info.pixels_size) != JPEGW_OK);
            CHECK(memcmp(&image, &sentinel, sizeof(image)) == 0);
        }
        free(pixels);
    }
    free(rgb);
#ifndef JPEGW_TEST_FIXTURES
    tjFree(jpeg);
#endif
}

int main(void)
{
    unsigned char bad[32] = {0};
    jpegw_info info, before;
    jpegw_image image = {0};
    memset(&before, 0, sizeof(before));
    before.width = 123;
    info = before;
    CHECK(jpegw_get_info(NULL, 10, JPEGW_RGB, &info) == JPEGW_INVALID_ARGUMENT);
    CHECK(jpegw_get_info(bad, 0, JPEGW_RGB, &info) == JPEGW_INVALID_ARGUMENT);
    CHECK(jpegw_get_info(bad, sizeof(bad), (jpegw_mode)0, &info) == JPEGW_INVALID_ARGUMENT);
    CHECK(jpegw_get_info(bad, sizeof(bad), JPEGW_RGB, NULL) == JPEGW_INVALID_ARGUMENT);
    CHECK(jpegw_get_info(bad, sizeof(bad), JPEGW_RGB, &info) == JPEGW_INVALID_JPEG);
    CHECK(memcmp(&info, &before, sizeof(info)) == 0);
    CHECK(jpegw_read(NULL, bad, sizeof(bad), JPEGW_RGB, bad, 1) == JPEGW_INVALID_ARGUMENT);
    CHECK(jpegw_read(&image, bad, sizeof(bad), JPEGW_RGB, NULL, 1) == JPEGW_INVALID_ARGUMENT);
    exercise(TJPF_RGB, TJSAMP_420, 0);
    exercise(TJPF_RGB, TJSAMP_444, TJFLAG_PROGRESSIVE);
    exercise(TJPF_GRAY, TJSAMP_GRAY, 0);
    exercise(TJPF_CMYK, TJSAMP_444, 0);
    puts("All wrapper tests passed.");
    return 0;
}
