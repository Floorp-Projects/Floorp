#!/bin/sh
##
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
show_help() {
    echo "usage: $self [options] <srcfile>"
    echo
    echo "Generate Makefile dependency information from assembly code source"
    echo
    exit 1
}
die_unknown(){
    echo "Unknown option \"$1\"."
    echo "See $0 --help for available options."
    exit 1
}
for opt do
    optval="${opt#*=}"
    case "$opt" in
    --build-pfx=*) pfx="${optval}"
    ;;
    --depfile=*) out="${optval}"
    ;;
    -I*) raw_inc_paths="${raw_inc_paths} ${opt}"
         inc_path="${inc_path} ${opt#-I}"
    ;;
    -h|--help) show_help
    ;;
    *) [ -f "$opt" ] && srcfile="$opt"
    ;;
    esac
done

[ -n "$srcfile" ] || show_help
sfx=${sfx:-asm}
includes=$(LC_ALL=C egrep -i "include +\"?[a-z0-9_/]+\.${sfx}" $srcfile |
           perl -p -e "s;.*?([a-z0-9_/]+.${sfx}).*;\1;")
#" restore editor state
for inc in ${includes}; do
    found_inc_path=
    for idir in ${inc_path}; do
        [ -f "${idir}/${inc}" ] && found_inc_path="${idir}" && break
    done
    if [ -f `dirname $srcfile`/$inc ]; then
        # Handle include files in the same directory as the source
        $self --build-pfx=$pfx --depfile=$out ${raw_inc_paths} `dirname $srcfile`/$inc
    elif [ -n "${found_inc_path}" ]; then
        # Handle include files on the include path
        $self --build-pfx=$pfx --depfile=$out ${raw_inc_paths} "${found_inc_path}/$inc"
    else
        # Handle generated includes in the build root (which may not exist yet)
        echo ${out} ${out%d}o: "${pfx}${inc}"
    fi
done
echo ${out} ${out%d}o: $srcfile
