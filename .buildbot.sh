#!/usr/bin/env bash
# Buildbot script to check that `split-llvm-extract`:
#   1. works when applied to `lua` v5.4.2 cross compiling to CHERI.

set -eou pipefail

echo "Running tests for 'morello-purecap' using QEMU..."
./tests/run_tests.sh morello-purecap
echo "Running tests for 'riscv64-purecap' using QEMU."
./tests/run_tests.sh riscv64-purecap
