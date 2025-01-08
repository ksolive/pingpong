#!/usr/bin/env bash
set -euo pipefail

clang++ src/bin/gen-pkt-buf/main.cpp -I. "$@"
