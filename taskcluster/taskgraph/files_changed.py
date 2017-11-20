# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Support for optimizing tasks based on the set of files that have changed.
"""

from __future__ import absolute_import, print_function, unicode_literals

import logging
import requests
from redo import retry
from mozpack.path import match as mozpackmatch, join as join_path

logger = logging.getLogger(__name__)
_cache = {}


def get_changed_files(repository, revision):
    """
    Get the set of files changed in the push headed by the given revision.
    Responses are cached, so multiple calls with the same arguments are OK.
    """
    key = repository, revision
    if key not in _cache:
        url = '%s/json-automationrelevance/%s' % (repository.rstrip('/'), revision)
        logger.debug("Querying version control for metadata: %s", url)

        def get_automationrelevance():
            response = requests.get(url, timeout=5)
            return response.json()
        contents = retry(get_automationrelevance, attempts=2, sleeptime=10)

        logger.debug('{} commits influencing task scheduling:'
                     .format(len(contents['changesets'])))
        changed_files = set()
        for c in contents['changesets']:
            logger.debug(" {cset} {desc}".format(
                cset=c['node'][0:12],
                desc=c['desc'].splitlines()[0].encode('ascii', 'ignore')))
            changed_files |= set(c['files'])

        _cache[key] = changed_files
    return _cache[key]


def check(params, file_patterns):
    """Determine whether any of the files changed in the indicated push to
    https://hg.mozilla.org match any of the given file patterns."""
    repository = params.get('head_repository')
    revision = params.get('head_rev')
    if not repository or not revision:
        logger.warning("Missing `head_repository` or `head_rev` parameters; "
                       "assuming all files have changed")
        return True

    changed_files = get_changed_files(repository, revision)

    if 'comm_repository' in params:
        repository = params.get('comm_head_repository')
        revision = params.get('comm_head_rev')
        if not revision:
            logger.warning("Missing `comm_head_rev` parameters; "
                           "assuming all files have changed")
            return True

        changed_files |= {
            join_path("comm", file) for file in
            get_changed_files(repository, revision)
        }

    for pattern in file_patterns:
        for path in changed_files:
            if mozpackmatch(path, pattern):
                return True

    return False
