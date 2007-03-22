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
# The Original Code is linux.gnuplot.in, released
# November 13, 2000.
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

# Treats the arguments as a command that is to be forked and observed;
# e.g.,
#
#   watch.sh ./mozilla -f bloaturls.txt
#
# Periodically snap-shots the virtual memory info of the process, and
# dumps the output to ``watch.out''

# Clear the output file
OUTPUT_FILE=watch.out
INTERVAL=10

while [ $# -gt 0 ]; do
    case "$1" in
    -o) OUTPUT_FILE=$2
        shift 2
        ;;
    -i) INTERVAL=$2
        shift 2
        ;;
    *)  break
        ;;
    esac
done

rm -f ${OUTPUT_FILE}

echo "vmsize vmexe vmlib vmdata vmstk vmrss" > ${OUTPUT_FILE}

# treat the arguments as the command to execute
$* &

# remember the process ID
PID=$!

while [ -e /proc/${PID} ]; do
    cat /proc/${PID}/status |\
    awk '$1=="VmSize:" { vmsize = $2; }
$1=="VmData:" { vmdata = $2; }
$1=="VmStk:" { vmstk = $2; }
$1=="VmExe:" { vmexe = $2; }
$1=="VmLib:" { vmlib = $2; }
$1=="VmRSS:" { vmrss = $2; }
END { print vmsize, vmexe, vmlib, vmdata, vmstk, vmrss; }' >> ${OUTPUT_FILE}
    sleep ${INTERVAL}
done
