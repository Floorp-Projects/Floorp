#! /bin/bash -vex

set -x -e

####
# Wrapper for building the CodeQL database with the build-linux script
####

cd /builds/worker

$MOZ_FETCHES_DIR/codeql/codeql database create \
  --language=cpp \
  -J=-Xmx32768M \
  --command="${GECKO_PATH}/taskcluster/scripts/builder/build-linux.sh" \
  codeql-database

# TODO Switch this to zst per 1637381 when this is used for static analysis jobs
tar cf codeql-db-cpp.tar codeql-database
xz codeql-db-cpp.tar
