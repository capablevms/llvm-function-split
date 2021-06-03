CC ?= clang
.PHONY: all

all: $(patsubst %.bc,lib%.so,$(wildcard *.bc)) program

program: _main.bc
	$(CC) $< -L. $(patsubst %.bc,-l%,$(filter-out _main.bc,$(wildcard *.bc))) -o program

lib%.so: %.bc
	$(CC) -shared -fuse-ld=lld -fPIC $< -o lib$(basename $<).so
