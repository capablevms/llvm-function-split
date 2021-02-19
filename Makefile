all:
	clang++ -glldb $(shell llvm-config --link-shared --cppflags --ldflags --system-libs --libs) -std=c++17 -fno-exceptions main.cpp -o ./main

bitcode: file.c
	clang -c -emit-llvm file.c
	./main file.bc

lib:
	clang -O2 -shared -fPIC lib.bc -o libex.so
	clang -fPIE -L. -lex  -Wl,-rpath,. main.bc -o test_lib
