#/bin/sh

set -e

export CHERI=$HOME/cheri/output/sdk
export LD_LIBRARY_PATH=$CHERI/lib

make CC=$CHERI/bin/clang CXX=$CHERI/bin/clang++ LLVM_CONFIG=$CHERI/bin/llvm-config
make bitcode CC=$CHERI/bin/clang CXX=$CHERI/bin/clang++ 

