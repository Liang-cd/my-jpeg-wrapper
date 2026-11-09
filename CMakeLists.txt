cmake_minimum_required(VERSION 3.16)
project(my_jpeg_wrapper VERSION 2.0.0 LANGUAGES C)

option(JPEGW_BUILD_TESTS "Build host-side wrapper tests" ON)
option(JPEGW_SANITIZE "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
if(JPEGW_SANITIZE AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

if(JPEGW_BUILD_TESTS)
    enable_testing()
endif()
# ルートと最小工程で同じソース一覧・構成ヘッダ・API 実装を使用します。
set(JPEGW_MINIMAL_TESTS ${JPEGW_BUILD_TESTS})
add_subdirectory(minimal)

add_library(jpeg_wrapper_example OBJECT examples/decode.c)
target_link_libraries(jpeg_wrapper_example PRIVATE jpeg_wrapper)
