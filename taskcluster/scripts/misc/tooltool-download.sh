# Fetch a tooltool manifest.

cd $MOZ_FETCHES_DIR

case "`uname -s`" in
Linux)
    TOOLTOOL_AUTH_FILE=/builds/relengapi.tok
    ;;
MINGW*)
    TOOLTOOL_AUTH_FILE=c:/builds/relengapi.tok
    ;;
esac

TOOLTOOL_DL_FLAGS=

if [ -e "$TOOLTOOL_AUTH_FILE" ]; then
    # When the worker has the relengapi token pass it down
    TOOLTOOL_DL_FLAGS="${TOOLTOOL_DL_FLAGS=} --authentication-file=$TOOLTOOL_AUTH_FILE"
fi

if [ -n "$TASKCLUSTER_PROXY_URL" ]; then
    # When the worker has the relengapi proxy setup, use it.
    TOOLTOOL_DL_FLAGS="${TOOLTOOL_DL_FLAGS=} --tooltool-url=${TASKCLUSTER_PROXY_URL}/tooltool.mozilla-releng.net/"
fi

if [ -n "$UPLOAD_DIR" ]; then
    TOOLTOOL_DL_FLAGS="${TOOLTOOL_DL_FLAGS=} --artifact-manifest $UPLOAD_DIR/toolchains.json"
fi

: TOOLTOOL_CACHE                ${TOOLTOOL_CACHE:=/builds/worker/tooltool-cache}
export TOOLTOOL_CACHE

if [ -n "$MOZ_TOOLCHAINS" ]; then
    echo This script should not be used for toolchain downloads anymore
    echo Use fetches
    exit 1
fi

if [ -z "$TOOLTOOL_MANIFEST" ]; then
    echo This script should not be used when there is no tooltool manifest set
    exit 1
fi

${GECKO_PATH}/mach artifact toolchain -v${TOOLTOOL_DL_FLAGS} --tooltool-manifest "${GECKO_PATH}/${TOOLTOOL_MANIFEST}"${TOOLTOOL_CACHE:+ --cache-dir ${TOOLTOOL_CACHE}} --retry 5

cd $OLDPWD
