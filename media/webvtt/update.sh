#!/bin/bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Usage: ./update.sh <webvtt_git_repository> <optional revision/branch/refspec>
#
# Copies the needed files from a directory containing the original
# libwebvtt source, and applies any local patches we're carrying.

function die() {
  echo "error: $1"
  exit 1
}

if [ $# -lt 1 ]; then
  die "Usage: update.sh /path/to/webvtt-repository/ commit/remote/branch (default HEAD)"
fi

git --version 2>&1 >/dev/null #executable not found? ok.
have_git=$?
hg --version 2>&1 >/dev/null #executable not found? ok.
have_hg=$?
if [ ${have_git} -ne 0 ]; then
  die "Git does not seem to be installed"
fi

start_dir=$PWD
webvtt_branch=HEAD
if [ $# -gt 1 ]; then
  webvtt_branch="$2"
fi

webvtt_dir=$(dirname $0)
webvtt_remote=$1
repo_dir=${webvtt_dir}/libwebvtt

cd ${webvtt_remote}
webvtt_isrepo=$(git rev-parse --is-inside-work-tree)
if [ "x${webvtt_isrepo}" != "xtrue" ]; then
  cd ${start_dir}
  die "$1 does not seem to be a git repository"
fi

webvtt_revision=$(git rev-parse ${webvtt_branch})
echo "Updating media/webvtt to revision ${webvtt_branch} (${webvtt_revision})"

#Ensure that ${repo_dir} is not present to prevent mkdir from failing
#Error hidden because most of the time it shouldn't be present anyways, so an
#error is generally expected.
rm -rf ${start_dir}/${repo_dir} 2>/dev/null

#Try to create temporary directory for repo archive. If this fails,
#print error and exit.
mkdir ${start_dir}/${repo_dir} || exit 1
git archive --format=tar  ${webvtt_revision} | tar -C ${start_dir}/${repo_dir} -xf -
cd ${start_dir}

sed -e "s/^[a-z0-9]\{40,40\}/${webvtt_revision}/" \
  ${webvtt_dir}/README_MOZILLA > ${webvtt_dir}/README_MOZILLA.sed
mv ${webvtt_dir}/README_MOZILLA.sed ${webvtt_dir}/README_MOZILLA \
  || die "Failed to overwrite README_MOZILLA"

rm -rf ${webvtt_dir}/include ${webvtt_dir}/*.c ${webvtt_dir}/*.h
#Create directories
mkdir ${webvtt_dir}/include ${webvtt_dir}/include/webvtt

#Copy C headers, excluding 'webvtt-config' files (hence [^w])
find ${repo_dir}/include/webvtt -type f -name '[^w]*.h' -exec cp '{}' \
    ${webvtt_dir}/include/webvtt/ \; #Copy C sources
find ${repo_dir}/src/libwebvtt -type f -name '*.[ch]' -exec cp '{}' \
    ${webvtt_dir}/ \;
cp ${repo_dir}/LICENSE ${webvtt_dir}/
rm -rf ${repo_dir}

# addremove automatically if mercurial is used
if [ ${have_hg} -eq 0 -a -d ${start_dir}/.hg ]; then
  hg addremove ${webvtt_dir}/
fi

# apply patches
cd ${webvtt_dir}
patch -p3 < 868629.patch

cd ${start_dir}
