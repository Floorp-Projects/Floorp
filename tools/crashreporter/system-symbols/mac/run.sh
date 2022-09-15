#!/bin/sh

set -v -e -x

base="$(realpath "$(dirname "$0")")"
export PATH="$PATH:/builds/worker/bin:${MOZ_FETCHES_DIR}/dmg:$base"

DUMP_SYMS_PATH="${MOZ_FETCHES_DIR}/dump_syms/dump_syms"

cd /builds/worker

if test "$PROCESSED_PACKAGES_INDEX" && test "$PROCESSED_PACKAGES_PATH" && test "$TASKCLUSTER_ROOT_URL"; then
  PROCESSED_PACKAGES="$TASKCLUSTER_ROOT_URL/api/index/v1/task/$PROCESSED_PACKAGES_INDEX/artifacts/$PROCESSED_PACKAGES_PATH"
fi

if test "$PROCESSED_PACKAGES"; then
  rm -f processed-packages
  if test `curl --output /dev/null --silent --head --location "$PROCESSED_PACKAGES" -w "%{http_code}"` = 200; then
    curl -L "$PROCESSED_PACKAGES" | gzip -dc > processed-packages
  elif test -f "$PROCESSED_PACKAGES"; then
    gzip -dc "$PROCESSED_PACKAGES" > processed-packages
  fi
  if test -f processed-packages; then
    # Prevent reposado from downloading packages that have previously been
    # dumped.
    for f in $(cat processed-packages); do
      mkdir -p "$(dirname "$f")"
      touch "$f"
    done
  fi
fi

mkdir -p /opt/data-reposado/html /opt/data-reposado/metadata

# First, just fetch all the update info.
python3 /usr/local/bin/repo_sync --no-download

# Next, fetch just the update packages we're interested in.
packages=$(python3 "${base}/list-packages.py")
# shellcheck disable=SC2086
python3 /usr/local/bin/repo_sync $packages

du -sh /opt/data-reposado

# Now scrape symbols out of anything that was downloaded.
mkdir -p symbols artifacts
python3 "${base}/PackageSymbolDumper.py" --tracking-file=/builds/worker/processed-packages --dump_syms="${DUMP_SYMS_PATH}" /opt/data-reposado/html/content/downloads /builds/worker/symbols

# Hand out artifacts
gzip -c processed-packages > artifacts/processed-packages.gz

cd symbols
zip -r9 /builds/worker/artifacts/target.crashreporter-symbols.zip ./* || echo "No symbols dumped"
