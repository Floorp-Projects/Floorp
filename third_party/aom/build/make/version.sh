#!/bin/sh
## Copyright (c) 2016, Alliance for Open Media. All rights reserved
##
## This source code is subject to the terms of the BSD 2 Clause License and
## the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
## was not distributed with this source code in the LICENSE file, you can
## obtain it at www.aomedia.org/license/software. If the Alliance for Open
## Media Patent License 1.0 was not distributed with this source code in the
## PATENTS file, you can obtain it at www.aomedia.org/license/patent.
##



for opt in "$@"; do
    optval="${opt#*=}"
    case "$opt" in
    --bare) bare=true ;;
    *) break ;;
    esac
    shift
done
source_path=${1:-.}
out_file=${2}
id=${3:-VERSION_STRING}

git_version_id=""
if [ -e "${source_path}/.git" ]; then
    # Source Path is a git working copy. Check for local modifications.
    # Note that git submodules may have a file as .git, not a directory.
    export GIT_DIR="${source_path}/.git"
    git_version_id=$(git describe --match=v[0-9]* 2>/dev/null)
fi

changelog_version=""
for p in "${source_path}" "${source_path}/.."; do
    if [ -z "$git_version_id" -a -f "${p}/CHANGELOG" ]; then
        changelog_version=$(grep -m 1 " v[0-9]" "${p}/CHANGELOG" \
            | awk '{print $2}')
        changelog_version="${changelog_version}"
        break
    fi
done
version_str="${changelog_version}${git_version_id}"
bare_version=${version_str#v}
major_version=${bare_version%%.*}
bare_version=${bare_version#*.}
minor_version=${bare_version%%.*}
bare_version=${bare_version#*.}
patch_version=${bare_version%%-*}
bare_version=${bare_version#${patch_version}}
extra_version=${bare_version##-}

#since they'll be used as integers below make sure they are or force to 0
for v in major_version minor_version patch_version; do
    if eval echo \$$v |grep -E -q '[^[:digit:]]'; then
        eval $v=0
    fi
done

if [ ${bare} ]; then
    echo "${changelog_version}${git_version_id}" > $$.tmp
else
    cat<<EOF>$$.tmp
#define VERSION_MAJOR  $major_version
#define VERSION_MINOR  $minor_version
#define VERSION_PATCH  $patch_version
#define VERSION_EXTRA  "$extra_version"
#define VERSION_PACKED ((VERSION_MAJOR<<16)|(VERSION_MINOR<<8)|(VERSION_PATCH))
#define ${id}_NOSP "${version_str}"
#define ${id}      " ${version_str}"
EOF
fi
if [ -n "$out_file" ]; then
diff $$.tmp ${out_file} >/dev/null 2>&1 || cat $$.tmp > ${out_file}
else
cat $$.tmp
fi
rm $$.tmp
