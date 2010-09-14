# Usage: sh update.sh <upstream_src_directory>
cp $1/include/nestegg/nestegg.h include
cp $1/src/nestegg.c src
cp $1/halloc/halloc.h src
cp $1/halloc/src/align.h src
cp $1/halloc/src/halloc.c src
cp $1/halloc/src/hlist.h src
cp $1/halloc/src/macros.h src
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
  sed -i "/The git commit ID used was/ s/[0-9a-f]\+\(-dirty\)\?\./$version./" README_MOZILLA
else
  echo "Remember to update README_MOZILLA with the version details."
fi

