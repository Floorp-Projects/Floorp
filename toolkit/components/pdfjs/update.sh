#!/bin/bash

set +vex

export ROOT=/builds/worker/pdf.js

pushd $ROOT
git fetch origin
git checkout origin/master

npm install --ignore-scripts

gulp mozcentral

popd


mkdir -p /builds/worker/pdf.js/build/mozcentral/browser/extensions/pdfjs/
touch "$ROOT/build/mozcentral/browser/extensions/pdfjs/README.mozilla"

cp "$ROOT/build/mozcentral/browser/extensions/pdfjs/content/LICENSE" "$GECKO_PATH/toolkit/components/pdfjs/"
cp "$ROOT/build/mozcentral/browser/extensions/pdfjs/content/README.mozilla" "$GECKO_PATH/toolkit/components/pdfjs/"
cp "$ROOT/build/mozcentral/browser/extensions/pdfjs/content/PdfJsDefaultPreferences.jsm" "$GECKO_PATH/toolkit/components/pdfjs/content/PdfJsDefaultPreferences.jsm"
rsync -a -v --delete "$ROOT/build/mozcentral/browser/extensions/pdfjs/content/build/" "$GECKO_PATH/toolkit/components/pdfjs/content/build/"
rsync -a -v --delete "$ROOT/build/mozcentral/browser/extensions/pdfjs/content/web/" "$GECKO_PATH/toolkit/components/pdfjs/content/web/"

ls -R "$ROOT/build/mozcentral/browser/"
cp "$ROOT"/build/mozcentral/browser/locales/en-US/pdfviewer/*.properties "$GECKO_PATH/browser/locales/en-US/pdfviewer/" || true

