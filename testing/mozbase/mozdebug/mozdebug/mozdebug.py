#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import mozinfo
from collections import namedtuple
from distutils.spawn import find_executable

__all__ = ['get_debugger_info',
           'get_default_debugger_name',
           'DebuggerSearch',
           'get_default_valgrind_args']

'''
Map of debugging programs to information about them, like default arguments
and whether or not they are interactive.

To add support for a new debugger, simply add the relative entry in
_DEBUGGER_INFO and optionally update the _DEBUGGER_PRIORITIES.
'''
_DEBUGGER_INFO = {
    # gdb requires that you supply the '--args' flag in order to pass arguments
    # after the executable name to the executable.
    'gdb': {
        'interactive': True,
        'args': ['-q', '--args']
    },

    'cgdb': {
        'interactive': True,
        'args': ['-q', '--args']
    },

    'lldb': {
        'interactive': True,
        'args': ['--'],
        'requiresEscapedArgs': True
    },

    # Visual Studio Debugger Support.
    'devenv.exe': {
        'interactive': True,
        'args': ['-debugexe']
    },

    # Visual C++ Express Debugger Support.
    'wdexpress.exe': {
        'interactive': True,
        'args': ['-debugexe']
    }
}

# Maps each OS platform to the preferred debugger programs found in _DEBUGGER_INFO.
_DEBUGGER_PRIORITIES = {
      'win': ['devenv.exe', 'wdexpress.exe'],
      'linux': ['gdb', 'cgdb', 'lldb'],
      'mac': ['lldb', 'gdb'],
      'unknown': ['gdb']
}

def get_debugger_info(debugger, debuggerArgs = None, debuggerInteractive = False):
    '''
    Get the information about the requested debugger.

    Returns a dictionary containing the |path| of the debugger executable,
    if it will run in |interactive| mode, its arguments and whether it needs
    to escape arguments it passes to the debugged program (|requiresEscapedArgs|).
    If the debugger cannot be found in the system, returns |None|.

    :param debugger: The name of the debugger.
    :param debuggerArgs: If specified, it's the arguments to pass to the debugger,
    as a string. Any debugger-specific separator arguments are appended after these
    arguments.
    :param debuggerInteractive: If specified, forces the debugger to be interactive.
    '''

    debuggerPath = None

    if debugger:
        # Append '.exe' to the debugger on Windows if it's not present,
        # so things like '--debugger=devenv' work.
        if (os.name == 'nt'
            and not debugger.lower().endswith('.exe')):
            debugger += '.exe'

        debuggerPath = find_executable(debugger)

    if not debuggerPath:
        print 'Error: Could not find debugger %s.' % debugger
        return None

    debuggerName = os.path.basename(debuggerPath).lower()

    def get_debugger_info(type, default):
        if debuggerName in _DEBUGGER_INFO and type in _DEBUGGER_INFO[debuggerName]:
            return _DEBUGGER_INFO[debuggerName][type]
        return default

    # Define a namedtuple to access the debugger information from the outside world.
    DebuggerInfo = namedtuple(
        'DebuggerInfo',
        ['path', 'interactive', 'args', 'requiresEscapedArgs']
    )

    debugger_arguments = []

    if debuggerArgs:
        # Append the provided debugger arguments at the end of the arguments list.
        debugger_arguments += debuggerArgs.split()

    debugger_arguments += get_debugger_info('args', [])

    # Override the default debugger interactive mode if needed.
    debugger_interactive = get_debugger_info('interactive', False)
    if debuggerInteractive:
        debugger_interactive = debuggerInteractive

    d = DebuggerInfo(
        debuggerPath,
        debugger_interactive,
        debugger_arguments,
        get_debugger_info('requiresEscapedArgs', False)
    )

    return d

# Defines the search policies to use in get_default_debugger_name.
class DebuggerSearch:
  OnlyFirst = 1
  KeepLooking = 2

def get_default_debugger_name(search=DebuggerSearch.OnlyFirst):
    '''
    Get the debugger name for the default debugger on current platform.

    :param search: If specified, stops looking for the debugger if the
     default one is not found (|DebuggerSearch.OnlyFirst|) or keeps
     looking for other compatible debuggers (|DebuggerSearch.KeepLooking|).
    '''

    # Find out which debuggers are preferred for use on this platform.
    debuggerPriorities = _DEBUGGER_PRIORITIES[mozinfo.os if mozinfo.os in _DEBUGGER_PRIORITIES else 'unknown']

    # Finally get the debugger information.
    for debuggerName in debuggerPriorities:
        debuggerPath = find_executable(debuggerName)
        if debuggerPath:
            return debuggerName
        elif not search == DebuggerSearch.KeepLooking:
            return None

    return None

# Defines default values for Valgrind flags.
#
# --smc-check=all-non-file is required to deal with code generation and
#   patching by the various JITS.  Note that this is only necessary on
#   x86 and x86_64, but not on ARM.  This flag is only necessary for
#   Valgrind versions prior to 3.11.
#
# --vex-iropt-register-updates=allregs-at-mem-access is required so that
#   Valgrind generates correct register values whenever there is a
#   segfault that is caught and handled.  In particular OdinMonkey
#   requires this.  More recent Valgrinds (3.11 and later) provide
#   --px-default=allregs-at-mem-access and
#   --px-file-backed=unwindregs-at-mem-access
#   which provide a significantly cheaper alternative, by restricting the
#   precise exception behaviour to JIT generated code only.
#
# --trace-children=yes is required to get Valgrind to follow into
#   content and other child processes.  The resulting output can be
#   difficult to make sense of, and --child-silent-after-fork=yes
#   helps by causing Valgrind to be silent for the child in the period
#   after fork() but before its subsequent exec().
#
# --trace-children-skip lists processes that we are not interested
#   in tracing into.
#
# --leak-check=full requests full stack traces for all leaked blocks
#   detected at process exit.
#
# --show-possibly-lost=no requests blocks for which only an interior
#   pointer was found to be considered not leaked.
#
#
# TODO: pass in the user supplied args for V (--valgrind-args=) and
# use this to detect if a different tool has been selected.  If so
# adjust tool-specific args appropriately.
#
# TODO: pass in the path to the Valgrind to be used (--valgrind=), and
# check what flags it accepts.  Possible args that might be beneficial:
#
# --num-transtab-sectors=24   [reduces re-jitting overheads in long runs]
# --px-default=allregs-at-mem-access
# --px-file-backed=unwindregs-at-mem-access
#                             [these reduce PX overheads as described above]
#
def get_default_valgrind_args():
    return (['--fair-sched=yes',
             '--smc-check=all-non-file',
             '--vex-iropt-register-updates=allregs-at-mem-access',
             '--trace-children=yes',
             '--child-silent-after-fork=yes',
             '--leak-check=full',
             '--show-possibly-lost=no',
             ('--trace-children-skip='
              + '/usr/bin/hg,/bin/rm,*/bin/certutil,*/bin/pk12util,'
              + '*/bin/ssltunnel,*/bin/uname,*/bin/which,*/bin/ps,'
              + '*/bin/grep,*/bin/java'),
            ]
            + get_default_valgrind_tool_specific_args())

def get_default_valgrind_tool_specific_args():
    return [
            '--partial-loads-ok=yes'
    ]
