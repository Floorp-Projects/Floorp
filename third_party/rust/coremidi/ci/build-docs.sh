#!/bin/bash

set -ex

cargo doc

REPO_NAME=$(echo $TRAVIS_REPO_SLUG | cut -d '/' -f 2)
echo "<meta http-equiv=refresh content=0;url=$REPO_NAME/index.html>" > target/doc/index.html
