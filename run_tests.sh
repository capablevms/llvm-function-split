#!/usr/bin/env bash
# Buildbot script to check that `split-llvm-extract`:
#   1. works when applied to `lua` v5.4.2 cross compiling to CHERI.

set -e

ARCH=
CHERI=

if [ "$1" = "morello-purecap" ]; then
	ARCH="morello"
	CHERI=$CHERI_DIR/"$ARCH"-sdk
	SSHPORT=10085
	args=(
	--architecture morello-purecap
	# Qemu System to use
	--qemu-cmd $CHERI_DIR/sdk/bin/qemu-system-morello
	--bios edk2-aarch64-code.fd
	--disk-image $CHERI_DIR/cheribsd-morello-purecap.img
	# Required build-dir in CheriBSD
	--build-dir .
	--ssh-port $SSHPORT
	--ssh-key $HOME/.ssh/id_ed25519.pub
	)
elif [ "$1" = "riscv64-purecap" ]; then
	ARCH="riscv64"
	CHERI=$CHERI_DIR/sdk
	SSHPORT=10021
	args=(
    --architecture riscv64
    # Qemu System to use
    --qemu-cmd $CHERI_DIR/sdk/bin/qemu-system-riscv64cheri
    # Kernel (to avoid the default one)
    --kernel $CHERI_DIR/rootfs-riscv64-purecap/boot/kernel/kernel
    # Bios (to avoid the default one)
    --bios bbl-riscv64cheri-virt-fw_jump.bin
    --disk-image $CHERI_DIR/cheribsd-riscv64-purecap.img
    # Required build-dir in CheriBSD
    --build-dir .
    --ssh-port $SSHPORT
    --ssh-key $HOME/.ssh/id_ed25519.pub
    )
else
	echo "Only purecap architectures, i.e. riscv64 and morello, are supported."
	# Exit gracefully because we do not want the build to fail
	exit 0
fi

find . -iname "*.c" -o -iname "*.h" -o -iname "*.cpp" -o -iname "*.hpp" | xargs $CHERI/bin/clang-format --dry-run -Werror

repodir=$(pwd)
cd $repodir

CONFIG_FLAGS="--config cheribsd-${ARCH}-purecap.cfg"
LD_LIBRARY_PATH=$CHERI/lib
CC=$CHERI/bin/clang
LLVM_EXTRACT=$CHERI/bin/llvm-extract
AR=$CHERI/bin/llvm-ar
RANLIB=$CHERI/bin/llvm-ranlib
LLVM_LINK=$CHERI/bin/llvm-link
LLVM_CONFIG=$CHERI/bin/llvm-config
LLVM_DIS=$CHERI/bin/llvm-dis
LLVM_AS=$CHERI/bin/llvm-as
SSH_OPTIONS='-o "StrictHostKeyChecking no"'

make CC=$CC CXX=$CHERI/bin/clang++ CFLAGS="${CONFIG_FLAGS}" LLVM_CONFIG=$LLVM_CONFIG LLVM_LINK=$LLVM_LINK -j4

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
make CC=$CC CXX=$CHERI/bin/clang++ CFLAGS="$CONFIG_FLAGS" LLVM_CONFIG=$CHERI/bin/llvm-config LLVM_LINK=$LLVM_LINK LD_LIBRARY_PATH=$LD_LIBRARY_PATH LLVM_EXTRACT=$LLVM_EXTRACT \
test-extern-const-constptr
# Test the handling of `extern` variables with internal/hidden attribute
make CC=$CC CXX=$CHERI/bin/clang++ CFLAGS="$CONFIG_FLAGS" LLVM_CONFIG=$CHERI/bin/llvm-config LLVM_LINK=$LLVM_LINK LD_LIBRARY_PATH=$LD_LIBRARY_PATH LLVM_EXTRACT=$LLVM_EXTRACT \
LLVM_DIS=$LLVM_DIS test-extern-internal
# Test visibility is correctly set on global variables and functions
make CC=$CC CXX=$CHERI/bin/clang++ CFLAGS="$CONFIG_FLAGS" LLVM_CONFIG=$CHERI/bin/llvm-config LLVM_LINK=$LLVM_LINK LD_LIBRARY_PATH=$LD_LIBRARY_PATH LLVM_EXTRACT=$LLVM_EXTRACT \
test-visibility

tmpdir=/tmp/lua-$(cat /dev/urandom | tr -dc 'a-zA-Z0-9' | fold -w 10 | head -n 1)/

# Install gllvm
go get github.com/capablevms/gllvm/cmd/...
GCLANG=$HOME/go/bin/gclang
GET_BC=$HOME/go/bin/get-bc
LLVM_COMPILER_PATH=$CHERI/bin
GLLVM_OBJCOPY=$LLVM_COMPILER_PATH/objcopy

mkdir -p $tmpdir
cd $tmpdir
	curl -O https://www.lua.org/ftp/lua-5.4.2.tar.gz
	tar -xvf lua-5.4.2.tar.gz
	cd lua-5.4.2/src
	wget https://raw.githubusercontent.com/CTSRD-CHERI/cheribsd-ports/main/lang/lua54/files/extra-patch-libedit-dl
	patch lua.c extra-patch-libedit-dl
	wget https://raw.githubusercontent.com/CTSRD-CHERI/cheribsd-ports/main/lang/lua54/files/patch-src_Makefile
	patch Makefile patch-src_Makefile
	make bsd CC=$GCLANG LLVM_COMPILER_PATH=$LLVM_COMPILER_PATH GLLVM_OBJCOPY=$GLLVM_OBJCOPY MYCFLAGS="-DLUA_USE_READLINE_DL -D__LP64__=1 -fPIC $CONFIG_FLAGS" MYLDFLAGS="$CONFIG_FLAGS" AR="${AR}" RANLIB=$RANLIB
	LLVM_COMPILER_PATH=$LLVM_COMPILER_PATH $GET_BC lua
	cp lua.bc $repodir
cd $repodir

# Split `lua`
rm -rfv $tmpdir
LD_LIBRARY_PATH=$LD_LIBRARY_PATH LLVM_EXTRACT=$LLVM_EXTRACT ./split-llvm-extract lua.bc -o out-lua
rm temp.bc
rm lua.bc

cd out-lua
	make CC=$CC CFLAGS="-DLUA_USE_READLINE_DL -D__LP64__=1 $CONFIG_FLAGS" LDFLAGS="$CONFIG_FLAGS" LLVM_CONFIG=$CHERI/bin/llvm-config
	# Copy the interpreter to the running qemu instance and execute the tests
	SSHPORT=$SSHPORT SSH_OPTIONS=$SSH_OPTIONS python3 ../tests/run_cheri_tests.py "${args[@]}"
	make clean
