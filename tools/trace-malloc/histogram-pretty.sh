#!/bin/sh
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#   histogram-pretty.sh [-c <count>] [-w <width>] <file>
#
# Pretty-print the histogram in file <file>, displaying at most
# <count> rows.

# How many rows are we gonna show?
COUNT=20
WIDTH=22

# Read arguments
while [ $# -gt 0 ]; do
    case "$1" in
    -c) COUNT=$2
        shift 2
        ;;
    -w) WIDTH=$2
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
  printf "%-${WIDTH}s  Count    Bytes %Total   %Cov\n", "Type";
  }
\$1 == "TOTAL" {
  tbytes = \$3;
  }
NR <= $COUNT {
  if (\$1 != "TOTAL") {
    covered += \$3;
  }
  printf "%-${WIDTH}s %6d %8d %6.2lf %6.2lf\n", \$1, \$2, \$3, 100.0 * \$3 / tbytes, 100.0 * covered / tbytes;
  }
NR > $COUNT {
  oobjs += \$2;  obytes += \$3; covered += \$3;
  }
END {
  printf "%-${WIDTH}s %6d %8d %6.2lf %6.2lf\n", "OTHER", oobjs, obytes, obytes * 100.0 / tbytes, covered * 100.0 / tbytes;
  }
EOF

# Now pretty print the file, and spit it out on stdout.
awk -f /tmp/$$.awk /tmp/$$.sorted

rm -f /tmp/$$.awk /tmp/$$.sorted
