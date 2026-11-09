#include "jpeg_wrapper.h"
#include <stdlib.h>

/* 入力と長さはファイル・通信・フラッシュの読み取り側から受け取ります。
 * malloc/free は組み込み環境の専用関数へ置き換えられます。
 * 成功時の imageTemp->pixels は呼び出し元が解放してください。
 * imageTemp が既存バッファを所有する場合は、呼び出し前に解放してください。
 */
jpegw_status example_decode(jpegw_image *imageTemp,
                            const void *jpeg_data, size_t jpeg_size)
{
    jpegw_info info;
    jpegw_status status;
    void *pixels;
    if (!imageTemp)
        return JPEGW_INVALID_ARGUMENT;
    status = jpegw_get_info(jpeg_data, jpeg_size, JPEGW_RGB, &info);
    if (status != JPEGW_OK)
        return status;
    /* 問い合わせた容量だけを確保し、成功時は所有権を呼び出し元へ渡します。 */
    pixels = malloc(info.pixels_size);
    if (!pixels)
        return JPEGW_DECODER_ERROR; /* アプリ側で独自のメモリ不足コードを使うこともできます。 */
    status = jpegw_read(imageTemp, jpeg_data, jpeg_size, JPEGW_RGB,
                        pixels, info.pixels_size);
    if (status != JPEGW_OK)
        free(pixels);
    return status;
}
