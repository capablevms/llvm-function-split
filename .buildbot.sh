#/bin/sh

set -e

export CHERI=$HOME/cheri/output/sdk
export LD_LIBRARY_PATH=$CHERI/lib
export CC=$CHERI/bin/clang 

make CC=$CC CXX=$CHERI/bin/clang++ LLVM_CONFIG=$CHERI/bin/llvm-config
make split CC=$CC CXX=$CHERI/bin/clang++ LLVM_CONFIG=$CHERI/bin/llvm-config

make test-included CC=$CC CXX=$CHERI/bin/clang++ 

LLVM_EXTRACT=$CHERI/bin/llvm-extract make test-llvm-extract CC=$CC CXX=$CHERI/bin/clang++ 
make CC=$CC test-compile 
rm -rfv out

LLVM_EXTRACT=$CHERI/bin/llvm-extract make test-llvm-extract-static CC=$CC CXX=$CHERI/bin/clang++ 
make CC=$CC test-compile 
cd out
for file in *.bc; do
    $CC -shared -fuse-ld=lld -fPIC $file -o lib${file/.bc/.so}
done
$CC _main.bc -L. -l_a -l_b -l_cc -l_data -o test
cd ..
rm -rfv out
