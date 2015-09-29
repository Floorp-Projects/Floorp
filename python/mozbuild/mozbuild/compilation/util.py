# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import shlex

def check_top_objdir(topobjdir):
    top_make = os.path.join(topobjdir, 'Makefile')
    if not os.path.exists(top_make):
        print('Your tree has not been built yet. Please run '
            '|mach build| with no arguments.')
        return False
    return True

def get_build_vars(directory, cmd):
    build_vars = {}

    def on_line(line):
        elements = [s.strip() for s in line.split('=', 1)]

        if len(elements) != 2:
            return

        build_vars[elements[0]] = elements[1]

    try:
        old_logger = cmd.log_manager.replace_terminal_handler(None)
        cmd._run_make(directory=directory, target='showbuild', log=False,
                print_directory=False, allow_parallel=False, silent=True,
                line_handler=on_line)
    finally:
        cmd.log_manager.replace_terminal_handler(old_logger)

    return build_vars

def get_flags(topobjdir, make_dir, build_vars, name):
    flags = ['-isystem', '-I', '-include', '-MF']
    new_args = []
    path = os.path.join(topobjdir, make_dir)

    # Take case to handle things such as the following correctly:
    #   * -DMOZ_APP_VERSION='"40.0a1"'
    #   * -DR_PLATFORM_INT_TYPES='<stdint.h>'
    #   * -DAPP_ID='{ec8030f7-c20a-464f-9b0e-13a3a9e97384}
    #   * -D__UNUSED__='__attribute__((unused))'
    lex = shlex.shlex(build_vars[name])
    lex.quotes = '"'
    lex.wordchars += '+/\'"-=.*{}()[]<>'

    for arg in list(lex):
        if new_args and new_args[-1] in flags:
            arg = os.path.normpath(os.path.join(path, arg))
        else:
            flag = [(f, arg[len(f):]) for f in flags + ['--sysroot=']
                    if arg.startswith(f)]
            if flag:
                flag, val = flag[0]
                if val:
                    arg = flag + os.path.normpath(os.path.join(path, val))
        new_args.append(arg)

    return ' '.join(new_args)
