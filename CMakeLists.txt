cmake_minimum_required(VERSION 3.18)
project(my_jpeg_wrapper VERSION 1.0.0 LANGUAGES C)

option(JPEGW_USE_BUNDLED "Build bundled libjpeg-turbo" ON)
option(JPEGW_BUILD_TESTS "Build wrapper tests" ON)
option(JPEGW_SANITIZE "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

if(JPEGW_SANITIZE AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

if(JPEGW_USE_BUNDLED)
    include(ExternalProject)
    set(turbo_binary "${CMAKE_CURRENT_BINARY_DIR}/libjpeg-turbo-build")
    set(turbo_name "${CMAKE_STATIC_LIBRARY_PREFIX}turbojpeg${CMAKE_STATIC_LIBRARY_SUFFIX}")
    if(MSVC)
        set(turbo_name "turbojpeg-static.lib")
    endif()
    set(turbo_archive "${turbo_binary}/${turbo_name}")
    if(CMAKE_CONFIGURATION_TYPES)
        set(turbo_archive "${turbo_binary}/$<CONFIG>/${turbo_name}")
    endif()
    set(turbo_args
        -DENABLE_SHARED=OFF -DENABLE_STATIC=ON -DWITH_TURBOJPEG=ON
        # 組み込み向けに、上流の設定だけで SIMD・アセンブリ最適化を無効化します。
        -DWITH_SIMD=OFF -DREQUIRE_SIMD=OFF
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        "-DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}")
    if(CMAKE_TOOLCHAIN_FILE)
        list(APPEND turbo_args "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}")
    endif()
    if(JPEGW_SANITIZE AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        list(APPEND turbo_args
            "-DCMAKE_C_FLAGS=${CMAKE_C_FLAGS} -fsanitize=address,undefined -fno-omit-frame-pointer")
    endif()
    ExternalProject_Add(jpegw_turbo_build
        SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libjpeg-turbo"
        BINARY_DIR "${turbo_binary}"
        CMAKE_ARGS ${turbo_args}
        BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR>
                      --config $<CONFIG> --target turbojpeg-static
        INSTALL_COMMAND ""
        BUILD_BYPRODUCTS "${turbo_archive}")
    add_library(jpegw_turbo STATIC IMPORTED)
    set_target_properties(jpegw_turbo PROPERTIES IMPORTED_LOCATION "${turbo_archive}")
    add_dependencies(jpegw_turbo jpegw_turbo_build)
    if(UNIX)
        set_property(TARGET jpegw_turbo PROPERTY INTERFACE_LINK_LIBRARIES m)
    endif()
    set(JPEGW_TURBOJPEG jpegw_turbo)
    set(JPEGW_TURBOJPEG_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libjpeg-turbo/src")
else()
    find_path(JPEGW_TURBOJPEG_INCLUDE turbojpeg.h REQUIRED)
    find_library(JPEGW_TURBOJPEG turbojpeg REQUIRED)
endif()

add_library(jpeg_wrapper STATIC src/jpeg_wrapper.c)
target_include_directories(jpeg_wrapper PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(jpeg_wrapper PRIVATE "${JPEGW_TURBOJPEG_INCLUDE}")
target_link_libraries(jpeg_wrapper PUBLIC ${JPEGW_TURBOJPEG})
target_compile_features(jpeg_wrapper PUBLIC c_std_99)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(jpeg_wrapper PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

if(JPEGW_BUILD_TESTS)
    enable_testing()
    add_executable(jpeg_wrapper_test tests/test_jpeg_wrapper.c)
    target_include_directories(jpeg_wrapper_test PRIVATE "${JPEGW_TURBOJPEG_INCLUDE}")
    target_link_libraries(jpeg_wrapper_test PRIVATE jpeg_wrapper)
    add_test(NAME jpeg_wrapper_test COMMAND jpeg_wrapper_test)
endif()

add_library(jpeg_wrapper_example OBJECT examples/decode.c)
target_link_libraries(jpeg_wrapper_example PRIVATE jpeg_wrapper)
