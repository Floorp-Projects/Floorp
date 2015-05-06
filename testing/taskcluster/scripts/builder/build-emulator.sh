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
test $MOZHARNESS_REF # mozharness ref
test $TARGET

. setup-ccache.sh

# First check if the mozharness directory is available. This is intended to be
# used locally in development to test mozharness changes:
#
#   $ docker -v your_mozharness:/home/worker/mozharness ...
#
tc-vcs checkout mozharness $MOZHARNESS_REPOSITORY $MOZHARNESS_REPOSITORY $MOZHARNESS_REV $MOZHARNESS_REF

# Figure out where the remote manifest is so we can use caches for it.

if [ -z "$MANIFEST" ]; then
  MANIFEST="$WORKSPACE/gecko/b2g/config/$TARGET/sources.xml"
fi

tc-vcs repo-checkout $WORKSPACE/B2G https://git.mozilla.org/b2g/B2G.git $MANIFEST

# Ensure symlink has been created to gecko...
rm -f $WORKSPACE/B2G/gecko
ln -s $WORKSPACE/gecko $WORKSPACE/B2G/gecko

debug_flag=""
if [ 0$B2G_DEBUG -ne 0 ]; then
  debug_flag='--debug'
fi

./mozharness/scripts/b2g_build.py \
  --config b2g/taskcluster-emulator.py \
  "$debug_flag" \
  --disable-mock \
  --work-dir=$WORKSPACE/B2G \
  --log-level=debug \
  --target=$TARGET \
  --b2g-config-dir=$TARGET \
  --checkout-revision=$GECKO_HEAD_REV \
  --base-repo=$GECKO_BASE_REPOSITORY \
  --repo=$GECKO_HEAD_REPOSITORY

# Move files into artifact locations!
mkdir -p $HOME/artifacts

ls -lah $WORKSPACE/B2G/out
ls -lah $WORKSPACE/B2G/objdir-gecko/dist/

mv $WORKSPACE/B2G/sources.xml $HOME/artifacts/sources.xml
mv $WORKSPACE/B2G/out/target/product/generic/tests/gaia-tests.zip $HOME/artifacts/gaia-tests.zip
mv $WORKSPACE/B2G/out/target/product/generic/tests/b2g-*.zip $HOME/artifacts/b2g-tests.zip
mv $WORKSPACE/B2G/out/emulator.tar.gz $HOME/artifacts/emulator.tar.gz
mv $WORKSPACE/B2G/objdir-gecko/dist/b2g-*.crashreporter-symbols.zip $HOME/artifacts/b2g-crashreporter-symbols.zip

ccache -s
