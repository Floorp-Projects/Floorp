#!/bin/sh

set -v -e -x

base="$(realpath "$(dirname "$0")")"

mkdir -p artifacts
PYTHONPATH=$PWD python "${base}/symsrv-fetch.py" artifacts/target.crashreporter-symbols.zip
