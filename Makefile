CC=clang
CXX=clang++
LLVM_CONFIG=llvm-config
CFORMAT ?= clang-format
CPPFILES = $(wildcard *.cpp)
CFILES = $(wildcard tests/*.c)
export

all: manual-split find-and-split-static split-llvm-extract

manual-split find-and-split-static split-llvm-extract: %: %.cpp
	$(CXX) -glldb $(shell $(LLVM_CONFIG) --link-shared --cppflags --ldflags --system-libs --libs) -std=c++17 -fno-exceptions $< -o $@

test-included: tests/test-mover-included.c
	$(CC) -c -emit-llvm $< -o test.bc
	./manual-split test.bc

test-llvm-extract: tests/test-llvm-extract.c
	mkdir -p out
	$(CC) -c -emit-llvm $< -o test.bc
	./split-llvm-extract test.bc -o out

test-llvm-extract-static: tests/test-llvm-extract-static.c
	mkdir -p out
	$(CC) -fPIC -c -emit-llvm $< -o test.bc
	./split-llvm-extract test.bc -o out

test-global-dependency: tests/test-global-dependency.c
	mkdir -p out
	$(CC) -fPIC -c -emit-llvm $< -o test.bc
	./split-llvm-extract test.bc -o out

test-compile: out
	$(CC) $(wildcard out/*.bc) -o out/executable

lib:
	$(CC) -O2 -shared -fPIC lib.bc -o libex.so
	$(CC) -fPIE -L. -lex  -Wl,-rpath,. main.bc -o test_lib

clang-format:
	$(CFORMAT) -i $(CPPFILES) $(CFILES)

clean:
	rm find-and-split-static manual-split split-llvm-extract
