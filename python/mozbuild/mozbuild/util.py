# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

# This file contains miscellaneous utility functions that don't belong anywhere
# in particular.

from __future__ import absolute_import, unicode_literals

import collections
import difflib
import errno
import functools
import hashlib
import itertools
import os
import stat
import sys
import time
import types

from collections import (
    defaultdict,
    OrderedDict,
)
from io import (
    StringIO,
    BytesIO,
)


if sys.version_info[0] == 3:
    str_type = str
else:
    str_type = basestring

def hash_file(path, hasher=None):
    """Hashes a file specified by the path given and returns the hex digest."""

    # If the default hashing function changes, this may invalidate
    # lots of cached data.  Don't change it lightly.
    h = hasher or hashlib.sha1()

    with open(path, 'rb') as fh:
        while True:
            data = fh.read(8192)

            if not len(data):
                break

            h.update(data)

    return h.hexdigest()


class EmptyValue(unicode):
    """A dummy type that behaves like an empty string and sequence.

    This type exists in order to support
    :py:class:`mozbuild.frontend.reader.EmptyConfig`. It should likely not be
    used elsewhere.
    """
    def __init__(self):
        super(EmptyValue, self).__init__()


class ReadOnlyDict(dict):
    """A read-only dictionary."""
    def __init__(self, *args, **kwargs):
        dict.__init__(self, *args, **kwargs)

    def __delitem__(self, key):
        raise Exception('Object does not support deletion.')

    def __setitem__(self, key, value):
        raise Exception('Object does not support assignment.')

    def update(self, *args, **kwargs):
        raise Exception('Object does not support update.')


class undefined_default(object):
    """Represents an undefined argument value that isn't None."""


undefined = undefined_default()


class ReadOnlyDefaultDict(ReadOnlyDict):
    """A read-only dictionary that supports default values on retrieval."""
    def __init__(self, default_factory, *args, **kwargs):
        ReadOnlyDict.__init__(self, *args, **kwargs)
        self._default_factory = default_factory

    def __missing__(self, key):
        value = self._default_factory()
        dict.__setitem__(self, key, value)
        return value


def ensureParentDir(path):
    """Ensures the directory parent to the given file exists."""
    d = os.path.dirname(path)
    if d and not os.path.exists(path):
        try:
            os.makedirs(d)
        except OSError, error:
            if error.errno != errno.EEXIST:
                raise


def readFileContent(name, mode):
    """Read the content of file, returns tuple (file existed, file content)"""
    existed = False
    old_content = None
    try:
        existing = open(name, mode)
        existed = True
    except IOError:
        pass
    else:
        try:
            old_content = existing.read()
        except IOError:
            pass
        finally:
            existing.close()
    return existed, old_content


class FileAvoidWrite(BytesIO):
    """File-like object that buffers output and only writes if content changed.

    We create an instance from an existing filename. New content is written to
    it. When we close the file object, if the content in the in-memory buffer
    differs from what is on disk, then we write out the new content. Otherwise,
    the original file is untouched.

    Instances can optionally capture diffs of file changes. This feature is not
    enabled by default because it a) doesn't make sense for binary files b)
    could add unwanted overhead to calls.
    """
    def __init__(self, filename, capture_diff=False, mode='rU'):
        BytesIO.__init__(self)
        self.name = filename
        self._capture_diff = capture_diff
        self.diff = None
        self.mode = mode
        self.force_update = False

    def write(self, buf):
        if isinstance(buf, unicode):
            buf = buf.encode('utf-8')
        BytesIO.write(self, buf)

    def close(self):
        """Stop accepting writes, compare file contents, and rewrite if needed.

        Returns a tuple of bools indicating what action was performed:

            (file existed, file updated)

        If ``capture_diff`` was specified at construction time and the
        underlying file was changed, ``.diff`` will be populated with the diff
        of the result.
        """
        buf = self.getvalue()
        BytesIO.close(self)

        existed, old_content = readFileContent(self.name, self.mode)
        if not self.force_update and old_content == buf:
            assert existed
            return existed, False

        ensureParentDir(self.name)
        with open(self.name, 'w') as file:
            file.write(buf)

        if self._capture_diff:
            try:
                old_lines = old_content.splitlines() if old_content else []
                new_lines = buf.splitlines()

                self.diff = difflib.unified_diff(old_lines, new_lines,
                    self.name, self.name, n=4, lineterm='')
            # FileAvoidWrite isn't unicode/bytes safe. So, files with non-ascii
            # content or opened and written in different modes may involve
            # implicit conversion and this will make Python unhappy. Since
            # diffing isn't a critical feature, we just ignore the failure.
            # This can go away once FileAvoidWrite uses io.BytesIO and
            # io.StringIO. But that will require a lot of work.
            except (UnicodeDecodeError, UnicodeEncodeError):
                self.diff = 'Binary or non-ascii file changed: %s' % self.name

        return existed, True

    def __enter__(self):
        return self
    def __exit__(self, type, value, traceback):
        self.close()


def resolve_target_to_make(topobjdir, target):
    r'''
    Resolve `target` (a target, directory, or file) to a make target.

    `topobjdir` is the object directory; all make targets will be
    rooted at or below the top-level Makefile in this directory.

    Returns a pair `(reldir, target)` where `reldir` is a directory
    relative to `topobjdir` containing a Makefile and `target` is a
    make target (possibly `None`).

    A directory resolves to the nearest directory at or above
    containing a Makefile, and target `None`.

    A regular (non-Makefile) file resolves to the nearest directory at
    or above the file containing a Makefile, and an appropriate
    target.

    A Makefile resolves to the nearest parent strictly above the
    Makefile containing a different Makefile, and an appropriate
    target.
    '''

    target = target.replace(os.sep, '/').lstrip('/')
    abs_target = os.path.join(topobjdir, target)

    # For directories, run |make -C dir|. If the directory does not
    # contain a Makefile, check parents until we find one. At worst,
    # this will terminate at the root.
    if os.path.isdir(abs_target):
        current = abs_target

        while True:
            make_path = os.path.join(current, 'Makefile')
            if os.path.exists(make_path):
                return (current[len(topobjdir) + 1:], None)

            current = os.path.dirname(current)

    # If it's not in a directory, this is probably a top-level make
    # target. Treat it as such.
    if '/' not in target:
        return (None, target)

    # We have a relative path within the tree. We look for a Makefile
    # as far into the path as possible. Then, we compute the make
    # target as relative to that directory.
    reldir = os.path.dirname(target)
    target = os.path.basename(target)

    while True:
        make_path = os.path.join(topobjdir, reldir, 'Makefile')

        # We append to target every iteration, so the check below
        # happens exactly once.
        if target != 'Makefile' and os.path.exists(make_path):
            return (reldir, target)

        target = os.path.join(os.path.basename(reldir), target)
        reldir = os.path.dirname(reldir)


class ListMixin(object):
    def __init__(self, iterable=[]):
        if not isinstance(iterable, list):
            raise ValueError('List can only be created from other list instances.')

        return super(ListMixin, self).__init__(iterable)

    def extend(self, l):
        if not isinstance(l, list):
            raise ValueError('List can only be extended with other list instances.')

        return super(ListMixin, self).extend(l)

    def __setslice__(self, i, j, sequence):
        if not isinstance(sequence, list):
            raise ValueError('List can only be sliced with other list instances.')

        return super(ListMixin, self).__setslice__(i, j, sequence)

    def __add__(self, other):
        # Allow None and EmptyValue is a special case because it makes undefined
        # variable references in moz.build behave better.
        other = [] if isinstance(other, (types.NoneType, EmptyValue)) else other
        if not isinstance(other, list):
            raise ValueError('Only lists can be appended to lists.')

        new_list = self.__class__(self)
        new_list.extend(other)
        return new_list

    def __iadd__(self, other):
        other = [] if isinstance(other, (types.NoneType, EmptyValue)) else other
        if not isinstance(other, list):
            raise ValueError('Only lists can be appended to lists.')

        return super(ListMixin, self).__iadd__(other)


class List(ListMixin, list):
    """A list specialized for moz.build environments.

    We overload the assignment and append operations to require that the
    appended thing is a list. This avoids bad surprises coming from appending
    a string to a list, which would just add each letter of the string.
    """


class UnsortedError(Exception):
    def __init__(self, srtd, original):
        assert len(srtd) == len(original)

        self.sorted = srtd
        self.original = original

        for i, orig in enumerate(original):
            s = srtd[i]

            if orig != s:
                self.i = i
                break

    def __str__(self):
        s = StringIO()

        s.write('An attempt was made to add an unsorted sequence to a list. ')
        s.write('The incoming list is unsorted starting at element %d. ' %
            self.i)
        s.write('We expected "%s" but got "%s"' % (
            self.sorted[self.i], self.original[self.i]))

        return s.getvalue()


class StrictOrderingOnAppendListMixin(object):
    @staticmethod
    def ensure_sorted(l):
        if isinstance(l, StrictOrderingOnAppendList):
            return

        srtd = sorted(l, key=lambda x: x.lower())

        if srtd != l:
            raise UnsortedError(srtd, l)

    def __init__(self, iterable=[]):
        StrictOrderingOnAppendListMixin.ensure_sorted(iterable)

        super(StrictOrderingOnAppendListMixin, self).__init__(iterable)

    def extend(self, l):
        StrictOrderingOnAppendListMixin.ensure_sorted(l)

        return super(StrictOrderingOnAppendListMixin, self).extend(l)

    def __setslice__(self, i, j, sequence):
        StrictOrderingOnAppendListMixin.ensure_sorted(sequence)

        return super(StrictOrderingOnAppendListMixin, self).__setslice__(i, j,
            sequence)

    def __add__(self, other):
        StrictOrderingOnAppendListMixin.ensure_sorted(other)

        return super(StrictOrderingOnAppendListMixin, self).__add__(other)

    def __iadd__(self, other):
        StrictOrderingOnAppendListMixin.ensure_sorted(other)

        return super(StrictOrderingOnAppendListMixin, self).__iadd__(other)


class StrictOrderingOnAppendList(ListMixin, StrictOrderingOnAppendListMixin,
        list):
    """A list specialized for moz.build environments.

    We overload the assignment and append operations to require that incoming
    elements be ordered. This enforces cleaner style in moz.build files.
    """


class MozbuildDeletionError(Exception):
    pass


def FlagsFactory(flags):
    """Returns a class which holds optional flags for an item in a list.

    The flags are defined in the dict given as argument, where keys are
    the flag names, and values the type used for the value of that flag.

    The resulting class is used by the various <TypeName>WithFlagsFactory
    functions below.
    """
    assert isinstance(flags, dict)
    assert all(isinstance(v, type) for v in flags.values())

    class Flags(object):
        __slots__ = flags.keys()
        _flags = flags

        def update(self, **kwargs):
            for k, v in kwargs.iteritems():
                setattr(self, k, v)

        def __getattr__(self, name):
            if name not in self.__slots__:
                raise AttributeError("'%s' object has no attribute '%s'" %
                                     (self.__class__.__name__, name))
            try:
                return object.__getattr__(self, name)
            except AttributeError:
                value = self._flags[name]()
                self.__setattr__(name, value)
                return value

        def __setattr__(self, name, value):
            if name not in self.__slots__:
                raise AttributeError("'%s' object has no attribute '%s'" %
                                     (self.__class__.__name__, name))
            if not isinstance(value, self._flags[name]):
                raise TypeError("'%s' attribute of class '%s' must be '%s'" %
                                (name, self.__class__.__name__,
                                 self._flags[name].__name__))
            return object.__setattr__(self, name, value)

        def __delattr__(self, name):
            raise MozbuildDeletionError('Unable to delete attributes for this object')

    return Flags


def StrictOrderingOnAppendListWithFlagsFactory(flags):
    """Returns a StrictOrderingOnAppendList-like object, with optional
    flags on each item.

    The flags are defined in the dict given as argument, where keys are
    the flag names, and values the type used for the value of that flag.

    Example:
        FooList = StrictOrderingOnAppendListWithFlagsFactory({
            'foo': bool, 'bar': unicode
        })
        foo = FooList(['a', 'b', 'c'])
        foo['a'].foo = True
        foo['b'].bar = 'bar'
    """
    class StrictOrderingOnAppendListWithFlags(StrictOrderingOnAppendList):
        def __init__(self, iterable=[]):
            StrictOrderingOnAppendList.__init__(self, iterable)
            self._flags_type = FlagsFactory(flags)
            self._flags = dict()

        def __getitem__(self, name):
            if name not in self._flags:
                if name not in self:
                    raise KeyError("'%s'" % name)
                self._flags[name] = self._flags_type()
            return self._flags[name]

        def __setitem__(self, name, value):
            raise TypeError("'%s' object does not support item assignment" %
                            self.__class__.__name__)

    return StrictOrderingOnAppendListWithFlags


class HierarchicalStringList(object):
    """A hierarchy of lists of strings.

    Each instance of this object contains a list of strings, which can be set or
    appended to. A sub-level of the hierarchy is also an instance of this class,
    can be added by appending to an attribute instead.

    For example, the moz.build variable EXPORTS is an instance of this class. We
    can do:

    EXPORTS += ['foo.h']
    EXPORTS.mozilla.dom += ['bar.h']

    In this case, we have 3 instances (EXPORTS, EXPORTS.mozilla, and
    EXPORTS.mozilla.dom), and the first and last each have one element in their
    list.
    """
    __slots__ = ('_strings', '_children')

    def __init__(self):
        self._strings = StrictOrderingOnAppendList()
        self._children = {}

    class StringListAdaptor(collections.Sequence):
        def __init__(self, hsl):
            self._hsl = hsl

        def __getitem__(self, index):
            return self._hsl._strings[index]

        def __len__(self):
            return len(self._hsl._strings)

        def flags_for(self, value):
            try:
                # Solely for the side-effect of throwing AttributeError
                object.__getattribute__(self._hsl, '__flag_slots__')
                # We now know we have a HierarchicalStringListWithFlags.
                # Get the flags, but use |get| so we don't create the
                # flags if they're not already there.
                return self._hsl._flags.get(value, None)
            except AttributeError:
                return None

    def walk(self):
        """Walk over all HierarchicalStringLists in the hierarchy.

        This is a generator of (path, sequence).

        The path is '' for the root level and '/'-delimited strings for
        any descendants.  The sequence is a read-only sequence of the
        strings contained at that level.  To support accessing the flags
        for a given string (e.g. when walking over a
        HierarchicalStringListWithFlagsFactory), the sequence supports a
        flags_for() method.  Given a string, the flags_for() method returns
        the flags for the string, if any, or None if there are no flags set.
        """

        if self._strings:
            path_to_here = ''
            yield path_to_here, self.StringListAdaptor(self)

        for k, l in sorted(self._children.items()):
            for p, v in l.walk():
                path_to_there = '%s/%s' % (k, p)
                yield path_to_there.strip('/'), v

    def __setattr__(self, name, value):
        if name in self.__slots__:
            return object.__setattr__(self, name, value)

        # __setattr__ can be called with a list when a simple assignment is
        # used:
        #
        # EXPORTS.foo = ['file.h']
        #
        # In this case, we need to overwrite foo's current list of strings.
        #
        # However, __setattr__ is also called with a HierarchicalStringList
        # to try to actually set the attribute. We want to ignore this case,
        # since we don't actually create an attribute called 'foo', but just add
        # it to our list of children (using _get_exportvariable()).
        self._set_exportvariable(name, value)

    def __getattr__(self, name):
        if name.startswith('__'):
            return object.__getattr__(self, name)
        return self._get_exportvariable(name)

    def __delattr__(self, name):
        raise MozbuildDeletionError('Unable to delete attributes for this object')

    def __iadd__(self, other):
        self._check_list(other)
        self._strings += other
        return self

    def __getitem__(self, name):
        return self._get_exportvariable(name)

    def __setitem__(self, name, value):
        self._set_exportvariable(name, value)

    def _get_exportvariable(self, name):
        return self._children.setdefault(name, HierarchicalStringList())

    def _set_exportvariable(self, name, value):
        exports = self._get_exportvariable(name)
        if not isinstance(value, HierarchicalStringList):
            exports._check_list(value)
            exports._strings = value

    def _check_list(self, value):
        if not isinstance(value, list):
            raise ValueError('Expected a list of strings, not %s' % type(value))
        for v in value:
            if not isinstance(v, str_type):
                raise ValueError(
                    'Expected a list of strings, not an element of %s' % type(v))


def HierarchicalStringListWithFlagsFactory(flags):
    """Returns a HierarchicalStringList-like object, with optional
    flags on each item.

    The flags are defined in the dict given as argument, where keys are
    the flag names, and values the type used for the value of that flag.

    Example:
        FooList = HierarchicalStringListWithFlagsFactory({
            'foo': bool, 'bar': unicode
        })
        foo = FooList(['a', 'b', 'c'])
        foo['a'].foo = True
        foo['b'].bar = 'bar'
        foo.sub = ['x, 'y']
        foo.sub['x'].foo = False
        foo.sub['y'].bar = 'baz'
    """
    class HierarchicalStringListWithFlags(HierarchicalStringList):
        __flag_slots__ = ('_flags_type', '_flags')

        def __init__(self):
            HierarchicalStringList.__init__(self)
            self._flags_type = FlagsFactory(flags)
            self._flags = dict()

        def __setattr__(self, name, value):
            if name in self.__flag_slots__:
                return object.__setattr__(self, name, value)
            HierarchicalStringList.__setattr__(self, name, value)

        def __getattr__(self, name):
            if name in self.__flag_slots__:
                return object.__getattr__(self, name)
            return HierarchicalStringList.__getattr__(self, name)

        def __getitem__(self, name):
            if name not in self._flags:
                if name not in self._strings:
                    raise KeyError("'%s'" % name)
                self._flags[name] = self._flags_type()
            return self._flags[name]

        def __setitem__(self, name, value):
            raise TypeError("'%s' object does not support item assignment" %
                            self.__class__.__name__)

        def _get_exportvariable(self, name):
            return self._children.setdefault(name, HierarchicalStringListWithFlags())

    return HierarchicalStringListWithFlags

class LockFile(object):
    """LockFile is used by the lock_file method to hold the lock.

    This object should not be used directly, but only through
    the lock_file method below.
    """

    def __init__(self, lockfile):
        self.lockfile = lockfile

    def __del__(self):
        while True:
            try:
                os.remove(self.lockfile)
                break
            except OSError as e:
                if e.errno == errno.EACCES:
                    # Another process probably has the file open, we'll retry.
                    # Just a short sleep since we want to drop the lock ASAP
                    # (but we need to let some other process close the file
                    # first).
                    time.sleep(0.1)
            else:
                # Re-raise unknown errors
                raise


def lock_file(lockfile, max_wait = 600):
    """Create and hold a lockfile of the given name, with the given timeout.

    To release the lock, delete the returned object.
    """

    # FUTURE This function and object could be written as a context manager.

    while True:
        try:
            fd = os.open(lockfile, os.O_EXCL | os.O_RDWR | os.O_CREAT)
            # We created the lockfile, so we're the owner
            break
        except OSError as e:
            if (e.errno == errno.EEXIST or
                (sys.platform == "win32" and e.errno == errno.EACCES)):
                pass
            else:
                # Should not occur
                raise

        try:
            # The lock file exists, try to stat it to get its age
            # and read its contents to report the owner PID
            f = open(lockfile, 'r')
            s = os.stat(lockfile)
        except EnvironmentError as e:
            if e.errno == errno.ENOENT or e.errno == errno.EACCES:
            # We didn't create the lockfile, so it did exist, but it's
            # gone now. Just try again
                continue

            raise Exception('{0} exists but stat() failed: {1}'.format(
                lockfile, e.strerror))

        # We didn't create the lockfile and it's still there, check
        # its age
        now = int(time.time())
        if now - s[stat.ST_MTIME] > max_wait:
            pid = f.readline().rstrip()
            raise Exception('{0} has been locked for more than '
                '{1} seconds (PID {2})'.format(lockfile, max_wait, pid))

        # It's not been locked too long, wait a while and retry
        f.close()
        time.sleep(1)

    # if we get here. we have the lockfile. Convert the os.open file
    # descriptor into a Python file object and record our PID in it
    f = os.fdopen(fd, 'w')
    f.write('{0}\n'.format(os.getpid()))
    f.close()

    return LockFile(lockfile)


class PushbackIter(object):
    '''Utility iterator that can deal with pushed back elements.

    This behaves like a regular iterable, just that you can call
    iter.pushback(item) to get the given item as next item in the
    iteration.
    '''
    def __init__(self, iterable):
        self.it = iter(iterable)
        self.pushed_back = []

    def __iter__(self):
        return self

    def __nonzero__(self):
        if self.pushed_back:
            return True

        try:
            self.pushed_back.insert(0, self.it.next())
        except StopIteration:
            return False
        else:
            return True

    def next(self):
        if self.pushed_back:
            return self.pushed_back.pop()
        return self.it.next()

    def pushback(self, item):
        self.pushed_back.append(item)


def shell_quote(s):
    '''Given a string, returns a version enclosed with single quotes for use
    in a shell command line.

    As a special case, if given an int, returns a string containing the int,
    not enclosed in quotes.
    '''
    if type(s) == int:
        return '%d' % s
    # Single quoted strings can contain any characters unescaped except the
    # single quote itself, which can't even be escaped, so the string needs to
    # be closed, an escaped single quote added, and reopened.
    t = type(s)
    return t("'%s'") % s.replace(t("'"), t("'\\''"))


class OrderedDefaultDict(OrderedDict):
    '''A combination of OrderedDict and defaultdict.'''
    def __init__(self, default_factory, *args, **kwargs):
        OrderedDict.__init__(self, *args, **kwargs)
        self._default_factory = default_factory

    def __missing__(self, key):
        value = self[key] = self._default_factory()
        return value


class KeyedDefaultDict(dict):
    '''Like a defaultdict, but the default_factory function takes the key as
    argument'''
    def __init__(self, default_factory, *args, **kwargs):
        dict.__init__(self, *args, **kwargs)
        self._default_factory = default_factory

    def __missing__(self, key):
        value = self._default_factory(key)
        dict.__setitem__(self, key, value)
        return value


class ReadOnlyKeyedDefaultDict(KeyedDefaultDict, ReadOnlyDict):
    '''Like KeyedDefaultDict, but read-only.'''


class memoize(dict):
    '''A decorator to memoize the results of function calls depending
    on its arguments.
    Both functions and instance methods are handled, although in the
    instance method case, the results are cache in the instance itself.
    '''
    def __init__(self, func):
        self.func = func
        functools.update_wrapper(self, func)

    def __call__(self, *args):
        if args not in self:
            self[args] = self.func(*args)
        return self[args]

    def method_call(self, instance, *args):
        name = '_%s' % self.func.__name__
        if not hasattr(instance, name):
            setattr(instance, name, {})
        cache = getattr(instance, name)
        if args not in cache:
            cache[args] = self.func(instance, *args)
        return cache[args]

    def __get__(self, instance, cls):
        return functools.update_wrapper(
            functools.partial(self.method_call, instance), self.func)


class memoized_property(object):
    '''A specialized version of the memoize decorator that works for
    class instance properties.
    '''
    def __init__(self, func):
        self.func = func

    def __get__(self, instance, cls):
        name = '_%s' % self.func.__name__
        if not hasattr(instance, name):
            setattr(instance, name, self.func(instance))
        return getattr(instance, name)


def TypedNamedTuple(name, fields):
    """Factory for named tuple types with strong typing.

    Arguments are an iterable of 2-tuples. The first member is the
    the field name. The second member is a type the field will be validated
    to be.

    Construction of instances varies from ``collections.namedtuple``.

    First, if a single tuple argument is given to the constructor, this is
    treated as the equivalent of passing each tuple value as a separate
    argument into __init__. e.g.::

        t = (1, 2)
        TypedTuple(t) == TypedTuple(1, 2)

    This behavior is meant for moz.build files, so vanilla tuples are
    automatically cast to typed tuple instances.

    Second, fields in the tuple are validated to be instances of the specified
    type. This is done via an ``isinstance()`` check. To allow multiple types,
    pass a tuple as the allowed types field.
    """
    cls = collections.namedtuple(name, (name for name, typ in fields))

    class TypedTuple(cls):
        __slots__ = ()

        def __new__(klass, *args, **kwargs):
            if len(args) == 1 and not kwargs and isinstance(args[0], tuple):
                args = args[0]

            return super(TypedTuple, klass).__new__(klass, *args, **kwargs)

        def __init__(self, *args, **kwargs):
            for i, (fname, ftype) in enumerate(self._fields):
                value = self[i]

                if not isinstance(value, ftype):
                    raise TypeError('field in tuple not of proper type: %s; '
                                    'got %s, expected %s' % (fname,
                                    type(value), ftype))

            super(TypedTuple, self).__init__(*args, **kwargs)

    TypedTuple._fields = fields

    return TypedTuple


class TypedListMixin(object):
    '''Mixin for a list with type coercion. See TypedList.'''

    def _ensure_type(self, l):
        if isinstance(l, self.__class__):
            return l

        return [self.normalize(e) for e in l]

    def __init__(self, iterable=[]):
        iterable = self._ensure_type(iterable)

        super(TypedListMixin, self).__init__(iterable)

    def extend(self, l):
        l = self._ensure_type(l)

        return super(TypedListMixin, self).extend(l)

    def __setslice__(self, i, j, sequence):
        sequence = self._ensure_type(sequence)

        return super(TypedListMixin, self).__setslice__(i, j,
            sequence)

    def __add__(self, other):
        other = self._ensure_type(other)

        return super(TypedListMixin, self).__add__(other)

    def __iadd__(self, other):
        other = self._ensure_type(other)

        return super(TypedListMixin, self).__iadd__(other)

    def append(self, other):
        self += [other]


@memoize
def TypedList(type, base_class=List):
    '''A list with type coercion.

    The given ``type`` is what list elements are being coerced to. It may do
    strict validation, throwing ValueError exceptions.

    A ``base_class`` type can be given for more specific uses than a List. For
    example, a Typed StrictOrderingOnAppendList can be created with:

       TypedList(unicode, StrictOrderingOnAppendList)
    '''
    class _TypedList(TypedListMixin, base_class):
        @staticmethod
        def normalize(e):
            if not isinstance(e, type):
                e = type(e)
            return e

    return _TypedList

def group_unified_files(files, unified_prefix, unified_suffix,
                        files_per_unified_file):
    """Return an iterator of (unified_filename, source_filenames) tuples.

    We compile most C and C++ files in "unified mode"; instead of compiling
    ``a.cpp``, ``b.cpp``, and ``c.cpp`` separately, we compile a single file
    that looks approximately like::

       #include "a.cpp"
       #include "b.cpp"
       #include "c.cpp"

    This function handles the details of generating names for the unified
    files, and determining which original source files go in which unified
    file."""

    # Make sure the input list is sorted. If it's not, bad things could happen!
    files = sorted(files)

    # Our last returned list of source filenames may be short, and we
    # don't want the fill value inserted by izip_longest to be an
    # issue.  So we do a little dance to filter it out ourselves.
    dummy_fill_value = ("dummy",)
    def filter_out_dummy(iterable):
        return itertools.ifilter(lambda x: x != dummy_fill_value,
                                 iterable)

    # From the itertools documentation, slightly modified:
    def grouper(n, iterable):
        "grouper(3, 'ABCDEFG', 'x') --> ABC DEF Gxx"
        args = [iter(iterable)] * n
        return itertools.izip_longest(fillvalue=dummy_fill_value, *args)

    for i, unified_group in enumerate(grouper(files_per_unified_file,
                                              files)):
        just_the_filenames = list(filter_out_dummy(unified_group))
        yield '%s%d.%s' % (unified_prefix, i, unified_suffix), just_the_filenames
