#!/bin/bash

set -ex

SNAP_NAME="$1"
SNAP_CHANNEL="${2:-stable}"
SNAP_INSTALL_LOCATION="${3:-/snap}"

SNAP_METADATA="$(curl --header 'X-Ubuntu-Series: 16' "https://api.snapcraft.io/api/v1/snaps/details/$SNAP_NAME?channel=$SNAP_CHANNEL")"

set +x
SNAP_SHA512="$(echo "$SNAP_METADATA" | jq '.download_sha512' -r)"
SNAP_DOWNLOAD_URL="$(echo "$SNAP_METADATA" | jq '.download_url' -r)"
SNAP_LAST_UPDATED="$(echo "$SNAP_METADATA" | jq '.last_updated' -r)"
SNAP_REVISION="$(echo "$SNAP_METADATA" | jq '.revision' -r)"
SNAP_VERSION="$(echo "$SNAP_METADATA" | jq '.version' -r)"
set -x

echo "Downloading $SNAP_NAME, version $SNAP_VERSION, revision $SNAP_REVISION (last updated: $SNAP_LAST_UPDATED)..."
curl --location "$SNAP_DOWNLOAD_URL" --output "$SNAP_NAME.snap"
sha512sum -c <(echo "$SNAP_SHA512  $SNAP_NAME.snap") 

mkdir -p "$SNAP_INSTALL_LOCATION/$SNAP_NAME"
unsquashfs -d "$SNAP_INSTALL_LOCATION/$SNAP_NAME/current" "$SNAP_NAME.snap"
rm "$SNAP_NAME.snap"

echo "$SNAP_NAME version $SNAP_VERSION has correctly been uploaded and installed."