#! /bin/bash -vex

### Check that require variables are defined
test $REPOSITORY  # Should be an hg repository url to pull from
test $REVISION    # Should be an hg revision to pull down
test $TARGET
test $B2G_CONFIG

# First check if the mozharness directory is available. This is intended to be
# used locally in development to test mozharness changes:
#
#   $ docker -v your_mozharness:/home/worker/mozharness ...
#
if [ ! -d mozharness ]; then
  hg clone https://hg.mozilla.org/build/mozharness mozharness
fi

OBJDIR="$HOME/object-folder-$B2G_CONFIG-$B2G_DEBUG"

if [ ! -d $OBJDIR ]; then
  mkdir -p $OBJDIR
fi

if [ ! -d $OBJDIR/B2G ]; then
  git clone https://git.mozilla.org/b2g/B2G.git $OBJDIR/B2G
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
  --b2g-config-dir=$B2G_CONFIG \
  --checkout-revision=$REVISION \
  --repo=$REPOSITORY

# Move files into artifact locations!
mkdir -p artifacts

mv $OBJDIR/B2G/sources.xml artifacts/sources.xml
mv $OBJDIR/B2G/out/target/product/generic/tests/gaia-tests.zip artifacts/gaia-tests.zip
mv $OBJDIR/B2G/out/target/product/generic/tests/b2g-*.zip artifacts/b2g-tests.zip
mv $OBJDIR/B2G/out/emulator.tar.gz artifacts/emulator.tar.gz
mv $OBJDIR/B2G/objdir-gecko/dist/b2g-*.crashreporter-symbols.zip artifacts/b2g-crashreporter-symbols.zip
