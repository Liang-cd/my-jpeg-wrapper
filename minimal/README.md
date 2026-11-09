# 标准 IJG libjpeg：嵌入式移植清单

本分支使用 IJG 官方 **libjpeg 10**，源码位于 `third_party/libjpeg`，未做修改。wrapper 只使用标准 libjpeg 解码接口，普通工程与最简工程共用 `src/jpeg_wrapper.c`。

## 源文件

完整列表见 [sources.txt](sources.txt)：共 **25 个 C 文件**，路径均相对于仓库根目录。CMake 与 Makefile 读取同一份清单；迁移到 IDE/厂商 SDK 时逐行加入即可，不要使用目录通配符把所有上游代码都编译进去。

- `src/jpeg_wrapper.c`：公开 API、严格内存输入 source manager、自定义错误处理、RGB/RGBA 输出。
- 24 个上游文件：解码入口、控制、标记、Huffman/算术解码、IDCT、颜色转换/采样、量化和内存工具。

不编译编码器、转码器、命令行工具、上游默认错误处理 `jerror.c`、文件输入 `jdatasrc.c`。渐进式 Huffman 解码包含在此版上游的 `jdhuff.c` 中。

上游调度仍引用量化、多个 IDCT 等模块，因此保留清单中的这些符号依赖；wrapper 实际选择 `JDCT_ISLOW` 整数 IDCT。`jidctflt.c` 是上游调度依赖，无硬件 FPU 的芯片应使用软件浮点工具链配置。不要直接删除这些模块，也不要混入其他版本的头文件或源文件。

只应使用公开的 `jpegw_*` API；本静态库没有提供完整 libjpeg 的编码入口、`jpeg_std_error()`、`jpeg_mem_src()` 等未使用的函数。

如果平台已经提供标准 libjpeg，可只编译 `src/jpeg_wrapper.c` 并链接平台的 libjpeg，不再编译本清单中的 24 个库文件。必须使用该平台库对应的 `jpeglib.h`、`jconfig.h` 和 ABI，不能将本分支的 libjpeg 10 头文件与另一版本库混用。

## Include 路径

```text
include
minimal/config
third_party/libjpeg
```

应用程序只需要 `include`；编译 wrapper 和上游库文件时使用上述全部路径。目录结构要保留。`minimal/config/jconfig.h` 是独立配置文件，不是上游源码补丁，也无需 CMake 生成。

## 预定义宏

命令行/IDE 只需要：

```text
NO_GETENV
```

`jconfig.h` 中的标准 C 配置：

| 宏 | 配置 |
| --- | --- |
| `HAVE_PROTOTYPES` | 定义 |
| `HAVE_UNSIGNED_CHAR` | 定义 |
| `HAVE_UNSIGNED_SHORT` | 定义 |
| `HAVE_STDDEF_H` | 定义 |
| `HAVE_STDLIB_H` | 定义 |
| `RIGHT_SHIFT_IS_UNSIGNED` | 默认不定义，假定有符号右移为算术右移 |

IJG 自带的 `jpeglib.h` 定义 `JPEG_LIB_VERSION=100`；`jmorecfg.h` 默认 `JPEG_DATA_PRECISION=8`、`BITS_IN_JSAMPLE=8`。不要在命令行用其他值覆盖，也不要添加 libjpeg-turbo 的 `WITH_SIMD`、`JCS_EXT_*` 等宏。

目标应提供 8 位字节、32 位 `int`、32/64 位 `size_t`。CPU、端序和 ABI 宏由交叉编译器提供，不手工伪造。如果编译器对有符号数进行逻辑右移，需在 `jconfig.h` 中定义 `RIGHT_SHIFT_IS_UNSIGNED`。

## 不使用 CMake

仓库根目录执行：

```sh
make -f minimal/Makefile -j4
```

输出：`build-minimal-make/libjpeg_wrapper.a`。

交叉编译示例（CPU 参数以 Cortex-M3 为例，替换为实际芯片）：

```sh
make -f minimal/Makefile -j4 \
  CC=arm-none-eabi-gcc AR=arm-none-eabi-ar \
  BUILD="$PWD/build-cortex-m3" \
  CFLAGS='-Os -mcpu=cortex-m3 -mthumb -mfloat-abi=soft -ffunction-sections -fdata-sections'
```

每个文件使用同样的 include 和宏。例如：

```sh
cc -std=c99 -Os -DNO_GETENV \
  -Iinclude -Iminimal/config -Ithird_party/libjpeg \
  -c src/jpeg_wrapper.c -o jpeg_wrapper.o
```

最终链接由固件工程完成，使用相同 CPU/ABI。可在支持的工具链中使用 `-Wl,--gc-sections`。本工程不要求额外的 `-lm`、线程库、NASM、PNG/zlib，也不设置 `-march=native` 或 LTO。

## 最简 CMake 参考

```sh
cmake -S minimal -B build-minimal -DCMAKE_BUILD_TYPE=MinSizeRel
cmake --build build-minimal -j4
```

输出：`build-minimal/libjpeg_wrapper.a`。这是一个静态库目标，读取显式列表；没有外部项目、下载、配置头生成或自动硬件加速探测。

交叉编译参考入口：

```sh
cmake -S minimal -B build-arm \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/minimal/arm-none-eabi-toolchain.cmake"
cmake --build build-arm -j4
```

工具链文件仅提供 Generic/ARM GCC 入口。请复用固件工程的 CPU、FPU/浮点 ABI、启动文件及链接脚本配置。未针对特定芯片做实机验证。

## 标准 API 与内存约定

`jpegw_get_info()` 自动解析宽高、stride 与所需输出容量；`jpegw_read()` 检查容量后读取。调用方仍需传 JPEG 的有效字节长度。

- RGB 使用标准 `JCS_RGB` 输出，每通道 1 Byte。
- RGBA 先将每行 RGB 写到调用方缓冲区该行的起始位置，再从后往前原地扩展为 RGBA，Alpha 255，不增加整幅像素缓冲区。
- 灰度、RGB、YCbCr 的 8 位 JPEG（含渐进式）受支持；其他色空间/精度返回 `JPEGW_UNSUPPORTED`。
- 失败时结构体不变，像素缓冲区可能部分写入；容量不足时不写像素。输出内存由外部释放，旧的 `imageTemp.pixels` 不会被自动释放。
- 输入、输出和结构体不能重叠。独立调用没有共享的 wrapper 可变状态。

解码上下文使用 `calloc/free`，上游内部使用 `malloc/free`，另需标准内存操作和 `setjmp/longjmp`。特殊内存池需要在平台运行库/链接层统一适配这些函数；不要混用分配器。`pixels_size` 不是解码峰值内存，特别是渐进式 JPEG，应由应用设置尺寸/容量上限。

不需要文件系统、环境变量、TLS 或线程 API。上游头文件需要 `<stdio.h>` 中的 `FILE` 类型，但本清单不引入 `fopen/fread/fprintf/exit` 运行时符号。错误通过回调返回状态码；不补造截断 JPEG 的结束标记，不向终端输出。

## 主机测试

```sh
cmake -S minimal -B build-minimal -DJPEGW_MINIMAL_TESTS=ON
cmake --build build-minimal -j4
ctest --test-dir build-minimal --output-on-failure
make -f minimal/Makefile -j4 check
```

主机测试使用已提交的 JPEG 数组，不将测试或编码器加入静态库。根目录可用 `JPEGW_SANITIZE=ON` 运行 ASan/UBSan。

可选的 `tests/generate_fixtures.c` 使用标准 libjpeg 编码 API 重新生成数组；它需要主机上包含编码器的完整 IJG libjpeg，不属于此解码库的移植文件。现有数组无需重新生成。
