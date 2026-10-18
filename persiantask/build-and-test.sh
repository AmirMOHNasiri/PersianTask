#!/usr/bin/env bash
#
# Builds task in ./build, and runs the PersianTask tests against it.
#
# Usage: persiantask/build-and-test.sh

set -euo pipefail

cd "$(dirname "$0")/.."

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target task_executable -j"$(nproc)"
python3 persiantask/test_jalali.py build/src/task
python3 persiantask/test_schedule.py build/src/task
