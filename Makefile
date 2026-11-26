.RECIPEPREFIX = >
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -g -Iinclude
SAN = -fsanitize=address,undefined
LIBS = -lsodium -lpng -lm
SRC = src/bmp.c src/lsb.c src/stegocrypt.c src/payload.c src/imgio.c src/wav.c src/detect.c src/report.c src/cli_extra.c
HDR = include/bmp.h include/lsb.h include/stegocrypt.h include/payload.h include/imgio.h include/wav.h include/detect.h include/report.h include/cli.h
TESTS = bmp lsb crypt keyed png wav detect report

all: build/stego

build:
> mkdir -p build

build/stego: src/main.c $(SRC) $(HDR) | build
> $(CC) $(CFLAGS) -O2 -o $@ src/main.c $(SRC) $(LIBS)

test: | build
> @for t in $(TESTS); do \
>   $(CC) $(CFLAGS) $(SAN) -o build/test_$$t tests/test_$$t.c $(SRC) $(LIBS) || exit 1; \
>   ./build/test_$$t || exit 1; \
> done

clean:
> rm -rf build

.PHONY: all test clean
