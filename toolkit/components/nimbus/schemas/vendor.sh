#!/bin/bash
set -euo pipefail

 # Path to mach relative to toolkit/components/nimbus/schemas/
MACH=$(realpath "../../../../mach")

if [[ $(uname -a) == *MSYS* ]]; then
  MACH="python ${MACH}"
fi

NPM="${MACH} npm"

# Strip the leading v from the tag to get the version number.
TAG="$1"
VERSION="${TAG:1}"
NAMESPACE="@mozilla/"
PACKAGE="nimbus-shared"
URL="https://registry.npmjs.org/${NAMESPACE}${PACKAGE}/-/${PACKAGE}-${VERSION}.tgz"

mkdir -p tmp
cd tmp

curl --proto '=https' --tlsv1.2 -sSf "${URL}" | tar -xzf - --strip-components 1

cp "schemas/experiments/NimbusExperiment.json" "../NimbusExperiment.schema.json"
cd ..

# Ensure the generated file ends with a newline
sed -i -e '$a\' NimbusExperiment.schema.json

rm -rf tmp
