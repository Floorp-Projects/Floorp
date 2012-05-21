# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

kdir=${1:-../kernel}
scriptdir=$(dirname $0)
udir=$(dirname $scriptdir)
cat $scriptdir/kernel-files.txt | \
(while read l r; do
    left=$(eval echo $l)
    right=$(eval echo $r)
#    echo $left $right
    diff -up $left $right
done)
