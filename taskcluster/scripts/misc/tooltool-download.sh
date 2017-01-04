# Fetch a tooltool manifest.

cd $HOME/workspace/build/src

chmod +x taskcluster/docker/recipes/tooltool.py
: TOOLTOOL_CACHE                ${TOOLTOOL_CACHE:=/home/worker/tooltool-cache}
export TOOLTOOL_CACHE

./taskcluster/docker/recipes/tooltool.py --url=http://relengapi/tooltool/ -m "${TOOLTOOL_MANIFEST}" fetch

cd $OLDPWD
