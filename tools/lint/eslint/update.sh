#!/bin/sh
# Script to regenerate the npm packages used for ESLint by the builders.
# Requires

# Force the scripts working directory to be projdir/tools/lint/eslint.
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd $DIR

echo "To complete this script you will need the following tokens from https://api.pub.build.mozilla.org/tokenauth/"
echo " - tooltool.upload.public"
echo " - tooltool.download.public"
echo ""
read -p "Are these tokens visible at the above URL (y/n)?" choice
case "$choice" in
  y|Y )
    echo ""
    echo "1. Go to https://api.pub.build.mozilla.org/"
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
cd ../../../
rm -rf node_modules/
rm -rf tools/lint/eslint/eslint-plugin-mozilla/node_modules
rm package-lock.json

echo "Installing eslint and external plugins..."
# ESLint and all _external_ plugins are listed in this directory's package.json,
# so a regular `npm install` will install them at the specified versions.
# The in-tree eslint-plugin-mozilla is kept out of this tooltool archive on
# purpose so that it can be changed by any developer without requiring tooltool
# access to make changes.
npm install

echo "Creating eslint.tar.gz..."
tar cvz --exclude=eslint-plugin-mozilla --exclude=eslint-plugin-spidermonkey-js -f eslint.tar.gz node_modules

echo "Adding eslint.tar.gz to tooltool..."
rm tools/lint/eslint/manifest.tt
./python/mozbuild/mozbuild/action/tooltool.py add --visibility public --unpack eslint.tar.gz --url="https://api.pub.build.mozilla.org/tooltool/"

echo "Uploading eslint.tar.gz to tooltool..."
./python/mozbuild/mozbuild/action/tooltool.py upload --authentication-file=~/.tooltool-token --message "node_modules folder update for tools/lint/eslint" --url="https://api.pub.build.mozilla.org/tooltool/"

echo "Cleaning up..."
mv manifest.tt tools/lint/eslint/manifest.tt
rm eslint.tar.gz

cd $DIR

echo ""
echo "Update complete, please commit and check in your changes."
