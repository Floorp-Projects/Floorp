# Fetch a tooltool manifest.

cd $HOME/workspace/build/src

: TOOLTOOL_CACHE                ${TOOLTOOL_CACHE:=/builds/worker/tooltool-cache}
export TOOLTOOL_CACHE

./mach artifact toolchain -v --tooltool-url=http://relengapi/tooltool/ --tooltool-manifest "${TOOLTOOL_MANIFEST}"${TOOLTOOL_CACHE:+ --cache-dir ${TOOLTOOL_CACHE}}

cd $OLDPWD
