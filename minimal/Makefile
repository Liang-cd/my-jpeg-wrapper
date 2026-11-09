# GNU Make + C99 コンパイラだけでビルドできます。CMake は不要です。
ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)
BUILD ?= $(ROOT)/build-minimal-make
CC ?= cc
AR ?= ar
CFLAGS ?= -Os
CPPFLAGS += -I$(ROOT)/include -I$(ROOT)/minimal/config \
            -I$(ROOT)/third_party/libjpeg -DNO_GETENV
SOURCES := $(shell cat $(ROOT)/minimal/sources.txt)
OBJECTS := $(addprefix $(BUILD)/,$(SOURCES:.c=.o))
HEADERS := $(wildcard $(ROOT)/minimal/config/*.h $(ROOT)/include/*.h \
                     $(ROOT)/third_party/libjpeg/*.h)
LIBRARY := $(BUILD)/libjpeg_wrapper.a

.PHONY: all check
all: $(LIBRARY)

# %.o は同じディレクトリ構造で保存し、同名ファイルの衝突を防ぎます。
$(BUILD)/%.o: $(ROOT)/%.c $(HEADERS) $(ROOT)/minimal/sources.txt
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -std=c99 -MMD -MP -c $< -o $@

$(LIBRARY): $(OBJECTS) $(ROOT)/minimal/sources.txt
	@mkdir -p $(dir $@)
	@rm -f $@
	$(AR) rcs $@ $(OBJECTS)

# check はホスト用です。ターゲットのファームウェアにはテストを含めません。
check: $(LIBRARY)
	$(CC) $(CPPFLAGS) $(CFLAGS) -std=c99 $(ROOT)/tests/test_jpeg_wrapper.c \
	    $(LIBRARY) $(LDFLAGS) $(LDLIBS) -o $(BUILD)/jpeg_wrapper_test
	$(BUILD)/jpeg_wrapper_test

-include $(OBJECTS:.o=.d)
