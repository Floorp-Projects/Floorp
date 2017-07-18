# Fetch a tooltool manifest.

cd $WORKSPACE/build/src

: TOOLTOOL_CACHE                ${TOOLTOOL_CACHE:=/home/worker/tooltool-cache}
export TOOLTOOL_CACHE

./mach artifact toolchain -v --tooltool-url=http://relengapi/tooltool/ --tooltool-manifest "${TOOLTOOL_MANIFEST}"${TOOLTOOL_CACHE:+ --cache-dir ${TOOLTOOL_CACHE}} --retry 5

cd $OLDPWD
