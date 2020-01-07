# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

"""Utility functions to handle test chunking."""

import os
import json

from manifestparser.filters import chunk_by_runtime
from mozbuild.util import memoize
from moztest.resolve import TestResolver, TestManifestLoader

from taskgraph import GECKO

here = os.path.abspath(os.path.dirname(__file__))
resolver = TestResolver.from_environment(cwd=here, loader_cls=TestManifestLoader)


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


@memoize
def get_chunked_manifests(flavor, subsuite, platform, chunks):
    """Compute which manifests should run in which chunks with the given category
    of tests.

    Args:
        flavor (str): The suite to run. Values are defined by the 'build_flavor' key
            in `moztest.resolve.TEST_SUITES`.
        subsuite (str): The subsuite to run or 'undefined' to denote no subsuite.
        platform (str): Platform used to find runtime data.
        chunks (int): Number of chunks to split manifests across.

    Returns:
        A list of manifests where each item contains the manifest that should
        run in the corresponding chunk.
    """
    tests = get_tests(flavor, subsuite)
    return [
        c[1] for c in chunk_by_runtime(
            None,
            chunks,
            get_runtimes(platform)
        ).get_chunked_manifests(tests)
    ]
