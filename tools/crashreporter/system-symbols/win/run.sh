#!/bin/sh

set -v -e -x

base="$(realpath "$(dirname "$0")")"

export DUMP_SYMS_PATH="${MOZ_FETCHES_DIR}/dump_syms/dump_syms"

mkdir -p artifacts && \
ulimit -n 16384 && \
python3 "${base}/symsrv-fetch.py" artifacts/target.crashreporter-symbols.zip
