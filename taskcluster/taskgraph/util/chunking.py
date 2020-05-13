# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

"""Utility functions to handle test chunking."""

import os
import json

from collections import defaultdict

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


def tests_by_top_directory(tests, depth):
    """Given a list of test objects, return a dictionary of test paths keyed by
    the top level group.

    Args:
        tests (list): List of test objects for the particular suite and subsuite.
        depth (int, optional): The maximum depth to consider when grouping tests.

    Returns:
        results (dict): Dictionary representation of test paths grouped by the
            top level group name.
    """
    results = defaultdict(list)

    # Iterate over the set of test objects to build a set of test paths keyed by
    # the top directory.
    for t in tests:

        path = os.path.dirname(t['name'])
        # Remove path elements beyond depth of 3, because WPT harness uses
        # --run-by-dir=3 by convention in Mozilla CI.
        # Note, this means /_mozilla tests retain an effective depth of 2 due to
        # the extra prefix element.
        while path.count('/') >= depth + 1:
            path = os.path.dirname(path)

        # Extract the first path component (top level directory) as the key.
        # This value should match the path in manifest-runtimes JSON data.
        # Mozilla WPT paths have one extra URL component in the front.
        components = 3 if t['name'].startswith('/_mozilla') else 2
        key = '/'.join(t['name'].split('/')[:components])

        results[key].append(path)
    return results


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
    # Compute all tests for the given suite/subsuite.
    tests = get_tests(flavor, subsuite)

    if flavor == 'web-platform-tests':
        paths = tests_by_top_directory(tests, 3)

        # Filter out non-web-platform-test runtime information.
        runtimes = get_runtimes(mozinfo['os'])
        runtimes = [(k, v) for k, v in runtimes.items()
                    if k.startswith('/') and not os.path.splitext(k)[-1]]

        # Keep track of test paths for each chunk, and the runtime information.
        chunked_manifests = [[[], 0] for _ in range(chunks)]

        # First, chunk tests that have runtime information available.
        for key, rt in sorted(runtimes, key=lambda x: x[1], reverse=True):
            # Sort the chunks from fastest to slowest, based on runtimme info
            # at x[1], then the number of test paths.
            chunked_manifests.sort(key=lambda x: (x[1], len(x[0])))
            test_paths = set(paths[key])
            if test_paths:
                # The full set of paths that live under the key must be added
                # to the chunks.
                # The smart scheduling algorithm uses paths of up to depth of 3
                # as generated by the WPT harness and adding the set of paths will
                # enable the scheduling algorithm to evaluate if the given path
                # should be scheduled or not.
                chunked_manifests[0][0].extend(test_paths)
                chunked_manifests[0][1] += rt
                # Remove from the list of paths that need scheduling.
                paths.pop(key)

        # Chunk remaining test paths that were not chunked in the previous step.
        # These are test paths that did not have runtime information available.
        for test_paths in paths.values():
            chunked_manifests.sort(key=lambda x: (x[1], len(x[0])))
            chunked_manifests[0][0].extend(set(test_paths))

        # One last sort by the runtime, then number of test paths.
        chunked_manifests.sort(key=lambda x: (x[1], len(x[0])))

        # Reassign variable to contain just the chunked test paths.
        chunked_manifests = [c[0] for c in chunked_manifests]
    else:
        chunker = chunk_by_runtime(None, chunks, get_runtimes(mozinfo['os']))
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
