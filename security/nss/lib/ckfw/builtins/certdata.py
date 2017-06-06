#!/usr/bin/env python
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import subprocess
import os
import sys

def main():
    args = [os.path.realpath(x) for x in sys.argv[1:]]
    script = os.path.dirname(os.path.abspath(__file__))+'/certdata.perl'
    subprocess.check_call([os.environ.get('PERL', 'perl'), script] + args,
                          env=os.environ)

if __name__ == '__main__':
    main()
