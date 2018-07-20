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

components_dir=$(realpath $(dirname $0))
repo=${components_dir}/../..
rm -rf ${components_dir}/jsoncpp

jsoncpp_repo=$1
rev=${2-HEAD}
(cd $jsoncpp_repo; git archive --prefix=toolkit/components/jsoncpp/ $rev) | (cd $repo; tar xf -)

# remove some extraneous bits
rm -rf \
  ${components_dir}/jsoncpp/.clang-format \
  ${components_dir}/jsoncpp/.gitattributes \
  ${components_dir}/jsoncpp/.gitignore \
  ${components_dir}/jsoncpp/.travis.yml \
  ${components_dir}/jsoncpp/CMakeLists.txt \
  ${components_dir}/jsoncpp/SConstruct \
  ${components_dir}/jsoncpp/amalgamate.py \
  ${components_dir}/jsoncpp/appveyor.yml \
  ${components_dir}/jsoncpp/dev.makefile \
  ${components_dir}/jsoncpp/devtools \
  ${components_dir}/jsoncpp/doc \
  ${components_dir}/jsoncpp/doxybuild.py \
  ${components_dir}/jsoncpp/include/CMakeLists.txt \
  ${components_dir}/jsoncpp/makefiles \
  ${components_dir}/jsoncpp/makerelease.py \
  ${components_dir}/jsoncpp/pkg-config \
  ${components_dir}/jsoncpp/scons-tools \
  ${components_dir}/jsoncpp/src/CMakeLists.txt \
  ${components_dir}/jsoncpp/src/jsontestrunner \
  ${components_dir}/jsoncpp/src/lib_json/CMakeLists.txt \
  ${components_dir}/jsoncpp/src/lib_json/sconscript \
  ${components_dir}/jsoncpp/src/lib_json/version.h.in \
  ${components_dir}/jsoncpp/src/test_lib_json \
  ${components_dir}/jsoncpp/test \
  ${components_dir}/jsoncpp/travis.sh \
  ${components_dir}/jsoncpp/version \
  ${components_dir}/jsoncpp/version.in

# restore our moz.build files
hg -R ${repo} st -n | grep "moz\.build$" | xargs hg revert --no-backup

# Record git rev
(cd $jsoncpp_repo; git rev-parse $rev) > ${components_dir}/jsoncpp/GIT-INFO

# remove any .orig files that snuck in
find ${components_dir}/jsoncpp -name "*.orig" -exec rm '{}' \;

hg addremove ${components_dir}/jsoncpp/
