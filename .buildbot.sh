#!/usr/bin/env bash
# Buildbot script to check that `split-llvm-extract` works when applied to `lua` v5.4.0.
# Only RISCV64 is tested at the moment.

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
export PYTHONPATH=~/build/test-scripts
export SSHPORT=10021
export SSH_OPTIONS='-o "StrictHostKeyChecking no"'
export SCP_OPTIONS='-o "StrictHostKeyChecking no"'

make CC=$CC CXX=$CHERI/bin/clang++ CFLAGS="--config cheribsd-riscv64-purecap.cfg" LLVM_CONFIG=$CHERI/bin/llvm-config LLVM_LINK=$LLVM_LINK -j4

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

# Test the handling of `extern` variables of type `const * const` when joining
make CC=$CC CXX=$CHERI/bin/clang++ CFLAGS="--config cheribsd-riscv64-purecap.cfg" LLVM_CONFIG=$CHERI/bin/llvm-config LLVM_LINK=$LLVM_LINK LD_LIBRARY_PATH=$LD_LIBRARY_PATH test-extern-const-constptr
# Test visibility is correctly set on global variables and functions
make CC=$CC CXX=$CHERI/bin/clang++ CFLAGS="--config cheribsd-riscv64-purecap.cfg" LLVM_CONFIG=$CHERI/bin/llvm-config LLVM_LINK=$LLVM_LINK LD_LIBRARY_PATH=$LD_LIBRARY_PATH test-visibility

tmpdir=/tmp/lua-$(cat /dev/urandom | tr -dc 'a-zA-Z0-9' | fold -w 10 | head -n 1)/

mkdir -p $tmpdir
cd $tmpdir
	git clone --depth 1 -b v5.4.0 https://github.com/lua/lua.git .
	# Dry run of `make` to list the commands executed
	make -n CC=$CC MYCFLAGS="-std=c99 --config cheribsd-riscv64-purecap.cfg -c -emit-llvm" MYLDFLAGS="-Wl,-E -Wl,-v" AR="$AR rc" RANLIB=$RANLIB MYLIBS=" -ldl" -j 16 > make-commands.sh
	# Remove conflicting `-march`
	sed -i 's/-march=native//g' make-commands.sh
	# Build `.bc` instead of `.o` files
	sed -i 's/\.o/\.bc/g' make-commands.sh
	sh make-commands.sh
	$LLVM_LINK lua.bc liblua.a -o lua.linked.bc
	cp lua.linked.bc $repodir
cd $repodir

# Split `lua`
rm -rfv $tmpdir
./split-llvm-extract lua.linked.bc -o out-lua

echo "Running tests for 'riscv64' using QEMU..."
args=(
    --architecture riscv64
    # Qemu System to use
    --qemu-cmd $HOME/cheri/output/sdk/bin/qemu-system-riscv64cheri
    # Kernel (to avoid the default one)
    --kernel $HOME/cheri/output/rootfs-riscv64-purecap/boot/kernel/kernel
    # Bios (to avoid the default one)
    --bios bbl-riscv64cheri-virt-fw_jump.bin
    --disk-image $HOME/cheri/output/cheribsd-riscv64-purecap.img
    # Required build-dir in CheriBSD
    --build-dir .
    --ssh-port $SSHPORT
    --ssh-key $HOME/.ssh/id_ed25519.pub
    )

cd out-lua
	# Re-build it as a set of shared libraries
	make CC=$CC CFLAGS="--config cheribsd-riscv64-purecap.cfg" LLVM_CONFIG=$CHERI/bin/llvm-config -j32
	# Copy the interpreter to the running qemu instance and execute the tests
	python3 ../tests/run_cheri_tests.py "${args[@]}"
