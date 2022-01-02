# Fetch a tooltool manifest.

cd $MOZ_FETCHES_DIR

TOOLTOOL_DL_FLAGS=

if [ -n "$UPLOAD_DIR" ]; then
    TOOLTOOL_DL_FLAGS="${TOOLTOOL_DL_FLAGS=} --artifact-manifest $UPLOAD_DIR/toolchains.json"
fi

: TOOLTOOL_CACHE                ${TOOLTOOL_CACHE:=/builds/worker/tooltool-cache}
export TOOLTOOL_CACHE

if [ -z "$TOOLTOOL_MANIFEST" ]; then
    echo This script should not be used when there is no tooltool manifest set
    exit 1
fi

${GECKO_PATH}/mach artifact toolchain -v${TOOLTOOL_DL_FLAGS} --tooltool-manifest "${GECKO_PATH}/${TOOLTOOL_MANIFEST}"${TOOLTOOL_CACHE:+ --cache-dir ${TOOLTOOL_CACHE}} --retry 5

cd $OLDPWD
