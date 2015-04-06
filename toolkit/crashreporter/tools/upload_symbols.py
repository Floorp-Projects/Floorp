#!/usr/bin/env python
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This script uploads a symbol zip file passed on the commandline
# to the Socorro symbol upload API hosted on crash-stats.mozilla.org.
#
# Using this script requires you to have generated an authentication
# token in the crash-stats web interface. You must put the token in a file
# and set SOCORRO_SYMBOL_UPLOAD_TOKEN_FILE to the path to the file in
# the mozconfig you're using.

from __future__ import print_function

import os
import requests
import sys

from buildconfig import substs

url = 'https://crash-stats.mozilla.com/symbols/upload'

def main():
    if len(sys.argv) != 2:
        print('Usage: uploadsymbols.py <zip file>', file=sys.stderr)
        return 1

    if not os.path.isfile(sys.argv[1]):
        print('Error: zip file "{0}" does not exist!'.format(sys.argv[1]),
              file=sys.stderr)
        return 1
    symbols_zip = sys.argv[1]

    if 'SOCORRO_SYMBOL_UPLOAD_TOKEN_FILE' not in substs:
        print('Error: you must set SOCORRO_SYMBOL_UPLOAD_TOKEN_FILE in your mozconfig!', file=sys.stderr)
        return 1
    token_file = substs['SOCORRO_SYMBOL_UPLOAD_TOKEN_FILE']

    if not os.path.isfile(token_file):
        print('Error: SOCORRO_SYMBOL_UPLOAD_TOKEN_FILE "{0}" does not exist!'.format(token_file), file=sys.stderr)
        return 1
    auth_token = open(token_file, 'r').read().strip()

    print('Uploading symbol file "{0}" to "{1}"...'.format(sys.argv[1], url))

    try:
        r = requests.post(
            url,
            files={'symbols.zip': open(sys.argv[1], 'rb')},
            headers={'Auth-Token': auth_token},
            allow_redirects=False,
            timeout=120,
        )
    except requests.exceptions.RequestException as e:
        print('Error: {0}'.format(e))
        return 1

    if r.status_code >= 200 and r.status_code < 300:
        print('Uploaded successfully!')
    elif r.status_code < 400:
        print('Error: bad auth token? ({0})'.format(r.status_code),
              file=sys.stderr)
        return 1
    else:
        print('Error: got HTTP response {0}'.format(r.status_code),
              file=sys.stderr)
        return 1
    return 0

if __name__ == '__main__':
    sys.exit(main())

