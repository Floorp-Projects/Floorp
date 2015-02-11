#! /bin/bash -vex

### Check that require variables are defined
test $GECKO_HEAD_REPOSITORY # Should be an hg repository url to pull from
test $GECKO_BASE_REPOSITORY # Should be an hg repository url to clone from
test $GECKO_HEAD_REV # Should be an hg revision to pull down
test $MOZHARNESS_REPOSITORY # mozharness repository
test $MOZHARNESS_REV # mozharness revision
test $TARGET
test $VARIANT

if ! validate_task.py; then
    echo "Not a valid task" >&2
    exit 1
fi

# First check if the mozharness directory is available. This is intended to be
# used locally in development to test mozharness changes:
#
#   $ docker -v your_mozharness:/home/worker/mozharness ...
#
if [ ! -d mozharness ]; then
  tc-vcs checkout mozharness $MOZHARNESS_REPOSITORY $MOZHARNESS_REPOSITORY $MOZHARNESS_REV
fi

OBJDIR="$HOME/object-folder"

if [ ! -d $OBJDIR ]; then
  mkdir -p $OBJDIR
fi

# Figure out where the remote manifest is so we can use caches for it.
MANIFEST=$(repository-url.py $GECKO_HEAD_REPOSITORY $GECKO_HEAD_REV b2g/config/$TARGET/sources.xml)
tc-vcs repo-checkout $OBJDIR/B2G https://git.mozilla.org/b2g/B2G.git $MANIFEST

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
  --variant=$VARIANT \
  --work-dir=$OBJDIR/B2G \
  --gaia-languages-file locales/languages_all.json \
  --log-level=debug \
  --target=$TARGET \
  --b2g-config-dir=$TARGET \
  --checkout-revision=$GECKO_HEAD_REV \
  --base-repo=$GECKO_BASE_REPOSITORY \
  --repo=$GECKO_HEAD_REPOSITORY

# Don't cache backups
rm -rf $OBJDIR/B2G/backup-*

# Move files into artifact locations!
mkdir -p artifacts

mv $OBJDIR/B2G/upload/sources.xml artifacts/sources.xml
mv $OBJDIR/B2G/upload/b2g-*.crashreporter-symbols.zip artifacts/b2g-crashreporter-symbols.zip
mv $OBJDIR/B2G/upload/b2g-*.android-arm.tar.gz artifacts/b2g-android-arm.tar.gz
mv $OBJDIR/B2G/upload/${TARGET}.zip artifacts/${TARGET}.zip
mv $OBJDIR/B2G/upload/gaia.zip artifacts/gaia.zip
