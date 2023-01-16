# How to use the split utility

## Overview

The split utility operates on an LLVM bitcode file, splitting all of the
functions in it into separate (`.bc`) files.

Usage:

```
./split-llvm-extract <program-to-split>.bc -o <output-dir>
```

## Documentation

You can read about the LLVM splitter project in the [CapableVMs documentation
repository](https://github.com/capablevms/docs/tree/master/splitter) and in this
[Google Doc](https://docs.google.com/document/d/1RZ6dyl4BWiqrENs_9kkllS8aslQhetlNlkM5m5NjV7o/edit).

## Building the splitter

### Prerequisites

The minimum supported LLVM version is 13.

### Building

To build the [split
utility](https://github.com/capablevms/llvm-function-split/blob/main/split-llvm-extract.cpp),
run `make split-llvm-extract`. By default, this will build the splitter using
the clang and LLVM libraries installed on your system.

If you built LLVM from source, set `CXX`, `LLVM_CONFIG`, `LLVM_LINK` and
`LLVM_EXTRACT`, and then run:

```bash
make CXX=$CXX \
    LLVM_CONFIG=$LLVM_CONFIG \
    LLVM_LINK=$LLVM_LINK \
    LLVM_EXTRACT=$LLVM_EXTRACT \
    split-llvm-extract
```

For example, if you built [CHERI
LLVM](https://github.com/CTSRD-CHERI/llvm-project) using `cheribuild`, set
`CHERI` to your `cheribuild` installation directory (which is
`~/cheri/output/<SDK>` by default) and run:

```bash
make CXX=$CHERI/bin/clang++ \
    LLVM_CONFIG=$CHERI/bin/llvm-config \
    LLVM_LINK=$CHERI/bin/llvm-link \
    LLVM_EXTRACT=$CHERI/bin/llvm-extract \
    split-llvm-extract
```

### CHERI cross-compilation

Steps specific to CHERI cross-compilation can be found in
[.buildbot.sh](https://github.com/capablevms/llvm-function-split/blob/main/.buildbot.sh).

## Using the splitter

Once you have built the split utility, you will need to build the programs you
wish to apply it to. Each input program needs to be compiled to a single LLVM
bitcode file.

Obtaining the final "fat" LLVM bitcode file can, theoretically, be done in
multiple ways. The recommended way is to use CapableVMs fork of "Go Whole
Program LLVM", `gllvm`. For example, to get the bitcode from a generic Makefile
project:

- Install `gllvm`:
`go get github.com/capablevms/gllvm/cmd/...`

- Build with `gllvm`:
```bash
GCLANG=$HOME/go/bin/gclang
export GET_BC=$HOME/go/bin/get-bc
CC=$GCLANG LLVM_COMPILER_PATH=<path_to_llvm>/bin make
```

- Extract the bitcode:

Finally, the split utility can be launched with:
```bash
./split-llvm-extract path/to/binary.bc -o outdir
```

To run the version of the binary just split, it is necessary to join the parts
(in `outdir`) together as shared libraries. A "joiner" Makefile can be found
[here](https://github.com/capablevms/llvm-function-split/blob/main/out-lua/Makefile).

## Other (old and not maintained) variants

There are 2 other utilities that reside in this repository:
* [manual-split.cpp](manual-split.cpp), which shows how splitting can be done
  using the LLVM API instead of using `llvm-extract`.
* [find-and-split-static.cpp](find-and-split-static.cpp), which focuses on
  finding functions which are not public and should be. It also tries to find
  ways to split the bitcode files while preserving the linkage of the functions.

These splitters are not meant for general-purpose use, and are kept as a
showcase of what is possible.
