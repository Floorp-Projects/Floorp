# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function
import sys


if len(sys.argv) != 4:
    raise Exception('Usage: minify_js_verify <exitcode> <orig> <minified>')

retcode = int(sys.argv[1])

if retcode:
    print('Error message', file=sys.stderr)

sys.exit(retcode)
