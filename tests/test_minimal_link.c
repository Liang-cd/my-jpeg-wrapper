/* ライブラリ自体が stdio や環境変数 API に依存しないことを調べるリンク用です。 */
#include "jpeg_wrapper.h"

int main(void)
{
    static const unsigned char truncated[] = {0xff, 0xd8};
    jpegw_info info;
    return jpegw_get_info(truncated, sizeof(truncated), JPEGW_RGB, &info) ==
           JPEGW_INVALID_JPEG ? 0 : 1;
}
