#!/usr/bin/env bash
set -euo pipefail

clang++ src/bin/gen-query-buf/main.cpp -I. "$@"
