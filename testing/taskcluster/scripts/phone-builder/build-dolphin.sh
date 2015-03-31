#! /bin/bash -vex

# Ensure all the scripts in this dir are on the path....
DIRNAME=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
PATH=$DIRNAME:$PATH

WORKSPACE=$1

### Check that require variables are defined
test -d $WORKSPACE
test $GECKO_HEAD_REPOSITORY # Should be an hg repository url to pull from
test $GECKO_BASE_REPOSITORY # Should be an hg repository url to clone from
test $GECKO_HEAD_REV # Should be an hg revision to pull down
test $MOZHARNESS_REPOSITORY # mozharness repository
test $MOZHARNESS_REV # mozharness revision
test $TARGET
test $VARIANT

. ../builder/setup-ccache.sh

# First check if the mozharness directory is available. This is intended to be
# used locally in development to test mozharness changes:
#
#   $ docker -v your_mozharness:/home/worker/mozharness ...
#
if [ ! -d mozharness ]; then
  tc-vcs checkout mozharness $MOZHARNESS_REPOSITORY $MOZHARNESS_REPOSITORY $MOZHARNESS_REV
fi

# Figure out where the remote manifest is so we can use caches for it.
MANIFEST=$(repository-url.py $GECKO_HEAD_REPOSITORY $GECKO_HEAD_REV b2g/config/$TARGET/sources.xml)
tc-vcs repo-checkout $WORKSPACE/B2G https://git.mozilla.org/b2g/B2G.git $MANIFEST

# Ensure symlink has been created to gecko...
rm -f $WORKSPACE/B2G/gecko
ln -s $WORKSPACE/gecko $WORKSPACE/B2G/gecko

debug_flag=""
if [ 0$B2G_DEBUG -ne 0 ]; then
  debug_flag='--debug'
fi

./mozharness/scripts/b2g_build.py \
  --config b2g/taskcluster-phone.py \
  "$debug_flag" \
  --disable-mock \
  --variant=$VARIANT \
  --work-dir=$WORKSPACE/B2G \
  --gaia-languages-file $WORKSPACE/B2G/device/sprd/scx15/languages.json \
  --log-level=debug \
  --target=$TARGET \
  --b2g-config-dir=$TARGET \
  --checkout-revision=$GECKO_HEAD_REV \
  --base-repo=$GECKO_BASE_REPOSITORY \
  --repo=$GECKO_HEAD_REPOSITORY

# Move files into artifact locations!
mkdir -p $HOME/artifacts

mv $WORKSPACE/B2G/upload/sources.xml $HOME/artifacts/sources.xml
mv $WORKSPACE/B2G/upload/b2g-*.crashreporter-symbols.zip $HOME/artifacts/b2g-crashreporter-symbols.zip
mv $WORKSPACE/B2G/upload/b2g-*.android-arm.tar.gz $HOME/artifacts/b2g-android-arm.tar.gz
mv $WORKSPACE/B2G/upload/${TARGET}.zip $HOME/artifacts/${TARGET}.zip
mv $WORKSPACE/B2G/upload/gaia.zip $HOME/artifacts/gaia.zip

ccache -s
