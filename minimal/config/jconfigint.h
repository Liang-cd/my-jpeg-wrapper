#ifndef JPEGW_MINIMAL_JCONFIGINT_H
#define JPEGW_MINIMAL_JCONFIGINT_H

#include <stdint.h>

/* ターゲットの size_t 幅を使用します。ホストの値をコピーしません。 */
#if SIZE_MAX == UINT32_MAX
#define SIZEOF_SIZE_T 4
#elif SIZE_MAX == UINT64_MAX
#define SIZEOF_SIZE_T 8
#else
#error "This configuration requires 32-bit or 64-bit size_t"
#endif

#define BUILD "minimal"
#define PACKAGE_NAME "libjpeg-turbo"
#define VERSION "3.2.1"
#define HIDDEN
#define INLINE inline
#define FALLTHROUGH

/* 最小構成は TurboJPEG を使わないため、TLS は不要です。 */
#define THREAD_LOCAL

/* 12/16 ビット用ソースの設定を含め、SIMD と組み込み命令は使いません。 */
#undef WITH_SIMD
#undef WITH_SIMDE
#undef WITH_PROFILE
#undef HAVE_BUILTIN_CTZL
#undef HAVE_INTRIN_H
#undef C_ARITH_CODING_SUPPORTED
#undef D_ARITH_CODING_SUPPORTED
#if BITS_IN_JSAMPLE == 8
#define D_ARITH_CODING_SUPPORTED 1
#define SIMD_ARCHITECTURE NONE
#endif

#endif
