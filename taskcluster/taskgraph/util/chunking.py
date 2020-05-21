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
        if key in platform:
            path = base.format(key)
            break
    else:
        path = base.format('unix')

    with open(path, 'r') as fh:
        return json.load(fh)


@memoize
def get_tests(flavor, subsuite):
    return list(resolver.resolve_tests(flavor=flavor, subsuite=subsuite))


# This is a bit of a hack to avoid evaluating the entire list of WPT test
# objects in multiple places. It can be removed when the TestResolver becomes
# the source of truth for WPT groups.
wpt_group_translation = defaultdict(set)


def get_wpt_group(test):
    """Get the group for a web-platform-test that matches those created by the
    WPT harness.

    Args:
        test (dict): The test object to compute the group for.

    Returns:
        str: Label representing the group name.
    """
    depth = 3

    path = os.path.dirname(test['name'])
    # Remove path elements beyond depth of 3, because WPT harness uses
    # --run-by-dir=3 by convention in Mozilla CI.
    # Note, this means /_mozilla tests retain an effective depth of 2 due to
    # the extra prefix element.
    while path.count('/') >= depth + 1:
        path = os.path.dirname(path)

    return path


@memoize
def get_manifests(flavor, subsuite, mozinfo):
    """Compute which manifests should run for the given flavor, subsuite and mozinfo.

    This function returns skipped manifests separately so that more balanced
    chunks can be achieved by only considering "active" manifests in the
    chunking algorithm.

    Args:
        flavor (str): The suite to run. Values are defined by the 'build_flavor' key
            in `moztest.resolve.TEST_SUITES`.
        subsuite (str): The subsuite to run or 'undefined' to denote no subsuite.
        mozinfo (frozenset): Set of data in the form of (<key>, <value>) used
                             for filtering.

    Returns:
        A tuple of two manifest lists. The first is the set of active manifests (will
        run at least one test. The second is a list of skipped manifests (all tests are
        skipped).
    """
    mozinfo = dict(mozinfo)
    # Compute all tests for the given suite/subsuite.
    tests = get_tests(flavor, subsuite)

    if flavor == "web-platform-tests":
        manifests = set()
        for t in tests:
            group = get_wpt_group(t)
            wpt_group_translation[t['manifest']].add(group)
            manifests.add(t['manifest'])

        return {"active": list(manifests), "skipped": []}

    manifests = set(chunk_by_runtime.get_manifest(t) for t in tests)

    # Compute  the active tests.
    m = TestManifest()
    m.tests = tests
    tests = m.active_tests(disabled=False, exists=False, **mozinfo)
    active = set(chunk_by_runtime.get_manifest(t) for t in tests)
    skipped = manifests - active
    return {"active": list(active), "skipped": list(skipped)}


def chunk_manifests(flavor, platform, chunks, manifests):
    """Run the chunking algorithm.

    Args:
        platform (str): Platform used to find runtime info.
        chunks (int): Number of chunks to split manifests into.
        manifests(list): Manifests to chunk.

    Returns:
        A list of length `chunks` where each item contains a list of manifests
        that run in that chunk.
    """

    if flavor != "web-platform-tests":
        return [
            c[1] for c in chunk_by_runtime(
                None,
                chunks,
                get_runtimes(platform)
            ).get_chunked_manifests(manifests)
        ]

    paths = {k: v for k, v in wpt_group_translation.items() if k in manifests}

    # Filter out non-web-platform-test runtime information.
    runtimes = get_runtimes(platform)
    runtimes = [(k, v) for k, v in runtimes.items()
                if k.startswith('/') and not os.path.splitext(k)[-1]
                if k in manifests]

    # Keep track of test paths for each chunk, and the runtime information.
    chunked_manifests = [[[], 0] for _ in range(chunks)]

    # First, chunk tests that have runtime information available.
    for key, rt in sorted(runtimes, key=lambda x: x[1], reverse=True):
        # Sort the chunks from fastest to slowest, based on runtimme info
        # at x[1], then the number of test paths.
        chunked_manifests.sort(key=lambda x: (x[1], len(x[0])))
        test_paths = paths[key]
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
        chunked_manifests[0][0].extend(test_paths)

    # One last sort by the runtime, then number of test paths.
    chunked_manifests.sort(key=lambda x: (x[1], len(x[0])))

    # Reassign variable to contain just the chunked test paths.
    chunked_manifests = [c[0] for c in chunked_manifests]
    return chunked_manifests
