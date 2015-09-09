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
test $TARGET

. setup-ccache.sh

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

rm -rf $WORKSPACE/B2G/out/target/product/generic/tests/

gecko_objdir=/home/worker/objdir-gecko/objdir

$WORKSPACE/gecko/testing/mozharness/scripts/b2g_build.py \
  --config b2g/taskcluster-emulator.py \
  "$debug_flag" \
  --disable-mock \
  --work-dir=$WORKSPACE/B2G \
  --log-level=debug \
  --target=$TARGET \
  --b2g-config-dir=$TARGET \
  --checkout-revision=$GECKO_HEAD_REV \
  --repo=$WORKSPACE/gecko \
  --gecko-objdir=$gecko_objdir

# Move files into artifact locations!
mkdir -p $HOME/artifacts

ls -lah $WORKSPACE/B2G/out
ls -lah $gecko_objdir/dist/

mv $WORKSPACE/B2G/sources.xml $HOME/artifacts/sources.xml
mv $WORKSPACE/B2G/out/target/product/generic/tests/gaia-tests.zip $HOME/artifacts/gaia-tests.zip
for name in common cppunittest reftest mochitest xpcshell web-platform; do
    mv $gecko_objdir/dist/*.$name.tests.zip  $HOME/artifacts/target.$name.tests.zip ;
done
mv $gecko_objdir/dist/test_packages_tc.json $HOME/artifacts/test_packages.json
mv $WORKSPACE/B2G/out/emulator.tar.gz $HOME/artifacts/emulator.tar.gz
mv $gecko_objdir/dist/b2g-*.crashreporter-symbols.zip $HOME/artifacts/b2g-crashreporter-symbols.zip

ccache -s
