#!/bin/sh
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
# The Original Code is historgram-diff.sh, released
# Dec 8, 2000.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 2000
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Chris Waterson <waterson@netscape.com>
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

#   histogram-diff.sh [-c <count>] <base> <incr>
#
# Compute incremental memory growth from histogram in file <base> to
# histogram in file <incr>, displaying at most <count> rows.

# How many rows are we gonna show?
COUNT=20

# Read arguments
while [ $# -gt 0 ]; do
    case "$1" in
    -c) COUNT=$2
        shift 2
        ;;
    *)  break
        ;;
    esac
done

BASE=$1
INCR=$2

# Sort the base and incremental files so that we can `join' them on
# the type name
sort $BASE > /tmp/$$.left
sort $INCR > /tmp/$$.right

# Do the join. The `awk' script computes the difference between
# the base and the incremental files.
join /tmp/$$.left /tmp/$$.right \
    | awk '{ print $1, $2, $3, $4, $5, $4 - $2, $5 - $3; }' \
    > /tmp/$$.joined

rm -f /tmp/$$.left /tmp/$$.right

# Now compute a `TOTAL' row.
awk '{ tobj1 += $2; tbytes1 += $3; tobj2 += $4; tbytes2 += $5; tdobj += $6; tdbytes += $7; } END { print "TOTAL", tobj1, tbytes1, tobj2, tbytes2, tdobj, tdbytes; }' /tmp/$$.joined \
    > /tmp/$$.sorted

# Then, we sort by the largest delta in bytes.
sort -nr +6 /tmp/$$.joined >> /tmp/$$.sorted

rm -f /tmp/$$.joined

# Pretty-print, including percentages
cat <<EOF > /tmp/$$.awk
BEGIN {
  print "                        ---- Base ----   ---- Incr ----   ----- Difference ----";
  print "Type                    Count    Bytes   Count    Bytes   Count    Bytes %Total";
  }
\$1 == "TOTAL" {
  tbytes = \$7;
  }
NR <= $COUNT {
  printf "%-22s %6d %8d  %6d %8d  %6d %8d %6.2lf\n", \$1, \$2, \$3, \$4, \$5, \$6, \$7, 100.0 * \$7 / tbytes;
  }
NR > $COUNT {
  oobjs1 += \$2;  obytes1 += \$3;
  oobjs2 += \$4;  obytes2 += \$5;
  odobjs += \$6;  odbytes += \$7;
  }
END {
  printf "%-22s %6d %8d  %6d %8d  %6d %8d %6.2lf\n", "OTHER", oobjs1, obytes1, oobjs2, obytes2, odobjs, odbytes, odbytes * 100.0 / tbytes;
  }
EOF

# Now pretty print the file, and spit it out on stdout.
awk -f /tmp/$$.awk /tmp/$$.sorted

rm -f /tmp/$$.awk /tmp/$$.sorted
