#!/bin/bash

NON_UNIFIED_CFG=$GECKO_PATH/build/non-unified-compat

cat "$NON_UNIFIED_CFG" | xargs ./mach static-analysis check-syntax
