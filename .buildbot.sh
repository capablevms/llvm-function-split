#!/usr/bin/env bash
# Buildbot script to check that `split-llvm-extract`:
#   1. Works under linux when applied to sqlite3 (proof of concept);
#   2. works when applied to `lua` v5.4.0 cross compiling to CHERI.
# Note: only RISCV64 is tested at the moment.

set -e

if ! [ "$1" = "riscv64" ]; then
	echo "Only riscv64 is supported."
	# Exit gracefully because we do not want the build to fail
	exit 0
fi

find . -iname "*.c" -o -iname "*.h" -o -iname "*.cpp" -o -iname "*.hpp" | xargs ~/cheri/output/sdk/bin/clang-format --dry-run -Werror

repodir=$(pwd)

# Cross compile and split lua
cd $repodir

CHERI=$HOME/cheri/output/sdk
LD_LIBRARY_PATH=$CHERI/lib
CC=$CHERI/bin/clang 
LLVM_EXTRACT=$CHERI/bin/llvm-extract
AR=$CHERI/bin/llvm-ar 
RANLIB=$CHERI/bin/llvm-ranlib
LLVM_LINK=$CHERI/bin/llvm-link
LLVM_CONFIG=$CHERI/bin/llvm-config
PYTHONPATH=~/build/test-scripts
SSH_OPTIONS='-o "StrictHostKeyChecking no"'

make clean
make CC=$CC CXX=$CHERI/bin/clang++ CFLAGS="--config cheribsd-riscv64-purecap.cfg" LLVM_CONFIG=$LLVM_CONFIG LLVM_LINK=$LLVM_LINK -j4

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
	CC=$CC LD_LIBRARY_PATH=$LD_LIBRARY_PATH LLVM_EXTRACT=$LLVM_EXTRACT cargo test
cd .. 

# Test the handling of `extern` variables of type `const * const` when joining
make CC=$CC CXX=$CHERI/bin/clang++ CFLAGS="--config cheribsd-riscv64-purecap.cfg" LLVM_CONFIG=$CHERI/bin/llvm-config LLVM_LINK=$LLVM_LINK LD_LIBRARY_PATH=$LD_LIBRARY_PATH LLVM_EXTRACT=$LLVM_EXTRACT \
test-extern-const-constptr
# Test visibility is correctly set on global variables and functions
make CC=$CC CXX=$CHERI/bin/clang++ CFLAGS="--config cheribsd-riscv64-purecap.cfg" LLVM_CONFIG=$CHERI/bin/llvm-config LLVM_LINK=$LLVM_LINK LD_LIBRARY_PATH=$LD_LIBRARY_PATH LLVM_EXTRACT=$LLVM_EXTRACT \
test-visibility

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
LD_LIBRARY_PATH=$LD_LIBRARY_PATH LLVM_EXTRACT=$LLVM_EXTRACT ./split-llvm-extract lua.linked.bc -o out-lua

SSHPORT=10021
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
	PYTHONPATH=$PYTHONPATH SSH_OPTIONS=$SSH_OPTIONS SCP_OPTIONS=$SCP_OPTIONS python3 ../tests/run_cheri_tests.py "${args[@]}"