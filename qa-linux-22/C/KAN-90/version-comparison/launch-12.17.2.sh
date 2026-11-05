#!/bin/bash
set -eu
export MOZ_APP_REMOTINGNAME=qa-kan90-compare-12.17.2
exec "/workspace/qa-linux-22/C/KAN-90/version-comparison/env.sh" "/workspace/qa-linux-22/C/KAN-90/version-comparison/products/12.17.2/floorp/floorp" "$@"
