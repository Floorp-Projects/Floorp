#! /bin/bash -vex

. pre-build.sh

if [ 0$B2G_DEBUG -ne 0 ]; then
    DEBUG_SUFFIX=-debug
fi

if [ $TARGET == "aries" -o $TARGET == "shinano" ]; then
  # caching objects might be dangerous for some devices (aka aries)
  rm -rf $WORKSPACE/B2G/objdir*
  rm -rf $WORKSPACE/B2G/out
fi

aws s3 cp s3://b2g-nightly-credentials/balrog_credentials .
mar_file=b2g-${TARGET%%-*}-gecko-update.mar

# We need different platform names for each variant (user, userdebug and
# eng). We do not append variant suffix for "user" to keep compability with
# verions already installed in the phones.
if [ $VARIANT == "user" ]; then
  PLATFORM=$TARGET
else
  PLATFORM=$TARGET-$VARIANT
fi

./mozharness/scripts/b2g_lightsaber.py \
  --config b2g/taskcluster-lightsaber-nightly.py \
  --config balrog/staging.py \
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
  --repo=$GECKO_HEAD_REPOSITORY \
  --platform $PLATFORM \
  --complete-mar-url https://queue.taskcluster.net/v1/task/$TASK_ID/runs/$RUN_ID/artifacts/public/build/$mar_file \

# Don't cache backups
rm -rf $WORKSPACE/B2G/backup-*
rm -f balrog_credentials

mkdir -p $HOME/artifacts
mkdir -p $HOME/artifacts-public

mv $WORKSPACE/B2G/upload-public/$mar_file $HOME/artifacts-public/
mv $WORKSPACE/B2G/upload/sources.xml $HOME/artifacts/sources.xml
#mv $WORKSPACE/B2G/upload/b2g-*.crashreporter-symbols.zip $HOME/artifacts/b2g-crashreporter-symbols.zip
mv $WORKSPACE/B2G/upload/b2g-*.android-arm.tar.gz $HOME/artifacts/b2g-android-arm.tar.gz
mv $WORKSPACE/B2G/upload/${TARGET}.zip $HOME/artifacts/${TARGET}.zip
mv $WORKSPACE/B2G/upload/gaia.zip $HOME/artifacts/gaia.zip
ccache -s

