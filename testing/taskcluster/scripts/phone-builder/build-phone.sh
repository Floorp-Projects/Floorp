#! /bin/bash -vex

. pre-build.sh

if [ $TARGET == "aries" -o $TARGET == "shinano" ]; then
  # caching objects might be dangerous for some devices (aka aries)
  rm -rf $WORKSPACE/B2G/objdir*
  rm -rf $WORKSPACE/B2G/out
fi

if ! test $MOZHARNESS_CONFIG; then
  MOZHARNESS_CONFIG=b2g/taskcluster-phone.py
fi

rm -rf $WORKSPACE/B2G/upload/

./mozharness/scripts/b2g_build.py \
  --config $MOZHARNESS_CONFIG \
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
