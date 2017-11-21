# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
import sys

from mozboot.util import get_state_dir
from mozbuild.base import MozbuildObject
from mozpack.files import FileFinder

from taskgraph.generator import TaskGraphGenerator
from taskgraph.parameters import (
    ParameterMismatch,
    load_parameters_file,
)

here = os.path.abspath(os.path.dirname(__file__))
build = MozbuildObject.from_environment(cwd=here)


PARAMETER_MISMATCH = """
ERROR - The parameters being used to generate tasks differ from those defined
in your working copy:

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
    params = params or "project=mozilla-central"

    cache_dir = os.path.join(get_state_dir()[0], 'cache', 'taskgraph')
    attr = 'full_task_set' if full else 'target_task_set'
    cache = os.path.join(cache_dir, attr)

    invalidate(cache, root)
    if os.path.isfile(cache):
        with open(cache, 'r') as fh:
            return fh.read().splitlines()

    if not os.path.isdir(cache_dir):
        os.makedirs(cache_dir)

    print("Task configuration changed, generating {}".format(attr.replace('_', ' ')))
    try:
        params = load_parameters_file(params, strict=False)
        params.check()
    except ParameterMismatch as e:
        print(PARAMETER_MISMATCH.format(e.args[0]))
        sys.exit(1)

    cwd = os.getcwd()
    os.chdir(build.topsrcdir)

    root = os.path.join(root, 'taskcluster', 'ci')
    tg = getattr(TaskGraphGenerator(root_dir=root, parameters=params), attr)
    labels = [label for label in tg.graph.visit_postorder()]

    os.chdir(cwd)

    with open(cache, 'w') as fh:
        fh.write('\n'.join(labels))
    return labels
