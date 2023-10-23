#!/bin/sh
# Script to regenerate the npm packages used for ESLint by the builders.
# Requires

# Force the scripts working directory to be projdir/tools/lint/eslint.
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
cd ../../../
rm -rf node_modules/
rm -rf tools/lint/eslint/eslint-plugin-mozilla/node_modules
rm package-lock.json

echo "Installing eslint and external plugins..."
# ESLint and all _external_ plugins are listed in this directory's package.json,
# so a regular `npm install` will install them at the specified versions.
# The in-tree eslint-plugin-mozilla is kept out of this tooltool archive on
# purpose so that it can be changed by any developer without requiring tooltool
# access to make changes.
./mach npm install

echo "Creating eslint.tar.gz..."
tar cvz --exclude=eslint-plugin-mozilla --exclude=eslint-plugin-spidermonkey-js -f eslint.tar.gz node_modules

echo "Adding eslint.tar.gz to tooltool..."
rm tools/lint/eslint/manifest.tt
./python/mozbuild/mozbuild/action/tooltool.py add --visibility public --unpack eslint.tar.gz

echo "Uploading eslint.tar.gz to tooltool..."
./python/mozbuild/mozbuild/action/tooltool.py upload --message "node_modules folder update for tools/lint/eslint"

echo "Cleaning up..."
mv manifest.tt tools/lint/eslint/manifest.tt
rm eslint.tar.gz

cd $DIR

echo ""
echo "Update complete, please commit and check in your changes."
