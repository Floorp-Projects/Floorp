# Fetch a tooltool manifest.

cd $HOME/workspace/build/src

chmod +x python/mozbuild/mozbuild/action/tooltool.py
: TOOLTOOL_CACHE                ${TOOLTOOL_CACHE:=/home/worker/tooltool-cache}
export TOOLTOOL_CACHE

./python/mozbuild/mozbuild/action/tooltool.py -v --url=http://relengapi/tooltool/ -m "${TOOLTOOL_MANIFEST}" fetch

cd $OLDPWD
