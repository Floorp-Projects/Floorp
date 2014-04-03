# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys


if len(sys.argv) != 4:
    raise Exception('Usage: minify_js_verify <exitcode> <orig> <minified>')

sys.exit(int(sys.argv[1]))
