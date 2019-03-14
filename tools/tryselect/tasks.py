# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import glob
import hashlib
import json
import os
import re
import shutil
import sys
from collections import defaultdict

from mozboot.util import get_state_dir
from mozbuild.base import MozbuildObject
from mozpack.files import FileFinder
from moztest.resolve import TestResolver, get_suite_definition

import taskgraph
from taskgraph.generator import TaskGraphGenerator
from taskgraph.parameters import (
    ParameterMismatch,
    parameters_loader,
)
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


def invalidate(cache, root):
    if not os.path.isfile(cache):
        return

    tc_dir = os.path.join(root, 'taskcluster')
    tmod = max(os.path.getmtime(os.path.join(tc_dir, p)) for p, _ in FileFinder(tc_dir))
    cmod = os.path.getmtime(cache)

    if tmod > cmod:
        os.remove(cache)


def generate_tasks(params, full, root):
    # Try to delete the old taskgraph cache directory.
    old_cache_dir = os.path.join(get_state_dir(), 'cache', 'taskgraph')
    if os.path.isdir(old_cache_dir):
        shutil.rmtree(old_cache_dir)

    root_hash = hashlib.sha256(os.path.abspath(root)).hexdigest()
    cache_dir = os.path.join(get_state_dir(), 'cache', root_hash, 'taskgraph')

    # Cleanup old cache files
    for path in glob.glob(os.path.join(cache_dir, '*_set')):
        os.remove(path)

    attr = 'full_task_graph' if full else 'target_task_graph'
    cache = os.path.join(cache_dir, attr)

    invalidate(cache, root)
    if os.path.isfile(cache):
        with open(cache, 'r') as fh:
            return TaskGraph.from_json(json.load(fh))[1]

    if not os.path.isdir(cache_dir):
        os.makedirs(cache_dir)

    print("Task configuration changed, generating {}".format(attr.replace('_', ' ')))

    taskgraph.fast = True
    cwd = os.getcwd()
    os.chdir(build.topsrcdir)

    root = os.path.join(root, 'taskcluster', 'ci')
    params = parameters_loader(params, strict=False, overrides={'try_mode': 'try_select'})
    try:
        tg = getattr(TaskGraphGenerator(root_dir=root, parameters=params), attr)
    except ParameterMismatch as e:
        print(PARAMETER_MISMATCH.format(e.args[0]))
        sys.exit(1)

    os.chdir(cwd)

    with open(cache, 'w') as fh:
        json.dump(tg.to_json(), fh)
    return tg


def filter_tasks_by_paths(tasks, paths):
    resolver = TestResolver.from_environment(cwd=here)
    run_suites, run_tests = resolver.resolve_metadata(paths)
    flavors = set([(t['flavor'], t.get('subsuite')) for t in run_tests])

    task_regexes = set()
    for flavor, subsuite in flavors:
        suite = get_suite_definition(flavor, subsuite, strict=True)
        if 'task_regex' not in suite:
            print("warning: no tasks could be resolved from flavor '{}'{}".format(
                    flavor, " and subsuite '{}'".format(subsuite) if subsuite else ""))
            continue

        task_regexes.update(suite['task_regex'])

    def match_task(task):
        return any(re.search(pattern, task) for pattern in task_regexes)

    return filter(match_task, tasks)


def resolve_tests_by_suite(paths):
    resolver = TestResolver.from_environment(cwd=here)
    _, run_tests = resolver.resolve_metadata(paths)

    suite_to_tests = defaultdict(list)
    for test in run_tests:
        key = test['flavor']
        subsuite = test.get('subsuite')
        if subsuite:
            key += '-' + subsuite
        suite_to_tests[key].append(test['srcdir_relpath'])

    return suite_to_tests
