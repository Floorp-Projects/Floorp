#!/bin/sh
# Script to regenerate the npm packages used for eslint-plugin-mozilla by the builders.
# Requires

# Force the scripts working directory to be projdir/tools/lint/eslint/eslint-plugin-mozilla.
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd $DIR

if [ -z "$TASKCLUSTER_ACCESS_TOKEN" -o -z "$TASKCLUSTER_CLIENT_ID" -o -z "$TASKCLUSTER_ROOT_URL" ]; then
  echo "Please ensure you have run the taskcluster shell correctly to set"
  echo "the TASKCLUSTER_ACCESS_TOKEN, TASKCLUSTER_CLIENT_ID and"
  echo "TASKCLUSTER_ROOT_URL environment variables."
  echo "See https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint/enabling-rules.html"
  exit 1;
fi

echo ""
echo "Removing node_modules and package-lock.json..."
# Move to the top-level directory.
rm -rf node_modules
rm package-lock.json

echo "Installing modules for eslint-plugin-mozilla..."
../../../../mach npm install

echo "Creating eslint-plugin-mozilla.tar.gz..."
tar cvz -f eslint-plugin-mozilla.tar.gz node_modules

echo "Adding eslint-plugin-mozilla.tar.gz to tooltool..."
rm -f manifest.tt
../../../../python/mozbuild/mozbuild/action/tooltool.py add --visibility public --unpack eslint-plugin-mozilla.tar.gz --url="https://tooltool.mozilla-releng.net/"

echo "Uploading eslint-plugin-mozilla.tar.gz to tooltool..."
../../../../python/mozbuild/mozbuild/action/tooltool.py upload --message "node_modules folder update for tools/lint/eslint/eslint-plugin-mozilla" --url="https://tooltool.mozilla-releng.net/"

echo "Cleaning up..."
rm eslint-plugin-mozilla.tar.gz

echo ""
echo "Update complete, please commit and check in your changes."
