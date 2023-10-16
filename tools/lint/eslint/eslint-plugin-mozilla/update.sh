#!/bin/sh
# Script to regenerate the npm packages used for eslint-plugin-mozilla by the builders.
# Requires

# Force the scripts working directory to be projdir/tools/lint/eslint/eslint-plugin-mozilla.
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd $DIR

echo "To complete this script you will need the following tokens from https://mozilla-releng.net/tokens"
echo " - tooltool.upload.public"
echo " - tooltool.download.public"
echo ""
read -p "Are these tokens visible at the above URL (y/n)?" choice
case "$choice" in
  y|Y )
    echo ""
    echo "1. Go to https://mozilla-releng.net"
    echo "2. Log in using your Mozilla LDAP account."
    echo "3. Click on \"Tokens.\""
    echo "4. Issue a user token with the permissions tooltool.upload.public and tooltool.download.public."
    echo ""
    echo "When you click issue you will be presented with a long string. Paste the string into a temporary file called ~/.tooltool-token."
    echo ""
    read -rsp $'Press any key to continue...\n' -n 1
    ;;
  n|N )
    echo ""
    echo "You will need to contact somebody that has these permissions... people most likely to have these permissions are members of the releng, ateam, a sheriff, mratcliffe, or jryans"
    exit 1
    ;;
  * )
    echo ""
  echo "Invalid input."
  continue
    ;;
esac

echo ""
echo "Removing node_modules and package-lock.json..."
# Move to the top-level directory.
rm -rf node_modules
rm package-lock.json

echo "Installing modules for eslint-plugin-mozilla..."
../../../../mach npm install

echo "Creating eslint-plugin-mozilla.tar.gz..."
tar cvz -f eslint-plugin-mozilla.tar.gz node_modules

echo "Adding eslint-plugin-mozilla.tar.gz to tooltool..."
rm -f manifest.tt
../../../../python/mozbuild/mozbuild/action/tooltool.py add --visibility public --unpack eslint-plugin-mozilla.tar.gz --url="https://tooltool.mozilla-releng.net/"

echo "Uploading eslint-plugin-mozilla.tar.gz to tooltool..."
../../../../python/mozbuild/mozbuild/action/tooltool.py upload --authentication-file=~/.tooltool-token --message "node_modules folder update for tools/lint/eslint/eslint-plugin-mozilla" --url="https://tooltool.mozilla-releng.net/"

echo "Cleaning up..."
rm eslint-plugin-mozilla.tar.gz

echo ""
echo "Update complete, please commit and check in your changes."
