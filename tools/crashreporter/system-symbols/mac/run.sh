#!/bin/sh

set -v -e -x

base="$(realpath "$(dirname "$0")")"
export PATH="$PATH:/home/worker/bin:$base"

cd /home/worker

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
repo_sync --no-download

# Next, fetch just the update packages we're interested in.
packages=$(python "${base}/list-packages.py")
# shellcheck disable=SC2086
repo_sync $packages

du -sh /opt/data-reposado

# Now scrape symbols out of anything that was downloaded.
mkdir -p symbols artifacts
python "${base}/PackageSymbolDumper.py" --tracking-file=/home/worker/processed-packages --dump_syms=/home/worker/bin/dump_syms_mac /opt/data-reposado/html/content/downloads /home/worker/symbols

# Hand out artifacts
gzip -c processed-packages > artifacts/processed-packages.gz

cd symbols
zip -r9 /home/worker/artifacts/target.crashreporter-symbols.zip ./* || echo "No symbols dumped"
