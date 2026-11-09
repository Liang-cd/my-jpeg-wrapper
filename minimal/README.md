# 嵌入式最简构建与移植清单

这套工程直接使用 libjpeg-turbo **3.2.1 的 libjpeg 解码接口**，对外仍提供 `include/jpeg_wrapper.h` 中相同的 `jpegw_get_info()`、`jpegw_read()` 和 `jpegw_status_string()`。

**移植时使用本目录的 `jpeg_wrapper.c`，不要再编译根目录 `src/jpeg_wrapper.c`。两者是同一 API 的替代实现，不能同时链接。** 原有根目录构建保持兼容；本目录不调用它，也不调用上游 CMake。

仅需 C99 编译器和标准 C 运行库。不依赖 CMake 生成任何头文件，不探测宿主 CPU，不使用汇编、SIMD、编译器内建位扫描、线程局部存储或操作系统环境变量。像素缓冲区仍由调用方分配、释放。解码明确选择整数 IDCT。

## 1. 到底编译哪些文件

唯一清单是 [sources.txt](sources.txt)：每行一个相对于**仓库根目录**的源文件，共 **56 个 `.c`**：

- `minimal/jpeg_wrapper.c`：1 个对外 API 实现，包含内存输入和错误处理。
- `third_party/libjpeg-turbo/src/` 及其 `wrapper/`：55 个上游解码依赖。

CMake 和 Makefile 都直接读取这同一份清单，不使用源文件通配符。迁移到 IDE、厂商 SDK、SCons 或自己的 Makefile 时，逐行添加即可；不要把上游整个目录中的 `.c` 全部加入编译。

上游代码按功能包含：

| 功能 | 对应文件族 |
| --- | --- |
| 解码入口、输入控制、JPEG 标记解析 | `jdapimin.c`、`jdapistd-*.c`、`jdinput.c`、`jdmarker.c` |
| 主控制与图像行缓冲 | `jdmaster.c`、`jdmainct-*.c`、`jdcoefct-*.c`、`jdpostct-*.c` |
| Huffman、渐进式与算术解码 | `jdhuff.c`、`jdphuff.c`、`jaricom.c`、`jdarith.c` |
| 无损解码的内部依赖 | `jdlhuff.c`、`jdlossls-*.c`、`jddiffct-*.c` |
| 颜色转换、采样与量化的内部依赖 | `jdcolor-*.c`、`jdsample-*.c`、`jdmerge-*.c`、`jquant1-*.c`、`jquant2-*.c` |
| IDCT | `jddctmgr-*.c`、`jidctint-*.c`、`jidctfst-*.c`、`jidctflt-*.c`、`jidctred-*.c` |
| 内存与公共工具 | `jmemmgr.c`、`jmemnobs.c`、`jcomapi.c`、`jpeg_nbits.c`、`jutils-*.c` |

`src/wrapper/*-8.c`、`*-12.c`、`*-16.c` 会通过 `#include` 编译相应的基础 `.c`。**不要再次单独编译它们包含的基础文件**，也不要在全局强制定义 `BITS_IN_JSAMPLE=8`，否则会破坏不同精度文件的配置。

虽然公开 wrapper 只接受 8 位图像，但上游的公共解码控制代码仍引用多个精度、无损、量化和不同 IDCT 模块。为了保持上游源码不变，本清单保留这些符号依赖；它是一个可独立链接、仅含解码器的移植基线，不声称是理论上最小的二进制。运行 wrapper 时选择整数 IDCT；`jidctflt-*` 是上游调度依赖，不能直接删掉。无硬件 FPU 时须由工具链选择软件浮点 ABI。

不编译：编码器、`turbojpeg.c`、`transupp.c`、PNG、zlib、BMP/PPM 文件读写、命令行工具、SIMD、上游默认错误输出模块 `jerror.c`、文件输入模块 `jdatasrc.c`。内存输入与错误处理已在本目录的 wrapper 中实现，损坏输入返回状态码，不补造 JPEG 结束标记，不调用 `exit()`，不输出诊断。

## 2. include 路径

按下面顺序加入，路径均相对于仓库根目录：

```text
include
minimal/config
third_party/libjpeg-turbo/src
```

- `include`：对外的 `jpeg_wrapper.h`，应用程序也需要此路径。
- `minimal/config`：本工程提供的固定配置头 `jconfig.h`、`jconfigint.h`、`jversion.h`。
- `third_party/libjpeg-turbo/src`：上游公共头、内部头和被包含的模板源文件。

这三个配置头是 wrapper 自己提供的文件，不是对上游源码的修改。不要同时加入另一套 libjpeg 的配置路径或从 PC 构建目录复制配置头。

## 3. 预定义宏与配置值

编译命令/IDE 的预定义宏只需要：

```text
NO_GETENV
NO_PUTENV
```

C99 示例：

```sh
cc -std=c99 -Os \
  -Iinclude -Iminimal/config -Ithird_party/libjpeg-turbo/src \
  -DNO_GETENV -DNO_PUTENV \
  -c minimal/jpeg_wrapper.c -o jpeg_wrapper.o
```

对清单中的所有 `.c` 使用同一组 include 路径与宏。CPU、端序、ABI 等目标宏由交叉编译器提供，不要手工伪造。

固定配置头中的关键值如下，无需再次放进命令行：

| 宏 | 配置与含义 |
| --- | --- |
| `JPEG_LIB_VERSION` | `62`，选择 libjpeg v6b ABI |
| `LIBJPEG_TURBO_VERSION` | `3.2.1` |
| `LIBJPEG_TURBO_VERSION_NUMBER` | `3002001` |
| `BITS_IN_JSAMPLE` | 默认 8；精度 wrapper 源文件可先定义为 12/16 |
| `D_ARITH_CODING_SUPPORTED` | 8 位构建定义为 1，保留算术解码 |
| `SIZEOF_SIZE_T` | 根据目标编译器的 `SIZE_MAX` 确定为 4 或 8，禁止写死宿主机值 |
| `INLINE` | 标准 C99 `inline` |
| `HIDDEN`、`THREAD_LOCAL`、`FALLTHROUGH` | 空，不依赖扩展属性或 TLS |
| `WITH_SIMD`、`WITH_SIMDE`、`WITH_PROFILE` | 不定义，关闭相应功能 |
| `HAVE_BUILTIN_CTZL`、`HAVE_INTRIN_H` | 不定义，使用普通 C 实现 |
| `C_ARITH_CODING_SUPPORTED` | 不定义，不编译编码器 |
| `MEM_SRCDST_SUPPORTED` | 不定义；内存输入使用 wrapper 自带的 source manager |

注意：上游大量使用 `#ifdef WITH_SIMD`。**不能用 `-DWITH_SIMD=0` 代替“不定义”**。这套最小构建不接受 `WITH_SIMD` 等上游 CMake 选项；所有配置以这里的配置头为准。

适用前提：8 位字节、32 位 `int`、32/64 位 `size_t`、正常的有符号算术右移。如果目标编译器对有符号数使用逻辑右移，需要在 `minimal/config/jconfig.h` 中增加 `RIGHT_SHIFT_IS_UNSIGNED=1`。16 位 `int`/`size_t` 平台不在此配置支持范围内。

## 4. 完全不使用 CMake

仓库根目录执行：

```sh
make -f minimal/Makefile -j4
```

输出：`build-minimal-make/libjpeg_wrapper.a`。不生成配置头，不调用 CMake。

例如使用 ARM GCC，以下 CPU 选项仅以 Cortex-M3 为例，应替换为实际芯片配置：

```sh
make -f minimal/Makefile -j4 \
  CC=arm-none-eabi-gcc AR=arm-none-eabi-ar \
  BUILD="$PWD/build-cortex-m3" \
  CFLAGS='-Os -mcpu=cortex-m3 -mthumb -mfloat-abi=soft -ffunction-sections -fdata-sections'
```

最终链接由你的固件工程完成，使用相同的 CPU/ABI 选项。若启用上述 section 选项，可在最终链接时使用 `-Wl,--gc-sections`。本工程不添加 `-march=native`，不启用 LTO，也不要求 NASM、线程库、PNG/zlib 或额外的 `-lm`。

## 5. 最简 CMake 参考工程

CMake 仅用于在 PC 上验证构建清单，或供你对照迁移。它只有一个静态库目标，没有 `ExternalProject_Add`、下载步骤、配置头生成或上游构建脚本。

```sh
cmake -S minimal -B build-minimal -DCMAKE_BUILD_TYPE=MinSizeRel
cmake --build build-minimal -j4
```

输出：`build-minimal/libjpeg_wrapper.a`。

交叉编译参考入口（要求自行安装对应工具链，并按芯片设置 `CMAKE_C_FLAGS`）：

```sh
cmake -S minimal -B build-arm \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/minimal/arm-none-eabi-toolchain.cmake"
cmake --build build-arm -j4
```

参考工具链文件没有替你选择芯片、FPU、链接脚本或启动文件；这些应复用固件工程的设置。本工程只生成静态库。

## 6. 运行库与内存要求

实际使用需要标准 C 的 `malloc`、`calloc`、`free`、内存操作和 `setjmp/longjmp`；不需要文件系统、环境变量或线程 API。上游公共头仍引用 `FILE` 类型并包含 `<stdio.h>`，因此工具链需要提供该头，但此清单不会引入 `fopen/fread/fprintf/exit` 等运行时符号。不要把 `jerror.c` 或 `jdatasrc.c` 加回来。

输出像素由调用方管理。wrapper 的解码上下文通过 `calloc/free` 管理，上游内存管理器也使用 `malloc/free`。如果全部内存都必须来自特殊内存池，需要由平台运行库/链接层提供相应分配函数；仅替换像素 buffer 的申请函数不能替换内部内存分配。不要让同一指针在不同分配器间释放。

渐进式 JPEG 会需要额外的系数缓冲；`pixels_size` 不是解码峰值内存。应在查询后自行限制宽高和像素容量。当前公开 API 保持不变，不包含全局内存配额参数。

仅应使用 `jpegw_*` API。本目录没有提供完整 libjpeg/TurboJPEG 的全部入口（如 `jpeg_std_error`、`jpeg_mem_src` 或编码函数）。普通 root 构建仍提供原有后端，不要把两个构建产生的库同时链接进同一程序。

最小后端会在头查询时对非 8 位精度返回 `JPEGW_UNSUPPORTED`；原有 TurboJPEG 后端可能在正式读取时才拒绝。其余公开的数据布局、所有权、容量检查和失败时结构体不变的约定相同。

## 7. 主机测试

```sh
cmake -S minimal -B build-minimal -DJPEGW_MINIMAL_TESTS=ON
cmake --build build-minimal -j4
ctest --test-dir build-minimal --output-on-failure

# 同一组测试，不使用 CMake
make -f minimal/Makefile -j4 check
```

测试使用已提交的 JPEG 数组，不链接编码器。涵盖 RGB/RGBA、灰度、渐进式 JPEG、CMYK 拒绝、容量和保护区、像素顺序、Alpha 及每个位置的截断。测试程序与数据只用于验证，不在库的源文件清单中。

如需重新生成测试数组，在原有根目录工程已构建的前提下，使用主机编译器编译 `tests/generate_fixtures.c`，链接该工程的 `libturbojpeg.a`，把其标准输出保存到 `tests/fixtures/jpeg_fixtures.h`。生成器不属于移植代码。
