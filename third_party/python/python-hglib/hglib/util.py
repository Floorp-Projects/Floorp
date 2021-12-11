import os, subprocess, sys
from hglib import error
try:
    from io import BytesIO
except ImportError:
    from cStringIO import StringIO as BytesIO

if sys.version_info[0] > 2:
    izip = zip
    integertypes = (int,)

    def b(s):
        """Encode the string as bytes."""
        return s.encode('latin-1')
else:
    from itertools import izip
    integertypes = (long, int)
    bytes = str  # Defined in Python 2.6/2.7, but to the same value.

    def b(s):
        """Encode the string as bytes."""
        return s

def strtobytes(s):
    """Return the bytes of the string representation of an object."""
    return str(s).encode('latin-1')

def grouper(n, iterable):
    ''' list(grouper(2, range(4))) -> [(0, 1), (2, 3)] '''
    args = [iter(iterable)] * n
    return izip(*args)

def eatlines(s, n):
    """
    >>> eatlines(b("1\\n2"), 1) == b('2')
    True
    >>> eatlines(b("1\\n2"), 2) == b('')
    True
    >>> eatlines(b("1\\n2"), 3) == b('')
    True
    >>> eatlines(b("1\\n2\\n3"), 1) == b('2\\n3')
    True
    """
    cs = BytesIO(s)

    for line in cs:
        n -= 1
        if n == 0:
            return cs.read()
    return b('')

def skiplines(s, prefix):
    """
    Skip lines starting with prefix in s

    >>> skiplines(b('a\\nb\\na\\n'), b('a')) == b('b\\na\\n')
    True
    >>> skiplines(b('a\\na\\n'), b('a')) == b('')
    True
    >>> skiplines(b(''), b('a')) == b('')
    True
    >>> skiplines(b('a\\nb'), b('b')) == b('a\\nb')
    True
    """
    cs = BytesIO(s)

    for line in cs:
        if not line.startswith(prefix):
            return line + cs.read()

    return b('')

def _cmdval(val):
    if isinstance(val, bytes):
        return val
    else:
        return strtobytes(val)

def cmdbuilder(name, *args, **kwargs):
    """
    A helper for building the command arguments

    args are the positional arguments

    kwargs are the options
    keys that are single lettered are prepended with '-', others with '--',
    underscores are replaced with dashes

    keys with False boolean values are ignored, lists add the key multiple times

    None arguments are skipped

    >>> cmdbuilder(b('cmd'), a=True, b=False, c=None) == [b('cmd'), b('-a')]
    True
    >>> cmdbuilder(b('cmd'), long=True) == [b('cmd'), b('--long')]
    True
    >>> cmdbuilder(b('cmd'), str=b('s')) == [b('cmd'), b('--str'), b('s')]
    True
    >>> cmdbuilder(b('cmd'), d_ash=True) == [b('cmd'), b('--d-ash')]
    True
    >>> cmdbuilder(b('cmd'), _=True) == [b('cmd'), b('-')]
    True
    >>> expect = [b('cmd'), b('--list'), b('1'), b('--list'), b('2')]
    >>> cmdbuilder(b('cmd'), list=[1, 2]) == expect
    True
    >>> cmdbuilder(b('cmd'), None) == [b('cmd')]
    True
    """
    cmd = [name]
    for arg, val in kwargs.items():
        if val is None:
            continue

        arg = arg.encode('latin-1').replace(b('_'), b('-'))
        if arg != b('-'):
            if len(arg) == 1:
                arg = b('-') + arg
            else:
                arg = b('--') + arg
        if isinstance(val, bool):
            if val:
                cmd.append(arg)
        elif isinstance(val, list):
            for v in val:
                cmd.append(arg)
                cmd.append(_cmdval(v))
        else:
            cmd.append(arg)
            cmd.append(_cmdval(val))

    for a in args:
        if a is not None:
            cmd.append(a)

    return cmd

class reterrorhandler(object):
    """This class is meant to be used with rawcommand() error handler
    argument. It remembers the return value the command returned if
    it's one of allowed values, which is only 1 if none are given.
    Otherwise it raises a CommandError.

    >>> e = reterrorhandler('')
    >>> bool(e)
    True
    >>> e(1, 'a', '')
    'a'
    >>> bool(e)
    False

    """
    def __init__(self, args, allowed=None):
        self.args = args
        self.ret = 0
        if allowed is None:
            self.allowed = [1]
        else:
            self.allowed = allowed

    def __call__(self, ret, out, err):
        self.ret = ret
        if ret not in self.allowed:
            raise error.CommandError(self.args, ret, out, err)
        return out

    def __nonzero__(self):
        """ Returns True if the return code was 0, False otherwise """
        return self.ret == 0

    def __bool__(self):
        return self.__nonzero__()

class propertycache(object):
    """
    Decorator that remembers the return value of a function call.

    >>> execcount = 0
    >>> class obj(object):
    ...     def func(self):
    ...         global execcount
    ...         execcount += 1
    ...         return []
    ...     func = propertycache(func)
    >>> o = obj()
    >>> o.func
    []
    >>> execcount
    1
    >>> o.func
    []
    >>> execcount
    1
    """
    def __init__(self, func):
        self.func = func
        self.name = func.__name__
    def __get__(self, obj, type=None):
        result = self.func(obj)
        setattr(obj, self.name, result)
        return result

close_fds = os.name == 'posix'

startupinfo = None
if os.name == 'nt':
    startupinfo = subprocess.STARTUPINFO()
    startupinfo.dwFlags |= subprocess.STARTF_USESHOWWINDOW

def popen(args, env=None):
    environ = None
    if env:
        environ = dict(os.environ)
        environ.update(env)

    return subprocess.Popen(args, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, close_fds=close_fds,
                            startupinfo=startupinfo, env=environ)
