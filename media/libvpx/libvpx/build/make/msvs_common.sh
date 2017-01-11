#!/bin/bash
##
##  Copyright (c) 2014 The WebM project authors. All Rights Reserved.
##
##  Use of this source code is governed by a BSD-style license
##  that can be found in the LICENSE file in the root of the source
##  tree. An additional intellectual property rights grant can be found
##  in the file PATENTS.  All contributing project authors may
##  be found in the AUTHORS file in the root of the source tree.
##

if [ "$(uname -o 2>/dev/null)" = "Cygwin" ] \
   && cygpath --help >/dev/null 2>&1; then
    FIXPATH='cygpath -m'
else
    FIXPATH='echo_path'
fi

die() {
    echo "${self_basename}: $@" >&2
    exit 1
}

die_unknown(){
    echo "Unknown option \"$1\"." >&2
    echo "See ${self_basename} --help for available options." >&2
    exit 1
}

echo_path() {
    for path; do
        echo "$path"
    done
}

# Output one, possibly changed based on the system, path per line.
fix_path() {
    $FIXPATH "$@"
}

# Corrects the paths in file_list in one pass for efficiency.
fix_file_list() {
    # TODO(jzern): this could be more generic and take the array as a param.
    files=$(fix_path "${file_list[@]}")
    local IFS=$'\n'
    file_list=($files)
}

generate_uuid() {
    local hex="0123456789ABCDEF"
    local i
    local uuid=""
    local j
    #93995380-89BD-4b04-88EB-625FBE52EBFB
    for ((i=0; i<32; i++)); do
        (( j = $RANDOM % 16 ))
        uuid="${uuid}${hex:$j:1}"
    done
    echo "${uuid:0:8}-${uuid:8:4}-${uuid:12:4}-${uuid:16:4}-${uuid:20:12}"
}

indent1="    "
indent=""
indent_push() {
    indent="${indent}${indent1}"
}
indent_pop() {
    indent="${indent%${indent1}}"
}

tag_attributes() {
    for opt in "$@"; do
        optval="${opt#*=}"
        [ -n "${optval}" ] ||
            die "Missing attribute value in '$opt' while generating $tag tag"
        echo "${indent}${opt%%=*}=\"${optval}\""
    done
}

open_tag() {
    local tag=$1
    shift
    if [ $# -ne 0 ]; then
        echo "${indent}<${tag}"
        indent_push
        tag_attributes "$@"
        echo "${indent}>"
    else
        echo "${indent}<${tag}>"
        indent_push
    fi
}

close_tag() {
    local tag=$1
    indent_pop
    echo "${indent}</${tag}>"
}

tag() {
    local tag=$1
    shift
    if [ $# -ne 0 ]; then
        echo "${indent}<${tag}"
        indent_push
        tag_attributes "$@"
        indent_pop
        echo "${indent}/>"
    else
        echo "${indent}<${tag}/>"
    fi
}

