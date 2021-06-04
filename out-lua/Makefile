CC ?= clang
.PHONY: all clean

all: $(patsubst %.bc,lib%.so,$(wildcard *.bc)) lua.shared

clean:
	rm *.so *.bc lua.shared

lua.shared: _main.bc
	$(CC) $< -L. $(patsubst %.bc,-l%,$(filter-out _main.bc,$(wildcard *.bc))) -L. -ldl -lm -g -Wl,-rpath,$(shell pwd) -o lua.shared
	
lib%.so: %.bc
	$(CC) -shared -fuse-ld=lld -fPIC $< -o lib$(basename $<).so
