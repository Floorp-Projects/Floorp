#! /bin/bash -vex

. pre-build.sh

# We need different platform names for each variant (user, userdebug and
# eng). We do not append variant suffix for "user" to keep compability with
# verions already installed in the phones.
if [ $VARIANT == "user" ]; then
  PLATFORM=$TARGET
else
  PLATFORM=$TARGET-$VARIANT
fi

./mozharness/scripts/b2g_lightsaber.py \
  --config b2g/taskcluster-lightsaber.py \
  "$debug_flag" \
  --disable-mock \
  --variant=$VARIANT \
  --work-dir=$WORKSPACE/B2G \
  --gaia-languages-file locales/languages_all.json \
  --log-level=debug \
  --target=$TARGET \
  --b2g-config-dir=$TARGET \
  --checkout-revision=$GECKO_HEAD_REV \
  --base-repo=$GECKO_BASE_REPOSITORY \
  --repo=$GECKO_HEAD_REPOSITORY

# Don't cache backups
rm -rf $WORKSPACE/B2G/backup-*

# Move files into artifact locations!
mkdir -p $HOME/artifacts

mv $WORKSPACE/B2G/upload/sources.xml $HOME/artifacts/sources.xml
mv $WORKSPACE/B2G/upload/b2g-*.crashreporter-symbols.zip $HOME/artifacts/b2g-crashreporter-symbols.zip
mv $WORKSPACE/B2G/upload/b2g-*.android-arm.tar.gz $HOME/artifacts/b2g-android-arm.tar.gz
mv $WORKSPACE/B2G/upload/${TARGET}.zip $HOME/artifacts/${TARGET}.zip
mv $WORKSPACE/B2G/upload/gaia.zip $HOME/artifacts/gaia.zip

ccache -s
