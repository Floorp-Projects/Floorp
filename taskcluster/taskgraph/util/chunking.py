# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

"""Utility functions to handle test chunking."""

import json
import logging
import os
from abc import ABCMeta, abstractmethod

import six
from manifestparser import TestManifest
from manifestparser.filters import chunk_by_runtime
from mozbuild.util import memoize
from moztest.resolve import (
    TEST_SUITES,
    TestResolver,
    TestManifestLoader,
)

from taskgraph import GECKO
from taskgraph.util.bugbug import BugbugTimeoutException, CT_LOW, push_schedules

logger = logging.getLogger(__name__)
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
        "asan": "asan" in task["build-attributes"]["build_platform"],
        "bits": 32 if "32" in task["build-attributes"]["build_platform"] else 64,
        "ccov": "ccov" in task["build-attributes"]["build_platform"],
        "debug": task["build-attributes"]["build_type"] == "debug",
        "e10s": task["attributes"]["e10s"],
        "fission": task["attributes"].get("unittest_variant") == "fission",
        "headless": "-headless" in task["test-name"],
        "tsan": "tsan" in task["build-attributes"]["build_platform"],
        "webrender": task.get("webrender", False),
    }
    for platform in ("android", "linux", "mac", "win"):
        if platform in task["build-attributes"]["build_platform"]:
            info["os"] = platform
            break
    else:
        raise ValueError(
            "{} is not a known platform!".format(
                task["build-attributes"]["build_platform"]
            )
        )

    # crashreporter is disabled for asan / tsan builds
    if info["asan"] or info["tsan"]:
        info["crashreporter"] = False
    else:
        info["crashreporter"] = True

    info["appname"] = "fennec" if info["os"] == "android" else "firefox"

    # guess processor
    if "aarch64" in task["build-attributes"]["build_platform"]:
        info["processor"] = "aarch64"
    elif info["os"] == "android" and "arm" in task["test-platform"]:
        info["processor"] = "arm"
    elif info["bits"] == 32:
        info["processor"] = "x86"
    else:
        info["processor"] = "x86_64"

    # guess toolkit
    if info["os"] == "android":
        info["toolkit"] = "android"
    elif info["os"] == "win":
        info["toolkit"] = "windows"
    elif info["os"] == "mac":
        info["toolkit"] = "cocoa"
    else:
        info["toolkit"] = "gtk"

    # guess os_version
    os_versions = {
        "linux1804": "18.04",
        "macosx1015": "10.15",
        "macosx1100": "11.00",
        "windows7": "6.1",
        "windows10": "10.0",
    }
    for platform, version in os_versions.items():
        if platform in task["test-platform"]:
            info["os_version"] = version
            break

    return info


@memoize
def get_runtimes(platform, suite_name):
    if not suite_name or not platform:
        raise TypeError("suite_name and platform cannot be empty.")

    base = os.path.join(GECKO, "testing", "runtimes", "manifest-runtimes-{}.json")
    for key in ("android", "windows"):
        if key in platform:
            path = base.format(key)
            break
    else:
        path = base.format("unix")

    if not os.path.exists(path):
        raise IOError("manifest runtime file at {} not found.".format(path))

    with open(path, "r") as fh:
        return json.load(fh)[suite_name]


def chunk_manifests(suite, platform, chunks, manifests):
    """Run the chunking algorithm.

    Args:
        platform (str): Platform used to find runtime info.
        chunks (int): Number of chunks to split manifests into.
        manifests(list): Manifests to chunk.

    Returns:
        A list of length `chunks` where each item contains a list of manifests
        that run in that chunk.
    """
    manifests = set(manifests)

    if "web-platform-tests" not in suite:
        runtimes = {
            k: v for k, v in get_runtimes(platform, suite).items() if k in manifests
        }
        return [
            c[1]
            for c in chunk_by_runtime(None, chunks, runtimes).get_chunked_manifests(
                manifests
            )
        ]

    # Keep track of test paths for each chunk, and the runtime information.
    chunked_manifests = [[] for _ in range(chunks)]

    # Spread out the test manifests evenly across all chunks.
    for index, key in enumerate(sorted(manifests)):
        chunked_manifests[index % chunks].append(key)

    # One last sort by the number of manifests. Chunk size should be more or less
    # equal in size.
    chunked_manifests.sort(key=lambda x: len(x))

    # Return just the chunked test paths.
    return chunked_manifests


@six.add_metaclass(ABCMeta)
class BaseManifestLoader(object):
    def __init__(self, params):
        self.params = params

    @abstractmethod
    def get_manifests(self, flavor, subsuite, mozinfo):
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
        pass


class DefaultLoader(BaseManifestLoader):
    """Load manifests using metadata from the TestResolver."""

    @memoize
    def get_tests(self, suite):
        suite_definition = TEST_SUITES[suite]
        return list(
            resolver.resolve_tests(
                flavor=suite_definition["build_flavor"],
                subsuite=suite_definition.get("kwargs", {}).get(
                    "subsuite", "undefined"
                ),
            )
        )

    @memoize
    def get_manifests(self, suite, mozinfo):
        mozinfo = dict(mozinfo)
        # Compute all tests for the given suite/subsuite.
        tests = self.get_tests(suite)

        if "web-platform-tests" in suite:
            manifests = set()
            for t in tests:
                manifests.add(t["manifest"])
            return {"active": list(manifests), "skipped": []}

        manifests = set(chunk_by_runtime.get_manifest(t) for t in tests)

        # Compute  the active tests.
        m = TestManifest()
        m.tests = tests
        tests = m.active_tests(disabled=False, exists=False, **mozinfo)
        active = set(chunk_by_runtime.get_manifest(t) for t in tests)
        skipped = manifests - active
        return {"active": list(active), "skipped": list(skipped)}


class BugbugLoader(DefaultLoader):
    """Load manifests using metadata from the TestResolver, and then
    filter them based on a query to bugbug."""

    CONFIDENCE_THRESHOLD = CT_LOW

    def __init__(self, *args, **kwargs):
        super(BugbugLoader, self).__init__(*args, **kwargs)
        self.timedout = False

    @memoize
    def get_manifests(self, suite, mozinfo):
        manifests = super(BugbugLoader, self).get_manifests(suite, mozinfo)

        # Don't prune any manifests if we're on a backstop push or there was a timeout.
        if self.params["backstop"] or self.timedout:
            return manifests

        try:
            data = push_schedules(self.params["project"], self.params["head_rev"])
        except BugbugTimeoutException:
            logger.warning("Timed out waiting for bugbug, loading all test manifests.")
            self.timedout = True
            return self.get_manifests(suite, mozinfo)

        bugbug_manifests = {
            m
            for m, c in data.get("groups", {}).items()
            if c >= self.CONFIDENCE_THRESHOLD
        }

        manifests["active"] = list(set(manifests["active"]) & bugbug_manifests)
        manifests["skipped"] = list(set(manifests["skipped"]) & bugbug_manifests)
        return manifests


manifest_loaders = {
    "bugbug": BugbugLoader,
    "default": DefaultLoader,
}

_loader_cache = {}


def get_manifest_loader(name, params):
    # Ensure we never create more than one instance of the same loader type for
    # performance reasons.
    if name in _loader_cache:
        return _loader_cache[name]

    loader = manifest_loaders[name](dict(params))
    _loader_cache[name] = loader
    return loader
