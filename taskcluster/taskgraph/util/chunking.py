# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

"""Utility functions to handle test chunking."""

import os
import json

from manifestparser import TestManifest
from manifestparser.filters import chunk_by_runtime
from mozbuild.util import memoize
from moztest.resolve import TestResolver, TestManifestLoader

from taskgraph import GECKO

here = os.path.abspath(os.path.dirname(__file__))
resolver = TestResolver.from_environment(cwd=here, loader_cls=TestManifestLoader)


def guess_mozinfo_from_task(task):
    """Attempt to build a mozinfo dict from a task definition.

    This won't be perfect and many values used in the manifests will be missing. But
    it should cover most of the major ones and be "good enough" for chunking in the
    taskgraph.

    Args:
        task (dict): A task definition.

    Returns:
        A dict that can be used as a mozinfo replacement.
    """
    info = {
        'asan': 'asan' in task['build-attributes']['build_platform'],
        'ccov': 'ccov' in task['build-attributes']['build_platform'],
        'debug': task['build-attributes']['build_type'] == 'debug',
        'e10s': task['attributes']['e10s'],
        'tsan': 'tsan' in task['build-attributes']['build_platform'],
        'webrender': task.get('webrender', False),
    }
    for platform in ('android', 'linux', 'mac', 'win'):
        if platform in task['build-attributes']['build_platform']:
            info['os'] = platform
            break
    else:
        raise ValueError("{} is not a known platform!".format(
                         task['build-attributes']['build_platform']))
    return info


@memoize
def get_runtimes(platform):
    base = os.path.join(GECKO, 'testing', 'runtimes', 'manifest-runtimes-{}.json')
    for key in ('android', 'windows'):
        if platform in key:
            path = base.format(key)
            break
    else:
        path = base.format('unix')

    with open(path, 'r') as fh:
        return json.load(fh)


@memoize
def get_tests(flavor, subsuite):
    return list(resolver.resolve_tests(flavor=flavor, subsuite=subsuite))


@memoize
def get_chunked_manifests(flavor, subsuite, chunks, mozinfo):
    """Compute which manifests should run in which chunks with the given category
    of tests.

    Args:
        flavor (str): The suite to run. Values are defined by the 'build_flavor' key
            in `moztest.resolve.TEST_SUITES`.
        subsuite (str): The subsuite to run or 'undefined' to denote no subsuite.
        chunks (int): Number of chunks to split manifests across.
        mozinfo (frozenset): Set of data in the form of (<key>, <value>) used
                             for filtering.

    Returns:
        A list of manifests where each item contains the manifest that should
        run in the corresponding chunk.
    """
    mozinfo = dict(mozinfo)
    chunker = chunk_by_runtime(None, chunks, get_runtimes(mozinfo['os']))

    # Compute all tests for the given suite/subsuite.
    tests = get_tests(flavor, subsuite)
    all_manifests = set(chunker.get_manifest(t) for t in tests)

    # Compute only the active tests.
    m = TestManifest()
    m.tests = tests
    tests = m.active_tests(disabled=False, exists=False, **mozinfo)
    active_manifests = set(chunker.get_manifest(t) for t in tests)

    # Run the chunking algorithm.
    chunked_manifests = [c[1] for c in chunker.get_chunked_manifests(active_manifests)]

    # Add all skipped manifests to the first chunk so they still show up in the
    # logs. They won't impact runtime much.
    skipped_manifests = all_manifests - active_manifests
    chunked_manifests[0].extend(skipped_manifests)
    return chunked_manifests
