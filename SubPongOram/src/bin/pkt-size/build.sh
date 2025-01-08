#!/usr/bin/env bash
set -euo pipefail

clang++ src/bin/pkt-size/main.cpp -I. "$@"
