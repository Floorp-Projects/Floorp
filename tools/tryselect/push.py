# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

import json
import os
import sys

from mozboot.util import get_state_dir
from mozbuild.base import MozbuildObject
from mozversioncontrol import get_repository_object, MissingVCSExtension

GIT_CINNABAR_NOT_FOUND = """
Could not detect `git-cinnabar`.

The `mach try` command requires git-cinnabar to be installed when
pushing from git. Please install it by running:

    $ ./mach vcs-setup
""".lstrip()

HG_PUSH_TO_TRY_NOT_FOUND = """
Could not detect `push-to-try`.

The `mach try` command requires the push-to-try extension enabled
when pushing from hg. Please install it by running:

    $ ./mach vcs-setup
""".lstrip()

VCS_NOT_FOUND = """
Could not detect version control. Only `hg` or `git` are supported.
""".strip()

UNCOMMITTED_CHANGES = """
ERROR please commit changes before continuing
""".strip()

MAX_HISTORY = 10

here = os.path.abspath(os.path.dirname(__file__))
build = MozbuildObject.from_environment(cwd=here)
vcs = get_repository_object(build.topsrcdir)

history_path = os.path.join(get_state_dir(srcdir=True), 'history', 'try_task_configs.json')


def write_task_config(try_task_config):
    config_path = os.path.join(vcs.path, 'try_task_config.json')
    with open(config_path, 'w') as fh:
        json.dump(try_task_config, fh, indent=4, separators=(',', ': '), sort_keys=True)
        fh.write('\n')
    return config_path


def write_task_config_history(msg, try_task_config):
    if not os.path.isfile(history_path):
        if not os.path.isdir(os.path.dirname(history_path)):
            os.makedirs(os.path.dirname(history_path))
        history = []
    else:
        with open(history_path, 'r') as fh:
            history = fh.read().strip().splitlines()

    history.insert(0, json.dumps([msg, try_task_config]))
    history = history[:MAX_HISTORY]
    with open(history_path, 'w') as fh:
        fh.write('\n'.join(history))


def check_working_directory(push=True):
    if not push:
        return

    if not vcs.working_directory_clean():
        print(UNCOMMITTED_CHANGES)
        sys.exit(1)


def push_to_try(method, msg, labels=None, templates=None, try_task_config=None,
                push=True, closed_tree=False, files_to_change=None):
    check_working_directory(push)

    # Format the commit message
    closed_tree_string = " ON A CLOSED TREE" if closed_tree else ""
    commit_message = ('%s%s\n\nPushed via `mach try %s`' %
                      (msg, closed_tree_string, method))

    if templates is not None:
        templates.setdefault('env', {}).update({'TRY_SELECTOR': method})

    if labels or labels == []:
        try_task_config = {
            'version': 1,
            'tasks': sorted(labels),
        }
        if templates:
            try_task_config['templates'] = templates
        if push:
            write_task_config_history(msg, try_task_config)

    config_path = None
    changed_files = []
    if try_task_config:
        config_path = write_task_config(try_task_config)
        changed_files.append(config_path)

    if files_to_change:
        for path, content in files_to_change.items():
            path = os.path.join(vcs.path, path)
            with open(path, 'w') as fh:
                fh.write(content)
            changed_files.append(path)

    try:
        if not push:
            print("Commit message:")
            print(commit_message)
            if config_path:
                print("Calculated try_task_config.json:")
                with open(config_path) as fh:
                    print(fh.read())
            return

        for path in changed_files:
            vcs.add_remove_files(path)

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
        if config_path and os.path.isfile(config_path):
            os.remove(config_path)
