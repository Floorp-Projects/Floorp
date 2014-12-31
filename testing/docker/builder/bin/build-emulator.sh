#! /bin/bash -vex

### Check that require variables are defined
test $GECKO_HEAD_REPOSITORY # Should be an hg repository url to pull from
test $GECKO_BASE_REPOSITORY # Should be an hg repository url to clone from
test $GECKO_HEAD_REV # Should be an hg revision to pull down
test $MOZHARNESS_REPOSITORY # mozharness repository
test $MOZHARNESS_REV # mozharness revision
test $TARGET

# First check if the mozharness directory is available. This is intended to be
# used locally in development to test mozharness changes:
#
#   $ docker -v your_mozharness:/home/worker/mozharness ...
#
if [ ! -d mozharness ]; then
  tc-vcs clone $MOZHARNESS_REPOSITORY -u $MOZHARNESS_REV mozharness
fi

OBJDIR="$HOME/object-folder"

if [ ! -d $OBJDIR ]; then
  mkdir -p $OBJDIR
fi

if [ ! -d $OBJDIR/B2G ]; then
  tc-vcs clone https://git.mozilla.org/b2g/B2G.git $OBJDIR/B2G
fi

debug_flag=""
if [ 0$B2G_DEBUG -ne 0 ]; then
  debug_flag='--debug'
fi

./mozharness/scripts/b2g_build.py \
  --config b2g/taskcluster-emulator.py \
  "$debug_flag" \
  --disable-mock \
  --work-dir=$OBJDIR/B2G \
  --log-level=debug \
  --target=$TARGET \
  --b2g-config-dir=$TARGET \
  --checkout-revision=$GECKO_HEAD_REV \
  --base-repo=$GECKO_BASE_REPOSITORY \
  --repo=$GECKO_HEAD_REPOSITORY

# Move files into artifact locations!
mkdir -p artifacts

mv $OBJDIR/B2G/sources.xml artifacts/sources.xml
mv $OBJDIR/B2G/out/target/product/generic/tests/gaia-tests.zip artifacts/gaia-tests.zip
mv $OBJDIR/B2G/out/target/product/generic/tests/b2g-*.zip artifacts/b2g-tests.zip
mv $OBJDIR/B2G/out/emulator.tar.gz artifacts/emulator.tar.gz
mv $OBJDIR/B2G/objdir-gecko/dist/b2g-*.crashreporter-symbols.zip artifacts/b2g-crashreporter-symbols.zip
