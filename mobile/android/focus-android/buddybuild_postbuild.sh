#!/usr/bin/env bash
set -e # Exit (and fail) immediately if any command in this scriptfails

# Check that our APK files are not bigger than they should be
python tools/metrics/apk_size.py

