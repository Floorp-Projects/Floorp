#!/bin/bash
set -euo pipefail
qa_root=/workspace/qa-linux-22/browser-ui/full-product/xpt-generation
# Existing production assembly and its isolated build virtualenv are prerequisites.
cd /workspace/product3033/runtime
MOZBUILD_STATE_PATH=/workspace/.floorp-cloud/mozbuild ./mach python --virtualenv build "$qa_root/generate.py" --generate
cd /workspace/full-product-3031-3034/runtime
MOZBUILD_STATE_PATH=/workspace/full-product-3031-3034/mozbuild ./mach python --virtualenv build "$qa_root/compare-combined.py"
python3 "$qa_root/supply-xpt.py"
for qa_stage in /workspace/product3033 /workspace/full-product-3031-3034; do
 cd "$qa_stage/runtime"
 # Byte equality to canonical source proves empty-archive checks were not removed.
 git show HEAD:toolkit/mozapps/installer/packager.mk | cmp - toolkit/mozapps/installer/packager.mk
 qa_state="$qa_stage/mozbuild"
 if [ "$qa_stage" = /workspace/product3033 ]; then qa_state=/workspace/.floorp-cloud/mozbuild; fi
 MOZBUILD_STATE_PATH="$qa_state" ./mach package
done
