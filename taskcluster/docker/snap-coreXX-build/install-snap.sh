#!/bin/bash

set -ex

SNAP_TO_INSTALL=$1

if [ -z "${SNAP_TO_INSTALL}" ]; then
  echo "Please give a snap name"
  exit 1
fi

# Grab the requested snap from the stable channel and unpack it in the proper
# place (the 'Snap-CDN: none' header allows building in restricted network
# environments such as Launchpad builders)

# shellcheck disable=SC2046
curl -L \
	-H 'Snap-CDN: none' \
	$(curl -H 'X-Ubuntu-Series: 16' "https://api.snapcraft.io/api/v1/snaps/details/${SNAP_TO_INSTALL}?channel=stable" | jq '.download_url' -r) \
	--output "${SNAP_TO_INSTALL}.snap"

mkdir -p "/snap/${SNAP_TO_INSTALL}"

unsquashfs -d "/snap/${SNAP_TO_INSTALL}/current" "${SNAP_TO_INSTALL}.snap"
