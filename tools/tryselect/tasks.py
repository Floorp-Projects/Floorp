# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os

from mozboot.util import get_state_dir
from mozbuild.base import MozbuildObject
from mozpack.files import FileFinder

from taskgraph.generator import TaskGraphGenerator
from taskgraph.parameters import load_parameters_file

here = os.path.abspath(os.path.dirname(__file__))
build = MozbuildObject.from_environment(cwd=here)


def invalidate(cache):
    if not os.path.isfile(cache):
        return

    tc_dir = os.path.join(build.topsrcdir, 'taskcluster')
    tmod = max(os.path.getmtime(os.path.join(tc_dir, p)) for p, _ in FileFinder(tc_dir))
    cmod = os.path.getmtime(cache)

    if tmod > cmod:
        os.remove(cache)


def generate_target(params='project=mozilla-central'):
    cache_dir = os.path.join(get_state_dir()[0], 'cache', 'taskgraph')
    cache = os.path.join(cache_dir, 'target_task_set')

    invalidate(cache)
    if os.path.isfile(cache):
        with open(cache, 'r') as fh:
            return fh.read().splitlines()

    if not os.path.isdir(cache_dir):
        os.makedirs(cache_dir)

    print("Task configuration changed, generating target tasks")
    params = load_parameters_file(params)
    params.check()

    root = os.path.join(build.topsrcdir, 'taskcluster', 'ci')
    tg = TaskGraphGenerator(root_dir=root, parameters=params).target_task_set
    labels = [label for label in tg.graph.visit_postorder()]

    with open(cache, 'w') as fh:
        fh.write('\n'.join(labels))
    return labels
