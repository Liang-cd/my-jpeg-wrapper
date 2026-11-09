/**
 * @file jpeg_wrapper.h
 * @brief メモリ上の JPEG を RGB/RGBA へ展開する読み取り専用 API。
 * @details 出力バッファの確保・解放は呼び出し元が管理します。
 * libjpeg-turbo 内部では標準ヒープによるメモリ確保が発生します。
 * 各呼び出しは独立したデコーダを使うため、独立したバッファで並行実行できます。
 */
#ifndef JPEG_WRAPPER_H
#define JPEG_WRAPPER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 出力形式。列挙値は 1 ピクセルのバイト数と一致します。 */
typedef enum {
    JPEGW_RGB = 3,  /**< R、G、B の順に各 1 バイト。 */
    JPEGW_RGBA = 4 /**< R、G、B、A の順に各 1 バイト。A は常に 255。 */
} jpegw_mode;

/** @brief API の処理結果。 */
typedef enum {
    JPEGW_OK = 0,           /**< 正常終了。 */
    JPEGW_INVALID_ARGUMENT, /**< NULL、入力長 0、または不正な出力形式。 */
    JPEGW_INVALID_JPEG,     /**< JPEG の解析・展開に失敗。警告も失敗として扱います。 */
    JPEGW_UNSUPPORTED,      /**< 対応外の色空間（CMYK/YCCK など）や、最小構成での非対応精度。 */
    JPEGW_SIZE_OVERFLOW,    /**< 入出力サイズがプラットフォームの上限を超過。 */
    JPEGW_BUFFER_TOO_SMALL, /**< 出力バッファの容量不足。 */
    JPEGW_DECODER_ERROR     /**< デコーダの初期化に失敗。 */
} jpegw_status;

/** @brief 展開後の画像。ピクセルの所有権は呼び出し元にあります。 */
typedef struct {
    uint32_t width;         /**< 画像の幅（ピクセル）。 */
    uint32_t height;        /**< 画像の高さ（ピクセル）。 */
    unsigned char *pixels; /**< 呼び出し元が渡した出力バッファ。 */
    jpegw_mode mode;        /**< 出力ピクセル形式。 */
} jpegw_image;

/** @brief ヘッダから取得した画像情報と必要なバッファ容量。 */
typedef struct {
    uint32_t width;      /**< 画像の幅（ピクセル）。 */
    uint32_t height;     /**< 画像の高さ（ピクセル）。 */
    jpegw_mode mode;     /**< 問い合わせ時に指定した出力形式。 */
    size_t stride;      /**< 1 行のバイト数。行末パディングはありません。 */
    size_t pixels_size; /**< 必要なバイト数。stride * height。 */
} jpegw_info;

/**
 * @brief JPEG ヘッダを読み、画像サイズと必要な出力容量を取得します。
 * @param[in] jpeg_data JPEG データの先頭。呼び出し中は有効かつ変更不可。
 * @param[in] jpeg_size JPEG データの実際のバイト数。画像の幅・高さではありません。
 * @param[in] mode JPEGW_RGB または JPEGW_RGBA。
 * @param[out] info 成功時の画像情報。失敗時は変更しません。
 * @return JPEGW_OK、または jpegw_status のエラー値。
 * @retval JPEGW_INVALID_ARGUMENT 必須ポインタ、入力長、または mode が不正。
 * @retval JPEGW_INVALID_JPEG ヘッダの解析に失敗。
 * @retval JPEGW_UNSUPPORTED 色空間に非対応。最小構成では 8 ビット以外も拒否。
 * @retval JPEGW_SIZE_OVERFLOW 必要なサイズが表現可能範囲を超過。
 * @retval JPEGW_DECODER_ERROR デコーダを生成できない。
 * @pre jpeg_data と info の領域は重複してはいけません。
 * @note 圧縮ピクセル全体の検証は行いません。この関数が成功しても、
 *       jpegw_read() が破損データや非対応のビット深度により失敗する場合があります。
 * @note 8 ビットのグレースケール、RGB、YCbCr とプログレッシブ JPEG に対応。
 *       EXIF 回転や ICC 色変換は適用しません。
 * @note minimal/ の実装は同じ公開 API を使用します。通常実装と同時にリンクしないでください。
 */
jpegw_status jpegw_get_info(const void *jpeg_data, size_t jpeg_size,
                            jpegw_mode mode, jpegw_info *info);

/**
 * @brief JPEG を呼び出し元のバッファへ展開し、画像構造体を設定します。
 * @param[out] image 成功時の画像。失敗時は変更しません。
 * @param[in] jpeg_data JPEG データの先頭。呼び出し中は有効かつ変更不可。
 * @param[in] jpeg_size JPEG データの実際のバイト数。
 * @param[in] mode JPEGW_RGB または JPEGW_RGBA。
 * @param[out] pixels 呼び出し元が確保した書き込み可能な出力バッファ。
 * @param[in] pixels_capacity pixels に書き込み可能なバイト数。
 * @return JPEGW_OK、または jpegw_status のエラー値。
 * @retval JPEGW_BUFFER_TOO_SMALL 必要容量に満たない。ピクセルは書き込みません。
 * @retval JPEGW_INVALID_JPEG データ破損、切り詰め、非対応ビット深度、または警告。
 * @retval JPEGW_INVALID_ARGUMENT 必須ポインタ、入力長、または mode が不正。
 * @retval JPEGW_UNSUPPORTED 色空間に非対応。最小構成では 8 ビット以外も拒否。
 * @retval JPEGW_SIZE_OVERFLOW 必要なサイズが表現可能範囲を超過。
 * @retval JPEGW_DECODER_ERROR デコーダを生成できない。
 * @pre jpeg_data、pixels、image の領域は互いに重複してはいけません。
 * @post 成功時は image->pixels == pixels となります。
 * @warning 展開中の失敗では pixels に一部のデータが書き込まれる場合があります。
 *          失敗時のピクセル内容は使用しないでください。
 * @details ピクセルは上から下、左から右へ隙間なく並びます。
 *          各チャンネルは 1 バイト、RGBA の A は 255 です。
 *          入力の幅・高さと容量はこの関数でも再検証します。
 *          既存の image->pixels や渡されたバッファを解放しません。
 *          呼び出し元が適切な解放関数で寿命を管理してください。
 * @see jpegw_get_info
 */
jpegw_status jpegw_read(jpegw_image *image, const void *jpeg_data,
                        size_t jpeg_size, jpegw_mode mode,
                        void *pixels, size_t pixels_capacity);

/**
 * @brief ステータスに対応する英語の固定文字列を取得します。
 * @param[in] status 処理結果。
 * @return 解放不要の静的文字列。不明な値には "unknown status" を返します。
 */
const char *jpegw_status_string(jpegw_status status);

#ifdef __cplusplus
}
#endif
#endif
