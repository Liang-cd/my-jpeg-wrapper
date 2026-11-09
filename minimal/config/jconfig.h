#ifndef JPEGW_MINIMAL_JCONFIG_H
#define JPEGW_MINIMAL_JCONFIG_H

/* libjpeg-turbo 3.2.1 用の固定設定。上流ファイルは変更しません。 */
#define JPEG_LIB_VERSION 62
#define LIBJPEG_TURBO_VERSION 3.2.1
#define LIBJPEG_TURBO_VERSION_NUMBER 3002001
#define D_ARITH_CODING_SUPPORTED 1

/* 精度別の wrapper ソース が指定する値を上書きしないでください。 */
#ifndef BITS_IN_JSAMPLE
#define BITS_IN_JSAMPLE 8
#endif

/* WITH_SIMD は 0 と定義せず、未定義のままにします。 */
#endif
