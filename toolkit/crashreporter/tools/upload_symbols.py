#!/usr/bin/env python
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This script uploads a symbol zip file passed on the commandline
# to the Tecken symbol upload API at https://symbols.mozilla.org/ .
#
# token in the Tecken web interface. You must store the token in a Taskcluster
# secret as the JSON blob `{"token": "<token>"}` and set the `SYMBOL_SECRET`
# environment variable to the name of the Taskcluster secret.

from __future__ import absolute_import, print_function, unicode_literals

import argparse
import logging
import os
import sys
from mozbuild.base import MozbuildObject
log = logging.getLogger('upload-symbols')
log.setLevel(logging.INFO)

DEFAULT_URL = 'https://symbols.mozilla.org/upload/'
MAX_RETRIES = 5

def print_error(r):
    if r.status_code < 400:
        log.error('Error: bad auth token? ({0}: {1})'.format(r.status_code,
                                                         r.reason),
                  file=sys.stderr)
    else:
        log.error('Error: got HTTP response {0}: {1}'.format(r.status_code,
                                                         r.reason),
                  file=sys.stderr)

    log.error('Response body:\n{sep}\n{body}\n{sep}\n'.format(
        sep='=' * 20,
        body=r.text
        ))

def get_taskcluster_secret(secret_name):
    import redo
    import requests

    secrets_url = 'http://taskcluster/secrets/v1/secret/{}'.format(secret_name)
    log.info(
        'Using symbol upload token from the secrets service: "{}"'.format(secrets_url))
    res = requests.get(secrets_url)
    res.raise_for_status()
    secret = res.json()
    auth_token = secret['secret']['token']

    return auth_token

def main():
    config = MozbuildObject.from_environment()
    config._activate_virtualenv()

    logging.basicConfig()
    parser = argparse.ArgumentParser(
        description='Upload symbols in ZIP using token from Taskcluster secrets service.')
    parser.add_argument('zip',
                        help='Symbols zip file')
    args = parser.parse_args()

    if not os.path.isfile(args.zip):
        log.error('Error: zip file "{0}" does not exist!'.format(args.zip),
                  file=sys.stderr)
        return 1

    secret_name = os.environ.get('SYMBOL_SECRET')
    if secret_name is None:
        log.error('You must set the SYMBOL_SECRET environment variable!')
        return 1
    auth_token = get_taskcluster_secret(secret_name)

    if os.environ.get('MOZ_SCM_LEVEL', '1') == '1':
        # Use the Tecken staging server for try uploads for now.
        # This will eventually be changed in bug 1138617.
        url = 'https://symbols.stage.mozaws.net/upload/'
    else:
        url = DEFAULT_URL

    log.info('Uploading symbol file "{0}" to "{1}"'.format(args.zip, url))

    for i, _ in enumerate(redo.retrier(attempts=MAX_RETRIES), start=1):
        log.info('Attempt %d of %d...' % (i, MAX_RETRIES))
        try:
            r = requests.post(
                url,
                files={'symbols.zip': open(args.zip, 'rb')},
                headers={'Auth-Token': auth_token},
                allow_redirects=False,
                timeout=120)
            # 500 is likely to be a transient failure.
            # Break out for success or other error codes.
            if r.status_code < 500:
                break
            print_error(r)
        except requests.exceptions.RequestException as e:
            log.error('Error: {0}'.format(e))
        log.info('Retrying...')
    else:
        log.warn('Maximum retries hit, giving up!')
        return 1

    if r.status_code >= 200 and r.status_code < 300:
        log.info('Uploaded successfully!')
        return 0

    print_error(r)
    return 1

if __name__ == '__main__':
    sys.exit(main())
