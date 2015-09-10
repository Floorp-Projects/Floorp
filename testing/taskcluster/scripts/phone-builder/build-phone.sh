#! /bin/bash -vex

. pre-build.sh

if [ $TARGET == "aries" -o $TARGET == "shinano" ]; then
  # caching objects might be dangerous for some devices (aka aries)
  rm -rf $gecko_objdir
  rm -rf $WORKSPACE/B2G/out
fi

MOZHARNESS_CONFIG=${MOZHARNESS_CONFIG:=b2g/taskcluster-phone.py}

rm -rf $WORKSPACE/B2G/upload/

$WORKSPACE/gecko/testing/mozharness/scripts/b2g_build.py \
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
  --repo=$WORKSPACE/gecko \
  --gecko-objdir=$gecko_objdir

. post-build.sh
