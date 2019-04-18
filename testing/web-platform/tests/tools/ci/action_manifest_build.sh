#!/bin/bash
set -ex

mkdir -p ~/meta

WPT_MANIFEST_FILE=~/meta/MANIFEST.json

./wpt manifest -p $WPT_MANIFEST_FILE
gzip -f --best $WPT_MANIFEST_FILE
