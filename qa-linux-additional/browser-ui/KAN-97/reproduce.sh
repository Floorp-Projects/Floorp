#!/bin/bash
set -euo pipefail
qa=/workspace/qa-linux-additional/browser-ui/KAN-97
# Existing official/combined/Firefox products, Xvfb/XTest/Openbox and esbuild are inputs.
# Clone the source branch at /workspace/floorp-kan97; no canonical product is modified.
export PYTHONPATH=/workspace/qa-linux-22/browser-ui/python
node "$qa/unit/bundle.cjs" "$qa/unit/after.js"
QA_BASELINE=1 node "$qa/unit/bundle.cjs" "$qa/unit/before.js"
node "$qa/prepare-candidate.cjs"
# prepare-candidate.py requires a fresh own /workspace/kan97-runtime/candidate.
python "$qa/prepare-candidate.py"
python "$qa/run.py" official 29974
python "$qa/run.py" combined 29975
python "$qa/run.py" firefox 29976
python "$qa/run.py" patched 29977
python "$qa/run.py" patched 29978 metadata
python "$qa/audit.py"
