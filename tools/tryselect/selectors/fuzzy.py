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
from moztest.resolve import TestResolver, TEST_FLAVORS

from .. import preset as pset
from ..cli import BaseTryParser
from ..tasks import generate_tasks
from ..vcs import VCSHelper

terminal = Terminal()

here = os.path.abspath(os.path.dirname(__file__))

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
          'help': "Use the given query instead of entering the selection "
                  "interface. Equivalent to typing <query><ctrl-a><enter> "
                  "from the interface.",
          }],
        [['-u', '--update'],
         {'action': 'store_true',
          'default': False,
          'help': "Update fzf before running.",
          }],
    ]
    common_groups = ['push', 'task', 'preset']
    templates = ['artifact', 'path', 'env', 'rebuild']


def run(cmd, cwd=None):
    is_win = platform.system() == 'Windows'
    return subprocess.call(cmd, cwd=cwd, shell=True if is_win else False)


def run_fzf_install_script(fzf_path):
    if platform.system() == 'Windows':
        cmd = ['bash', '-c', './install --bin']
    else:
        cmd = ['./install', '--bin']

    if run(cmd, cwd=fzf_path):
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

    fzf_path = os.path.join(get_state_dir()[0], 'fzf')
    if update and not os.path.isdir(fzf_path):
        print("fzf installed somewhere other than {}, please update manually".format(fzf_path))
        sys.exit(1)

    def get_fzf():
        return find_executable('fzf', os.path.join(fzf_path, 'bin'))

    if update:
        ret = run(['git', 'pull'], cwd=fzf_path)
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


def filter_by_paths(tasks, paths):
    resolver = TestResolver.from_environment(cwd=here)
    run_suites, run_tests = resolver.resolve_metadata(paths)
    flavors = set([t['flavor'] for t in run_tests])
    task_regexes = [TEST_FLAVORS[f]['task_regex']
                    for f in flavors if 'task_regex' in TEST_FLAVORS[f]]

    def match_task(task):
        return any(re.search(pattern, task) for pattern in task_regexes)

    return filter(match_task, tasks)


def run_fuzzy_try(update=False, query=None, templates=None, full=False, parameters=None,
                  save=False, preset=None, mod_presets=False, push=True, message='{msg}',
                  paths=None, **kwargs):
    if mod_presets:
        return getattr(pset, mod_presets)(section='fuzzy')

    fzf = fzf_bootstrap(update)

    if not fzf:
        print(FZF_NOT_FOUND)
        return

    vcs = VCSHelper.create()
    vcs.check_working_directory(push)

    all_tasks = generate_tasks(parameters, full, root=vcs.root)

    if paths:
        all_tasks = filter_by_paths(all_tasks, paths)

    key_shortcuts = [k + ':' + v for k, v in fzf_shortcuts.iteritems()]
    cmd = [
        fzf, '-m',
        '--bind', ','.join(key_shortcuts),
        '--header', format_header(),
        # Using python to split the preview string is a bit convoluted,
        # but is guaranteed to be available on all platforms.
        '--preview', 'python -c "print(\\"\\n\\".join(sorted([s.strip(\\"\'\\") for s in \\"{+}\\".split()])))"',  # noqa
        '--preview-window=right:20%',
        '--print-query',
    ]

    if query:
        cmd.extend(['-f', query])
    elif preset:
        value = pset.load(preset, section='fuzzy')[0]
        cmd.extend(['-f', value])

    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stdin=subprocess.PIPE)
    out = proc.communicate('\n'.join(all_tasks))[0].splitlines()

    selected = []
    if out:
        query = out[0]
        selected = out[1:]

    if not selected:
        print("no tasks selected")
        return

    if save:
        pset.save('fuzzy', save, query)

    # build commit message
    msg = "Fuzzy"
    args = []
    if paths:
        args.append("paths={}".format(':'.join(paths)))
    if query:
        args.append("query={}".format(query))
    if args:
        msg = "{} {}".format(msg, '&'.join(args))
    return vcs.push_to_try('fuzzy', message.format(msg=msg), selected, templates, push=push,
                           closed_tree=kwargs["closed_tree"])
