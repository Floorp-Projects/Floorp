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
# The Original Code is basesummary.win.bash code, released
# Nov 15, 2002.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 2002
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Garrett Arch Blythe, 15-November-2002
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

MANIFEST="$SRCROOT/embedding/config/basebrowser-win"

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
#   Figure out which modules in specific we care about.
#   The relevant set meaning that the map file name prefix must be found
#       in the file mozilla/embedding/config/basebrowser-win.
#
RELEVANTSETFILE="$MYTMPDIR/relevant.set"
grep -v '\;' < $MANIFEST | sed 's/.*\\//' | grep '\.[eEdD][xXlL][eElL]' | sed 's/\.[eEdD][xXlL][eElL]//' > $RELEVANTSETFILE
RELEVANTARG=`xargs -n 1 echo --match-module < $RELEVANTSETFILE`


#
#   Produce the TSV output.
#
RAWTSVFILE="$MYTMPDIR/raw.tsv"
$OBJROOT/dist/bin/msmap2tsv --symdb $SYMDBFILE --batch $RELEVANTARG < $ALLMAPSFILE > $RAWTSVFILE 2> /dev/null


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
