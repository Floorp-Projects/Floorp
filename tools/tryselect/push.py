# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

import json
import os
import sys

from mozbuild.base import MozbuildObject
from mozversioncontrol import get_repository_object, MissingVCSExtension

GIT_CINNABAR_NOT_FOUND = """
Could not detect `git-cinnabar`.

The `mach try` command requires git-cinnabar to be installed when
pushing from git. For more information and installation instruction,
please see:

    https://github.com/glandium/git-cinnabar
""".lstrip()

HG_PUSH_TO_TRY_NOT_FOUND = """
Could not detect `push-to-try`.

The `mach try` command requires the push-to-try extension enabled
when pushing from hg. Please install it by running:

    $ ./mach mercurial-setup
""".lstrip()

VCS_NOT_FOUND = """
Could not detect version control. Only `hg` or `git` are supported.
""".strip()

UNCOMMITTED_CHANGES = """
ERROR please commit changes before continuing
""".strip()


here = os.path.abspath(os.path.dirname(__file__))
build = MozbuildObject.from_environment(cwd=here)
vcs = get_repository_object(build.topsrcdir)


def write_task_config(labels, templates=None):
    config = os.path.join(vcs.path, 'try_task_config.json')
    with open(config, 'w') as fh:
        try_task_config = {'tasks': sorted(labels)}
        if templates:
            try_task_config['templates'] = templates

        json.dump(try_task_config, fh, indent=2, separators=(',', ':'))
        fh.write('\n')
    return config


def check_working_directory(push=True):
    if not push:
        return

    if not vcs.working_directory_clean():
        print(UNCOMMITTED_CHANGES)
        sys.exit(1)


def push_to_try(method, msg, labels=None, templates=None, push=True, closed_tree=False):
    check_working_directory(push)

    # Format the commit message
    closed_tree_string = " ON A CLOSED TREE" if closed_tree else ""
    commit_message = ('%s%s\n\nPushed via `mach try %s`' %
                      (msg, closed_tree_string, method))

    config = None
    if labels or labels == []:
        config = write_task_config(labels, templates)
    try:
        if not push:
            print("Commit message:")
            print(commit_message)
            if config:
                print("Calculated try_task_config.json:")
                with open(config) as fh:
                    print(fh.read())
            return

        if config:
            vcs.add_remove_files(config)

        try:
            vcs.push_to_try(commit_message)
        except MissingVCSExtension as e:
            if e.ext == 'push-to-try':
                print(HG_PUSH_TO_TRY_NOT_FOUND)
            elif e.ext == 'cinnabar':
                print(GIT_CINNABAR_NOT_FOUND)
            else:
                raise
            sys.exit(1)
    finally:
        if config and os.path.isfile(config):
            os.remove(config)
