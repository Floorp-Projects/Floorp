#!/bin/sh
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

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
