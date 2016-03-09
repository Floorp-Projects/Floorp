# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import subprocess

def main(fp, input):
    temporary_file = 'mozilla-trace.h.tmp'
    subprocess.check_call(['dtrace', '-x', 'nolibs', '-h', '-s', input, '-o', temporary_file])
    
    with open(temporary_file, 'r') as temporary_fp:
        output = temporary_fp.read()
    fp.write(output.replace('if _DTRACE_VERSION', 'ifdef INCLUDE_MOZILLA_DTRACE'))
    os.remove(temporary_file)
