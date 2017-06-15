# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

######################################################################
# DO NOT UPDATE THIS FILE WITHOUT SIGN-OFF FROM A BUILD MODULE PEER. #
######################################################################

r"""This module contains the data structure (context) holding the configuration
from a moz.build. The data emitted by the frontend derives from those contexts.

It also defines the set of variables and functions available in moz.build.
If you are looking for the absolute authority on what moz.build files can
contain, you've come to the right place.
"""

from __future__ import absolute_import, unicode_literals

import os

from collections import (
    Counter,
    OrderedDict,
)
from mozbuild.util import (
    HierarchicalStringList,
    KeyedDefaultDict,
    List,
    ListWithAction,
    memoize,
    memoized_property,
    ReadOnlyKeyedDefaultDict,
    StrictOrderingOnAppendList,
    StrictOrderingOnAppendListWithAction,
    StrictOrderingOnAppendListWithFlagsFactory,
    TypedList,
    TypedNamedTuple,
)

from ..testing import (
    all_test_flavors,
    read_manifestparser_manifest,
    read_reftest_manifest,
    read_wpt_manifest,
)

import mozpack.path as mozpath
from types import FunctionType

import itertools


class ContextDerivedValue(object):
    """Classes deriving from this one receive a special treatment in a
    Context. See Context documentation.
    """
    __slots__ = ()


class Context(KeyedDefaultDict):
    """Represents a moz.build configuration context.

    Instances of this class are filled by the execution of sandboxes.
    At the core, a Context is a dict, with a defined set of possible keys we'll
    call variables. Each variable is associated with a type.

    When reading a value for a given key, we first try to read the existing
    value. If a value is not found and it is defined in the allowed variables
    set, we return a new instance of the class for that variable. We don't
    assign default instances until they are accessed because this makes
    debugging the end-result much simpler. Instead of a data structure with
    lots of empty/default values, you have a data structure with only the
    values that were read or touched.

    Instances of variables classes are created by invoking ``class_name()``,
    except when class_name derives from ``ContextDerivedValue`` or
    ``SubContext``, in which case ``class_name(instance_of_the_context)`` or
    ``class_name(self)`` is invoked. A value is added to those calls when
    instances are created during assignment (setitem).

    allowed_variables is a dict of the variables that can be set and read in
    this context instance. Keys in this dict are the strings representing keys
    in this context which are valid. Values are tuples of stored type,
    assigned type, default value, a docstring describing the purpose of the
    variable, and a tier indicator (see comment above the VARIABLES declaration
    in this module).

    config is the ConfigEnvironment for this context.
    """
    def __init__(self, allowed_variables={}, config=None, finder=None):
        self._allowed_variables = allowed_variables
        self.main_path = None
        self.current_path = None
        # There aren't going to be enough paths for the performance of scanning
        # a list to be a problem.
        self._all_paths = []
        self.config = config
        self._sandbox = None
        self._finder = finder
        KeyedDefaultDict.__init__(self, self._factory)

    def push_source(self, path):
        """Adds the given path as source of the data from this context and make
        it the current path for the context."""
        assert os.path.isabs(path)
        if not self.main_path:
            self.main_path = path
        else:
            # Callers shouldn't push after main_path has been popped.
            assert self.current_path
        self.current_path = path
        # The same file can be pushed twice, so don't remove any previous
        # occurrence.
        self._all_paths.append(path)

    def pop_source(self):
        """Get back to the previous current path for the context."""
        assert self.main_path
        assert self.current_path
        last = self._all_paths.pop()
        # Keep the popped path in the list of all paths, but before the main
        # path so that it's not popped again.
        self._all_paths.insert(0, last)
        if last == self.main_path:
            self.current_path = None
        else:
            self.current_path = self._all_paths[-1]
        return last

    def add_source(self, path):
        """Adds the given path as source of the data from this context."""
        assert os.path.isabs(path)
        if not self.main_path:
            self.main_path = self.current_path = path
        # Insert at the beginning of the list so that it's always before the
        # main path.
        if path not in self._all_paths:
            self._all_paths.insert(0, path)

    @property
    def error_is_fatal(self):
        """Returns True if the error function should be fatal."""
        return self.config and getattr(self.config, 'error_is_fatal', True)

    @property
    def all_paths(self):
        """Returns all paths ever added to the context."""
        return set(self._all_paths)

    @property
    def source_stack(self):
        """Returns the current stack of pushed sources."""
        if not self.current_path:
            return []
        return self._all_paths[self._all_paths.index(self.main_path):]

    @memoized_property
    def objdir(self):
        return mozpath.join(self.config.topobjdir, self.relobjdir).rstrip('/')

    @memoize
    def _srcdir(self, path):
        return mozpath.join(self.config.topsrcdir,
            self._relsrcdir(path)).rstrip('/')

    @property
    def srcdir(self):
        return self._srcdir(self.current_path or self.main_path)

    @memoize
    def _relsrcdir(self, path):
        return mozpath.relpath(mozpath.dirname(path), self.config.topsrcdir)

    @property
    def relsrcdir(self):
        assert self.main_path
        return self._relsrcdir(self.current_path or self.main_path)

    @memoized_property
    def relobjdir(self):
        assert self.main_path
        return mozpath.relpath(mozpath.dirname(self.main_path),
            self.config.topsrcdir)

    def _factory(self, key):
        """Function called when requesting a missing key."""
        defaults = self._allowed_variables.get(key)
        if not defaults:
            raise KeyError('global_ns', 'get_unknown', key)

        # If the default is specifically a lambda (or, rather, any function
        # --but not a class that can be called), then it is actually a rule to
        # generate the default that should be used.
        default = defaults[0]
        if issubclass(default, ContextDerivedValue):
            return default(self)
        else:
            return default()

    def _validate(self, key, value, is_template=False):
        """Validates whether the key is allowed and if the value's type
        matches.
        """
        stored_type, input_type, docs = \
            self._allowed_variables.get(key, (None, None, None))

        if stored_type is None or not is_template and key in TEMPLATE_VARIABLES:
            raise KeyError('global_ns', 'set_unknown', key, value)

        # If the incoming value is not the type we store, we try to convert
        # it to that type. This relies on proper coercion rules existing. This
        # is the responsibility of whoever defined the symbols: a type should
        # not be in the allowed set if the constructor function for the stored
        # type does not accept an instance of that type.
        if not isinstance(value, (stored_type, input_type)):
            raise ValueError('global_ns', 'set_type', key, value, input_type)

        return stored_type

    def __setitem__(self, key, value):
        stored_type = self._validate(key, value)

        if not isinstance(value, stored_type):
            if issubclass(stored_type, ContextDerivedValue):
                value = stored_type(self, value)
            else:
                value = stored_type(value)

        return KeyedDefaultDict.__setitem__(self, key, value)

    def update(self, iterable={}, **kwargs):
        """Like dict.update(), but using the context's setitem.

        This function is transactional: if setitem fails for one of the values,
        the context is not updated at all."""
        if isinstance(iterable, dict):
            iterable = iterable.items()

        update = {}
        for key, value in itertools.chain(iterable, kwargs.items()):
            stored_type = self._validate(key, value)
            # Don't create an instance of stored_type if coercion is needed,
            # until all values are validated.
            update[key] = (value, stored_type)
        for key, (value, stored_type) in update.items():
            if not isinstance(value, stored_type):
                update[key] = stored_type(value)
            else:
                update[key] = value
        KeyedDefaultDict.update(self, update)


class TemplateContext(Context):
    def __init__(self, template=None, allowed_variables={}, config=None):
        self.template = template
        super(TemplateContext, self).__init__(allowed_variables, config)

    def _validate(self, key, value):
        return Context._validate(self, key, value, True)


class SubContext(Context, ContextDerivedValue):
    """A Context derived from another Context.

    Sub-contexts are intended to be used as context managers.

    Sub-contexts inherit paths and other relevant state from the parent
    context.
    """
    def __init__(self, parent):
        assert isinstance(parent, Context)

        Context.__init__(self, allowed_variables=self.VARIABLES,
                         config=parent.config)

        # Copy state from parent.
        for p in parent.source_stack:
            self.push_source(p)
        self._sandbox = parent._sandbox

    def __enter__(self):
        if not self._sandbox or self._sandbox() is None:
            raise Exception('a sandbox is required')

        self._sandbox().push_subcontext(self)

    def __exit__(self, exc_type, exc_value, traceback):
        self._sandbox().pop_subcontext(self)


class InitializedDefines(ContextDerivedValue, OrderedDict):
    def __init__(self, context, value=None):
        OrderedDict.__init__(self)
        for define in context.config.substs.get('MOZ_DEBUG_DEFINES', ()):
            self[define] = 1
        if value:
            self.update(value)


class FinalTargetValue(ContextDerivedValue, unicode):
    def __new__(cls, context, value=""):
        if not value:
            value = 'dist/'
            if context['XPI_NAME']:
                value += 'xpi-stage/' + context['XPI_NAME']
            else:
                value += 'bin'
            if context['DIST_SUBDIR']:
                value += '/' + context['DIST_SUBDIR']
        return unicode.__new__(cls, value)


def Enum(*values):
    assert len(values)
    default = values[0]

    class EnumClass(object):
        def __new__(cls, value=None):
            if value is None:
                return default
            if value in values:
                return value
            raise ValueError('Invalid value. Allowed values are: %s'
                             % ', '.join(repr(v) for v in values))
    return EnumClass


class PathMeta(type):
    """Meta class for the Path family of classes.

    It handles calling __new__ and __init__ with the right arguments
    in cases where a Path is instantiated with another instance of
    Path instead of having received a context.

    It also makes Path(context, value) instantiate one of the
    subclasses depending on the value, allowing callers to do
    standard type checking (isinstance(path, ObjDirPath)) instead
    of checking the value itself (path.startswith('!')).
    """
    def __call__(cls, context, value=None):
        if isinstance(context, Path):
            assert value is None
            value = context
            context = context.context
        else:
            assert isinstance(context, Context)
            if isinstance(value, Path):
                context = value.context
        if not issubclass(cls, (SourcePath, ObjDirPath, AbsolutePath)):
            if value.startswith('!'):
                cls = ObjDirPath
            elif value.startswith('%'):
                cls = AbsolutePath
            else:
                cls = SourcePath
        return super(PathMeta, cls).__call__(context, value)

class Path(ContextDerivedValue, unicode):
    """Stores and resolves a source path relative to a given context

    This class is used as a backing type for some of the sandbox variables.
    It expresses paths relative to a context. Supported paths are:
      - '/topsrcdir/relative/paths'
      - 'srcdir/relative/paths'
      - '!/topobjdir/relative/paths'
      - '!objdir/relative/paths'
      - '%/filesystem/absolute/paths'
    """
    __metaclass__ = PathMeta

    def __new__(cls, context, value=None):
        return super(Path, cls).__new__(cls, value)

    def __init__(self, context, value=None):
        # Only subclasses should be instantiated.
        assert self.__class__ != Path
        self.context = context
        self.srcdir = context.srcdir

    def join(self, *p):
        """ContextDerived equivalent of mozpath.join(self, *p), returning a
        new Path instance.
        """
        return Path(self.context, mozpath.join(self, *p))

    def __cmp__(self, other):
        if isinstance(other, Path) and self.srcdir != other.srcdir:
            return cmp(self.full_path, other.full_path)
        return cmp(unicode(self), other)

    # __cmp__ is not enough because unicode has __eq__, __ne__, etc. defined
    # and __cmp__ is only used for those when they don't exist.
    def __eq__(self, other):
        return self.__cmp__(other) == 0

    def __ne__(self, other):
        return self.__cmp__(other) != 0

    def __lt__(self, other):
        return self.__cmp__(other) < 0

    def __gt__(self, other):
        return self.__cmp__(other) > 0

    def __le__(self, other):
        return self.__cmp__(other) <= 0

    def __ge__(self, other):
        return self.__cmp__(other) >= 0

    def __repr__(self):
        return '<%s (%s)%s>' % (self.__class__.__name__, self.srcdir, self)

    def __hash__(self):
        return hash(self.full_path)

    @memoized_property
    def target_basename(self):
        return mozpath.basename(self.full_path)


class SourcePath(Path):
    """Like Path, but limited to paths in the source directory."""
    def __init__(self, context, value):
        if value.startswith('!'):
            raise ValueError('Object directory paths are not allowed')
        if value.startswith('%'):
            raise ValueError('Filesystem absolute paths are not allowed')
        super(SourcePath, self).__init__(context, value)

        if value.startswith('/'):
            path = None
            # If the path starts with a '/' and is actually relative to an
            # external source dir, use that as base instead of topsrcdir.
            if context.config.external_source_dir:
                path = mozpath.join(context.config.external_source_dir,
                                    value[1:])
            if not path or not os.path.exists(path):
                path = mozpath.join(context.config.topsrcdir,
                                    value[1:])
        else:
            path = mozpath.join(self.srcdir, value)
        self.full_path = mozpath.normpath(path)

    @memoized_property
    def translated(self):
        """Returns the corresponding path in the objdir.

        Ideally, we wouldn't need this function, but the fact that both source
        path under topsrcdir and the external source dir end up mixed in the
        objdir (aka pseudo-rework), this is needed.
        """
        return ObjDirPath(self.context, '!%s' % self).full_path


class RenamedSourcePath(SourcePath):
    """Like SourcePath, but with a different base name when installed.

    The constructor takes a tuple of (source, target_basename).

    This class is not meant to be exposed to moz.build sandboxes as of now,
    and is not supported by the RecursiveMake backend.
    """
    def __init__(self, context, value):
        assert isinstance(value, tuple)
        source, self._target_basename = value
        super(RenamedSourcePath, self).__init__(context, source)

    @property
    def target_basename(self):
        return self._target_basename


class ObjDirPath(Path):
    """Like Path, but limited to paths in the object directory."""
    def __init__(self, context, value=None):
        if not value.startswith('!'):
            raise ValueError('Object directory paths must start with ! prefix')
        super(ObjDirPath, self).__init__(context, value)

        if value.startswith('!/'):
            path = mozpath.join(context.config.topobjdir,value[2:])
        else:
            path = mozpath.join(context.objdir, value[1:])
        self.full_path = mozpath.normpath(path)


class AbsolutePath(Path):
    """Like Path, but allows arbitrary paths outside the source and object directories."""
    def __init__(self, context, value=None):
        if not value.startswith('%'):
            raise ValueError('Absolute paths must start with % prefix')
        if not os.path.isabs(value[1:]):
            raise ValueError('Path \'%s\' is not absolute' % value[1:])
        super(AbsolutePath, self).__init__(context, value)

        self.full_path = mozpath.normpath(value[1:])


@memoize
def ContextDerivedTypedList(klass, base_class=List):
    """Specialized TypedList for use with ContextDerivedValue types.
    """
    assert issubclass(klass, ContextDerivedValue)
    class _TypedList(ContextDerivedValue, TypedList(klass, base_class)):
        def __init__(self, context, iterable=[]):
            self.context = context
            super(_TypedList, self).__init__(iterable)

        def normalize(self, e):
            if not isinstance(e, klass):
                e = klass(self.context, e)
            return e

    return _TypedList

@memoize
def ContextDerivedTypedListWithItems(type, base_class=List):
    """Specialized TypedList for use with ContextDerivedValue types.
    """
    class _TypedListWithItems(ContextDerivedTypedList(type, base_class)):
        def __getitem__(self, name):
            name = self.normalize(name)
            return super(_TypedListWithItems, self).__getitem__(name)

    return _TypedListWithItems


@memoize
def ContextDerivedTypedRecord(*fields):
    """Factory for objects with certain properties and dynamic
    type checks.

    This API is extremely similar to the TypedNamedTuple API,
    except that properties may be mutated. This supports syntax like:

    VARIABLE_NAME.property += [
      'item1',
      'item2',
    ]
    """

    class _TypedRecord(ContextDerivedValue):
        __slots__ = tuple([name for name, _ in fields])

        def __init__(self, context):
            for fname, ftype in self._fields.items():
                if issubclass(ftype, ContextDerivedValue):
                    setattr(self, fname, self._fields[fname](context))
                else:
                    setattr(self, fname, self._fields[fname]())

        def __setattr__(self, name, value):
            if name in self._fields and not isinstance(value, self._fields[name]):
                value = self._fields[name](value)
            object.__setattr__(self, name, value)

    _TypedRecord._fields = dict(fields)
    return _TypedRecord


@memoize
def ContextDerivedTypedHierarchicalStringList(type):
    """Specialized HierarchicalStringList for use with ContextDerivedValue
    types."""
    class _TypedListWithItems(ContextDerivedValue, HierarchicalStringList):
        __slots__ = ('_strings', '_children', '_context')

        def __init__(self, context):
            self._strings = ContextDerivedTypedList(
                type, StrictOrderingOnAppendList)(context)
            self._children = {}
            self._context = context

        def _get_exportvariable(self, name):
            child = self._children.get(name)
            if not child:
                child = self._children[name] = _TypedListWithItems(
                    self._context)
            return child

    return _TypedListWithItems

def OrderedListWithAction(action):
    """Returns a class which behaves as a StrictOrderingOnAppendList, but
    invokes the given callable with each input and a context as it is
    read, storing a tuple including the result and the original item.

    This used to extend moz.build reading to make more data available in
    filesystem-reading mode.
    """
    class _OrderedListWithAction(ContextDerivedValue,
                                 StrictOrderingOnAppendListWithAction):
        def __init__(self, context, *args):
            def _action(item):
                return item, action(context, item)
            super(_OrderedListWithAction, self).__init__(action=_action, *args)

    return _OrderedListWithAction

def TypedListWithAction(typ, action):
    """Returns a class which behaves as a TypedList with the provided type, but
    invokes the given given callable with each input and a context as it is
    read, storing a tuple including the result and the original item.

    This used to extend moz.build reading to make more data available in
    filesystem-reading mode.
    """
    class _TypedListWithAction(ContextDerivedValue, TypedList(typ), ListWithAction):
        def __init__(self, context, *args):
            def _action(item):
                return item, action(context, item)
            super(_TypedListWithAction, self).__init__(action=_action, *args)
    return _TypedListWithAction

WebPlatformTestManifest = TypedNamedTuple("WebPlatformTestManifest",
                                          [("manifest_path", unicode),
                                           ("test_root", unicode)])
ManifestparserManifestList = OrderedListWithAction(read_manifestparser_manifest)
ReftestManifestList = OrderedListWithAction(read_reftest_manifest)
WptManifestList = TypedListWithAction(WebPlatformTestManifest, read_wpt_manifest)

OrderedSourceList = ContextDerivedTypedList(SourcePath, StrictOrderingOnAppendList)
OrderedTestFlavorList = TypedList(Enum(*all_test_flavors()),
                                  StrictOrderingOnAppendList)
OrderedStringList = TypedList(unicode, StrictOrderingOnAppendList)
DependentTestsEntry = ContextDerivedTypedRecord(('files', OrderedSourceList),
                                                ('tags', OrderedStringList),
                                                ('flavors', OrderedTestFlavorList))
BugzillaComponent = TypedNamedTuple('BugzillaComponent',
                        [('product', unicode), ('component', unicode)])


class Files(SubContext):
    """Metadata attached to files.

    It is common to want to annotate files with metadata, such as which
    Bugzilla component tracks issues with certain files. This sub-context is
    where we stick that metadata.

    The argument to this sub-context is a file matching pattern that is applied
    against the host file's directory. If the pattern matches a file whose info
    is currently being sought, the metadata attached to this instance will be
    applied to that file.

    Patterns are collections of filename characters with ``/`` used as the
    directory separate (UNIX-style paths) and ``*`` and ``**`` used to denote
    wildcard matching.

    Patterns without the ``*`` character are literal matches and will match at
    most one entity.

    Patterns with ``*`` or ``**`` are wildcard matches. ``*`` matches files
    at least within a single directory. ``**`` matches files across several
    directories.

    ``foo.html``
       Will match only the ``foo.html`` file in the current directory.
    ``*.jsm``
       Will match all ``.jsm`` files in the current directory.
    ``**/*.cpp``
       Will match all ``.cpp`` files in this and all child directories.
    ``foo/*.css``
       Will match all ``.css`` files in the ``foo/`` directory.
    ``bar/*``
       Will match all files in the ``bar/`` directory and all of its
       children directories.
    ``bar/**``
       This is equivalent to ``bar/*`` above.
    ``bar/**/foo``
       Will match all ``foo`` files in the ``bar/`` directory and all of its
       children directories.

    The difference in behavior between ``*`` and ``**`` is only evident if
    a pattern follows the ``*`` or ``**``. A pattern ending with ``*`` is
    greedy. ``**`` is needed when you need an additional pattern after the
    wildcard. e.g. ``**/foo``.
    """

    VARIABLES = {
        'BUG_COMPONENT': (BugzillaComponent, tuple,
            """The bug component that tracks changes to these files.

            Values are a 2-tuple of unicode describing the Bugzilla product and
            component. e.g. ``('Core', 'Build Config')``.
            """),

        'FINAL': (bool, bool,
            """Mark variable assignments as finalized.

            During normal processing, values from newer Files contexts
            overwrite previously set values. Last write wins. This behavior is
            not always desired. ``FINAL`` provides a mechanism to prevent
            further updates to a variable.

            When ``FINAL`` is set, the value of all variables defined in this
            context are marked as frozen and all subsequent writes to them
            are ignored during metadata reading.

            See :ref:`mozbuild_files_metadata_finalizing` for more info.
            """),
        'IMPACTED_TESTS': (DependentTestsEntry, list,
            """File patterns, tags, and flavors for tests relevant to these files.

            Maps source files to the tests potentially impacted by those files.
            Tests can be specified by file pattern, tag, or flavor.

            For example:

            with Files('runtests.py'):
               IMPACTED_TESTS.files += [
                   '**',
               ]

            in testing/mochitest/moz.build will suggest that any of the tests
            under testing/mochitest may be impacted by a change to runtests.py.

            File patterns may be made relative to the topsrcdir with a leading
            '/', so

            with Files('httpd.js'):
               IMPACTED_TESTS.files += [
                   '/testing/mochitest/tests/Harness_sanity/**',
               ]

            in netwerk/test/httpserver/moz.build will suggest that any change to httpd.js
            will be relevant to the mochitest sanity tests.

            Tags and flavors are sorted string lists (flavors are limited to valid
            values).

            For example:

            with Files('toolkit/devtools/*'):
                IMPACTED_TESTS.tags += [
                    'devtools',
                ]

            in the root moz.build would suggest that any test tagged 'devtools' would
            potentially be impacted by a change to a file under toolkit/devtools, and

            with Files('dom/base/nsGlobalWindow.cpp'):
                IMPACTED_TESTS.flavors += [
                    'mochitest',
                ]

            Would suggest that nsGlobalWindow.cpp is potentially relevant to
            any plain mochitest.
            """),
    }

    def __init__(self, parent, pattern=None):
        super(Files, self).__init__(parent)
        self.pattern = pattern
        self.finalized = set()
        self.test_files = set()
        self.test_tags = set()
        self.test_flavors = set()

    def __iadd__(self, other):
        assert isinstance(other, Files)

        self.test_files |= other.test_files
        self.test_tags |= other.test_tags
        self.test_flavors |= other.test_flavors

        for k, v in other.items():
            if k == 'IMPACTED_TESTS':
                self.test_files |= set(mozpath.relpath(e.full_path, e.context.config.topsrcdir)
                                       for e in v.files)
                self.test_tags |= set(v.tags)
                self.test_flavors |= set(v.flavors)
                continue

            # Ignore updates to finalized flags.
            if k in self.finalized:
                continue

            # Only finalize variables defined in this instance.
            if k == 'FINAL':
                self.finalized |= set(other) - {'FINAL'}
                continue

            self[k] = v

        return self

    def asdict(self):
        """Return this instance as a dict with built-in data structures.

        Call this to obtain an object suitable for serializing.
        """
        d = {}
        if 'BUG_COMPONENT' in self:
            bc = self['BUG_COMPONENT']
            d['bug_component'] = (bc.product, bc.component)

        return d

    @staticmethod
    def aggregate(files):
        """Given a mapping of path to Files, obtain aggregate results.

        Consumers may want to extract useful information from a collection of
        Files describing paths. e.g. given the files info data for N paths,
        recommend a single bug component based on the most frequent one. This
        function provides logic for deriving aggregate knowledge from a
        collection of path File metadata.

        Note: the intent of this function is to operate on the result of
        :py:func:`mozbuild.frontend.reader.BuildReader.files_info`. The
        :py:func:`mozbuild.frontend.context.Files` instances passed in are
        thus the "collapsed" (``__iadd__``ed) results of all ``Files`` from all
        moz.build files relevant to a specific path, not individual ``Files``
        instances from a single moz.build file.
        """
        d = {}

        bug_components = Counter()

        for f in files.values():
            bug_component = f.get('BUG_COMPONENT')
            if bug_component:
                bug_components[bug_component] += 1

        d['bug_component_counts'] = []
        for c, count in bug_components.most_common():
            component = (c.product, c.component)
            d['bug_component_counts'].append((c, count))

            if 'recommended_bug_component' not in d:
                d['recommended_bug_component'] = component
                recommended_count = count
            elif count == recommended_count:
                # Don't recommend a component if it doesn't have a clear lead.
                d['recommended_bug_component'] = None

        # In case no bug components.
        d.setdefault('recommended_bug_component', None)

        return d


# This defines functions that create sub-contexts.
#
# Values are classes that are SubContexts. The class name will be turned into
# a function that when called emits an instance of that class.
#
# Arbitrary arguments can be passed to the class constructor. The first
# argument is always the parent context. It is up to each class to perform
# argument validation.
SUBCONTEXTS = [
    Files,
]

for cls in SUBCONTEXTS:
    if not issubclass(cls, SubContext):
        raise ValueError('SUBCONTEXTS entry not a SubContext class: %s' % cls)

    if not hasattr(cls, 'VARIABLES'):
        raise ValueError('SUBCONTEXTS entry does not have VARIABLES: %s' % cls)

SUBCONTEXTS = {cls.__name__: cls for cls in SUBCONTEXTS}


# This defines the set of mutable global variables.
#
# Each variable is a tuple of:
#
#   (storage_type, input_types, docs)

VARIABLES = {
    'ALLOW_COMPILER_WARNINGS': (bool, bool,
        """Whether to allow compiler warnings (i.e. *not* treat them as
        errors).

        This is commonplace (almost mandatory, in fact) in directories
        containing third-party code that we regularly update from upstream and
        thus do not control, but is otherwise discouraged.
        """),

    # Variables controlling reading of other frontend files.
    'ANDROID_GENERATED_RESFILES': (StrictOrderingOnAppendList, list,
        """Android resource files generated as part of the build.

        This variable contains a list of files that are expected to be
        generated (often by preprocessing) into a 'res' directory as
        part of the build process, and subsequently merged into an APK
        file.
        """),

    'ANDROID_APK_NAME': (unicode, unicode,
        """The name of an Android APK file to generate.
        """),

    'ANDROID_APK_PACKAGE': (unicode, unicode,
        """The name of the Android package to generate R.java for, like org.mozilla.gecko.
        """),

    'ANDROID_EXTRA_PACKAGES': (StrictOrderingOnAppendList, list,
        """The name of extra Android packages to generate R.java for, like ['org.mozilla.other'].
        """),

    'ANDROID_EXTRA_RES_DIRS': (ContextDerivedTypedListWithItems(Path, List), list,
        """Android extra package resource directories.

        This variable contains a list of directories containing static files
        to package into a 'res' directory and merge into an APK file.  These
        directories are packaged into the APK but are assumed to be static
        unchecked dependencies that should not be otherwise re-distributed.
        """),

    'ANDROID_RES_DIRS': (ContextDerivedTypedListWithItems(Path, List), list,
        """Android resource directories.

        This variable contains a list of directories containing static
        files to package into a 'res' directory and merge into an APK
        file.
        """),

    'ANDROID_ASSETS_DIRS': (ContextDerivedTypedListWithItems(Path, List), list,
        """Android assets directories.

        This variable contains a list of directories containing static
        files to package into an 'assets' directory and merge into an
        APK file.
        """),

    'SOURCES': (ContextDerivedTypedListWithItems(Path, StrictOrderingOnAppendListWithFlagsFactory({'no_pgo': bool, 'flags': List})), list,
        """Source code files.

        This variable contains a list of source code files to compile.
        Accepts assembler, C, C++, Objective C/C++.
        """),

    'FILES_PER_UNIFIED_FILE': (int, int,
        """The number of source files to compile into each unified source file.

        """),

    'IS_RUST_LIBRARY': (bool, bool,
        """Whether the current library defined by this moz.build is built by Rust.

        The library defined by this moz.build should have a build definition in
        a Cargo.toml file that exists in this moz.build's directory.
        """),

    'RUST_LIBRARY_FEATURES': (List, list,
        """Cargo features to activate for this library.

        This variable should not be used directly; you should be using the
        RustLibrary template instead.
        """),

    'RUST_LIBRARY_TARGET_DIR': (unicode, unicode,
        """Where CARGO_TARGET_DIR should point when compiling this library.  If
        not set, it defaults to the current objdir.  It should be a relative path
        to the current objdir; absolute paths should not be used.

        This variable should not be used directly; you should be using the
        RustLibrary template instead.
        """),

    'HOST_RUST_LIBRARY_FEATURES': (List, list,
        """Cargo features to activate for this host library.

        This variable should not be used directly; you should be using the
        HostRustLibrary template instead.
        """),

    'UNIFIED_SOURCES': (ContextDerivedTypedList(SourcePath, StrictOrderingOnAppendList), list,
        """Source code files that can be compiled together.

        This variable contains a list of source code files to compile,
        that can be concatenated all together and built as a single source
        file. This can help make the build faster and reduce the debug info
        size.
        """),

    'GENERATED_FILES': (StrictOrderingOnAppendListWithFlagsFactory({
                'script': unicode,
                'inputs': list,
                'flags': list, }), list,
        """Generic generated files.

        This variable contains a list of files for the build system to
        generate at export time. The generation method may be declared
        with optional ``script``, ``inputs`` and ``flags`` attributes on
        individual entries.
        If the optional ``script`` attribute is not present on an entry, it
        is assumed that rules for generating the file are present in
        the associated Makefile.in.

        Example::

           GENERATED_FILES += ['bar.c', 'baz.c', 'foo.c']
           bar = GENERATED_FILES['bar.c']
           bar.script = 'generate.py'
           bar.inputs = ['datafile-for-bar']
           foo = GENERATED_FILES['foo.c']
           foo.script = 'generate.py'
           foo.inputs = ['datafile-for-foo']

        This definition will generate bar.c by calling the main method of
        generate.py with a open (for writing) file object for bar.c, and
        the string ``datafile-for-bar``. In a similar fashion, the main
        method of generate.py will also be called with an open
        (for writing) file object for foo.c and the string
        ``datafile-for-foo``. Please note that only string arguments are
        supported for passing to scripts, and that all arguments provided
        to the script should be filenames relative to the directory in which
        the moz.build file is located.

        To enable using the same script for generating multiple files with
        slightly different non-filename parameters, alternative entry points
        into ``script`` can be specified::

          GENERATED_FILES += ['bar.c']
          bar = GENERATED_FILES['bar.c']
          bar.script = 'generate.py:make_bar'

        The chosen script entry point may optionally return a set of strings,
        indicating extra files the output depends on.

        When the ``flags`` attribute is present, the given list of flags is
        passed as extra arguments following the inputs.
        """),

    'DEFINES': (InitializedDefines, dict,
        """Dictionary of compiler defines to declare.

        These are passed in to the compiler as ``-Dkey='value'`` for string
        values, ``-Dkey=value`` for numeric values, or ``-Dkey`` if the
        value is True. Note that for string values, the outer-level of
        single-quotes will be consumed by the shell. If you want to have
        a string-literal in the program, the value needs to have
        double-quotes.

        Example::

           DEFINES['NS_NO_XPCOM'] = True
           DEFINES['MOZ_EXTENSIONS_DB_SCHEMA'] = 15
           DEFINES['DLL_SUFFIX'] = '".so"'

        This will result in the compiler flags ``-DNS_NO_XPCOM``,
        ``-DMOZ_EXTENSIONS_DB_SCHEMA=15``, and ``-DDLL_SUFFIX='".so"'``,
        respectively. These could also be combined into a single
        update::

           DEFINES.update({
               'NS_NO_XPCOM': True,
               'MOZ_EXTENSIONS_DB_SCHEMA': 15,
               'DLL_SUFFIX': '".so"',
           })
        """),

    'DELAYLOAD_DLLS': (List, list,
        """Delay-loaded DLLs.

        This variable contains a list of DLL files which the module being linked
        should load lazily.  This only has an effect when building with MSVC.
        """),

    'DIRS': (ContextDerivedTypedList(SourcePath), list,
        """Child directories to descend into looking for build frontend files.

        This works similarly to the ``DIRS`` variable in make files. Each str
        value in the list is the name of a child directory. When this file is
        done parsing, the build reader will descend into each listed directory
        and read the frontend file there. If there is no frontend file, an error
        is raised.

        Values are relative paths. They can be multiple directory levels
        above or below. Use ``..`` for parent directories and ``/`` for path
        delimiters.
        """),

    'HAS_MISC_RULE': (bool, bool,
        """Whether this directory should be traversed in the ``misc`` tier.

        Many ``libs`` rules still exist in Makefile.in files. We highly prefer
        that these rules exist in the ``misc`` tier/target so that they can be
        executed concurrently during tier traversal (the ``misc`` tier is
        fully concurrent).

        Presence of this variable indicates that this directory should be
        traversed by the ``misc`` tier.

        Please note that converting ``libs`` rules to the ``misc`` tier must
        be done with care, as there are many implicit dependencies that can
        break the build in subtle ways.
        """),

    'FINAL_TARGET_FILES': (ContextDerivedTypedHierarchicalStringList(Path), list,
        """List of files to be installed into the application directory.

        ``FINAL_TARGET_FILES`` will copy (or symlink, if the platform supports it)
        the contents of its files to the directory specified by
        ``FINAL_TARGET`` (typically ``dist/bin``). Files that are destined for a
        subdirectory can be specified by accessing a field, or as a dict access.
        For example, to export ``foo.png`` to the top-level directory and
        ``bar.svg`` to the directory ``images/do-not-use``, append to
        ``FINAL_TARGET_FILES`` like so::

           FINAL_TARGET_FILES += ['foo.png']
           FINAL_TARGET_FILES.images['do-not-use'] += ['bar.svg']
        """),

    'DISABLE_STL_WRAPPING': (bool, bool,
        """Disable the wrappers for STL which allow it to work with C++ exceptions
        disabled.
        """),

    'FINAL_TARGET_PP_FILES': (ContextDerivedTypedHierarchicalStringList(Path), list,
        """Like ``FINAL_TARGET_FILES``, with preprocessing.
        """),

    'OBJDIR_FILES': (ContextDerivedTypedHierarchicalStringList(Path), list,
        """List of files to be installed anywhere in the objdir. Use sparingly.

        ``OBJDIR_FILES`` is similar to FINAL_TARGET_FILES, but it allows copying
        anywhere in the object directory. This is intended for various one-off
        cases, not for general use. If you wish to add entries to OBJDIR_FILES,
        please consult a build peer.
        """),

    'OBJDIR_PP_FILES': (ContextDerivedTypedHierarchicalStringList(Path), list,
        """Like ``OBJDIR_FILES``, with preprocessing. Use sparingly.
        """),

    'FINAL_LIBRARY': (unicode, unicode,
        """Library in which the objects of the current directory will be linked.

        This variable contains the name of a library, defined elsewhere with
        ``LIBRARY_NAME``, in which the objects of the current directory will be
        linked.
        """),

    'CPP_UNIT_TESTS': (StrictOrderingOnAppendList, list,
        """Compile a list of C++ unit test names.

        Each name in this variable corresponds to an executable built from the
        corresponding source file with the same base name.

        If the configuration token ``BIN_SUFFIX`` is set, its value will be
        automatically appended to each name. If a name already ends with
        ``BIN_SUFFIX``, the name will remain unchanged.
        """),

    'FORCE_SHARED_LIB': (bool, bool,
        """Whether the library in this directory is a shared library.
        """),

    'FORCE_STATIC_LIB': (bool, bool,
        """Whether the library in this directory is a static library.
        """),

    'USE_STATIC_LIBS': (bool, bool,
        """Whether the code in this directory is a built against the static
        runtime library.

        This variable only has an effect when building with MSVC.
        """),

    'HOST_SOURCES': (ContextDerivedTypedList(SourcePath, StrictOrderingOnAppendList), list,
        """Source code files to compile with the host compiler.

        This variable contains a list of source code files to compile.
        with the host compiler.
        """),

    'HOST_LIBRARY_NAME': (unicode, unicode,
        """Name of target library generated when cross compiling.
        """),

    'JAVA_JAR_TARGETS': (dict, dict,
        """Defines Java JAR targets to be built.

        This variable should not be populated directly. Instead, it should
        populated by calling add_java_jar().
        """),

    'LIBRARY_DEFINES': (OrderedDict, dict,
        """Dictionary of compiler defines to declare for the entire library.

        This variable works like DEFINES, except that declarations apply to all
        libraries that link into this library via FINAL_LIBRARY.
        """),

    'LIBRARY_NAME': (unicode, unicode,
        """The code name of the library generated for a directory.

        By default STATIC_LIBRARY_NAME and SHARED_LIBRARY_NAME take this name.
        In ``example/components/moz.build``,::

           LIBRARY_NAME = 'xpcomsample'

        would generate ``example/components/libxpcomsample.so`` on Linux, or
        ``example/components/xpcomsample.lib`` on Windows.
        """),

    'SHARED_LIBRARY_NAME': (unicode, unicode,
        """The name of the static library generated for a directory, if it needs to
        differ from the library code name.

        Implies FORCE_SHARED_LIB.
        """),

    'IS_FRAMEWORK': (bool, bool,
        """Whether the library to build should be built as a framework on OSX.

        This implies the name of the library won't be prefixed nor suffixed.
        Implies FORCE_SHARED_LIB.
        """),

    'STATIC_LIBRARY_NAME': (unicode, unicode,
        """The name of the static library generated for a directory, if it needs to
        differ from the library code name.

        Implies FORCE_STATIC_LIB.
        """),

    'USE_LIBS': (StrictOrderingOnAppendList, list,
        """List of libraries to link to programs and libraries.
        """),

    'HOST_USE_LIBS': (StrictOrderingOnAppendList, list,
        """List of libraries to link to host programs and libraries.
        """),

    'HOST_OS_LIBS': (List, list,
        """List of system libraries for host programs and libraries.
        """),

    'LOCAL_INCLUDES': (ContextDerivedTypedList(Path, StrictOrderingOnAppendList), list,
        """Additional directories to be searched for include files by the compiler.
        """),

    'NO_PGO': (bool, bool,
        """Whether profile-guided optimization is disable in this directory.
        """),

    'NO_VISIBILITY_FLAGS': (bool, bool,
        """Build sources listed in this file without VISIBILITY_FLAGS.
        """),

    'OS_LIBS': (List, list,
        """System link libraries.

        This variable contains a list of system libaries to link against.
        """),
    'RCFILE': (unicode, unicode,
        """The program .rc file.

        This variable can only be used on Windows.
        """),

    'RESFILE': (unicode, unicode,
        """The program .res file.

        This variable can only be used on Windows.
        """),

    'RCINCLUDE': (unicode, unicode,
        """The resource script file to be included in the default .res file.

        This variable can only be used on Windows.
        """),

    'DEFFILE': (unicode, unicode,
        """The program .def (module definition) file.

        This variable can only be used on Windows.
        """),

    'LD_VERSION_SCRIPT': (unicode, unicode,
        """The linker version script for shared libraries.

        This variable can only be used on Linux.
        """),

    'SYMBOLS_FILE': (Path, unicode,
        """A file containing a list of symbols to export from a shared library.

        The given file contains a list of symbols to be exported, and is
        preprocessed.
        A special marker "@DATA@" must be added after a symbol name if it
        points to data instead of code, so that the Windows linker can treat
        them correctly.
        """),

    'BRANDING_FILES': (ContextDerivedTypedHierarchicalStringList(Path), list,
        """List of files to be installed into the branding directory.

        ``BRANDING_FILES`` will copy (or symlink, if the platform supports it)
        the contents of its files to the ``dist/branding`` directory. Files that
        are destined for a subdirectory can be specified by accessing a field.
        For example, to export ``foo.png`` to the top-level directory and
        ``bar.png`` to the directory ``images/subdir``, append to
        ``BRANDING_FILES`` like so::

           BRANDING_FILES += ['foo.png']
           BRANDING_FILES.images.subdir += ['bar.png']
        """),

    'SIMPLE_PROGRAMS': (StrictOrderingOnAppendList, list,
        """Compile a list of executable names.

        Each name in this variable corresponds to an executable built from the
        corresponding source file with the same base name.

        If the configuration token ``BIN_SUFFIX`` is set, its value will be
        automatically appended to each name. If a name already ends with
        ``BIN_SUFFIX``, the name will remain unchanged.
        """),

    'SONAME': (unicode, unicode,
        """The soname of the shared object currently being linked

        soname is the "logical name" of a shared object, often used to provide
        version backwards compatibility. This variable makes sense only for
        shared objects, and is supported only on some unix platforms.
        """),

    'HOST_SIMPLE_PROGRAMS': (StrictOrderingOnAppendList, list,
        """Compile a list of host executable names.

        Each name in this variable corresponds to a hosst executable built
        from the corresponding source file with the same base name.

        If the configuration token ``HOST_BIN_SUFFIX`` is set, its value will
        be automatically appended to each name. If a name already ends with
        ``HOST_BIN_SUFFIX``, the name will remain unchanged.
        """),

    'RUST_PROGRAMS': (StrictOrderingOnAppendList, list,
        """Compile a list of Rust host executable names.

        Each name in this variable corresponds to an executable built from
        the Cargo.toml in the same directory.
        """),

    'HOST_RUST_PROGRAMS': (StrictOrderingOnAppendList, list,
        """Compile a list of Rust executable names.

        Each name in this variable corresponds to an executable built from
        the Cargo.toml in the same directory.
        """),

    'CONFIGURE_SUBST_FILES': (ContextDerivedTypedList(SourcePath, StrictOrderingOnAppendList), list,
        """Output files that will be generated using configure-like substitution.

        This is a substitute for ``AC_OUTPUT`` in autoconf. For each path in this
        list, we will search for a file in the srcdir having the name
        ``{path}.in``. The contents of this file will be read and variable
        patterns like ``@foo@`` will be substituted with the values of the
        ``AC_SUBST`` variables declared during configure.
        """),

    'CONFIGURE_DEFINE_FILES': (ContextDerivedTypedList(SourcePath, StrictOrderingOnAppendList), list,
        """Output files generated from configure/config.status.

        This is a substitute for ``AC_CONFIG_HEADER`` in autoconf. This is very
        similar to ``CONFIGURE_SUBST_FILES`` except the generation logic takes
        into account the values of ``AC_DEFINE`` instead of ``AC_SUBST``.
        """),

    'EXPORTS': (ContextDerivedTypedHierarchicalStringList(Path), list,
        """List of files to be exported, and in which subdirectories.

        ``EXPORTS`` is generally used to list the include files to be exported to
        ``dist/include``, but it can be used for other files as well. This variable
        behaves as a list when appending filenames for export in the top-level
        directory. Files can also be appended to a field to indicate which
        subdirectory they should be exported to. For example, to export
        ``foo.h`` to the top-level directory, and ``bar.h`` to ``mozilla/dom/``,
        append to ``EXPORTS`` like so::

           EXPORTS += ['foo.h']
           EXPORTS.mozilla.dom += ['bar.h']

        Entries in ``EXPORTS`` are paths, so objdir paths may be used, but
        any files listed from the objdir must also be listed in
        ``GENERATED_FILES``.
        """),

    'PROGRAM' : (unicode, unicode,
        """Compiled executable name.

        If the configuration token ``BIN_SUFFIX`` is set, its value will be
        automatically appended to ``PROGRAM``. If ``PROGRAM`` already ends with
        ``BIN_SUFFIX``, ``PROGRAM`` will remain unchanged.
        """),

    'HOST_PROGRAM' : (unicode, unicode,
        """Compiled host executable name.

        If the configuration token ``HOST_BIN_SUFFIX`` is set, its value will be
        automatically appended to ``HOST_PROGRAM``. If ``HOST_PROGRAM`` already
        ends with ``HOST_BIN_SUFFIX``, ``HOST_PROGRAM`` will remain unchanged.
        """),

    'DIST_INSTALL': (Enum(None, False, True), bool,
        """Whether to install certain files into the dist directory.

        By default, some files types are installed in the dist directory, and
        some aren't. Set this variable to True to force the installation of
        some files that wouldn't be installed by default. Set this variable to
        False to force to not install some files that would be installed by
        default.

        This is confusing for historical reasons, but eventually, the behavior
        will be made explicit.
        """),

    'JAR_MANIFESTS': (ContextDerivedTypedList(SourcePath, StrictOrderingOnAppendList), list,
        """JAR manifest files that should be processed as part of the build.

        JAR manifests are files in the tree that define how to package files
        into JARs and how chrome registration is performed. For more info,
        see :ref:`jar_manifests`.
        """),

    # IDL Generation.
    'XPIDL_SOURCES': (StrictOrderingOnAppendList, list,
        """XPCOM Interface Definition Files (xpidl).

        This is a list of files that define XPCOM interface definitions.
        Entries must be files that exist. Entries are almost certainly ``.idl``
        files.
        """),

    'XPIDL_MODULE': (unicode, unicode,
        """XPCOM Interface Definition Module Name.

        This is the name of the ``.xpt`` file that is created by linking
        ``XPIDL_SOURCES`` together. If unspecified, it defaults to be the same
        as ``MODULE``.
        """),

    'XPIDL_NO_MANIFEST': (bool, bool,
        """Indicate that the XPIDL module should not be added to a manifest.

        This flag exists primarily to prevent test-only XPIDL modules from being
        added to the application's chrome manifest. Most XPIDL modules should
        not use this flag.
        """),

    'IPDL_SOURCES': (StrictOrderingOnAppendList, list,
        """IPDL source files.

        These are ``.ipdl`` files that will be parsed and converted to
        ``.cpp`` files.
        """),

    'WEBIDL_FILES': (StrictOrderingOnAppendList, list,
        """WebIDL source files.

        These will be parsed and converted to ``.cpp`` and ``.h`` files.
        """),

    'GENERATED_EVENTS_WEBIDL_FILES': (StrictOrderingOnAppendList, list,
        """WebIDL source files for generated events.

        These will be parsed and converted to ``.cpp`` and ``.h`` files.
        """),

    'TEST_WEBIDL_FILES': (StrictOrderingOnAppendList, list,
         """Test WebIDL source files.

         These will be parsed and converted to ``.cpp`` and ``.h`` files
         if tests are enabled.
         """),

    'GENERATED_WEBIDL_FILES': (StrictOrderingOnAppendList, list,
         """Generated WebIDL source files.

         These will be generated from some other files.
         """),

    'PREPROCESSED_TEST_WEBIDL_FILES': (StrictOrderingOnAppendList, list,
         """Preprocessed test WebIDL source files.

         These will be preprocessed, then parsed and converted to .cpp
         and ``.h`` files if tests are enabled.
         """),

    'PREPROCESSED_WEBIDL_FILES': (StrictOrderingOnAppendList, list,
         """Preprocessed WebIDL source files.

         These will be preprocessed before being parsed and converted.
         """),

    'WEBIDL_EXAMPLE_INTERFACES': (StrictOrderingOnAppendList, list,
        """Names of example WebIDL interfaces to build as part of the build.

        Names in this list correspond to WebIDL interface names defined in
        WebIDL files included in the build from one of the \*WEBIDL_FILES
        variables.
        """),

    # Test declaration.
    'A11Y_MANIFESTS': (ManifestparserManifestList, list,
        """List of manifest files defining a11y tests.
        """),

    'BROWSER_CHROME_MANIFESTS': (ManifestparserManifestList, list,
        """List of manifest files defining browser chrome tests.
        """),

    'JETPACK_PACKAGE_MANIFESTS': (ManifestparserManifestList, list,
        """List of manifest files defining jetpack package tests.
        """),

    'JETPACK_ADDON_MANIFESTS': (ManifestparserManifestList, list,
        """List of manifest files defining jetpack addon tests.
        """),

    'ANDROID_INSTRUMENTATION_MANIFESTS': (ManifestparserManifestList, list,
        """List of manifest files defining Android instrumentation tests.
        """),

    'FIREFOX_UI_FUNCTIONAL_MANIFESTS': (ManifestparserManifestList, list,
        """List of manifest files defining firefox-ui-functional tests.
        """),

    'FIREFOX_UI_UPDATE_MANIFESTS': (ManifestparserManifestList, list,
        """List of manifest files defining firefox-ui-update tests.
        """),

    'PUPPETEER_FIREFOX_MANIFESTS': (ManifestparserManifestList, list,
        """List of manifest files defining puppeteer unit tests for Firefox.
        """),

    'MARIONETTE_LAYOUT_MANIFESTS': (ManifestparserManifestList, list,
        """List of manifest files defining marionette-layout tests.
        """),

    'MARIONETTE_UNIT_MANIFESTS': (ManifestparserManifestList, list,
        """List of manifest files defining marionette-unit tests.
        """),

    'MARIONETTE_WEBAPI_MANIFESTS': (ManifestparserManifestList, list,
        """List of manifest files defining marionette-webapi tests.
        """),

    'METRO_CHROME_MANIFESTS': (ManifestparserManifestList, list,
        """List of manifest files defining metro browser chrome tests.
        """),

    'MOCHITEST_CHROME_MANIFESTS': (ManifestparserManifestList, list,
        """List of manifest files defining mochitest chrome tests.
        """),

    'MOCHITEST_MANIFESTS': (ManifestparserManifestList, list,
        """List of manifest files defining mochitest tests.
        """),

    'REFTEST_MANIFESTS': (ReftestManifestList, list,
        """List of manifest files defining reftests.

        These are commonly named reftest.list.
        """),

    'CRASHTEST_MANIFESTS': (ReftestManifestList, list,
        """List of manifest files defining crashtests.

        These are commonly named crashtests.list.
        """),

    'WEB_PLATFORM_TESTS_MANIFESTS': (WptManifestList, list,
        """List of (manifest_path, test_path) defining web-platform-tests.
        """),

    'WEBRTC_SIGNALLING_TEST_MANIFESTS': (ManifestparserManifestList, list,
        """List of manifest files defining WebRTC signalling tests.
        """),

    'XPCSHELL_TESTS_MANIFESTS': (ManifestparserManifestList, list,
        """List of manifest files defining xpcshell tests.
        """),

    'PYTHON_UNITTEST_MANIFESTS': (ManifestparserManifestList, list,
        """List of manifest files defining python unit tests.
        """),


    # The following variables are used to control the target of installed files.
    'XPI_NAME': (unicode, unicode,
        """The name of an extension XPI to generate.

        When this variable is present, the results of this directory will end up
        being packaged into an extension instead of the main dist/bin results.
        """),

    'DIST_SUBDIR': (unicode, unicode,
        """The name of an alternate directory to install files to.

        When this variable is present, the results of this directory will end up
        being placed in the $(DIST_SUBDIR) subdirectory of where it would
        otherwise be placed.
        """),

    'FINAL_TARGET': (FinalTargetValue, unicode,
        """The name of the directory to install targets to.

        The directory is relative to the top of the object directory. The
        default value is dependent on the values of XPI_NAME and DIST_SUBDIR. If
        neither are present, the result is dist/bin. If XPI_NAME is present, the
        result is dist/xpi-stage/$(XPI_NAME). If DIST_SUBDIR is present, then
        the $(DIST_SUBDIR) directory of the otherwise default value is used.
        """),

    'USE_EXTENSION_MANIFEST': (bool, bool,
        """Controls the name of the manifest for JAR files.

        By default, the name of the manifest is ${JAR_MANIFEST}.manifest.
        Setting this variable to ``True`` changes the name of the manifest to
        chrome.manifest.
        """),

    'NO_JS_MANIFEST': (bool, bool,
        """Explicitly disclaims responsibility for manifest listing in EXTRA_COMPONENTS.

        Normally, if you have .js files listed in ``EXTRA_COMPONENTS`` or
        ``EXTRA_PP_COMPONENTS``, you are expected to have a corresponding
        .manifest file to go with those .js files.  Setting ``NO_JS_MANIFEST``
        indicates that the relevant .manifest file and entries for those .js
        files are elsehwere (jar.mn, for instance) and this state of affairs
        is OK.
        """),

    'GYP_DIRS': (StrictOrderingOnAppendListWithFlagsFactory({
            'variables': dict,
            'input': unicode,
            'sandbox_vars': dict,
            'no_chromium': bool,
            'no_unified': bool,
            'non_unified_sources': StrictOrderingOnAppendList,
            'action_overrides': dict,
        }), list,
        """Defines a list of object directories handled by gyp configurations.

        Elements of this list give the relative object directory. For each
        element of the list, GYP_DIRS may be accessed as a dictionary
        (GYP_DIRS[foo]). The object this returns has attributes that need to be
        set to further specify gyp processing:
            - input, gives the path to the root gyp configuration file for that
              object directory.
            - variables, a dictionary containing variables and values to pass
              to the gyp processor.
            - sandbox_vars, a dictionary containing variables and values to
              pass to the mozbuild processor on top of those derived from gyp
              configuration.
            - no_chromium, a boolean which if set to True disables some
              special handling that emulates gyp_chromium.
            - no_unified, a boolean which if set to True disables source
              file unification entirely.
            - non_unified_sources, a list containing sources files, relative to
              the current moz.build, that should be excluded from source file
              unification.
            - action_overrides, a dict of action_name to values of the `script`
              attribute to use for GENERATED_FILES for the specified action.

        Typical use looks like:
            GYP_DIRS += ['foo', 'bar']
            GYP_DIRS['foo'].input = 'foo/foo.gyp'
            GYP_DIRS['foo'].variables = {
                'foo': 'bar',
                (...)
            }
            (...)
        """),

    'SPHINX_TREES': (dict, dict,
        """Describes what the Sphinx documentation tree will look like.

        Keys are relative directories inside the final Sphinx documentation
        tree to install files into. Values are directories (relative to this
        file) whose content to copy into the Sphinx documentation tree.
        """),

    'SPHINX_PYTHON_PACKAGE_DIRS': (StrictOrderingOnAppendList, list,
        """Directories containing Python packages that Sphinx documents.
        """),

    'CFLAGS': (List, list,
        """Flags passed to the C compiler for all of the C source files
           declared in this directory.

           Note that the ordering of flags matters here, these flags will be
           added to the compiler's command line in the same order as they
           appear in the moz.build file.
        """),

    'CXXFLAGS': (List, list,
        """Flags passed to the C++ compiler for all of the C++ source files
           declared in this directory.

           Note that the ordering of flags matters here; these flags will be
           added to the compiler's command line in the same order as they
           appear in the moz.build file.
        """),

    'HOST_DEFINES': (InitializedDefines, dict,
        """Dictionary of compiler defines to declare for host compilation.
        See ``DEFINES`` for specifics.
        """),

    'CMFLAGS': (List, list,
        """Flags passed to the Objective-C compiler for all of the Objective-C
           source files declared in this directory.

           Note that the ordering of flags matters here; these flags will be
           added to the compiler's command line in the same order as they
           appear in the moz.build file.
        """),

    'CMMFLAGS': (List, list,
        """Flags passed to the Objective-C++ compiler for all of the
           Objective-C++ source files declared in this directory.

           Note that the ordering of flags matters here; these flags will be
           added to the compiler's command line in the same order as they
           appear in the moz.build file.
        """),

    'ASFLAGS': (List, list,
        """Flags passed to the assembler for all of the assembly source files
           declared in this directory.

           Note that the ordering of flags matters here; these flags will be
           added to the assembler's command line in the same order as they
           appear in the moz.build file.
        """),

    'HOST_CFLAGS': (List, list,
        """Flags passed to the host C compiler for all of the C source files
           declared in this directory.

           Note that the ordering of flags matters here, these flags will be
           added to the compiler's command line in the same order as they
           appear in the moz.build file.
        """),

    'HOST_CXXFLAGS': (List, list,
        """Flags passed to the host C++ compiler for all of the C++ source files
           declared in this directory.

           Note that the ordering of flags matters here; these flags will be
           added to the compiler's command line in the same order as they
           appear in the moz.build file.
        """),

    'LDFLAGS': (List, list,
        """Flags passed to the linker when linking all of the libraries and
           executables declared in this directory.

           Note that the ordering of flags matters here; these flags will be
           added to the linker's command line in the same order as they
           appear in the moz.build file.
        """),

    'EXTRA_DSO_LDOPTS': (List, list,
        """Flags passed to the linker when linking a shared library.

           Note that the ordering of flags matter here, these flags will be
           added to the linker's command line in the same order as they
           appear in the moz.build file.
        """),

    'WIN32_EXE_LDFLAGS': (List, list,
        """Flags passed to the linker when linking a Windows .exe executable
           declared in this directory.

           Note that the ordering of flags matter here, these flags will be
           added to the linker's command line in the same order as they
           appear in the moz.build file.

           This variable only has an effect on Windows.
        """),

    'TEST_HARNESS_FILES': (ContextDerivedTypedHierarchicalStringList(Path), list,
        """List of files to be installed for test harnesses.

        ``TEST_HARNESS_FILES`` can be used to install files to any directory
        under $objdir/_tests. Files can be appended to a field to indicate
        which subdirectory they should be exported to. For example,
        to export ``foo.py`` to ``_tests/foo``, append to
        ``TEST_HARNESS_FILES`` like so::
           TEST_HARNESS_FILES.foo += ['foo.py']

        Files from topsrcdir and the objdir can also be installed by prefixing
        the path(s) with a '/' character and a '!' character, respectively::
           TEST_HARNESS_FILES.path += ['/build/bar.py', '!quux.py']
        """),

    'NO_EXPAND_LIBS': (bool, bool,
        """Forces to build a real static library, and no corresponding fake
           library.
        """),

    'NO_COMPONENTS_MANIFEST': (bool, bool,
        """Do not create a binary-component manifest entry for the
        corresponding XPCOMBinaryComponent.
        """),

    'USE_YASM': (bool, bool,
        """Use the yasm assembler to assemble assembly files from SOURCES.

        By default, the build will use the toolchain assembler, $(AS), to
        assemble source files in assembly language (.s or .asm files). Setting
        this value to ``True`` will cause it to use yasm instead.

        If yasm is not available on this system, or does not support the
        current target architecture, an error will be raised.
        """),
}

# Sanity check: we don't want any variable above to have a list as storage type.
for name, (storage_type, input_types, docs) in VARIABLES.items():
    if storage_type == list:
        raise RuntimeError('%s has a "list" storage type. Use "List" instead.'
            % name)

# Set of variables that are only allowed in templates:
TEMPLATE_VARIABLES = {
    'CPP_UNIT_TESTS',
    'FORCE_SHARED_LIB',
    'HOST_PROGRAM',
    'HOST_LIBRARY_NAME',
    'HOST_SIMPLE_PROGRAMS',
    'IS_FRAMEWORK',
    'LIBRARY_NAME',
    'PROGRAM',
    'SIMPLE_PROGRAMS',
}

# Add a note to template variable documentation.
for name in TEMPLATE_VARIABLES:
    if name not in VARIABLES:
        raise RuntimeError('%s is in TEMPLATE_VARIABLES but not in VARIABLES.'
            % name)
    storage_type, input_types, docs = VARIABLES[name]
    docs += 'This variable is only available in templates.\n'
    VARIABLES[name] = (storage_type, input_types, docs)


# The set of functions exposed to the sandbox.
#
# Each entry is a tuple of:
#
#  (function returning the corresponding function from a given sandbox,
#   (argument types), docs)
#
# The first element is an attribute on Sandbox that should be a function type.
#
FUNCTIONS = {
    'include': (lambda self: self._include, (SourcePath,),
        """Include another mozbuild file in the context of this one.

        This is similar to a ``#include`` in C languages. The filename passed to
        the function will be read and its contents will be evaluated within the
        context of the calling file.

        If a relative path is given, it is evaluated as relative to the file
        currently being processed. If there is a chain of multiple include(),
        the relative path computation is from the most recent/active file.

        If an absolute path is given, it is evaluated from ``TOPSRCDIR``. In
        other words, ``include('/foo')`` references the path
        ``TOPSRCDIR + '/foo'``.

        Example usage
        ^^^^^^^^^^^^^

        Include ``sibling.build`` from the current directory.::

           include('sibling.build')

        Include ``foo.build`` from a path within the top source directory::

           include('/elsewhere/foo.build')
        """),

    'add_java_jar': (lambda self: self._add_java_jar, (str,),
        """Declare a Java JAR target to be built.

        This is the supported way to populate the JAVA_JAR_TARGETS
        variable.

        The parameters are:
        * dest - target name, without the trailing .jar. (required)

        This returns a rich Java JAR type, described at
        :py:class:`mozbuild.frontend.data.JavaJarData`.
        """),

    'export': (lambda self: self._export, (str,),
        """Make the specified variable available to all child directories.

        The variable specified by the argument string is added to the
        environment of all directories specified in the DIRS and TEST_DIRS
        variables. If those directories themselves have child directories,
        the variable will be exported to all of them.

        The value used for the variable is the final value at the end of the
        moz.build file, so it is possible (but not recommended style) to place
        the export before the definition of the variable.

        This function is limited to the upper-case variables that have special
        meaning in moz.build files.

        NOTE: Please consult with a build peer before adding a new use of this
        function.

        Example usage
        ^^^^^^^^^^^^^

        To make all children directories install as the given extension::

          XPI_NAME = 'cool-extension'
          export('XPI_NAME')
        """),

    'warning': (lambda self: self._warning, (str,),
        """Issue a warning.

        Warnings are string messages that are printed during execution.

        Warnings are ignored during execution.
        """),

    'error': (lambda self: self._error, (str,),
        """Issue a fatal error.

        If this function is called, processing is aborted immediately.
        """),

    'template': (lambda self: self._template_decorator, (FunctionType,),
        """Decorator for template declarations.

        Templates are a special kind of functions that can be declared in
        mozbuild files. Uppercase variables assigned in the function scope
        are considered to be the result of the template.

        Contrary to traditional python functions:
           - return values from template functions are ignored,
           - template functions don't have access to the global scope.

        Example template
        ^^^^^^^^^^^^^^^^

        The following ``Program`` template sets two variables ``PROGRAM`` and
        ``USE_LIBS``. ``PROGRAM`` is set to the argument given on the template
        invocation, and ``USE_LIBS`` to contain "mozglue"::

           @template
           def Program(name):
               PROGRAM = name
               USE_LIBS += ['mozglue']

        Template invocation
        ^^^^^^^^^^^^^^^^^^^

        A template is invoked in the form of a function call::

           Program('myprog')

        The result of the template, being all the uppercase variable it sets
        is mixed to the existing set of variables defined in the mozbuild file
        invoking the template::

           FINAL_TARGET = 'dist/other'
           USE_LIBS += ['mylib']
           Program('myprog')
           USE_LIBS += ['otherlib']

        The above mozbuild results in the following variables set:

           - ``FINAL_TARGET`` is 'dist/other'
           - ``USE_LIBS`` is ['mylib', 'mozglue', 'otherlib']
           - ``PROGRAM`` is 'myprog'

        """),
}


TestDirsPlaceHolder = List()


# Special variables. These complement VARIABLES.
#
# Each entry is a tuple of:
#
#  (function returning the corresponding value from a given context, type, docs)
#
SPECIAL_VARIABLES = {
    'TOPSRCDIR': (lambda context: context.config.topsrcdir, str,
        """Constant defining the top source directory.

        The top source directory is the parent directory containing the source
        code and all build files. It is typically the root directory of a
        cloned repository.
        """),

    'TOPOBJDIR': (lambda context: context.config.topobjdir, str,
        """Constant defining the top object directory.

        The top object directory is the parent directory which will contain
        the output of the build. This is commonly referred to as "the object
        directory."
        """),

    'RELATIVEDIR': (lambda context: context.relsrcdir, str,
        """Constant defining the relative path of this file.

        The relative path is from ``TOPSRCDIR``. This is defined as relative
        to the main file being executed, regardless of whether additional
        files have been included using ``include()``.
        """),

    'SRCDIR': (lambda context: context.srcdir, str,
        """Constant defining the source directory of this file.

        This is the path inside ``TOPSRCDIR`` where this file is located. It
        is the same as ``TOPSRCDIR + RELATIVEDIR``.
        """),

    'OBJDIR': (lambda context: context.objdir, str,
        """The path to the object directory for this file.

        Is is the same as ``TOPOBJDIR + RELATIVEDIR``.
        """),

    'CONFIG': (lambda context: ReadOnlyKeyedDefaultDict(
            lambda key: context.config.substs_unicode.get(key)), dict,
        """Dictionary containing the current configuration variables.

        All the variables defined by the configuration system are available
        through this object. e.g. ``ENABLE_TESTS``, ``CFLAGS``, etc.

        Values in this container are read-only. Attempts at changing values
        will result in a run-time error.

        Access to an unknown variable will return None.
        """),

    'EXTRA_COMPONENTS': (lambda context: context['FINAL_TARGET_FILES'].components._strings, list,
        """Additional component files to distribute.

       This variable contains a list of files to copy into
       ``$(FINAL_TARGET)/components/``.
        """),

    'EXTRA_PP_COMPONENTS': (lambda context: context['FINAL_TARGET_PP_FILES'].components._strings, list,
        """Javascript XPCOM files.

       This variable contains a list of files to preprocess.  Generated
       files will be installed in the ``/components`` directory of the distribution.
        """),

    'JS_PREFERENCE_FILES': (lambda context: context['FINAL_TARGET_FILES'].defaults.pref._strings, list,
        """Exported javascript files.

        A list of files copied into the dist directory for packaging and installation.
        Path will be defined for gre or application prefs dir based on what is building.
        """),

    'JS_PREFERENCE_PP_FILES': (lambda context: context['FINAL_TARGET_PP_FILES'].defaults.pref._strings, list,
        """Like JS_PREFERENCE_FILES, preprocessed..
        """),

    'RESOURCE_FILES': (lambda context: context['FINAL_TARGET_FILES'].res, list,
        """List of resources to be exported, and in which subdirectories.

        ``RESOURCE_FILES`` is used to list the resource files to be exported to
        ``dist/bin/res``, but it can be used for other files as well. This variable
        behaves as a list when appending filenames for resources in the top-level
        directory. Files can also be appended to a field to indicate which
        subdirectory they should be exported to. For example, to export
        ``foo.res`` to the top-level directory, and ``bar.res`` to ``fonts/``,
        append to ``RESOURCE_FILES`` like so::

           RESOURCE_FILES += ['foo.res']
           RESOURCE_FILES.fonts += ['bar.res']
        """),

    'EXTRA_JS_MODULES': (lambda context: context['FINAL_TARGET_FILES'].modules, list,
        """Additional JavaScript files to distribute.

        This variable contains a list of files to copy into
        ``$(FINAL_TARGET)/modules.
        """),

    'EXTRA_PP_JS_MODULES': (lambda context: context['FINAL_TARGET_PP_FILES'].modules, list,
        """Additional JavaScript files to distribute.

        This variable contains a list of files to copy into
        ``$(FINAL_TARGET)/modules``, after preprocessing.
        """),

    'TESTING_JS_MODULES': (lambda context: context['TEST_HARNESS_FILES'].modules, list,
        """JavaScript modules to install in the test-only destination.

        Some JavaScript modules (JSMs) are test-only and not distributed
        with Firefox. This variable defines them.

        To install modules in a subdirectory, use properties of this
        variable to control the final destination. e.g.

        ``TESTING_JS_MODULES.foo += ['module.jsm']``.
        """),

    'TEST_DIRS': (lambda context: context['DIRS'] if context.config.substs.get('ENABLE_TESTS')
                                  else TestDirsPlaceHolder, list,
        """Like DIRS but only for directories that contain test-only code.

        If tests are not enabled, this variable will be ignored.

        This variable may go away once the transition away from Makefiles is
        complete.
        """),
}

# Deprecation hints.
DEPRECATION_HINTS = {
    'CPP_UNIT_TESTS': '''
        Please use'

            CppUnitTests(['foo', 'bar'])

        instead of

            CPP_UNIT_TESTS += ['foo', 'bar']
        ''',

    'HOST_PROGRAM': '''
        Please use

            HostProgram('foo')

        instead of

            HOST_PROGRAM = 'foo'
        ''',

    'HOST_LIBRARY_NAME': '''
        Please use

            HostLibrary('foo')

        instead of

            HOST_LIBRARY_NAME = 'foo'
        ''',

    'HOST_SIMPLE_PROGRAMS': '''
        Please use

            HostSimplePrograms(['foo', 'bar'])

        instead of

            HOST_SIMPLE_PROGRAMS += ['foo', 'bar']"
        ''',

    'LIBRARY_NAME': '''
        Please use

            Library('foo')

        instead of

            LIBRARY_NAME = 'foo'
        ''',

    'PROGRAM': '''
        Please use

            Program('foo')

        instead of

            PROGRAM = 'foo'"
        ''',

    'SIMPLE_PROGRAMS': '''
        Please use

            SimplePrograms(['foo', 'bar'])

        instead of

            SIMPLE_PROGRAMS += ['foo', 'bar']"
        ''',

    'FORCE_SHARED_LIB': '''
        Please use

            SharedLibrary('foo')

        instead of

            Library('foo') [ or LIBRARY_NAME = 'foo' ]
            FORCE_SHARED_LIB = True
        ''',

    'IS_FRAMEWORK': '''
        Please use

            Framework('foo')

        instead of

            Library('foo') [ or LIBRARY_NAME = 'foo' ]
            IS_FRAMEWORK = True
        ''',

    'TOOL_DIRS': 'Please use the DIRS variable instead.',

    'TEST_TOOL_DIRS': 'Please use the TEST_DIRS variable instead.',

    'PARALLEL_DIRS': 'Please use the DIRS variable instead.',

    'NO_DIST_INSTALL': '''
        Please use

            DIST_INSTALL = False

        instead of

            NO_DIST_INSTALL = True
    ''',

    'GENERATED_SOURCES': '''
        Please use

            SOURCES += [ '!foo.cpp' ]

        instead of

            GENERATED_SOURCES += [ 'foo.cpp']
    ''',

    'GENERATED_INCLUDES': '''
        Please use

            LOCAL_INCLUDES += [ '!foo' ]

        instead of

            GENERATED_INCLUDES += [ 'foo' ]
    ''',

    'DIST_FILES': '''
        Please use

            FINAL_TARGET_PP_FILES += [ 'foo' ]

        instead of

            DIST_FILES += [ 'foo' ]
    ''',
}

# Make sure that all template variables have a deprecation hint.
for name in TEMPLATE_VARIABLES:
    if name not in DEPRECATION_HINTS:
        raise RuntimeError('Missing deprecation hint for %s' % name)
