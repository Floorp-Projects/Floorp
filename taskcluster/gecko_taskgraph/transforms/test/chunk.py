# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import copy

import gecko_taskgraph
from gecko_taskgraph.transforms.base import TransformSequence
from gecko_taskgraph.util.attributes import keymatch
from gecko_taskgraph.util.chunking import (
    chunk_manifests,
    get_manifest_loader,
    get_runtimes,
    guess_mozinfo_from_task,
    DefaultLoader,
)
from gecko_taskgraph.util.perfile import perfile_number_of_chunks
from gecko_taskgraph.util.treeherder import split_symbol, join_symbol


DYNAMIC_CHUNK_DURATION = 20 * 60  # seconds
"""The approximate time each test chunk should take to run."""


DYNAMIC_CHUNK_MULTIPLIER = {
    # Desktop xpcshell tests run in parallel. Reduce the total runtime to
    # compensate.
    "^(?!android).*-xpcshell.*": 0.2,
}
"""A multiplication factor to tweak the total duration per platform / suite."""


transforms = TransformSequence()


@transforms.add
def set_test_verify_chunks(config, tasks):
    """Set the number of chunks we use for test-verify."""
    for task in tasks:
        if any(task["suite"].startswith(s) for s in ("test-verify", "test-coverage")):
            env = config.params.get("try_task_config", {}) or {}
            env = env.get("templates", {}).get("env", {})
            task["chunks"] = perfile_number_of_chunks(
                config.params.is_try(),
                env.get("MOZHARNESS_TEST_PATHS", ""),
                config.params.get("head_repository", ""),
                config.params.get("head_rev", ""),
                task["test-name"],
            )

            # limit the number of chunks we run for test-verify mode because
            # test-verify is comprehensive and takes a lot of time, if we have
            # >30 tests changed, this is probably an import of external tests,
            # or a patch renaming/moving files in bulk
            maximum_number_verify_chunks = 3
            if task["chunks"] > maximum_number_verify_chunks:
                task["chunks"] = maximum_number_verify_chunks

        yield task


@transforms.add
def set_test_manifests(config, tasks):
    """Determine the set of test manifests that should run in this task."""

    for task in tasks:
        # When a task explicitly requests no 'test_manifest_loader', test
        # resolving will happen at test runtime rather than in the taskgraph.
        if "test-manifest-loader" in task and task["test-manifest-loader"] is None:
            yield task
            continue

        # Set 'tests_grouped' to "1", so we can differentiate between suites that are
        # chunked at the test runtime and those that are chunked in the taskgraph.
        task.setdefault("tags", {})["tests_grouped"] = "1"

        if gecko_taskgraph.fast:
            # We want to avoid evaluating manifests when gecko_taskgraph.fast is set. But
            # manifests are required for dynamic chunking. Just set the number of
            # chunks to one in this case.
            if task["chunks"] == "dynamic":
                task["chunks"] = 1
            yield task
            continue

        manifests = task.get("test-manifests")
        if manifests:
            if isinstance(manifests, list):
                task["test-manifests"] = {"active": manifests, "skipped": []}
            yield task
            continue

        mozinfo = guess_mozinfo_from_task(task)

        loader_name = task.pop(
            "test-manifest-loader", config.params["test_manifest_loader"]
        )
        loader = get_manifest_loader(loader_name, config.params)

        task["test-manifests"] = loader.get_manifests(
            task["suite"],
            frozenset(mozinfo.items()),
        )

        # The default loader loads all manifests. If we use a non-default
        # loader, we'll only run some subset of manifests and the hardcoded
        # chunk numbers will no longer be valid. Dynamic chunking should yield
        # better results.
        if not isinstance(loader, DefaultLoader):
            task["chunks"] = "dynamic"

        yield task


@transforms.add
def resolve_dynamic_chunks(config, tasks):
    """Determine how many chunks are needed to handle the given set of manifests."""

    for task in tasks:
        if task["chunks"] != "dynamic":
            yield task
            continue

        if not task.get("test-manifests"):
            raise Exception(
                "{} must define 'test-manifests' to use dynamic chunking!".format(
                    task["test-name"]
                )
            )

        runtimes = {
            m: r
            for m, r in get_runtimes(task["test-platform"], task["suite"]).items()
            if m in task["test-manifests"]["active"]
        }

        # Truncate runtimes that are above the desired chunk duration. They
        # will be assigned to a chunk on their own and the excess duration
        # shouldn't cause additional chunks to be needed.
        times = [min(DYNAMIC_CHUNK_DURATION, r) for r in runtimes.values()]
        avg = round(sum(times) / len(times), 2) if times else 0
        total = sum(times)

        # If there are manifests missing from the runtimes data, fill them in
        # with the average of all present manifests.
        missing = [m for m in task["test-manifests"]["active"] if m not in runtimes]
        total += avg * len(missing)

        # Apply any chunk multipliers if found.
        key = "{}-{}".format(task["test-platform"], task["test-name"])
        matches = keymatch(DYNAMIC_CHUNK_MULTIPLIER, key)
        if len(matches) > 1:
            raise Exception(
                "Multiple matching values for {} found while "
                "determining dynamic chunk multiplier!".format(key)
            )
        elif matches:
            total = total * matches[0]

        chunks = int(round(total / DYNAMIC_CHUNK_DURATION))

        # Make sure we never exceed the number of manifests, nor have a chunk
        # length of 0.
        task["chunks"] = min(chunks, len(task["test-manifests"]["active"])) or 1
        yield task


@transforms.add
def split_chunks(config, tasks):
    """Based on the 'chunks' key, split tests up into chunks by duplicating
    them and assigning 'this-chunk' appropriately and updating the treeherder
    symbol.
    """

    for task in tasks:
        # If test-manifests are set, chunk them ahead of time to avoid running
        # the algorithm more than once.
        chunked_manifests = None
        if "test-manifests" in task:
            manifests = task["test-manifests"]
            chunked_manifests = chunk_manifests(
                task["suite"],
                task["test-platform"],
                task["chunks"],
                manifests["active"],
            )

            # Add all skipped manifests to the first chunk of backstop pushes
            # so they still show up in the logs. They won't impact runtime much
            # and this way tools like ActiveData are still aware that they
            # exist.
            if config.params["backstop"] and manifests["active"]:
                chunked_manifests[0].extend(manifests["skipped"])

        for i in range(task["chunks"]):
            this_chunk = i + 1

            # copy the test and update with the chunk number
            chunked = copy.deepcopy(task)
            chunked["this-chunk"] = this_chunk

            if chunked_manifests is not None:
                chunked["test-manifests"] = sorted(chunked_manifests[i])

            group, symbol = split_symbol(chunked["treeherder-symbol"])
            if task["chunks"] > 1 or not symbol:
                # add the chunk number to the TH symbol
                symbol += str(this_chunk)
                chunked["treeherder-symbol"] = join_symbol(group, symbol)

            yield chunked
