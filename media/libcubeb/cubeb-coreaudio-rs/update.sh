# Usage: sh update.sh <upstream_src_directory>
set -e

cp -p $1/LICENSE .
cp -p $1/Cargo.toml .
test -d src || mkdir -p src
# Copy all the files under src folder, except tests.
rsync -av --progress $1/src/ src/ --exclude backend/tests
# Separate build settings of coreaudio-sys-utils in gecko for now (Bug 1569003).
rsync -av --progress $1/coreaudio-sys-utils . --exclude Cargo.toml

if [ -d $1/.git ]; then
  rev=$(cd $1 && git rev-parse --verify HEAD)
  date=$(cd $1 && git show -s --format=%ci HEAD)
  dirty=$(cd $1 && git diff-index --name-only HEAD)
  set +e
  pre_rev=$(grep -o '[[:xdigit:]]\{40\}' README_MOZILLA)
  commits=$(cd $1 && git log --pretty=format:'%h - %s' $pre_rev..$rev)
  set -e
fi

if [ -n "$rev" ]; then
  version=$rev
  if [ -n "$dirty" ]; then
    version=$version-dirty
    echo "WARNING: updating from a dirty git repository."
  fi
  echo "$version ($date)"
  sed -i.bak -e "/The git commit ID used was/ s/[0-9a-f]\{40\}\(-dirty\)\{0,1\} .\{1,100\}/$version ($date)/" README_MOZILLA
  rm README_MOZILLA.bak
  [[ -n "$commits" ]] && echo -e "Pick commits:\n$commits"
else
  echo "Remember to update README_MOZILLA with the version details."
fi

# Apply patches for gecko
for patch in *.patch; do
  [ -f "$patch" ] || continue
  echo "Apply $patch"
  patch -p4 < $patch
done