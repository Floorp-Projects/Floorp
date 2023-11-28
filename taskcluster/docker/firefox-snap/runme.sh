#!/bin/bash

set -xe

# Thunderbird Snap builds will set this to "thunderbird"
: PRODUCT                       "${PRODUCT:=firefox}"

# Required env variables
test "$VERSION"
test "$BUILD_NUMBER"
test "$CANDIDATES_DIR"
test "$L10N_CHANGESETS"

# Optional env variables
: WORKSPACE                     "${WORKSPACE:=/home/worker/workspace}"
: ARTIFACTS_DIR                 "${ARTIFACTS_DIR:=/home/worker/artifacts}"
: PUSH_TO_CHANNEL               ""

SCRIPT_DIRECTORY="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

TARGET="target.snap"
TARGET_FULL_PATH="$ARTIFACTS_DIR/$TARGET"
SOURCE_DEST="${WORKSPACE}/source"

mkdir -p "$ARTIFACTS_DIR"
rm -rf "$SOURCE_DEST" && mkdir -p "$SOURCE_DEST"

CURL="curl --location --retry 10 --retry-delay 10"

# Download and extract en-US linux64 binary
$CURL -o "${WORKSPACE}/${PRODUCT}.tar.bz2" \
    "${CANDIDATES_DIR}/${VERSION}-candidates/build${BUILD_NUMBER}/linux-x86_64/en-US/${PRODUCT}-${VERSION}.tar.bz2"
tar -C "$SOURCE_DEST" -xf "${WORKSPACE}/${PRODUCT}.tar.bz2" --strip-components=1

DISTRIBUTION_DIR="$SOURCE_DEST/distribution"
if [[ "$PRODUCT" == "firefox" ]]; then
    # Get Ubuntu configuration
    PARTNER_CONFIG_DIR="$WORKSPACE/partner_config"
    git clone https://github.com/mozilla-partners/canonical.git "$PARTNER_CONFIG_DIR"
    mv "$PARTNER_CONFIG_DIR/desktop/ubuntu/distribution" "$DISTRIBUTION_DIR"
else
    mkdir -p "$DISTRIBUTION_DIR"
fi

cp -v "$SCRIPT_DIRECTORY/${PRODUCT}.desktop" "$DISTRIBUTION_DIR"

# Add a group policy file to disable app updates, as those are handled by snapd
cp -v "$SCRIPT_DIRECTORY/policies.json" "$DISTRIBUTION_DIR"

# Use list of locales to fetch L10N XPIs
$CURL -o "${WORKSPACE}/l10n_changesets.json" "$L10N_CHANGESETS"
locales=$(python3 "$SCRIPT_DIRECTORY/extract_locales_from_l10n_json.py" "${WORKSPACE}/l10n_changesets.json")

mkdir -p "$DISTRIBUTION_DIR/extensions"
for locale in $locales; do
    $CURL -o "$SOURCE_DEST/distribution/extensions/langpack-${locale}@${PRODUCT}.mozilla.org.xpi" \
        "$CANDIDATES_DIR/${VERSION}-candidates/build${BUILD_NUMBER}/linux-x86_64/xpi/${locale}.xpi"
done

# In addition to the packages downloaded below, snapcraft fetches deb packages from ubuntu.com,
# when a snap is built,. They may bump packages there and remove the old ones. Updating the
# database allows snapcraft to find the latest packages.
# For more context, see 1448239
apt-get update

# Extract gtk30.mo from Ubuntu language packs
apt download language-pack-gnome-*-base
for i in *.deb; do
    # shellcheck disable=SC2086
    dpkg-deb --fsys-tarfile $i | tar xv -C "$SOURCE_DEST" --wildcards "./usr/share/locale-langpack/*/LC_MESSAGES/gtk30.mo" || true
done

# Add wrapper script to set TMPDIR appropriate for the snap
cp -v "$SCRIPT_DIRECTORY/tmpdir" "$SOURCE_DEST"

# Generate snapcraft manifest
sed -e "s/@VERSION@/${VERSION}/g" -e "s/@BUILD_NUMBER@/${BUILD_NUMBER}/g" "${PRODUCT}.snapcraft.yaml.in" > "${WORKSPACE}/snapcraft.yaml"
cd "${WORKSPACE}"

# Make sure snapcraft knows we're building amd64, even though we may not be on this arch.
export SNAP_ARCH='amd64'

snapcraft

mv -- *.snap "$TARGET_FULL_PATH"

cd "$ARTIFACTS_DIR"

# Generate checksums file
size=$(stat --printf="%s" "$TARGET_FULL_PATH")
sha=$(sha512sum "$TARGET_FULL_PATH" | awk '{print $1}')
echo "$sha sha512 $size $TARGET" > "$TARGET.checksums"

echo "Generating signing manifest"
hash=$(sha512sum "$TARGET.checksums" | awk '{print $1}')

cat << EOF > signing_manifest.json
[{"file_to_sign": "$TARGET.checksums", "hash": "$hash"}]
EOF

# For posterity
find . -ls
cat "$TARGET.checksums"
cat signing_manifest.json
