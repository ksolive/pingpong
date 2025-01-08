#!/usr/bin/env bash
set -euo pipefail

oeedger8r --search-path /opt/openenclave/include --search-path /opt/openenclave/include/openenclave/edl/sgx rworam.edl --trusted --trusted-dir api
