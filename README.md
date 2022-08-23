# How to use the split utility

The split utility operates on a LLVM bitcode file and it splits all of the
functions in it into separate (`.bc`) files.

Obtaining the final "fat" LLVM bitcode file can, theoretically, be done in
multiple ways. The recommended way is to use CapableVMs fork of "Go Whole
Program LLVM", gllvm. For example, to get the bitcode from a generic Makefile
project: 

- Install gllvm: 
go get github.com/capablevms/gllvm/cmd/...

- Build with GLLVM:
```bash
GCLANG=$HOME/go/bin/gclang
export GET_BC=$HOME/go/bin/get-bc
CC=$GCLANG LLVM_COMPILER_PATH=path/to/llvm/bin make
# or CC=$GCLANG ./configure if you are feeling lucky
```

- Extract the bitcode:

Finally, the split utility can be launched with:
```bash
./split-llvm-extract path/to/binary.bc -o outdir
```

To run the version of the binary just split, it is necessary to join the parts
(in `outdir`) together as shared libraries. A "joiner" Makefile can be found
[here](https://github.com/capablevms/llvm-function-split/blob/main/out-lua/Makefile).

Steps specific to CHERI cross-compilation can be found in
[.buildbot.sh](https://github.com/capablevms/llvm-function-split/blob/main/.buildbot.sh).

## Other (old and not maintained) variants

There are 2 other utilities that reside in this repository. The
[firstone](manual-split.cpp) shows how splitting can be done using the llvm API
instead of using `llvm-extract`.  The [other](find-and-split-static.cpp) utility
focuses on finding functions which are not public and should be. It also tries
to find ways to split the bitcode files while preserving the linkage of the
functions. The 2 previously presented splitters are not meant for general
purpose use, and are kept as a showcase of what is possible.


