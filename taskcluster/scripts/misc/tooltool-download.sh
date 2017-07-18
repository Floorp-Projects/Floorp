# Fetch a tooltool manifest.

cd $WORKSPACE/build/src

TOOLTOOL_DL_FLAGS=

if [ -n "$RELENGAPI_PORT" ]; then
    # When the worker has the relengapi proxy setup, use it.
    TOOLTOOL_DL_FLAGS="${TOOLTOOL_DL_FLAGS=} --tooltool-url=http://relengapi/tooltool/"
fi

: TOOLTOOL_CACHE                ${TOOLTOOL_CACHE:=/home/worker/tooltool-cache}
export TOOLTOOL_CACHE

./mach artifact toolchain -v${TOOLTOOL_DL_FLAGS} --tooltool-manifest "${TOOLTOOL_MANIFEST}"${TOOLTOOL_CACHE:+ --cache-dir ${TOOLTOOL_CACHE}} --retry 5

cd $OLDPWD
