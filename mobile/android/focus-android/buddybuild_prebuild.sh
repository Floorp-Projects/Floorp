#!/usr/bin/env bash

# Pre-build script for buddybuild
# http://docs.buddybuild.com/docs/custom-prebuild-and-postbuild-steps#section-pre-build

# Make sure the translations are not missing placeholders etc.
python tools/l10n/check_translations.py

# Add a dummy Adjust token so that we can build release builds and run unit tests for them.
echo "--" > .adjust_token

