#!/bin/sh
set -euo pipefail

# Path to mach relative to /third_party/js/cfworker
MACH=$(realpath "../../../mach")

if [[ $(uname -a) == *MSYS* ]]; then
  MACH="python ${MACH}"
fi

NODE="${MACH} node"
NPM="${MACH} npm"

# Delete empty vestigial directories.
rm -rf .changeset/ .github/ .vscode/

# Patch typescript config to ensure we have LF endings.
patch tsconfig-base.json tsconfig-base.json.patch

cd packages/json-schema

# Install dependencies.
${NPM} install --also=dev

# Compile TypeScript into JavaScript.
${NPM} run build

# Install rollup and bundle into a single module.
${NPM} install rollup@~2.67.x
${NODE} node_modules/rollup/dist/bin/rollup \
    dist/index.js \
    --file json-schema.js \
    --format cjs

cd ../..

# Patch the CommonJS module into a regular JS file and include a copyright notice.
patch packages/json-schema/json-schema.js json-schema.js.patch
awk -f exports.awk packages/json-schema/json-schema.js >json-schema.js

# Remove source files we no longer need.
rm -rf packages/ tsconfig-base.json
