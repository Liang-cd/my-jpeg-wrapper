cmake_minimum_required(VERSION 3.16)
project(jpeg_wrapper_minimal LANGUAGES C)

# 外部プロジェクト、上流 CMake、構成ヘッダの生成は使用しません。
get_filename_component(JPEGW_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/sources.txt" JPEGW_SOURCES)
list(TRANSFORM JPEGW_SOURCES PREPEND "${JPEGW_ROOT}/")

add_library(jpeg_wrapper STATIC ${JPEGW_SOURCES})
target_compile_features(jpeg_wrapper PUBLIC c_std_99)
target_include_directories(jpeg_wrapper
    PUBLIC "${JPEGW_ROOT}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/config"
            "${JPEGW_ROOT}/third_party/libjpeg-turbo/src")
# OS の環境変数 API を不要にします。画像ファイル形式のマクロは定義しません。
target_compile_definitions(jpeg_wrapper PRIVATE NO_GETENV NO_PUTENV)

# ホストでの確認専用。組み込み向けライブラリのビルドでは不要です。
option(JPEGW_MINIMAL_TESTS "Build host-side wrapper tests" OFF)
if(JPEGW_MINIMAL_TESTS)
    enable_testing()
    add_executable(jpeg_wrapper_test "${JPEGW_ROOT}/tests/test_jpeg_wrapper.c")
    target_include_directories(jpeg_wrapper_test PRIVATE
        "${JPEGW_ROOT}/third_party/libjpeg-turbo/src")
    target_compile_definitions(jpeg_wrapper_test PRIVATE JPEGW_TEST_FIXTURES)
    target_link_libraries(jpeg_wrapper_test PRIVATE jpeg_wrapper)
    add_test(NAME jpeg_wrapper_minimal_test COMMAND jpeg_wrapper_test)
    # ランタイム依存を調べるため、stdio を使わない小さな実行ファイルもリンクします。
    add_executable(jpeg_wrapper_link_test "${JPEGW_ROOT}/tests/test_minimal_link.c")
    target_link_libraries(jpeg_wrapper_link_test PRIVATE jpeg_wrapper)
    add_test(NAME jpeg_wrapper_minimal_link COMMAND jpeg_wrapper_link_test)
endif()
