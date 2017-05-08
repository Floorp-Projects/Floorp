#!/bin/bash
## Copyright (c) 2016, Alliance for Open Media. All rights reserved
##
## This source code is subject to the terms of the BSD 2 Clause License and
## the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
## was not distributed with this source code in the LICENSE file, you can
## obtain it at www.aomedia.org/license/software. If the Alliance for Open
## Media Patent License 1.0 was not distributed with this source code in the
## PATENTS file, you can obtain it at www.aomedia.org/license/patent.
##

self=$0
self_basename=${self##*/}
EOL=$'\n'

show_help() {
    cat <<EOF
Usage: ${self_basename} [options] file1 [file2 ...]

This script generates a MSVC module definition file containing a list of symbols
to export from a DLL. Source files are technically bash scripts (and thus may
use #comment syntax) but in general, take the form of a list of symbols:

  <kind> symbol1 [symbol2, symbol3, ...]

where <kind> is either 'text' or 'data'


Options:
    --help                      Print this message
    --out=filename              Write output to a file [stdout]
    --name=project_name         Name of the library (required)
EOF
    exit 1
}

die() {
    echo "${self_basename}: $@"
    exit 1
}

die_unknown(){
    echo "Unknown option \"$1\"."
    echo "See ${self_basename} --help for available options."
    exit 1
}

text() {
    for sym in "$@"; do
        echo "  $sym" >> ${outfile}
    done
}

data() {
    for sym in "$@"; do
        printf "  %-40s DATA\n" "$sym" >> ${outfile}
    done
}

# Process command line
for opt in "$@"; do
    optval="${opt#*=}"
    case "$opt" in
    --help|-h) show_help
    ;;
    --out=*) outfile="$optval"
    ;;
    --name=*) name="${optval}"
    ;;
     -*) die_unknown $opt
    ;;
    *) file_list[${#file_list[@]}]="$opt"
    esac
done
outfile=${outfile:-/dev/stdout}
[ -n "$name" ] || die "Library name (--name) must be specified!"

echo "LIBRARY ${name}" > ${outfile}
echo "EXPORTS" >> ${outfile}
for f in "${file_list[@]}"; do
    . $f
done
