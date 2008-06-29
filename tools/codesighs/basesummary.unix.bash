#!/bin/bash
#
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is basesummary.linx.bash code, released
# Nov 15, 2002.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 2002
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Garrett Arch Blythe, 15-November-2002
#   Simon Fraser <sfraser@netscape.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

#
# Check for optional objdir
# 
if [ "$1" = "-o" ]; then 
OBJROOT="$2"
shift
shift
else
OBJROOT="./mozilla"
fi

if [ "$1" = "-s" ]; then
SRCROOT="$2"
shift
shift
else
SRCROOT="./mozilla"
fi

OSTYPE=`uname -s`

if [ $OSTYPE == "Darwin" ]; then
MANIFEST="$SRCROOT/embedding/config/basebrowser-mac-macho"
else
MANIFEST="$SRCROOT/embedding/config/basebrowser-unix"
fi

#
#   A little help for my friends.
#
if [ "-h" == "$1" ];then 
    SHOWHELP="1"
fi
if [ "--help" == "$1" ];then 
    SHOWHELP="1"
fi
if [ "" == "$1" ]; then
    SHOWHELP="1"
fi
if [ "" == "$2" ]; then
    SHOWHELP="1"
fi
if [ "" == "$3" ]; then
    SHOWHELP="1"
fi


#
#   Show the help if required.
#
if [ $SHOWHELP ]; then
    echo "usage: $0 <save_results> <old_results> <summary>"
    echo "  <save_results> is a file that will receive the results of this run."
    echo "    This file can be used in a future run as the old results."
    echo "  <old_results> is a results file from a previous run."
    echo "    It is used to diff with current results and come up with a summary"
    echo "      of changes."
    echo "    It is OK if the file does not exist, just supply the argument."
    echo "  <summary> is a file which will contain a human readable report."
    echo "    This file is most useful by providing more information than the"
    echo "      normally single digit output of this script."
    echo ""
    echo "Run this command from the parent directory of the mozilla tree."
    echo ""
    echo "This command will output two numbers to stdout that will represent"
    echo "  the total size of all code and data, and a delta from the prior."
    echo "  the old results."
    echo "For much more detail on size drifts refer to the summary report."
    echo ""
    echo "This tool reports on executables listed in the following file:"
    echo "$MANIFEST"
    exit
fi


#
#   Stash our arguments away.
#
COPYSORTTSV="$1"
OLDTSVFILE="$2"
SUMMARYFILE="$3"


#
#   On Mac OS X, use the --zerodrift option to maptsvdifftool
#
if [ $OSTYPE == "Darwin" ]; then
ZERODRIFT="--zerodrift"
else
ZERODRIFT=""
fi


#
#   Create our temporary directory.
#   mktemp on Darwin doesn't support -d (suckage)
#
if [ $OSTYPE == "Darwin" ]; then
MYTMPDIR=`mktemp ./codesighs.tmp.XXXXXXXX`
rm $MYTMPDIR
mkdir $MYTMPDIR
else
MYTMPDIR=`mktemp -d ./codesighs.tmp.XXXXXXXX`
fi


# Check whether we have 'eu-readelf' or 'readelf' available.
# If we do, it will give more accurate symbol sizes than nm.

if [ $OSTYPE == "Darwin" ]; then
  USE_READELF=
else
READELF_PROG=`which eu-readelf 2>/dev/null | grep /eu-readelf$`
if test "$READELF_PROG"; then
  USE_READELF=1
else
  READELF_PROG=`which readelf 2>/dev/null | grep /readelf$`
  if test "$READELF_PROG"; then
    # Check whether we need -W
    if readelf --help | grep "\--wide" >&/dev/null; then
      READELF_PROG="readelf -W"
    else
      READELF_PROG="readelf"
    fi
    USE_READELF=1
  else
    USE_READELF=
  fi
fi
fi

#
#   Find all relevant files.
#
ALLFILES="$MYTMPDIR/allfiles.list"
grep -v '[\;\[]' < $MANIFEST | grep -v '^$' | sed "s|^|${OBJROOT}/dist/bin/|" > $ALLFILES


RAWTSVFILE="$MYTMPDIR/raw.tsv"

if test "$USE_READELF"; then
export READELF_PROG
xargs -n 1 $SRCROOT/tools/codesighs/readelf_wrap.pl < $ALLFILES > $RAWTSVFILE
else

#
#   Produce the cumulative nm output.
#   We are very particular on what switches to use.
#   nm --format=bsd --size-sort --print-file-name --demangle
#
#   Darwin (Mac OS X) has a lame nm that we have to wrap in a perl
#   script to get decent output.
#
NMRESULTS="$MYTMPDIR/nm.txt"
if [ $OSTYPE == "Darwin" ]; then
xargs -n 1 $SRCROOT/tools/codesighs/nm_wrap_osx.pl < $ALLFILES  > $NMRESULTS 2> /dev/null
else
xargs -n 1 nm --format=bsd --size-sort --print-file-name --demangle < $ALLFILES > $NMRESULTS 2> /dev/null
fi

#
#   Produce the TSV output.
#
$OBJROOT/dist/bin/nm2tsv --input $NMRESULTS > $RAWTSVFILE

fi  # USE_READELF

#
#   Sort the TSV output for useful diffing and eyeballing in general.
#
sort -r $RAWTSVFILE > $COPYSORTTSV


#
#   If a historical file was specified, diff it with our sorted tsv values.
#   Run it through a tool to summaries the diffs to the module
#       level report.
#   Otherwise, generate the module level report from our new data.
#
rm -f $SUMMARYFILE
DIFFFILE="$MYTMPDIR/diff.txt"
if [ -e $OLDTSVFILE ]; then
  diff $OLDTSVFILE $COPYSORTTSV > $DIFFFILE
  $OBJROOT/dist/bin/maptsvdifftool $ZERODRIFT --input $DIFFFILE >> $SUMMARYFILE
else
  $OBJROOT/dist/bin/codesighs --modules --input $COPYSORTTSV >> $SUMMARYFILE
fi


#
#   Output our numbers, that will let tinderbox specify everything all
#       at once.
#   First number is in fact the total size of all code and data in the map
#       files parsed.
#   Second number, if present, is growth/shrinkage.
#

if [ $TINDERBOX_OUTPUT ]; then
    echo -n "__codesize:"
fi
$OBJROOT/dist/bin/codesighs --totalonly --input $COPYSORTTSV

if [ -e $DIFFFILE ]; then
if [ $TINDERBOX_OUTPUT ]; then
    echo -n "__codesizeDiff:"
fi
    $OBJROOT/dist/bin/maptsvdifftool $ZERODRIFT --summary --input $DIFFFILE
fi

#
#   Remove our temporary directory.
#
rm -rf $MYTMPDIR
