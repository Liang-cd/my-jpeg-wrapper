/* ホスト側でのみ実行し、最小構成のテスト用 JPEG 配列を生成します。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <turbojpeg.h>

static void generate(const char *name, int format, int subsamp, int flags)
{
    enum { W = 17, H = 13 };
    unsigned char input[W * H * 4];
    unsigned char *jpeg = NULL;
    unsigned long length = 0;
    tjhandle compressor = tjInitCompress();
    size_t i;
    memset(input, 100, sizeof(input));
    for (i = 0; i < W * H; ++i) {
        if (format == TJPF_RGB) {
            input[i * 3] = (unsigned char)(20 + (i % W) * 4);
            input[i * 3 + 1] = (unsigned char)(50 + (i / W) * 6);
            input[i * 3 + 2] = 180;
        } else if (format == TJPF_GRAY) {
            input[i] = (unsigned char)(40 + (i % W) * 5 + (i / W) * 4);
        }
    }
    if (!compressor || tjCompress2(compressor, input, W, 0, H, format,
                                   &jpeg, &length, subsamp, 95, flags) != 0)
        exit(1);
    printf("static const unsigned char %s[] = {\n", name);
    for (i = 0; i < length; ++i) {
        printf("0x%02x,%s", jpeg[i], i % 12 == 11 ? "\n" : " ");
    }
    puts("\n};");
    tjFree(jpeg);
    tjDestroy(compressor);
}

int main(void)
{
    puts("/* 自動生成したテストデータ。ライブラリ本体には含めません。 */");
    generate("fixture_rgb", TJPF_RGB, TJSAMP_420, 0);
    generate("fixture_progressive", TJPF_RGB, TJSAMP_444, TJFLAG_PROGRESSIVE);
    generate("fixture_gray", TJPF_GRAY, TJSAMP_GRAY, 0);
    generate("fixture_cmyk", TJPF_CMYK, TJSAMP_444, 0);
    return 0;
}
