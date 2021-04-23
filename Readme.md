# How to use the split utility

The split utility is used by giving it a LLVM bitcode file and it splits all of
the functions in it into separate files. 

Obtaining the final LLVM bitcode file can be done in multiple ways. The easiest
one being to use
[whole-program-llvm](https://github.com/travitch/whole-program-llvm) that gives
you the bitcode used to create the executable.

For example to get the bitcode from the cpython executable you build it like
that: 
```
LLVM_COMPILER="clang" CC="wllvm" ./configure 
LLVM_COMPILER="clang" make
``` 

Then extract the bitcode file from the binary using the `extract-bc` utility
from the `whole-program-llvm` project:
```
extract-bc python 
``` 

The last step would be to execute the split utility. This can be done
by running it like that: 
```
split python.bc -o outdir
```

