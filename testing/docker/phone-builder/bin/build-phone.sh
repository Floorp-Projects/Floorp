#! /bin/bash -vex

### Check that require variables are defined
test $REPOSITORY  # Should be an hg repository url to pull from
test $REVISION    # Should be an hg revision to pull down
test $TARGET

# First check if the mozharness directory is available. This is intended to be
# used locally in development to test mozharness changes:
#
#   $ docker -v your_mozharness:/home/worker/mozharness ...
#
if [ ! -d mozharness ]; then
  git clone https://github.com/walac/build-mozharness.git mozharness
fi

OBJDIR="$HOME/object-folder-$TARGET-$B2G_DEBUG"

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

backup_file=$(aws --output=text s3 ls s3://b2g-phone-backups/$TARGET/ | tail -1 | awk '{print $NF}')

if echo $backup_file | grep '\.tar\.bz2'; then
    aws s3 cp s3://b2g-phone-backups/$TARGET/$backup_file .
    tar -xjf $backup_file -C $OBJDIR/B2G
    rm -f $backup_file
fi

./mozharness/scripts/b2g_build.py \
  --config b2g/taskcluster-phone.py \
  "$debug_flag" \
  --disable-mock \
  --work-dir=$OBJDIR/B2G \
  --log-level=debug \
  --target=$TARGET \
  --b2g-config-dir=$TARGET \
  --checkout-revision=$REVISION \
  --repo=$REPOSITORY

# Don't cache backups
rm -rf $OBJDIR/B2G/backup-*

# Move files into artifact locations!
mkdir -p artifacts

mv $OBJDIR/B2G/upload/sources.xml artifacts/sources.xml
mv $OBJDIR/B2G/upload/b2g-*.crashreporter-symbols.zip artifacts/b2g-crashreporter-symbols.zip
mv $OBJDIR/B2G/upload/b2g-*.android-arm.tar.gz artifacts/b2g-android-arm.tar.gz
mv $OBJDIR/B2G/upload/flame-kk.zip artifacts/flame-kk.zip
mv $OBJDIR/B2G/upload/gaia.zip artifacts/gaia.zip
