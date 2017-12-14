#!/bin/bash

set -xe

# Required env variables
test $VERSION
test $BUILD_NUMBER
test $CANDIDATES_DIR

# Optional env variables
: WORKSPACE                     ${WORKSPACE:=/home/worker/workspace}
: ARTIFACTS_DIR                 ${ARTIFACTS_DIR:=/home/worker/artifacts}

SCRIPT_DIRECTORY="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

TARGET="firefox-${VERSION}.snap"
TARGET_FULL_PATH="$ARTIFACTS_DIR/$TARGET"

mkdir -p "$ARTIFACTS_DIR"
rm -rf "${WORKSPACE}/source" && mkdir -p "${WORKSPACE}/source/opt" "${WORKSPACE}/source/usr/bin"

CURL="curl --location --retry 10 --retry-delay 10"

# Download and extract en-US linux64 binary
$CURL -o "${WORKSPACE}/firefox.tar.bz2" \
    "${CANDIDATES_DIR}/${VERSION}-candidates/build${BUILD_NUMBER}/linux-x86_64/en-US/firefox-${VERSION}.tar.bz2"
tar -C "${WORKSPACE}/source/opt" -xf "${WORKSPACE}/firefox.tar.bz2"

# Get Ubuntu configuration
PARTNER_CONFIG_DIR="$WORKSPACE/partner_config"
git clone https://github.com/mozilla-partners/canonical.git "$PARTNER_CONFIG_DIR"

DISTRIBUTION_DIR="$WORKSPACE/source/opt/firefox/distribution"
mv "$PARTNER_CONFIG_DIR/desktop/ubuntu/distribution" "$DISTRIBUTION_DIR"
cp -v "$SCRIPT_DIRECTORY/firefox.desktop" "$DISTRIBUTION_DIR"

# Use release-specific list of locales to fetch L10N XPIs
$CURL -o "${WORKSPACE}/l10n_changesets.txt" "${CANDIDATES_DIR}/${VERSION}-candidates/build${BUILD_NUMBER}/l10n_changesets.txt"
cat "${WORKSPACE}/l10n_changesets.txt"

mkdir -p "$DISTRIBUTION_DIR/extensions"
for locale in $(grep -v ja-JP-mac "${WORKSPACE}/l10n_changesets.txt" | awk '{print $1}'); do
    $CURL -o "${WORKSPACE}/source/opt/firefox/distribution/extensions/langpack-${locale}@firefox.mozilla.org.xpi" \
        "$CANDIDATES_DIR/${VERSION}-candidates/build${BUILD_NUMBER}/linux-x86_64/xpi/${locale}.xpi"
done

# Symlink firefox binary to /usr/bin to make it available in PATH
ln -s ../../opt/firefox/firefox "${WORKSPACE}/source/usr/bin"

# Generate snapcraft manifest
sed -e "s/@VERSION@/${VERSION}/g" -e "s/@BUILD_NUMBER@/${BUILD_NUMBER}/g" snapcraft.yaml.in > ${WORKSPACE}/snapcraft.yaml
cd ${WORKSPACE}
snapcraft

mv *.snap "$TARGET_FULL_PATH"

cd $ARTIFACTS_DIR

# Generate checksums file
size=$(stat --printf="%s" "$TARGET_FULL_PATH")
sha=$(sha512sum "$TARGET_FULL_PATH" | awk '{print $1}')
echo "$sha sha512 $size $TARGET" > $TARGET.checksums

echo "Generating signing manifest"
hash=$(sha512sum $TARGET.checksums | awk '{print $1}')

cat << EOF > signing_manifest.json
[{"file_to_sign": "$TARGET.checksums", "hash": "$hash"}]
EOF

# For posterity
find . -ls
cat $TARGET.checksums
cat signing_manifest.json


# Upload Beta snaps to Ubuntu Snap Store (No channel)
# TODO: Add a release channel once ready for broader audience
# TODO: Don't filter out non-beta releases
# TODO: Parametrize channel depending on beta vs release
# TODO: Make this part an independent task
if [ "$PUSH_TO_CHANNEL" != "" ]; then
  echo "Beta version detected. Uploading to Ubuntu Store (no channel)..."
  bash "$SCRIPT_DIRECTORY/fetch_macaroons.sh" 'http://taskcluster/secrets/v1/secret/project/releng/snapcraft/firefox/$PUSH_TO_CHANNEL'
  snapcraft push "$TARGET_FULL_PATH"
else
  echo "Non-beta version detected. Nothing else to do."
fi
