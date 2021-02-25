CC=clang
CXX=clang++
LLVM_CONFIG=llvm-config
export

all: main find-static

main: main.cpp
	$(CXX) -glldb $(shell $(LLVM_CONFIG) --link-shared --cppflags --ldflags --system-libs --libs) -std=c++17 -fno-exceptions main.cpp -o ./main

find-static: find-static.cpp
	$(CXX) -glldb $(shell $(LLVM_CONFIG) --link-shared --cppflags --ldflags --system-libs --libs) -std=c++17 -fno-exceptions find-static.cpp -o ./find-static

bitcode: file.c
	$(CC) -c -emit-llvm file.c
	./main file.bc

lib:
	$(CC) -O2 -shared -fPIC lib.bc -o libex.so
	$(CC) -fPIE -L. -lex  -Wl,-rpath,. main.bc -o test_lib
