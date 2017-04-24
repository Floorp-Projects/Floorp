#!/bin/bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

set -v -e -x

# Usage: update-jsoncpp.sh <path to jsoncpp git clone> [rev, defaults to HEAD]

if [ $# -lt 1 ]; then
  echo "Usage: update-jsoncpp.sh /path/to/jsoncpp/src [rev]"
  exit 1
fi

crashreporter_dir=$(realpath $(dirname $0))
repo=${crashreporter_dir}/../..
rm -rf ${crashreporter_dir}/jsoncpp

jsoncpp_repo=$1
rev=${2-HEAD}
(cd $jsoncpp_repo; git archive --prefix=toolkit/components/jsoncpp/ $rev) | (cd $repo; tar xf -)

# remove some extraneous bits
rm -rf \
  ${crashreporter_dir}/jsoncpp/.clang-format \
  ${crashreporter_dir}/jsoncpp/.gitattributes \
  ${crashreporter_dir}/jsoncpp/.gitignore \
  ${crashreporter_dir}/jsoncpp/.travis.yml \
  ${crashreporter_dir}/jsoncpp/CMakeLists.txt \
  ${crashreporter_dir}/jsoncpp/SConstruct \
  ${crashreporter_dir}/jsoncpp/amalgamate.py \
  ${crashreporter_dir}/jsoncpp/appveyor.yml \
  ${crashreporter_dir}/jsoncpp/dev.makefile \
  ${crashreporter_dir}/jsoncpp/devtools \
  ${crashreporter_dir}/jsoncpp/doc \
  ${crashreporter_dir}/jsoncpp/doxybuild.py \
  ${crashreporter_dir}/jsoncpp/include/CMakeLists.txt \
  ${crashreporter_dir}/jsoncpp/makefiles \
  ${crashreporter_dir}/jsoncpp/makerelease.py \
  ${crashreporter_dir}/jsoncpp/pkg-config \
  ${crashreporter_dir}/jsoncpp/scons-tools \
  ${crashreporter_dir}/jsoncpp/src/CMakeLists.txt \
  ${crashreporter_dir}/jsoncpp/src/jsontestrunner \
  ${crashreporter_dir}/jsoncpp/src/lib_json/CMakeLists.txt \
  ${crashreporter_dir}/jsoncpp/src/lib_json/sconscript \
  ${crashreporter_dir}/jsoncpp/src/lib_json/version.h.in \
  ${crashreporter_dir}/jsoncpp/src/test_lib_json \
  ${crashreporter_dir}/jsoncpp/test \
  ${crashreporter_dir}/jsoncpp/travis.sh \
  ${crashreporter_dir}/jsoncpp/version \
  ${crashreporter_dir}/jsoncpp/version.in

# restore our moz.build files
hg -R ${repo} st -n | grep "moz\.build$" | xargs hg revert --no-backup

# Record git rev
(cd $jsoncpp_repo; git rev-parse $rev) > ${crashreporter_dir}/jsoncpp/GIT-INFO

# remove any .orig files that snuck in
find ${crashreporter_dir}/jsoncpp -name "*.orig" -exec rm '{}' \;

hg addremove ${crashreporter_dir}/jsoncpp/
