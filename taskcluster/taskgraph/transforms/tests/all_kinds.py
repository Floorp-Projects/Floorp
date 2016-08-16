# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Changes here apply to all tests, regardless of kind.

This is a great place for:

 * Applying rules based on platform, project, etc. that should span kinds
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.util.treeherder import split_symbol, join_symbol
from taskgraph.transforms.base import TransformSequence, get_keyed_by

import copy


transforms = TransformSequence()


@transforms.add
def set_worker_implementation(config, tests):
    """Set the worker implementation based on the test platform."""
    for test in tests:
        # this is simple for now, but soon will not be!
        test['worker-implementation'] = 'docker-worker'
        yield test


@transforms.add
def set_tier(config, tests):
    """Set the tier based on policy for all test descriptions that do not
    specify a tier otherwise."""
    for test in tests:
        # only override if not set for the test
        if 'tier' not in test:
            if test['test-platform'] in ['linux64/debug',
                                         'linux64-asan/opt',
                                         'android-4.3-arm7-api-15/debug']:
                test['tier'] = 1
            else:
                test['tier'] = 2
        yield test


@transforms.add
def set_expires_after(config, tests):
    """Try jobs expire after 2 weeks; everything else lasts 1 year.  This helps
    keep storage costs low."""
    for test in tests:
        if 'expires-after' not in test:
            if config.params['project'] == 'try':
                test['expires-after'] = "14 days"
            else:
                test['expires-after'] = "1 year"
        yield test


@transforms.add
def set_download_symbols(config, tests):
    """In general, we download symbols immediately for debug builds, but only
    on demand for everything else. ASAN builds shouldn't download
    symbols since they don't product symbol zips see bug 1283879"""
    for test in tests:
        if test['test-platform'].split('/')[-1] == 'debug':
            test['mozharness']['download-symbols'] = True
        elif test['build-platform'] == 'linux64-asan/opt':
            if 'download-symbols' in test['mozharness']:
                del test['mozharness']['download-symbols']
        else:
            test['mozharness']['download-symbols'] = 'ondemand'
        yield test


@transforms.add
def resolve_keyed_by(config, tests):
    """Resolve fields that can be keyed by platform, etc."""
    fields = [
        'instance-size',
        'max-run-time',
        'chunks',
    ]
    for test in tests:
        for field in fields:
            test[field] = get_keyed_by(item=test, field=field, item_name=test['test-name'])
        test['mozharness']['config'] = get_keyed_by(item=test,
                                                    field='mozharness',
                                                    subfield='config',
                                                    item_name=test['test-name'])
        yield test


@transforms.add
def split_chunks(config, tests):
    """Based on the 'chunks' key, split tests up into chunks by duplicating
    them and assigning 'this-chunk' appropriately and updating the treeherder
    symbol."""
    for test in tests:
        if test['chunks'] == 1:
            test['this-chunk'] = 1
            yield test
            continue

        for this_chunk in range(1, test['chunks'] + 1):
            # copy the test and update with the chunk number
            chunked = copy.deepcopy(test)
            chunked['this-chunk'] = this_chunk

            # add the chunk number to the TH symbol
            group, symbol = split_symbol(chunked['treeherder-symbol'])
            symbol += str(this_chunk)
            chunked['treeherder-symbol'] = join_symbol(group, symbol)

            yield chunked
