#!/bin/bash
#
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is uuidgrep.bash code, released
# Nov 26, 2002.
#
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 2002 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s):
#    Garrett Arch Blythe, 26-November-2002
#
# Alternatively, the contents of this file may be used under the
# terms of the GNU Public License (the "GPL"), in which case the
# provisions of the GPL are applicable instead of those above.
# If you wish to allow use of your version of this file only
# under the terms of the GPL and not to allow others to use your
# version of this file under the MPL, indicate your decision by
# deleting the provisions above and replace them with the notice
# and other provisions required by the GPL.  If you do not delete
# the provisions above, a recipient may use your version of this
# file under either the MPL or the GPL.
#

#
# This file is meant to be run from the parent directory of the
#  source tree.
# It does some fairly brain dead grepping to determine where
#  uuids are defined, and where they may be refereced.
#
# A report is generated in the end, which could be saved.
# There are two sections to the report, on one usage and one on
#  definitions.
#
# One day a stronger tool will likely be written, but this is a start
#  on reporting source dependencies on uuids.
#


# Place to store stuff.
MYTMPDIR=`mktemp -d /tmp/deps.tmp.XXXXXXXX`

# What we are matching on.
# If you want only CIDs, or IIDs, change.
SEARCHING4="[~#]*NS_DEFINE_[CI]ID[:space:]*(.*,.*)[:space:]*;"

# Find the source files.
# Exclude the dist directory to find the headers in their natural dirs.
ALLSOURCEFILES=$MYTMPDIR/allsources.txt
find . -type f -and \( -name \*.cpp -or -name \*.c -or -name \*.h \) > $ALLSOURCEFILES

# Go through the sources and find what we want.
# Assuming it is all on one line....
export IDMATCHFILE=$MYTMPDIR/idmatches.txt
xargs -l grep -Hn $SEARCHING4 < $ALLSOURCEFILES > $IDMATCHFILE

# Seperate the variable names out of the matches.
# We have the possibility here of having duplicates with differing names
#  or of having different CIDs with the same names here, but this is as
#  good as it gets for now.
VARNAMESFILE=$MYTMPDIR/varnames.txt
sed "{ s/.*://; s/\/\/.*//; s/\/\*.*\*\///; s/.*(//; s/[#,].*//; s/ *//; }" < $IDMATCHFILE | grep -v \^\$ | sort | uniq > $VARNAMESFILE

# Create a file that has states which variable were defined where.
# This also helps with identification of duplicate names
export DEFINITIONFILE=$MYTMPDIR/definevars.txt
testdefinition () {
    FILENAMES=`grep $0 $IDMATCHFILE | sed s/:.*//`
    if [ "" != "$FILENAMES" ]; then
	echo $0:$FILENAMES
    fi
}
export -f testdefinition
xargs -l bash -c testdefinition < $VARNAMESFILE > $DEFINITIONFILE
export -n testdefinition

# Find all sources which use variable names.
# This will imply which libraries use the IDs, subsequently linking with said
#  library would cause a dependency.
# This is an inferior matching method compared to actually looking at the
#  symbols in resultant binaries.
export GREPVARMATCHFILE=$MYTMPDIR/grepvarmatches.txt
xargs -l grep -F -Hn --file=$VARNAMESFILE < $ALLSOURCEFILES > $GREPVARMATCHFILE

# Make a variable match file that is more readable.
# Basically, remove the actual code and leave only varaible to file mapping.
export VARMATCHFILE=$MYTMPDIR/usevars.txt
testvarname () {
    grep $0 $GREPVARMATCHFILE | sed s/:.*$0.*/:$0/
}
export -f testvarname
xargs -l bash -c testvarname < $VARNAMESFILE | sort | uniq > $VARMATCHFILE
export -n testvarname

# Make a file which only contains filenames that use variables.
LISTUSERFILES=$MYTMPDIR/listuserfiles.txt
stripfname() {
    THEFNAME=`echo $0 | sed s/:.*//`
    echo $THEFNAME
}
export -f stripfname
xargs -l bash -c stripfname < $VARMATCHFILE | sort | uniq > $LISTUSERFILES
export -n stripfname

# Output a delimiter.
# Output a list of files that use the vars.
# With each file, output the variable names.
echo -e \*\*\* DELIMITER \*\*\*  FILE depends on ID\\n
listusers() {
    echo -e $0 depends on:
    SYMBOLS=`grep $0 $VARMATCHFILE | sed s/.*://`
    for symbol in $SYMBOLS; do
	echo -e \\t$symbol
    done
    echo -e \\n
}
export -f listusers
xargs -l bash -c listusers < $LISTUSERFILES
export -n listusers

# Output a delimiter.
# Output a list of variables.
# With each variable, output the files which defined them.
echo -e \*\*\* DELIMITER \*\*\*  ID defined in FILE\\n
listdefs() {
    echo -e $0 defined in:
    DEFINES=`grep $0 $DEFINITIONFILE | sed s/.*://`
    for define in $DEFINES; do
	echo -e \\t$define
    done
    echo -e \\n
}
export -f listdefs
xargs -l bash -c listdefs < $VARNAMESFILE
export -n listdefs

# Done with the temporary stuff.
rm -rf $MYTMPDIR
