CC=clang
CXX=clang++
LLVM_CONFIG=llvm-config
LLVM_LINK?=llvm-link
LLVM_DIS?=llvm-dis
CFORMAT ?= clang-format
CPPFILES = $(wildcard *.cpp)
CFILES = $(wildcard tests/**/*.c) $(wildcard tests/**/**/*.c)
export

all: manual-split find-and-split-static split-llvm-extract

manual-split find-and-split-static split-llvm-extract: %: %.cpp
	$(CXX) -glldb $(shell $(LLVM_CONFIG) --link-shared --cxxflags --ldflags --system-libs --libs) -std=c++17 $< -o $@

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

test-extern-const-constptr: $(wildcard tests/test-extern-const-constptr/*.c)
	rm -Rf *.bc
	rm -Rf out
	mkdir -p out
	$(CC) -fPIC -c -emit-llvm $^
	$(LLVM_LINK) *.bc -o test.bc
	./split-llvm-extract test.bc -o out
	cp tests/Makefile out/.
	cd out && $(MAKE)
	rm -Rf out

test-extern-internal: $(wildcard tests/test-extern-internal/*.c)
	rm -Rf *.bc
	rm -Rf out
	mkdir -p out
	$(CC) -fPIC -c -emit-llvm $^
	./split-llvm-extract test-extern-internal.bc -o out
	cd out && $(LLVM_DIS) _main.bc && cat _main.ll | grep "hidden"

test-visibility: $(wildcard tests/test-visibility/*.c)
	rm -Rf *.bc
	rm -Rf out
	mkdir -p out
	$(CC) -fPIC -c -emit-llvm $^
	$(LLVM_LINK) *.bc -o test.bc
	./split-llvm-extract test.bc -o out
	cp tests/Makefile out/.
	cd out && $(MAKE)
	rm -Rf out

test-compile: out
	$(CC) $(wildcard out/*.bc) -o out/executable

lib:
	$(CC) -O2 -shared -fPIC lib.bc -o libex.so
	$(CC) -fPIE -L. -lex  -Wl,-rpath,. main.bc -o test_lib

clang-format:
	$(CFORMAT) -i $(CPPFILES) $(CFILES)

clean:
	rm find-and-split-static manual-split split-llvm-extract
