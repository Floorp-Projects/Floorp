#!/bin/sh
#
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is historgram-diff.sh, released Dec 8, 2000.
#
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 2000 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s):
#    Chris Waterson <waterson@netscape.com>
#
# This shell script takes two `object histograms' produced using
# `histogram.pl' and prints the delta, sorted such that the objects
# that increased the most in raw memory usage are displayed at the
# top.
#
# Usage:
#
#   histogram-pretty.sh [-c <count>] <file>
#
# Pretty-print the histogram in file <file>, displaying at most
# <count> rows.

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

FILE=$1

# The first `awk' script computes a `TOTAL' row. Then, we sort by the
# larges delta in bytes.
awk '{ tobj += $2; tbytes += $3; } END { print "TOTAL", tobj, tbytes; }' ${FILE} > /tmp/$$.sorted

sort -nr +2 ${FILE} >> /tmp/$$.sorted

# Pretty-print, including percentages
cat <<EOF > /tmp/$$.awk
BEGIN {
  print "Type                    Count    Bytes %Total";
  }
\$1 == "TOTAL" {
  tbytes = \$3;
  }
NR <= $COUNT {
  printf "%-22s %6d %8d %6.2lf\n", \$1, \$2, \$3, 100.0 * \$3 / tbytes;
  }
NR > $COUNT {
  oobjs += \$2;  obytes += \$3;
  }
END {
  printf "%-22s %6d %8d %6.2lf\n", "OTHER", oobjs, obytes, obytes * 100.0 / tbytes;
  }
EOF

# Now pretty print the file, and spit it out on stdout.
awk -f /tmp/$$.awk /tmp/$$.sorted

rm -f /tmp/$$.awk /tmp/$$.sorted
