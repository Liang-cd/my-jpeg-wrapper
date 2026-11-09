# my-jpeg-wrapper — standard-libjpeg

本分支使用 **Independent JPEG Group（IJG）官方 libjpeg 10**，通过标准 libjpeg API 实现 JPEG 内存读取。对外保持原有 C API：`jpegw_get_info()`、`jpegw_read()`、`jpegw_status_string()`。

依赖的完整官方源码位于 `third_party/libjpeg`，未做修改。来源、版本、下载校验值与许可证位置见 [third_party/README.md](third_party/README.md)。

## 使用

```c
#include "jpeg_wrapper.h"
#include <stdlib.h>

/* jpeg_data 为 JPEG 数据，jpeg_size 为它的实际字节长度。 */
jpegw_image imageTemp = {0};
jpegw_info info;
jpegw_status rc = jpegw_get_info(jpeg_data, jpeg_size, JPEGW_RGBA, &info);
if (rc != JPEGW_OK)
    return;

/* 根据嵌入式平台需要替换申请/释放函数，并在此检查尺寸与容量上限。 */
void *pixels = malloc(info.pixels_size);
if (!pixels)
    return;

rc = jpegw_read(&imageTemp, jpeg_data, jpeg_size, JPEGW_RGBA,
                pixels, info.pixels_size);
if (rc != JPEGW_OK) {
    free(pixels);
    return;
}
/* 使用 imageTemp.width / height / pixels / mode。 */
free(imageTemp.pixels);
imageTemp.pixels = NULL;
```

可编译的函数示例见 [examples/decode.c](examples/decode.c)。必须传入 JPEG 指针和有效数据字节长度；宽高由 JPEG 头解析，指针本身不包含数据长度。

- RGB：每像素 `R G B`，各通道 1 Byte；RGBA：`R G B A`，A 固定为 255。
- 像素按从上到下、从左到右连续存放，无行填充。
- 标准 libjpeg 解码为 `JCS_RGB`。RGBA 在输出缓冲区中逐行从后向前原地扩展，不使用 TurboJPEG API 或 `JCS_EXT_*` 扩展，也不额外申请整幅 RGB 缓冲区。
- 支持 8 位灰度、RGB、YCbCr 和渐进式 JPEG。CMYK/YCCK、其他精度返回 `JPEGW_UNSUPPORTED`。不处理 EXIF 旋转或 ICC 色彩管理。
- 查询只解析头，查询成功不代表整个 JPEG 完整。损坏、截断和库警告在正式读取时返回失败，不补造结束标记。
- 失败时结果结构体不变；输出像素可能已被部分写入，不能使用。容量不足时不写像素。
- wrapper 不释放旧的 `imageTemp.pixels`。输入、输出和结果结构体不能重叠；独立调用使用独立解码上下文，可以并发。

## 嵌入式移植

完整说明见 [minimal/README.md](minimal/README.md)。

| 项目 | 配置 |
| --- | --- |
| 实现 | `src/jpeg_wrapper.c`（普通构建和最简构建共用） |
| 源文件 | [minimal/sources.txt](minimal/sources.txt)，共 25 个 C 文件 |
| Include | `include`、`minimal/config`、`third_party/libjpeg` |
| 命令行宏 | `NO_GETENV` |
| 配置头 | [minimal/config/jconfig.h](minimal/config/jconfig.h)，无需生成 |
| 不需要 | SIMD、汇编、TLS、操作系统环境变量、文件系统、PNG/zlib |

无 CMake 构建：

```sh
make -f minimal/Makefile -j4
```

输出 `build-minimal-make/libjpeg_wrapper.a`。交叉编译时传入 `CC`、`AR` 和芯片对应的 `CFLAGS`，配置 CPU 与浮点 ABI。

内存输入、错误处理均由 wrapper 使用标准 libjpeg 回调实现，不引入上游文件输入和默认错误输出。像素内存由外部管理，但解码上下文与 libjpeg 内部仍需要 `calloc/malloc/free`。运行库也需提供 `setjmp/longjmp` 和内存操作。专用内存池的适配说明见移植文档。

## CMake 与测试

根目录构建使用相同的显式文件清单，不调用上游 configure/CMake，不下载依赖：

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=MinSizeRel
cmake --build build -j4
ctest --test-dir build --output-on-failure
```

静态库为 `build/minimal/libjpeg_wrapper.a`；`JPEGW_BUILD_TESTS=OFF` 关闭测试，GCC/Clang 可用 `JPEGW_SANITIZE=ON` 开启 ASan/UBSan。

独立最简 CMake：

```sh
cmake -S minimal -B build-minimal -DJPEGW_MINIMAL_TESTS=ON
cmake --build build-minimal -j4
ctest --test-dir build-minimal --output-on-failure
```

纯 Make 主机测试：`make -f minimal/Makefile -j4 check`。测试使用已提交的 JPEG 数组，覆盖像素布局、RGB/RGBA、Alpha、灰度、渐进式、CMYK 拒绝、容量、边界和所有截断位置，不引入编码器。

在已有 CMake 工程中：

```cmake
set(JPEGW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(path/to/my-jpeg-wrapper)
target_link_libraries(your_app PRIVATE jpeg_wrapper)
```

头文件接口和实现关键步骤均保留日语注释。运行 `doxygen Doxyfile` 可生成日语接口文档，入口为 `build-docs/html/index.html`。

This software is based in part on the work of the Independent JPEG Group.
