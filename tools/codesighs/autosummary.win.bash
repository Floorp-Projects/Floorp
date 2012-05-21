#!/bin/bash
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

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
    echo "This tool reports on all executables in the directory tree."
    exit
fi


#
#   Stash our arguments away.
#
COPYSORTTSV="$1"
OLDTSVFILE="$2"
SUMMARYFILE="$3"


#
#   Create our temporary directory.
#
MYTMPDIR=`mktemp -d ./codesighs.tmp.XXXXXXXX`


#
#   Find the types of files we are interested in.
#
ONEFINDPASS="$MYTMPDIR/onefind.list"
/usr/bin/find $OBJROOT -type f -name "*.obj" -or -name "*.map" | while read FNAME; do
    cygpath -m $FNAME >> $ONEFINDPASS
done


#
#   Find all object files.
#
ALLOBJSFILE="$MYTMPDIR/allobjs.list"
grep -i "\.obj$" < $ONEFINDPASS > $ALLOBJSFILE


#
#   Get a dump of the symbols in every object file.
#
ALLOBJSYMSFILE="$MYTMPDIR/allobjsyms.list"
xargs -n 1 dumpbin.exe /symbols < $ALLOBJSFILE > $ALLOBJSYMSFILE 2> /dev/null


#
#   Produce the symdb for the symbols in all object files.
#
SYMDBFILE="$MYTMPDIR/symdb.tsv"
$OBJROOT/dist/bin/msdump2symdb --input $ALLOBJSYMSFILE | /usr/bin/sort > $SYMDBFILE 2> /dev/null


#
#   Find all map files.
#
ALLMAPSFILE="$MYTMPDIR/allmaps.list"
grep -i "\.map$" < $ONEFINDPASS > $ALLMAPSFILE


#
#   Produce the TSV output.
#
RAWTSVFILE="$MYTMPDIR/raw.tsv"
$OBJROOT/dist/bin/msmap2tsv --symdb $SYMDBFILE --batch < $ALLMAPSFILE > $RAWTSVFILE 2> /dev/null


#
#   Sort the TSV output for useful diffing and eyeballing in general.
#
/usr/bin/sort -r $RAWTSVFILE > $COPYSORTTSV


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
  $OBJROOT/dist/bin/maptsvdifftool --negation --input $DIFFFILE | dos2unix >> $SUMMARYFILE
else
  $OBJROOT/dist/bin/codesighs --modules --input $COPYSORTTSV | dos2unix >> $SUMMARYFILE
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
$OBJROOT/dist/bin/codesighs --totalonly --input $COPYSORTTSV | dos2unix


if [ -e $DIFFFILE ]; then
if [ $TINDERBOX_OUTPUT ]; then
    echo -n "__codesizeDiff:"
fi
    $OBJROOT/dist/bin/maptsvdifftool --negation --summary --input $DIFFFILE | dos2unix
fi

#
#   Remove our temporary directory.
#
rm -rf $MYTMPDIR
