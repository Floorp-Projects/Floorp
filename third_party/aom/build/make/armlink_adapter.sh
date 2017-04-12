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

verbose=0
set -- $*
for i; do
    if [ "$i" = "-o" ]; then
        on_of=1
    elif [ "$i" = "-v" ]; then
        verbose=1
    elif [ "$i" = "-g" ]; then
        args="${args} --debug"
    elif [ "$on_of" = "1" ]; then
        outfile=$i
        on_of=0
    elif [ -f "$i" ]; then
        infiles="$infiles $i"
    elif [ "${i#-l}" != "$i" ]; then
        libs="$libs ${i#-l}"
    elif [ "${i#-L}" != "$i" ]; then
        libpaths="${libpaths} ${i#-L}"
    else
        args="${args} ${i}"
    fi
    shift
done

# Absolutize library file names
for f in $libs; do
    found=0
    for d in $libpaths; do
        [ -f "$d/$f" ] && infiles="$infiles $d/$f" && found=1 && break
        [ -f "$d/lib${f}.so" ] && infiles="$infiles $d/lib${f}.so" && found=1 && break
        [ -f "$d/lib${f}.a" ] && infiles="$infiles $d/lib${f}.a" && found=1 && break
    done
    [ $found -eq 0 ] && infiles="$infiles $f"
done
for d in $libpaths; do
    [ -n "$libsearchpath" ] && libsearchpath="${libsearchpath},"
    libsearchpath="${libsearchpath}$d"
done

cmd="armlink $args --userlibpath=$libsearchpath --output=$outfile $infiles"
[ $verbose -eq 1 ] && echo $cmd
$cmd
