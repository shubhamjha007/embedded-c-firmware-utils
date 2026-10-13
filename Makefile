CC      ?= gcc
CFLAGS  := -std=c99 -Wall -Wextra -Wpedantic -Werror -Iinclude -g
SRC     := src/ring_buffer.c src/crc16.c src/packet.c

.PHONY: test asan clean

test: build/test_runner
	./build/test_runner

build/test_runner: $(SRC) tests/test_main.c | build
	$(CC) $(CFLAGS) $^ -o $@

# Same tests under AddressSanitizer + UBSan to catch memory errors
asan: $(SRC) tests/test_main.c | build
	$(CC) $(CFLAGS) -fsanitize=address,undefined $^ -o build/test_asan
	./build/test_asan

build:
	mkdir -p build

clean:
	rm -rf build
