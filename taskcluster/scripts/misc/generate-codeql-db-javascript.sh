#! /bin/bash -vex

set -x -e

export PATH=$MOZ_FETCHES_DIR/node/bin:$PATH

cd /builds/worker

$MOZ_FETCHES_DIR/codeql/codeql database create \
	--language=javascript \
	-J=-Xmx32768M \
	--source-root=$GECKO_PATH \
	codeql-database

# TODO Switch this to zst per 1637381 when this is used for static analysis jobs
tar cf codeql-db-javascript.tar codeql-database
xz codeql-db-javascript.tar
