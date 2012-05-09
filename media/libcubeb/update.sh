# Usage: sh update.sh <upstream_src_directory>
set -e

cp $1/include/cubeb/cubeb.h include
cp $1/src/cubeb_alsa.c src
cp $1/src/cubeb_winmm.c src
cp $1/src/cubeb_audiounit.c src
cp $1/src/cubeb_pulse.c src
cp $1/LICENSE .
cp $1/README .
cp $1/AUTHORS .
if [ -d $1/.git ]; then
  rev=$(cd $1 && git rev-parse --verify HEAD)
  dirty=$(cd $1 && git diff-index --name-only HEAD)
fi

if [ -n "$rev" ]; then
  version=$rev
  if [ -n "$dirty" ]; then
    version=$version-dirty
    echo "WARNING: updating from a dirty git repository."
  fi
  sed -i.bak -e "/The git commit ID used was/ s/[0-9a-f]\+\(-dirty\)\?\./$version./" README_MOZILLA
  rm README_MOZILLA.bak
else
  echo "Remember to update README_MOZILLA with the version details."
fi

