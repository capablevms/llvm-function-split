#/bin/sh

set -e

find . -iname "*.c" -o -iname "*.h" -o -iname "*.cpp" -o -iname "*.hpp" | xargs ~/cheri/output/sdk/bin/clang-format --dry-run -Werror

export CHERI=$HOME/cheri/output/sdk
export LD_LIBRARY_PATH=$CHERI/lib
export CC=$CHERI/bin/clang 
export LLVM_EXTRACT=$CHERI/bin/llvm-extract

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
