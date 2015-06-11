#! /bin/bash -vex

. pre-build.sh

if [ $TARGET == "aries" -o $TARGET == "shinano" ]; then
  # caching objects might be dangerous for some devices (aka aries)
  rm -rf $WORKSPACE/B2G/objdir*
  rm -rf $WORKSPACE/B2G/out
fi

PLATFORM=${TARGET%%-*}

aws s3 cp s3://b2g-nightly-credentials/balrog_credentials .
mar_file=b2g-$PLATFORM-gecko-update.mar

# We need different platform names for each variant (user, userdebug and
# eng). We do not append variant suffix for "user" to keep compability with
# verions already installed in the phones.
if [ 0$DOGFOOD -ne 1 -a $VARIANT != "user" ]; then
  PLATFORM=$PLATFORM-$VARIANT
fi

if ! test $MOZHARNESS_CONFIG; then
  MOZHARNESS_CONFIG=b2g/taskcluster-phone-ota.py
fi

if ! test $BALROG_SERVER_CONFIG; then
  BALROG_SERVER_CONFIG=balrog/docker-worker.py
fi

rm -rf $WORKSPACE/B2G/upload-public/
rm -rf $WORKSPACE/B2G/upload/

./mozharness/scripts/b2g_build.py \
  --config $MOZHARNESS_CONFIG \
  --config $BALROG_SERVER_CONFIG \
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
  --complete-mar-url https://queue.taskcluster.net/v1/task/$TASK_ID/runs/$RUN_ID/artifacts/public/build/$mar_file

# Don't cache backups
rm -rf $WORKSPACE/B2G/backup-*
rm -f balrog_credentials

mkdir -p $HOME/artifacts
mkdir -p $HOME/artifacts-public

mv $WORKSPACE/B2G/upload-public/$mar_file $HOME/artifacts-public/
mv $WORKSPACE/B2G/upload/sources.xml $HOME/artifacts/sources.xml
mv $WORKSPACE/B2G/upload/b2g-*.android-arm.tar.gz $HOME/artifacts/b2g-android-arm.tar.gz
mv $WORKSPACE/B2G/upload/${TARGET}.zip $HOME/artifacts/${TARGET}.zip
mv $WORKSPACE/B2G/upload/gaia.zip $HOME/artifacts/gaia.zip

if [ -f $WORKSPACE/B2G/upload/b2g-*.crashreporter-symbols.zip ]; then
  mv $WORKSPACE/B2G/upload/b2g-*.crashreporter-symbols.zip $HOME/artifacts/b2g-crashreporter-symbols.zip
fi

ccache -s

