#!/usr/bin/env bash
#
# Builds task in ./build, and runs the Jalali calendar tests against it.
#
# Usage: jalali/build-and-test.sh

set -euo pipefail

cd "$(dirname "$0")/.."

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target task_executable -j"$(nproc)"
python3 jalali/test_jalali.py build/src/task
