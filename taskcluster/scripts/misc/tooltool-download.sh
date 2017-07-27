# Fetch a tooltool manifest.

cd $WORKSPACE/build/src

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

if [ -n "$RELENGAPI_PORT" ]; then
    # When the worker has the relengapi proxy setup, use it.
    TOOLTOOL_DL_FLAGS="${TOOLTOOL_DL_FLAGS=} --tooltool-url=http://relengapi/tooltool/"
fi

if [ -n "$UPLOAD_DIR" ]; then
    TOOLTOOL_DL_FLAGS="${TOOLTOOL_DL_FLAGS=} --artifact-manifest $UPLOAD_DIR/toolchains.json"
fi

: TOOLTOOL_CACHE                ${TOOLTOOL_CACHE:=/home/worker/tooltool-cache}
export TOOLTOOL_CACHE

./mach artifact toolchain -v${TOOLTOOL_DL_FLAGS}${TOOLTOOL_MANIFEST:+ --tooltool-manifest "${TOOLTOOL_MANIFEST}"}${TOOLTOOL_CACHE:+ --cache-dir ${TOOLTOOL_CACHE}} --retry 5${MOZ_TOOLCHAINS:+ ${MOZ_TOOLCHAINS}}

cd $OLDPWD
