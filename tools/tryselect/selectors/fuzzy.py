# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
import platform
import re
import subprocess
import sys
from distutils.spawn import find_executable

from mozboot.util import get_state_dir
from mozterm import Terminal

from ..cli import BaseTryParser
from ..tasks import generate_tasks, filter_tasks_by_paths
from ..push import check_working_directory, push_to_try, generate_try_task_config

terminal = Terminal()

here = os.path.abspath(os.path.dirname(__file__))

# Some tasks show up in the target task set, but are either special cases
# or uncommon enough that they should only be selectable with --full.
TARGET_TASK_FILTERS = (
    '.*-ccov\/.*',
    'windows10-aarch64/opt.*',
    'android-hw.*'
)


FZF_NOT_FOUND = """
Could not find the `fzf` binary.

The `mach try fuzzy` command depends on fzf. Please install it following the
appropriate instructions for your platform:

    https://github.com/junegunn/fzf#installation

Only the binary is required, if you do not wish to install the shell and
editor integrations, download the appropriate binary and put it on your $PATH:

    https://github.com/junegunn/fzf-bin/releases
""".lstrip()

FZF_INSTALL_FAILED = """
Failed to install fzf.

Please install fzf manually following the appropriate instructions for your
platform:

    https://github.com/junegunn/fzf#installation

Only the binary is required, if you do not wish to install the shell and
editor integrations, download the appropriate binary and put it on your $PATH:

    https://github.com/junegunn/fzf-bin/releases
""".lstrip()

FZF_HEADER = """
For more shortcuts, see {t.italic_white}mach help try fuzzy{t.normal} and {t.italic_white}man fzf
{shortcuts}
""".strip()

fzf_shortcuts = {
    'ctrl-a': 'select-all',
    'ctrl-d': 'deselect-all',
    'ctrl-t': 'toggle-all',
    'alt-bspace': 'beginning-of-line+kill-line',
    '?': 'toggle-preview',
}

fzf_header_shortcuts = {
    'cursor-up': 'ctrl-k',
    'cursor-down': 'ctrl-j',
    'toggle-select': 'tab',
    'select-all': 'ctrl-a',
    'accept': 'enter',
    'cancel': 'ctrl-c',
}


class FuzzyParser(BaseTryParser):
    name = 'fuzzy'
    arguments = [
        [['-q', '--query'],
         {'metavar': 'STR',
          'action': 'append',
          'default': [],
          'help': "Use the given query instead of entering the selection "
                  "interface. Equivalent to typing <query><ctrl-a><enter> "
                  "from the interface. Specifying multiple times schedules "
                  "the union of computed tasks.",
          }],
        [['-i', '--interactive'],
         {'action': 'store_true',
          'default': False,
          'help': "Force running fzf interactively even when using presets or "
                  "queries with -q/--query."
          }],
        [['-x', '--and'],
         {'dest': 'intersection',
          'action': 'store_true',
          'default': False,
          'help': "When specifying queries on the command line with -q/--query, "
                  "use the intersection of tasks rather than the union. This is "
                  "especially useful for post filtering presets.",
          }],
        [['-e', '--exact'],
         {'action': 'store_true',
          'default': False,
          'help': "Enable exact match mode. Terms will use an exact match "
                  "by default, and terms prefixed with ' will become fuzzy."
          }],
        [['-u', '--update'],
         {'action': 'store_true',
          'default': False,
          'help': "Update fzf before running.",
          }],
    ]
    common_groups = ['push', 'task', 'preset']
    templates = [
        'artifact',
        'browsertime',
        'chemspill-prio',
        'debian-buster',
        'disable-pgo',
        'env',
        'gecko-profile',
        'path',
        'rebuild',
        'visual-metrics-jobs',
    ]


def run_cmd(cmd, cwd=None):
    is_win = platform.system() == 'Windows'
    return subprocess.call(cmd, cwd=cwd, shell=True if is_win else False)


def run_fzf_install_script(fzf_path):
    if platform.system() == 'Windows':
        cmd = ['bash', '-c', './install --bin']
    else:
        cmd = ['./install', '--bin']

    if run_cmd(cmd, cwd=fzf_path):
        print(FZF_INSTALL_FAILED)
        sys.exit(1)


def fzf_bootstrap(update=False):
    """Bootstrap fzf if necessary and return path to the executable.

    The bootstrap works by cloning the fzf repository and running the included
    `install` script. If update is True, we will pull the repository and re-run
    the install script.
    """
    fzf_bin = find_executable('fzf')
    if fzf_bin and not update:
        return fzf_bin

    fzf_path = os.path.join(get_state_dir(), 'fzf')
    if update and not os.path.isdir(fzf_path):
        print("fzf installed somewhere other than {}, please update manually".format(fzf_path))
        sys.exit(1)

    def get_fzf():
        return find_executable('fzf', os.path.join(fzf_path, 'bin'))

    if update:
        ret = run_cmd(['git', 'pull'], cwd=fzf_path)
        if ret:
            print("Update fzf failed.")
            sys.exit(1)

        run_fzf_install_script(fzf_path)
        return get_fzf()

    if os.path.isdir(fzf_path):
        fzf_bin = get_fzf()
        if fzf_bin:
            return fzf_bin
        # Fzf is cloned, but binary doesn't exist. Try running the install script
        return fzf_bootstrap(update=True)

    install = raw_input("Could not detect fzf, install it now? [y/n]: ")
    if install.lower() != 'y':
        return

    if not find_executable('git'):
        print("Git not found.")
        print(FZF_INSTALL_FAILED)
        sys.exit(1)

    cmd = ['git', 'clone', '--depth', '1', 'https://github.com/junegunn/fzf.git']
    if subprocess.call(cmd, cwd=os.path.dirname(fzf_path)):
        print(FZF_INSTALL_FAILED)
        sys.exit(1)

    run_fzf_install_script(fzf_path)

    print("Installed fzf to {}".format(fzf_path))
    return get_fzf()


def format_header():
    shortcuts = []
    for action, key in sorted(fzf_header_shortcuts.iteritems()):
        shortcuts.append('{t.white}{action}{t.normal}: {t.yellow}<{key}>{t.normal}'.format(
                         t=terminal, action=action, key=key))
    return FZF_HEADER.format(shortcuts=', '.join(shortcuts), t=terminal)


def run_fzf(cmd, tasks):
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stdin=subprocess.PIPE)
    out = proc.communicate('\n'.join(tasks))[0].splitlines()

    selected = []
    query = None
    if out:
        query = out[0]
        selected = out[1:]
    return query, selected


def filter_target_task(task):
    return not any(re.search(pattern, task) for pattern in TARGET_TASK_FILTERS)


def run(update=False, query=None, intersect_query=None, try_config=None, full=False,
        parameters=None, save_query=False, push=True, message='{msg}',
        test_paths=None, exact=False, closed_tree=False):
    fzf = fzf_bootstrap(update)

    if not fzf:
        print(FZF_NOT_FOUND)
        return 1

    check_working_directory(push)
    tg = generate_tasks(parameters, full)
    all_tasks = sorted(tg.tasks.keys())

    if not full:
        all_tasks = filter(filter_target_task, all_tasks)

    if test_paths:
        all_tasks = filter_tasks_by_paths(all_tasks, test_paths)
        if not all_tasks:
            return 1

    key_shortcuts = [k + ':' + v for k, v in fzf_shortcuts.iteritems()]
    base_cmd = [
        fzf, '-m',
        '--bind', ','.join(key_shortcuts),
        '--header', format_header(),
        # Using python to split the preview string is a bit convoluted,
        # but is guaranteed to be available on all platforms.
        '--preview', 'python -c "print(\\"\\n\\".join(sorted([s.strip(\\"\'\\") for s in \\"{+}\\".split()])))"',  # noqa
        '--preview-window=right:30%',
        '--print-query',
    ]

    if exact:
        base_cmd.append('--exact')

    selected = set()
    queries = []

    def get_tasks(query_arg=None, candidate_tasks=all_tasks):
        cmd = base_cmd[:]
        if query_arg and query_arg != 'INTERACTIVE':
            cmd.extend(['-f', query_arg])

        query_str, tasks = run_fzf(cmd, sorted(candidate_tasks))
        queries.append(query_str)
        return set(tasks)

    for q in query or []:
        selected |= get_tasks(q)

    for q in intersect_query or []:
        if not selected:
            tasks = get_tasks(q)
            selected |= tasks
        else:
            tasks = get_tasks(q, selected)
            selected &= tasks

    if not queries:
        selected = get_tasks()

    if not selected:
        print("no tasks selected")
        return

    if save_query:
        return queries

    # build commit message
    msg = "Fuzzy"
    args = ["query={}".format(q) for q in queries]
    if test_paths:
        args.append("paths={}".format(':'.join(test_paths)))
    if args:
        msg = "{} {}".format(msg, '&'.join(args))
    return push_to_try('fuzzy', message.format(msg=msg),
                       try_task_config=generate_try_task_config('fuzzy', selected, try_config),
                       push=push, closed_tree=closed_tree)
