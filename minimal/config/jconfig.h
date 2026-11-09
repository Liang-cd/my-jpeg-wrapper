#ifndef JPEGW_IJG_JCONFIG_H
#define JPEGW_IJG_JCONFIG_H

/* 標準 C99 向けの IJG libjpeg 設定です。上流ファイルは変更しません。 */
#define HAVE_PROTOTYPES
#define HAVE_UNSIGNED_CHAR
#define HAVE_UNSIGNED_SHORT
#define HAVE_STDDEF_H
#define HAVE_STDLIB_H

/* 通常の算術右シフトを使用するターゲットを想定します。 */
/* 論理右シフトを行う処理系のみ RIGHT_SHIFT_IS_UNSIGNED を定義してください。 */
#endif
