# Usage: sh update.sh <upstream_src_directory>
set -e
echo "copy source from libvpx"

cp $1/third_party/libmkv/EbmlBufferWriter.c .
cp $1/third_party/libmkv/WebMElement.c .
cp $1/third_party/libmkv/EbmlWriter.c .
cp $1/third_party/libmkv/EbmlWriter.h .
cp $1/third_party/libmkv/EbmlBufferWriter.h .
cp $1/third_party/libmkv/WebMElement.h .
cp $1/third_party/libmkv/EbmlIDs.h .

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
  sed -i.bak -e "/The git commit ID used was/ s/[0-9a-f]\{40\}\(-dirty\)\{0,1\}\./$version./" README_MOZILLA
  rm README_MOZILLA.bak
else
  echo "Remember to update README_MOZILLA with the version details."
fi

# Apply any patches against upstream here.
patch -p1 < source_fix.patch
patch -p1 < gecko_fix.patch
patch -p1 < const_fix.patch
patch -p3 < bock_fix.patch
