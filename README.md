# my-jpeg-wrapper

C99 编写的 libjpeg-turbo 只读 wrapper。输入内存中的 JPEG，输出连续排列的 RGB / RGBA 像素。输出内存完全由调用方管理，可以使用 `malloc/free` 或嵌入式专用分配器。

仓库已包含官方 **libjpeg-turbo 3.2.1** 完整源码（`third_party/libjpeg-turbo`），无需另外下载。上游版本、提交号和许可证位置见 [third_party/README.md](third_party/README.md)。

## 使用

```c
#include "jpeg_wrapper.h"
#include <stdlib.h>

/* jpeg_data 指向 JPEG 编码数据，jpeg_size 为这段数据的实际字节长度。 */
jpegw_image imageTemp = {0};
jpegw_info info;
jpegw_mode mode = JPEGW_RGBA; /* 或 JPEGW_RGB */
jpegw_status rc = jpegw_get_info(jpeg_data, jpeg_size, mode, &info);
if (rc != JPEGW_OK) {
    /* 用 jpegw_status_string(rc) 获取错误描述。 */
    return;
}

/* 可先检查 info.width / height / pixels_size，限制应用允许的图片大小。 */
unsigned char *buffer = malloc(info.pixels_size); /* 换成你的专用分配器 */
if (!buffer)
    return;

rc = jpegw_read(&imageTemp, jpeg_data, jpeg_size, mode,
                buffer, info.pixels_size);
if (rc != JPEGW_OK) {
    free(buffer);
    return;
}

/* imageTemp.width / height / pixels / mode 已填充。
 * (x,y) 像素地址：imageTemp.pixels + y * info.stride + x * mode
 * mode 的数值为通道数：RGB=3，RGBA=4。
 */
/* ...使用像素... */
free(imageTemp.pixels); /* 配对使用你自己的释放函数 */
imageTemp.pixels = NULL;
```

可编译的辅助函数见 [examples/decode.c](examples/decode.c)。示例使用 `malloc`；核心 API 不申请或释放输出像素内存。

**必须传 JPEG 数据长度。** 宽高可以从 JPEG 头自动解析，但 C 指针不携带缓冲区长度，不能安全地仅凭指针扫描到图片结尾；不要用 `strlen`。长度应由文件大小、网络响应或存储记录提供。

## API 约定

- `jpegw_get_info(data, length, mode, &info)`：读取头，返回宽高、`stride` 和 `pixels_size`。它不会验证整个压缩数据流，查询成功后正式读取仍可能失败。
- `jpegw_read(&imageTemp, data, length, mode, buffer, capacity)`：重新解析尺寸，检查容量后解码。成功时 `imageTemp.pixels == buffer`，wrapper 不持有该内存。重复读取不会释放旧的 `imageTemp.pixels`，调用方负责其生命周期。
- 每通道 1 Byte，无行填充；从上到下、从左到右排列。RGB 为 `R G B R G B ...`，RGBA 为 `R G B A ...`，A 固定为 255（JPEG 无透明通道）。
- 支持 8 位灰度、RGB、YCbCr JPEG，以及渐进式 JPEG；灰度扩展为 RGB/RGBA。CMYK/YCCK 返回 `JPEGW_UNSUPPORTED`，不进行未经色彩管理的自动转换。其他位深在正式解码时返回失败。
- 解码错误或警告（含截断数据）均视为失败。不向标准错误输出库诊断，也不会因普通 JPEG 错误调用 `exit`。
- 查询失败时 `info` 不变；读取失败时 `imageTemp` 不变，但像素缓冲区可能已被部分写入，不能使用其中结果。容量不足时不写像素。
- 输入、输出像素和结果结构体不得重叠；调用期间输入必须有效且不能修改。查询与读取之间输入或 mode 改变时，应重新查询并分配。
- 无全局可变 wrapper 状态，独立调用可并发执行。不应用 EXIF 旋转或 ICC 色彩配置，也不做缩放。

### 嵌入式内存说明

外部管理的是**输出像素内存**。libjpeg-turbo 内部仍会通过标准运行库分配解码器、行缓存、渐进式系数等内存；此 API 不保证解码全过程零堆分配。若平台需要所有分配走特殊内存池，必须进一步适配上游内存管理/平台 C 运行库。`pixels_size` 也不等于解码全过程峰值内存；应用应在分配之前设置尺寸/容量上限。

## 构建和测试

需要 C 编译器、CMake（建议 3.18+；上游平台要求见其 BUILDING.md）和构建工具。x86 SIMD 默认开启，推荐安装 NASM；没有 NASM 时可显式关闭 SIMD：

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DWITH_SIMD=OFF
cmake --build build -j
ctest --test-dir build --output-on-failure
```

wrapper 目标为 `jpeg_wrapper`。在同一 CMake 工程中链接该目标，它会传递所需的 TurboJPEG 依赖。上游不支持 `add_subdirectory`，这里通过 `ExternalProject_Add` 构建原始源码，不修改上游文件。

```cmake
add_subdirectory(path/to/my-jpeg-wrapper)
target_link_libraries(your_app PRIVATE jpeg_wrapper)
```

`JPEGW_BUILD_TESTS=OFF` 可关闭测试；`JPEGW_USE_BUNDLED=OFF` 可改用系统已安装的 `turbojpeg.h` 和 `libturbojpeg`（TurboJPEG 2.x API 或兼容的 3.x）。链接的库是 **turbojpeg**，不是仅 `jpeg`。

交叉编译时传入绝对路径的 `-DCMAKE_TOOLCHAIN_FILE=...`，该文件也会传给上游构建；同时用 `-DJPEGW_BUILD_TESTS=OFF` 关闭宿主运行测试。工具链中使用绝对路径定位资源。上游其他平台配置见 [BUILDING.md](third_party/libjpeg-turbo/BUILDING.md)。

GCC/Clang 内存检查：

```sh
cmake -S . -B build-asan -DWITH_SIMD=OFF -DJPEGW_SANITIZE=ON
cmake --build build-asan -j
ctest --test-dir build-asan --output-on-failure
```

测试覆盖 RGB/RGBA、灰度、渐进式、CMYK 拒绝、宽高和容量、通道值/Alpha、输出边界保护、无效参数和 JPEG 每个截断位置。测试会在内存中生成 JPEG，不依赖外部图片。

## 接口文档

头文件中的接口、参数、返回值和内存约定均使用日语 Doxygen 注释，wrapper 关键步骤也使用日语注释。安装 Doxygen 后，在仓库根目录运行 `doxygen Doxyfile`，生成的文档入口为 `build-docs/html/index.html`。
