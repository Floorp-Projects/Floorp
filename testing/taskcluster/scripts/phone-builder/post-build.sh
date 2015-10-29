#! /bin/bash -vex

# Don't cache backups
rm -rf $WORKSPACE/B2G/backup-*

if [ -f balrog_credentials ]; then
  rm -f balrog_credentials
fi

mkdir -p $HOME/artifacts
mkdir -p $HOME/artifacts-public

DEVICE=${TARGET%%-*}

mv $WORKSPACE/B2G/upload/sources.xml $HOME/artifacts/sources.xml
mv $WORKSPACE/B2G/upload/b2g-*.android-arm.tar.gz $HOME/artifacts/b2g-android-arm.tar.gz
mv $WORKSPACE/B2G/upload/${TARGET}.zip $HOME/artifacts/${TARGET}.zip
mv $WORKSPACE/B2G/upload/gaia.zip $HOME/artifacts/gaia.zip

if [ -f $WORKSPACE/B2G/upload/b2g-*.crashreporter-symbols.zip ]; then
  mv $WORKSPACE/B2G/upload/b2g-*.crashreporter-symbols.zip $HOME/artifacts/b2g-crashreporter-symbols.zip
fi

if [ -f $WORKSPACE/B2G/upload-public/*.blobfree-dist.zip ]; then
  mv $WORKSPACE/B2G/upload-public/*.blobfree-dist.zip $HOME/artifacts-public/
fi

if [ -f $WORKSPACE/B2G/upload-public/$mar_file ]; then
  mv $WORKSPACE/B2G/upload-public/$mar_file $HOME/artifacts-public/
fi

ccache -s

