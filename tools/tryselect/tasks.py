# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import json
import os
import re
import sys
from collections import defaultdict

import mozpack.path as mozpath
import taskgraph
from mach.util import get_state_dir
from mozbuild.base import MozbuildObject
from mozpack.files import FileFinder
from moztest.resolve import TestManifestLoader, TestResolver, get_suite_definition
from taskgraph.generator import TaskGraphGenerator
from taskgraph.parameters import ParameterMismatch, parameters_loader
from taskgraph.taskgraph import TaskGraph

here = os.path.abspath(os.path.dirname(__file__))
build = MozbuildObject.from_environment(cwd=here)

PARAMETER_MISMATCH = """
ERROR - The parameters being used to generate tasks differ from those expected
by your working copy:

    {}

To fix this, either rebase onto the latest mozilla-central or pass in
-p/--parameters. For more information on how to define parameters, see:
https://firefox-source-docs.mozilla.org/taskcluster/taskcluster/mach.html#parameters
"""


def invalidate(cache):
    try:
        cmod = os.path.getmtime(cache)
    except OSError as e:
        # File does not exist. We catch OSError rather than use `isfile`
        # because the recommended watchman hook could possibly invalidate the
        # cache in-between the check to `isfile` and the call to `getmtime`
        # below.
        if e.errno == 2:
            return
        raise

    tc_dir = os.path.join(build.topsrcdir, "taskcluster")
    tmod = max(os.path.getmtime(os.path.join(tc_dir, p)) for p, _ in FileFinder(tc_dir))

    if tmod > cmod:
        os.remove(cache)


def cache_key(attr, params, disable_target_task_filter):
    key = attr
    if params and params["project"] not in ("autoland", "mozilla-central"):
        key += f"-{params['project']}"

    if disable_target_task_filter and "full" not in attr:
        key += "-uncommon"
    return key


def generate_tasks(params=None, full=False, disable_target_task_filter=False):
    attr = "full_task_set" if full else "target_task_set"
    target_tasks_method = (
        "try_select_tasks"
        if not disable_target_task_filter
        else "try_select_tasks_uncommon"
    )
    params = parameters_loader(
        params,
        strict=False,
        overrides={
            "try_mode": "try_select",
            "target_tasks_method": target_tasks_method,
        },
    )
    root = os.path.join(build.topsrcdir, "taskcluster")
    taskgraph.fast = True
    generator = TaskGraphGenerator(root_dir=root, parameters=params)

    def add_chunk_patterns(tg):
        for task_name, task in tg.tasks.items():
            chunk_index = -1
            if task_name.endswith("-cf"):
                chunk_index = -2

            chunks = task.task.get("extra", {}).get("chunks", {})
            if isinstance(chunks, int):
                task.chunk_pattern = "{}-*/{}".format(
                    "-".join(task_name.split("-")[:chunk_index]), chunks
                )
            else:
                assert isinstance(chunks, dict)
                if chunks.get("total", 1) == 1:
                    task.chunk_pattern = task_name
                else:
                    task.chunk_pattern = "{}-*".format(
                        "-".join(task_name.split("-")[:chunk_index])
                    )
        return tg

    cache_dir = os.path.join(
        get_state_dir(specific_to_topsrcdir=True), "cache", "taskgraph"
    )
    key = cache_key(attr, generator.parameters, disable_target_task_filter)
    cache = os.path.join(cache_dir, key)

    invalidate(cache)
    if os.path.isfile(cache):
        with open(cache) as fh:
            return add_chunk_patterns(TaskGraph.from_json(json.load(fh))[1])

    if not os.path.isdir(cache_dir):
        os.makedirs(cache_dir)

    print("Task configuration changed, generating {}".format(attr.replace("_", " ")))

    cwd = os.getcwd()
    os.chdir(build.topsrcdir)

    def generate(attr):
        try:
            tg = getattr(generator, attr)
        except ParameterMismatch as e:
            print(PARAMETER_MISMATCH.format(e.args[0]))
            sys.exit(1)

        # write cache
        key = cache_key(attr, generator.parameters, disable_target_task_filter)
        with open(os.path.join(cache_dir, key), "w") as fh:
            json.dump(tg.to_json(), fh)
        return add_chunk_patterns(tg)

    # Cache both full_task_set and target_task_set regardless of whether or not
    # --full was requested. Caching is cheap and can potentially save a lot of
    # time.
    tg_full = generate("full_task_set")
    tg_target = generate("target_task_set")

    # discard results from these, we only need cache.
    if full:
        generate("full_task_graph")
    generate("target_task_graph")

    os.chdir(cwd)
    if full:
        return tg_full
    return tg_target


def filter_tasks_by_worker_type(tasks, params):
    worker_types = params.get("try_task_config", {}).get("worker-types", [])
    if worker_types:
        retVal = {}
        for t in tasks:
            if tasks[t].task["workerType"] in worker_types:
                retVal[t] = tasks[t]
        return retVal
    return tasks


def filter_tasks_by_paths(tasks, paths=[], tag=""):
    resolver = TestResolver.from_environment(cwd=here, loader_cls=TestManifestLoader)

    if paths:
        run_suites, run_tests = resolver.resolve_metadata(paths)
    elif not paths and tag:
        run_tests = list(resolver.resolve_tests(paths=[], tags=tag))

    if not run_tests:
        return {}

    flavors = {(t["flavor"], t.get("subsuite")) for t in run_tests}

    task_regexes = set()
    for flavor, subsuite in flavors:
        _, suite = get_suite_definition(flavor, subsuite, strict=True)
        if "task_regex" not in suite:
            print(
                "warning: no tasks could be resolved from flavor '{}'{}".format(
                    flavor, " and subsuite '{}'".format(subsuite) if subsuite else ""
                )
            )
            continue

        task_regexes.update(suite["task_regex"])

    def match_task(task):
        return any(re.search(pattern, task) for pattern in task_regexes)

    return {
        task_name: task for task_name, task in tasks.items() if match_task(task_name)
    }


def resolve_tests_by_suite(paths):
    resolver = TestResolver.from_environment(cwd=here, loader_cls=TestManifestLoader)
    _, run_tests = resolver.resolve_metadata(paths)

    suite_to_tests = defaultdict(list)

    # A dictionary containing all the input paths that we haven't yet
    # assigned to a specific test flavor.
    remaining_paths_by_suite = defaultdict(lambda: set(paths))

    for test in run_tests:
        key, _ = get_suite_definition(test["flavor"], test.get("subsuite"), strict=True)

        test_path = test.get("srcdir_relpath")
        if test_path is None:
            continue
        found_path = None
        manifest_relpath = None
        if "manifest_relpath" in test:
            manifest_relpath = mozpath.normpath(test["manifest_relpath"])
        for path in remaining_paths_by_suite[key]:
            if test_path.startswith(path) or manifest_relpath == path:
                found_path = path
                break
        if found_path:
            suite_to_tests[key].append(found_path)
            remaining_paths_by_suite[key].remove(found_path)

    return suite_to_tests
