#!/bin/bash
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
#
# Runs codesign commands to codesign a Firefox .app bundle and enable macOS
# Hardened Runtime. Intended to be manually run by developers working on macOS
# 10.14 who want to enable Hardened Runtime for manual testing. This is
# provided as a stop-gap until automated build tooling is available that signs
# binaries with a certificate generated during builds (bug 1522409). This
# script requires macOS 10.14 because Hardened Runtime is only available for
# applications running on 10.14 despite support for the codesign "-o runtime"
# option being available in 10.13.6 and newer.
#
# The script requires an identity string (-i option) from an Apple Developer
# ID certificate. This can be found in the macOS KeyChain after configuring an
# Apple Developer ID certificate.
#
# Example usage on macOS 10.14:
#
#   $ ./mach build
#   $ ./mach build package
#   $ open </PATH/TO/DMG/FILE.dmg>
#   <Drag Nightly.app to ~>
#   $ ./security/mac/hardenedruntime/codesign.bash \
#         -a ~/Nightly.app \
#         -i <MY-IDENTITY-STRING> \
#         -e security/mac/hardenedruntime/developer.entitlements.xml
#   $ open ~/Nightly.app
#

usage ()
{
  echo  "Usage: $0 "
  echo  "    -a <PATH-TO-BROWSER.app>"
  echo  "    -i <IDENTITY>"
  echo  "    -e <ENTITLEMENTS-FILE>"
  echo  "    [-o <OUTPUT-ZIP-FILE>]"
  exit -1
}

# Make sure we are running on macOS with the sw_vers command available.
SWVERS=/usr/bin/sw_vers
if [ ! -x ${SWVERS} ]; then
    echo "ERROR: macOS 10.14 or later is required"
    exit -1
fi

# Require macOS 10.14 or newer.
OSVERSION=`${SWVERS} -productVersion|sed -En 's/[0-9]+\.([0-9]+)\.[0-9]+/\1/p'`;
if [ ${OSVERSION} \< 14 ]; then
    echo "ERROR: macOS 10.14 or later is required"
    exit -1
fi

while getopts "a:i:e:o:" opt; do
  case ${opt} in
    a ) BUNDLE=$OPTARG ;;
    i ) IDENTITY=$OPTARG ;;
    e ) ENTITLEMENTS_FILE=$OPTARG ;;
    o ) OUTPUT_ZIP_FILE=$OPTARG ;;
    \? ) usage; exit -1 ;;
  esac
done

if [ -z "${BUNDLE}" ] ||
   [ -z "${IDENTITY}" ] ||
   [ -z "${ENTITLEMENTS_FILE}" ]; then
    usage
    exit -1
fi

if [ ! -d "${BUNDLE}" ]; then
  echo "Invalid bundle. Bundle should be a .app directory"
  usage
  exit -1
fi

if [ ! -e "${ENTITLEMENTS_FILE}" ]; then
  echo "Invalid entitlements file"
  usage
  exit -1
fi

# Zip file output flag is optional
if [ ! -z "${OUTPUT_ZIP_FILE}" ] &&
   [ -e "${OUTPUT_ZIP_FILE}" ]; then
  echo "Output zip file ${OUTPUT_ZIP_FILE} exists. Please delete it first."
  usage
  exit -1
fi

echo "-------------------------------------------------------------------------"
echo "bundle:                        $BUNDLE"
echo "identity:                      $IDENTITY"
echo "browser entitlements file:     $ENTITLEMENTS_FILE"
echo "output zip file (optional):    $OUTPUT_ZIP_FILE"
echo "-------------------------------------------------------------------------"

# Clear extended attributes which cause codesign to fail
xattr -cr "${BUNDLE}"

# Sign these binaries first. Signing of some binaries has an ordering
# requirement where other binaries must be signed first.
codesign --force -o runtime --verbose --sign "$IDENTITY" \
--entitlements ${ENTITLEMENTS_FILE} \
"${BUNDLE}/Contents/MacOS/XUL" \
"${BUNDLE}/Contents/MacOS/pingsender" \
"${BUNDLE}"/Contents/MacOS/*.dylib \
"${BUNDLE}"/Contents/MacOS/crashreporter.app/Contents/MacOS/minidump-analyzer \
"${BUNDLE}"/Contents/MacOS/crashreporter.app/Contents/MacOS/crashreporter \
"${BUNDLE}"/Contents/MacOS/firefox-bin \
"${BUNDLE}"/Contents/MacOS/plugin-container.app/Contents/MacOS/plugin-container \
"${BUNDLE}"/Contents/MacOS/updater.app/Contents/MacOS/org.mozilla.updater \
"${BUNDLE}"/Contents/MacOS/firefox

# Sign all files in the .app
find "${BUNDLE}" -type f -exec \
codesign --force -o runtime --verbose --sign "$IDENTITY" \
--entitlements ${ENTITLEMENTS_FILE} {} \;

# Sign the bundle
codesign --force -o runtime --verbose --sign "$IDENTITY" \
--entitlements ${ENTITLEMENTS_FILE} "${BUNDLE}"

# Validate
codesign -vvv --deep --strict "${BUNDLE}"

# Zip up the bundle
if [ ! -z "${OUTPUT_ZIP_FILE}" ]; then
  echo "Zipping bundle to ${OUTPUT_ZIP_FILE}"
  ditto -c -k "${BUNDLE}" "${OUTPUT_ZIP_FILE}"
  echo "Done"
fi
