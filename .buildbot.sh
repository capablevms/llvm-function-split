#!/bin/sh

set -e

find . -iname "*.c" -o -iname "*.h" -o -iname "*.cpp" -o -iname "*.hpp" | xargs ~/cheri/output/sdk/bin/clang-format --dry-run -Werror

repodir=$(pwd)

export CHERI=$HOME/cheri/output/sdk
export LD_LIBRARY_PATH=$CHERI/lib
export CC=$CHERI/bin/clang 
export LLVM_EXTRACT=$CHERI/bin/llvm-extract
export AR=$CHERI/bin/llvm-ar 
export RANLIB=$CHERI/bin/llvm-ranlib
export LLVM_LINK=$CHERI/bin/llvm-link

make CC=$CC CXX=$CHERI/bin/clang++ LLVM_CONFIG=$CHERI/bin/llvm-config -j3

cd tests
	if ! [ -x "$(command -v cargo)" ]; then
		curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs > rustup.sh
		sh rustup.sh --default-host x86_64-unknown-linux-gnu \
			--default-toolchain stable \
			--no-modify-path \
			--profile minimal \
			-y
		source $HOME/.cargo/env
	fi
	cargo test
cd .. 

tmpdir=/tmp/lua-$(cat /dev/urandom | tr -dc 'a-zA-Z0-9' | fold -w 10 | head -n 1)/

mkdir -p $tmpdir
cd $tmpdir
	git clone --depth 1 -b v5.4.0 https://github.com/lua/lua.git .
	make CC=$CC MYCFLAGS="-std=c99 -DLUA_USE_LINUX -fembed-bitcode -flto" MYLDFLAGS="-Wl,-E -flto -fembed-bitcode" AR="$AR rc" RANLIB=$RANLIB MYLIBS=" -ldl" -j 16
	$LLVM_LINK *.o -o lua.linked.bc
	cp lua.linked.bc $repodir
cd $repodir

rm -rfv $tmpdir
mkdir -p out-lua
./split-llvm-extract lua.linked.bc -o out-lua

cd out-lua
	make -j 16
cd ..

cd tests-lua
	bash test.sh
cd ..
