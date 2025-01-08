#!/usr/bin/env bash
set -euo pipefail

# Make sure you are in the build dir, in which the built rworam-enclave binary can be found
oesign sign -e rworam-enclave -c ../rworam.conf -k ../rworam.key
