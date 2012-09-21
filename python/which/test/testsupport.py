#!/usr/bin/env python
# Copyright (c) 2002-2003 ActiveState Corp.
# Author: Trent Mick (TrentM@ActiveState.com)

import os
import sys
import types


#---- Support routines

def _escapeArg(arg):
    """Escape the given command line argument for the shell."""
    #XXX There is a *lot* more that we should escape here.
    return arg.replace('"', r'\"')


def _joinArgv(argv):
    r"""Join an arglist to a string appropriate for running.
        >>> import os
        >>> _joinArgv(['foo', 'bar "baz'])
        'foo "bar \\"baz"'
    """
    cmdstr = ""
    for arg in argv:
        if ' ' in arg:
            cmdstr += '"%s"' % _escapeArg(arg)
        else:
            cmdstr += _escapeArg(arg)
        cmdstr += ' '
    if cmdstr.endswith(' '): cmdstr = cmdstr[:-1]  # strip trailing space
    return cmdstr


def run(argv):
    """Prepare and run the given arg vector, 'argv', and return the
    results.  Returns (<stdout lines>, <stderr lines>, <return value>).
    Note: 'argv' may also just be the command string.
    """
    if type(argv) in (types.ListType, types.TupleType):
        cmd = _joinArgv(argv)
    else:
        cmd = argv
    if sys.platform.startswith('win'):
        i, o, e = os.popen3(cmd)
        output = o.read()
        error = e.read()
        i.close()
        e.close()
        try:
            retval = o.close()
        except IOError:
            # IOError is raised iff the spawned app returns -1. Go
            # figure.
            retval = -1 
        if retval is None:
            retval = 0
    else:
        import popen2
        p = popen2.Popen3(cmd, 1)
        i, o, e = p.tochild, p.fromchild, p.childerr
        output = o.read()
        error = e.read()
        i.close()
        o.close()
        e.close()
        retval = (p.wait() & 0xFF00) >> 8
        if retval > 2**7: # 8-bit signed 1's-complement conversion
            retval -= 2**8
    return output, error, retval


def _rmtreeOnError(rmFunction, filePath, excInfo):
    if excInfo[0] == OSError:
        # presuming because file is read-only
        os.chmod(filePath, 0777)
        rmFunction(filePath)

def rmtree(dirname):
    import shutil
    shutil.rmtree(dirname, 0, _rmtreeOnError)


