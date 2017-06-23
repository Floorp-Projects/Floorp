# Usage: sh update.sh <upstream_src_directory>
set -e

cp -p $1/AUTHORS .
cp -p $1/LICENSE .
cp -p $1/README.md .
cp -p $1/Cargo.toml .
test -d src || mkdir -p src
cp -pr $1/src/* src/
test -d cubeb-ffi/src || mkdir -p cubeb-ffi/src
cp -pr $1/cubeb-ffi/Cargo.toml cubeb-ffi/
cp -pr $1/cubeb-ffi/src/* cubeb-ffi/src/
test -d pulse-ffi/src || mkdir -p pulse-ffi/src
cp -pr $1/pulse-ffi/Cargo.toml pulse-ffi/
cp -pr $1/pulse-ffi/src/* pulse-ffi/src/
test -d pulse-rs/src || mkdir -p pulse-rs/src
cp -pr $1/pulse-rs/Cargo.toml pulse-rs/
cp -pr $1/pulse-rs/src/* pulse-rs/src/

if [ -d $1/.git ]; then
  rev=$(cd $1 && git rev-parse --verify HEAD)
  date=$(cd $1 && git show -s --format=%ci HEAD)
  dirty=$(cd $1 && git diff-index --name-only HEAD)
fi

if [ -n "$rev" ]; then
  version=$rev
  if [ -n "$dirty" ]; then
    version=$version-dirty
    echo "WARNING: updating from a dirty git repository."
  fi
  sed -i.bak -e "/The git commit ID used was/ s/[0-9a-f]\{40\}\(-dirty\)\{0,1\} .\{1,100\}/$version ($date)/" README_MOZILLA
  rm README_MOZILLA.bak
else
  echo "Remember to update README_MOZILLA with the version details."
fi
