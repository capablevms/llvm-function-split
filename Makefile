CC=clang
CXX=clang++
LLVM_CONFIG=llvm-config
export

all: main find-static

main: main.cpp
	$(CXX) -fuse-ld=lld -glldb $(shell $(LLVM_CONFIG) --link-shared --cppflags --ldflags --system-libs --libs) -std=c++17 -fno-exceptions main.cpp -o ./main

find-static: find-static.cpp
	$(CXX) -fuse-ld=lld -glldb $(shell $(LLVM_CONFIG) --link-shared --cppflags --ldflags --system-libs --libs) -std=c++17 -fno-exceptions find-static.cpp -o ./find-static

split: split.cpp
	$(CXX) ${CXXFLAGS} -fuse-ld=lld -O0 -g3 $(shell $(LLVM_CONFIG) --link-shared --cppflags --ldflags --system-libs --libs) -std=c++17 -fno-exceptions split.cpp -o ./split

test-included: tests/test-mover-included.c
	$(CC) -c -emit-llvm $< -o test.bc
	./main test.bc

test-llvm-extract: tests/test-llvm-extract.c
	mkdir -p out
	$(CC) -c -emit-llvm $< -o test.bc
	./split test.bc -o out

test-llvm-extract-static: tests/test-llvm-extract-static.c
	mkdir -p out
	$(CC) -fPIC -c -emit-llvm $< -o test.bc
	./split test.bc -o out

test-compile: out
	$(CC) $(wildcard out/*.bc) -o out/executable

lib:
	$(CC) -O2 -shared -fPIC lib.bc -o libex.so
	$(CC) -fPIE -L. -lex  -Wl,-rpath,. main.bc -o test_lib
