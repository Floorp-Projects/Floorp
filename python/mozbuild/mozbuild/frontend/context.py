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

from __future__ import unicode_literals

import os

from collections import OrderedDict
from mozbuild.util import (
    HierarchicalStringList,
    HierarchicalStringListWithFlagsFactory,
    KeyedDefaultDict,
    List,
    memoize,
    memoized_property,
    ReadOnlyKeyedDefaultDict,
    StrictOrderingOnAppendList,
    StrictOrderingOnAppendListWithFlagsFactory,
    TypedList,
    TypedNamedTuple,
)
import mozpack.path as mozpath
from types import FunctionType
from UserString import UserString

import itertools


class ContextDerivedValue(object):
    """Classes deriving from this one receive a special treatment in a
    Context. See Context documentation.
    """


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
    def __init__(self, allowed_variables={}, config=None):
        self._allowed_variables = allowed_variables
        self.main_path = None
        self.current_path = None
        # There aren't going to be enough paths for the performance of scanning
        # a list to be a problem.
        self._all_paths = []
        self.config = config
        self.execution_time = 0
        self._sandbox = None
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
        stored_type, input_type, docs, tier = \
            self._allowed_variables.get(key, (None, None, None, None))

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

    def resolve_path(self, path):
        """Resolves a path using moz.build conventions.

        Paths may be relative to the current srcdir or objdir, or to the
        environment's topsrcdir or topobjdir.  Different resolution contexts
        are denoted by characters at the beginning of the path:

            * '/' - relative to topsrcdir;
            * '!/' - relative to topobjdir;
            * '!' - relative to objdir; and
            * any other character - relative to srcdir.
        """
        if path.startswith('/'):
            resolved = mozpath.join(self.config.topsrcdir, path[1:])
        elif path.startswith('!/'):
            resolved = mozpath.join(self.config.topobjdir, path[2:])
        elif path.startswith('!'):
            resolved = mozpath.join(self.objdir, path[1:])
        else:
            resolved = mozpath.join(self.srcdir, path)

        return mozpath.normpath(resolved)

    @staticmethod
    def is_objdir_path(path):
        return path[0] == '!'

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

    def get_affected_tiers(self):
        """Returns the list of tiers affected by the variables set in the
        context.
        """
        tiers = (VARIABLES[key][3] for key in self if key in VARIABLES)
        return set(tier for tier in tiers if tier)


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


class SourcePath(ContextDerivedValue, UserString):
    """Stores and resolves a source path relative to a given context

    This class is used as a backing type for some of the sandbox variables.
    It expresses paths relative to a context. Paths starting with a '/'
    are considered relative to the topsrcdir, and other paths relative
    to the current source directory for the associated context.
    """
    def __new__(cls, context, value=None):
        if not isinstance(context, Context) and value is None:
            return unicode(context)
        return super(SourcePath, cls).__new__(cls)

    def __init__(self, context, value=None):
        self.context = context
        self.srcdir = context.srcdir
        self.value = value

    @memoized_property
    def data(self):
        """Serializes the path for UserString."""
        if self.value.startswith('/'):
            ret = None
            # If the path starts with a '/' and is actually relative to an
            # external source dir, use that as base instead of topsrcdir.
            if self.context.config.external_source_dir:
                ret = mozpath.join(self.context.config.external_source_dir,
                    self.value[1:])
            if not ret or not os.path.exists(ret):
                ret = mozpath.join(self.context.config.topsrcdir,
                    self.value[1:])
        else:
            ret = mozpath.join(self.srcdir, self.value)
        return mozpath.normpath(ret)

    def __unicode__(self):
        # UserString doesn't implement a __unicode__ function at all, so add
        # ours.
        return self.data

    @memoized_property
    def translated(self):
        """Returns the corresponding path in the objdir.

        Ideally, we wouldn't need this function, but the fact that both source
        path under topsrcdir and the external source dir end up mixed in the
        objdir (aka pseudo-rework), this is needed.
        """
        if self.value.startswith('/'):
            ret = mozpath.join(self.context.config.topobjdir, self.value[1:])
        else:
            ret = mozpath.join(self.context.objdir, self.value)
        return mozpath.normpath(ret)

    def join(self, *p):
        """Lazy mozpath.join(self, *p), returning a new SourcePath instance.

        In an ideal world, this wouldn't be required, but with the
        external_source_dir business, and the fact that comm-central and
        mozilla-central have directories in common, resolving a SourcePath
        before doing mozpath.join doesn't work out properly.
        """
        return SourcePath(self.context, mozpath.join(self.value, *p))


@memoize
def ContextDerivedTypedList(type, base_class=List):
    """Specialized TypedList for use with ContextDerivedValue types.
    """
    assert issubclass(type, ContextDerivedValue)
    class _TypedList(ContextDerivedValue, TypedList(type, base_class)):
        def __init__(self, context, iterable=[]):
            class _Type(type):
                def __new__(cls, obj):
                    return type(context, obj)
            self.TYPE = _Type
            super(_TypedList, self).__init__(iterable)

    return _TypedList


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
            """, None),

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
            """, None),
    }

    def __init__(self, parent, pattern=None):
        super(Files, self).__init__(parent)
        self.pattern = pattern
        self.finalized = set()

    def __iadd__(self, other):
        assert isinstance(other, Files)

        for k, v in other.items():
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
#   (storage_type, input_types, docs, tier)
#
# Tier says for which specific tier the variable has an effect.
# Valid tiers are:
# - 'export'
# - 'libs': everything that is not built from C/C++/ObjC source and that has
#      traditionally been in the libs tier.
# - 'misc': like libs, but with parallel build. Eventually, everything that
#      currently is in libs should move here.
# A value of None means the variable has no direct effect on any tier.

VARIABLES = {
    # Variables controlling reading of other frontend files.
    'ANDROID_GENERATED_RESFILES': (StrictOrderingOnAppendList, list,
        """Android resource files generated as part of the build.

        This variable contains a list of files that are expected to be
        generated (often by preprocessing) into a 'res' directory as
        part of the build process, and subsequently merged into an APK
        file.
        """, 'export'),

    'ANDROID_RES_DIRS': (List, list,
        """Android resource directories.

        This variable contains a list of directories, each relative to
        the srcdir, containing static files to package into a 'res'
        directory and merge into an APK file.
        """, 'export'),

    'ANDROID_ECLIPSE_PROJECT_TARGETS': (dict, dict,
        """Defines Android Eclipse project targets.

        This variable should not be populated directly. Instead, it should
        populated by calling add_android_eclipse{_library}_project().
        """, 'export'),

    'SOURCES': (StrictOrderingOnAppendListWithFlagsFactory({'no_pgo': bool, 'flags': List}), list,
        """Source code files.

        This variable contains a list of source code files to compile.
        Accepts assembler, C, C++, Objective C/C++.
        """, None),

    'GENERATED_SOURCES': (StrictOrderingOnAppendList, list,
        """Generated source code files.

        This variable contains a list of generated source code files to
        compile. Accepts assembler, C, C++, Objective C/C++.
        """, None),

    'FILES_PER_UNIFIED_FILE': (int, int,
        """The number of source files to compile into each unified source file.

        """, 'None'),

    'UNIFIED_SOURCES': (StrictOrderingOnAppendList, list,
        """Source code files that can be compiled together.

        This variable contains a list of source code files to compile,
        that can be concatenated all together and built as a single source
        file. This can help make the build faster and reduce the debug info
        size.
        """, None),

    'GENERATED_UNIFIED_SOURCES': (StrictOrderingOnAppendList, list,
        """Generated source code files that can be compiled together.

        This variable contains a list of generated source code files to
        compile, that can be concatenated all together, with UNIFIED_SOURCES,
        and built as a single source file. This can help make the build faster
        and reduce the debug info size.
        """, None),

    'GENERATED_FILES': (StrictOrderingOnAppendListWithFlagsFactory({
                'script': unicode,
                'inputs': list }), list,
        """Generic generated files.

        This variable contains a list of files for the build system to
        generate at export time. The generation method may be declared
        with optional ``script`` and ``inputs`` flags on individual entries.
        If the optional ``script`` flag is not present on an entry, it
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
        """, 'export'),

    'DEFINES': (OrderedDict, dict,
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
        """, None),

    'DELAYLOAD_DLLS': (List, list,
        """Delay-loaded DLLs.

        This variable contains a list of DLL files which the module being linked
        should load lazily.  This only has an effect when building with MSVC.
        """, None),

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
        """, None),

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
        """, 'misc'),

    'FINAL_TARGET_FILES': (HierarchicalStringList, list,
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
        """, None),

    'DISABLE_STL_WRAPPING': (bool, bool,
        """Disable the wrappers for STL which allow it to work with C++ exceptions
        disabled.
        """, None),

    'DIST_FILES': (StrictOrderingOnAppendList, list,
        """Additional files to place in ``FINAL_TARGET`` (typically ``dist/bin``).

        Unlike ``FINAL_TARGET_FILES``, these files are preprocessed.
        """, 'libs'),

    'EXTRA_COMPONENTS': (StrictOrderingOnAppendList, list,
        """Additional component files to distribute.

       This variable contains a list of files to copy into
       ``$(FINAL_TARGET)/components/``.
        """, 'misc'),

    'EXTRA_JS_MODULES': (HierarchicalStringList, list,
        """Additional JavaScript files to distribute.

        This variable contains a list of files to copy into
        ``$(FINAL_TARGET)/modules.
        """, 'misc'),

    'EXTRA_PP_JS_MODULES': (HierarchicalStringList, list,
        """Additional JavaScript files to distribute.

        This variable contains a list of files to copy into
        ``$(FINAL_TARGET)/modules``, after preprocessing.
        """, 'misc'),

    'TESTING_JS_MODULES': (HierarchicalStringList, list,
        """JavaScript modules to install in the test-only destination.

        Some JavaScript modules (JSMs) are test-only and not distributed
        with Firefox. This variable defines them.

        To install modules in a subdirectory, use properties of this
        variable to control the final destination. e.g.

        ``TESTING_JS_MODULES.foo += ['module.jsm']``.
        """, None),

    'EXTRA_PP_COMPONENTS': (StrictOrderingOnAppendList, list,
        """Javascript XPCOM files.

       This variable contains a list of files to preprocess.  Generated
       files will be installed in the ``/components`` directory of the distribution.
        """, 'misc'),

    'FINAL_LIBRARY': (unicode, unicode,
        """Library in which the objects of the current directory will be linked.

        This variable contains the name of a library, defined elsewhere with
        ``LIBRARY_NAME``, in which the objects of the current directory will be
        linked.
        """, None),

    'CPP_UNIT_TESTS': (StrictOrderingOnAppendList, list,
        """Compile a list of C++ unit test names.

        Each name in this variable corresponds to an executable built from the
        corresponding source file with the same base name.

        If the configuration token ``BIN_SUFFIX`` is set, its value will be
        automatically appended to each name. If a name already ends with
        ``BIN_SUFFIX``, the name will remain unchanged.
        """, None),

    'FAIL_ON_WARNINGS': (bool, bool,
        """Whether to treat warnings as errors.
        """, None),

    'FORCE_SHARED_LIB': (bool, bool,
        """Whether the library in this directory is a shared library.
        """, None),

    'FORCE_STATIC_LIB': (bool, bool,
        """Whether the library in this directory is a static library.
        """, None),

    'USE_STATIC_LIBS': (bool, bool,
        """Whether the code in this directory is a built against the static
        runtime library.

        This variable only has an effect when building with MSVC.
        """, None),

    'GENERATED_INCLUDES' : (StrictOrderingOnAppendList, list,
        """Directories generated by the build system to be searched for include
        files by the compiler.
        """, None),

    'HOST_SOURCES': (StrictOrderingOnAppendList, list,
        """Source code files to compile with the host compiler.

        This variable contains a list of source code files to compile.
        with the host compiler.
        """, None),

    'IS_COMPONENT': (bool, bool,
        """Whether the library contains a binary XPCOM component manifest.

        Implies FORCE_SHARED_LIB.
        """, None),

    'PYTHON_UNIT_TESTS': (StrictOrderingOnAppendList, list,
        """A list of python unit tests.
        """, None),

    'HOST_LIBRARY_NAME': (unicode, unicode,
        """Name of target library generated when cross compiling.
        """, None),

    'JAVA_JAR_TARGETS': (dict, dict,
        """Defines Java JAR targets to be built.

        This variable should not be populated directly. Instead, it should
        populated by calling add_java_jar().
        """, 'libs'),

    'JS_PREFERENCE_FILES': (StrictOrderingOnAppendList, list,
        """Exported javascript files.

        A list of files copied into the dist directory for packaging and installation.
        Path will be defined for gre or application prefs dir based on what is building.
        """, 'libs'),

    'LIBRARY_DEFINES': (OrderedDict, dict,
        """Dictionary of compiler defines to declare for the entire library.

        This variable works like DEFINES, except that declarations apply to all
        libraries that link into this library via FINAL_LIBRARY.
        """, None),

    'LIBRARY_NAME': (unicode, unicode,
        """The code name of the library generated for a directory.

        By default STATIC_LIBRARY_NAME and SHARED_LIBRARY_NAME take this name.
        In ``example/components/moz.build``,::

           LIBRARY_NAME = 'xpcomsample'

        would generate ``example/components/libxpcomsample.so`` on Linux, or
        ``example/components/xpcomsample.lib`` on Windows.
        """, None),

    'SHARED_LIBRARY_NAME': (unicode, unicode,
        """The name of the static library generated for a directory, if it needs to
        differ from the library code name.

        Implies FORCE_SHARED_LIB.
        """, None),

    'IS_FRAMEWORK': (bool, bool,
        """Whether the library to build should be built as a framework on OSX.

        This implies the name of the library won't be prefixed nor suffixed.
        Implies FORCE_SHARED_LIB.
        """, None),

    'STATIC_LIBRARY_NAME': (unicode, unicode,
        """The name of the static library generated for a directory, if it needs to
        differ from the library code name.

        Implies FORCE_STATIC_LIB.
        """, None),

    'USE_LIBS': (StrictOrderingOnAppendList, list,
        """List of libraries to link to programs and libraries.
        """, None),

    'HOST_USE_LIBS': (StrictOrderingOnAppendList, list,
        """List of libraries to link to host programs and libraries.
        """, None),

    'HOST_OS_LIBS': (List, list,
        """List of system libraries for host programs and libraries.
        """, None),

    'LOCAL_INCLUDES': (StrictOrderingOnAppendList, list,
        """Additional directories to be searched for include files by the compiler.
        """, None),

    'NO_PGO': (bool, bool,
        """Whether profile-guided optimization is disable in this directory.
        """, None),

    'NO_VISIBILITY_FLAGS': (bool, bool,
        """Build sources listed in this file without VISIBILITY_FLAGS.
        """, None),

    'OS_LIBS': (List, list,
        """System link libraries.

        This variable contains a list of system libaries to link against.
        """, None),
    'RCFILE': (unicode, unicode,
        """The program .rc file.

        This variable can only be used on Windows.
        """, None),

    'RESFILE': (unicode, unicode,
        """The program .res file.

        This variable can only be used on Windows.
        """, None),

    'RCINCLUDE': (unicode, unicode,
        """The resource script file to be included in the default .res file.

        This variable can only be used on Windows.
        """, None),

    'DEFFILE': (unicode, unicode,
        """The program .def (module definition) file.

        This variable can only be used on Windows.
        """, None),

    'LD_VERSION_SCRIPT': (unicode, unicode,
        """The linker version script for shared libraries.

        This variable can only be used on Linux.
        """, None),

    'BRANDING_FILES': (HierarchicalStringListWithFlagsFactory({'source': unicode}), list,
        """List of files to be installed into the branding directory.

        ``BRANDING_FILES`` will copy (or symlink, if the platform supports it)
        the contents of its files to the ``dist/branding`` directory. Files that
        are destined for a subdirectory can be specified by accessing a field.
        For example, to export ``foo.png`` to the top-level directory and
        ``bar.png`` to the directory ``images/subdir``, append to
        ``BRANDING_FILES`` like so::

           BRANDING_FILES += ['foo.png']
           BRANDING_FILES.images.subdir += ['bar.png']

        If the source and destination have different file names, add the
        destination name to the list and set the ``source`` property on the
        entry, like so::

           BRANDING_FILES.dir += ['baz.png']
           BRANDING_FILES.dir['baz.png'].source = 'quux.png'
        """, None),

    'RESOURCE_FILES': (HierarchicalStringListWithFlagsFactory({'preprocess': bool}), list,
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

        Added files also have a 'preprocess' attribute, which will cause the
        affected file to be run through the preprocessor, using any ``DEFINES``
        set. It is used like this::

           RESOURCE_FILES.fonts += ['baz.res.in']
           RESOURCE_FILES.fonts['baz.res.in'].preprocess = True
        """, None),

    'SDK_LIBRARY': (bool, bool,
        """Whether the library built in the directory is part of the SDK.

        The library will be copied into ``SDK_LIB_DIR`` (``$DIST/sdk/lib``).
        """, None),

    'SIMPLE_PROGRAMS': (StrictOrderingOnAppendList, list,
        """Compile a list of executable names.

        Each name in this variable corresponds to an executable built from the
        corresponding source file with the same base name.

        If the configuration token ``BIN_SUFFIX`` is set, its value will be
        automatically appended to each name. If a name already ends with
        ``BIN_SUFFIX``, the name will remain unchanged.
        """, None),

    'SONAME': (unicode, unicode,
        """The soname of the shared object currently being linked

        soname is the "logical name" of a shared object, often used to provide
        version backwards compatibility. This variable makes sense only for
        shared objects, and is supported only on some unix platforms.
        """, None),

    'HOST_SIMPLE_PROGRAMS': (StrictOrderingOnAppendList, list,
        """Compile a list of host executable names.

        Each name in this variable corresponds to a hosst executable built
        from the corresponding source file with the same base name.

        If the configuration token ``HOST_BIN_SUFFIX`` is set, its value will
        be automatically appended to each name. If a name already ends with
        ``HOST_BIN_SUFFIX``, the name will remain unchanged.
        """, None),

    'TEST_DIRS': (ContextDerivedTypedList(SourcePath), list,
        """Like DIRS but only for directories that contain test-only code.

        If tests are not enabled, this variable will be ignored.

        This variable may go away once the transition away from Makefiles is
        complete.
        """, None),

    'CONFIGURE_SUBST_FILES': (StrictOrderingOnAppendList, list,
        """Output files that will be generated using configure-like substitution.

        This is a substitute for ``AC_OUTPUT`` in autoconf. For each path in this
        list, we will search for a file in the srcdir having the name
        ``{path}.in``. The contents of this file will be read and variable
        patterns like ``@foo@`` will be substituted with the values of the
        ``AC_SUBST`` variables declared during configure.
        """, None),

    'CONFIGURE_DEFINE_FILES': (StrictOrderingOnAppendList, list,
        """Output files generated from configure/config.status.

        This is a substitute for ``AC_CONFIG_HEADER`` in autoconf. This is very
        similar to ``CONFIGURE_SUBST_FILES`` except the generation logic takes
        into account the values of ``AC_DEFINE`` instead of ``AC_SUBST``.
        """, None),

    'EXPORTS': (HierarchicalStringList, list,
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
        """, None),

    'PROGRAM' : (unicode, unicode,
        """Compiled executable name.

        If the configuration token ``BIN_SUFFIX`` is set, its value will be
        automatically appended to ``PROGRAM``. If ``PROGRAM`` already ends with
        ``BIN_SUFFIX``, ``PROGRAM`` will remain unchanged.
        """, None),

    'HOST_PROGRAM' : (unicode, unicode,
        """Compiled host executable name.

        If the configuration token ``HOST_BIN_SUFFIX`` is set, its value will be
        automatically appended to ``HOST_PROGRAM``. If ``HOST_PROGRAM`` already
        ends with ``HOST_BIN_SUFFIX``, ``HOST_PROGRAM`` will remain unchanged.
        """, None),

    'DIST_INSTALL': (Enum(None, False, True), bool,
        """Whether to install certain files into the dist directory.

        By default, some files types are installed in the dist directory, and
        some aren't. Set this variable to True to force the installation of
        some files that wouldn't be installed by default. Set this variable to
        False to force to not install some files that would be installed by
        default.

        This is confusing for historical reasons, but eventually, the behavior
        will be made explicit.
        """, None),

    'JAR_MANIFESTS': (StrictOrderingOnAppendList, list,
        """JAR manifest files that should be processed as part of the build.

        JAR manifests are files in the tree that define how to package files
        into JARs and how chrome registration is performed. For more info,
        see :ref:`jar_manifests`.
        """, 'libs'),

    # IDL Generation.
    'XPIDL_SOURCES': (StrictOrderingOnAppendList, list,
        """XPCOM Interface Definition Files (xpidl).

        This is a list of files that define XPCOM interface definitions.
        Entries must be files that exist. Entries are almost certainly ``.idl``
        files.
        """, 'export'),

    'XPIDL_MODULE': (unicode, unicode,
        """XPCOM Interface Definition Module Name.

        This is the name of the ``.xpt`` file that is created by linking
        ``XPIDL_SOURCES`` together. If unspecified, it defaults to be the same
        as ``MODULE``.
        """, None),

    'XPIDL_NO_MANIFEST': (bool, bool,
        """Indicate that the XPIDL module should not be added to a manifest.

        This flag exists primarily to prevent test-only XPIDL modules from being
        added to the application's chrome manifest. Most XPIDL modules should
        not use this flag.
        """, None),

    'IPDL_SOURCES': (StrictOrderingOnAppendList, list,
        """IPDL source files.

        These are ``.ipdl`` files that will be parsed and converted to
        ``.cpp`` files.
        """, 'export'),

    'WEBIDL_FILES': (StrictOrderingOnAppendList, list,
        """WebIDL source files.

        These will be parsed and converted to ``.cpp`` and ``.h`` files.
        """, 'export'),

    'GENERATED_EVENTS_WEBIDL_FILES': (StrictOrderingOnAppendList, list,
        """WebIDL source files for generated events.

        These will be parsed and converted to ``.cpp`` and ``.h`` files.
        """, 'export'),

    'TEST_WEBIDL_FILES': (StrictOrderingOnAppendList, list,
         """Test WebIDL source files.

         These will be parsed and converted to ``.cpp`` and ``.h`` files
         if tests are enabled.
         """, 'export'),

    'GENERATED_WEBIDL_FILES': (StrictOrderingOnAppendList, list,
         """Generated WebIDL source files.

         These will be generated from some other files.
         """, 'export'),

    'PREPROCESSED_TEST_WEBIDL_FILES': (StrictOrderingOnAppendList, list,
         """Preprocessed test WebIDL source files.

         These will be preprocessed, then parsed and converted to .cpp
         and ``.h`` files if tests are enabled.
         """, 'export'),

    'PREPROCESSED_WEBIDL_FILES': (StrictOrderingOnAppendList, list,
         """Preprocessed WebIDL source files.

         These will be preprocessed before being parsed and converted.
         """, 'export'),

    'WEBIDL_EXAMPLE_INTERFACES': (StrictOrderingOnAppendList, list,
        """Names of example WebIDL interfaces to build as part of the build.

        Names in this list correspond to WebIDL interface names defined in
        WebIDL files included in the build from one of the \*WEBIDL_FILES
        variables.
        """, 'export'),

    # Test declaration.
    'A11Y_MANIFESTS': (StrictOrderingOnAppendList, list,
        """List of manifest files defining a11y tests.
        """, None),

    'BROWSER_CHROME_MANIFESTS': (StrictOrderingOnAppendList, list,
        """List of manifest files defining browser chrome tests.
        """, None),

    'JETPACK_PACKAGE_MANIFESTS': (StrictOrderingOnAppendList, list,
        """List of manifest files defining jetpack package tests.
        """, None),

    'JETPACK_ADDON_MANIFESTS': (StrictOrderingOnAppendList, list,
        """List of manifest files defining jetpack addon tests.
        """, None),

    'CRASHTEST_MANIFESTS': (StrictOrderingOnAppendList, list,
        """List of manifest files defining crashtests.

        These are commonly named crashtests.list.
        """, None),

    'ANDROID_INSTRUMENTATION_MANIFESTS': (StrictOrderingOnAppendList, list,
        """List of manifest files defining Android instrumentation tests.
        """, None),

    'METRO_CHROME_MANIFESTS': (StrictOrderingOnAppendList, list,
        """List of manifest files defining metro browser chrome tests.
        """, None),

    'MOCHITEST_CHROME_MANIFESTS': (StrictOrderingOnAppendList, list,
        """List of manifest files defining mochitest chrome tests.
        """, None),

    'MOCHITEST_MANIFESTS': (StrictOrderingOnAppendList, list,
        """List of manifest files defining mochitest tests.
        """, None),

    'MOCHITEST_WEBAPPRT_CONTENT_MANIFESTS': (StrictOrderingOnAppendList, list,
        """List of manifest files defining webapprt mochitest content tests.
        """, None),

    'MOCHITEST_WEBAPPRT_CHROME_MANIFESTS': (StrictOrderingOnAppendList, list,
        """List of manifest files defining webapprt mochitest chrome tests.
        """, None),

    'REFTEST_MANIFESTS': (StrictOrderingOnAppendList, list,
        """List of manifest files defining reftests.

        These are commonly named reftest.list.
        """, None),

    'WEBRTC_SIGNALLING_TEST_MANIFESTS': (StrictOrderingOnAppendList, list,
        """List of manifest files defining WebRTC signalling tests.
        """, None),

    'XPCSHELL_TESTS_MANIFESTS': (StrictOrderingOnAppendList, list,
        """List of manifest files defining xpcshell tests.
        """, None),

    # The following variables are used to control the target of installed files.
    'XPI_NAME': (unicode, unicode,
        """The name of an extension XPI to generate.

        When this variable is present, the results of this directory will end up
        being packaged into an extension instead of the main dist/bin results.
        """, 'libs'),

    'DIST_SUBDIR': (unicode, unicode,
        """The name of an alternate directory to install files to.

        When this variable is present, the results of this directory will end up
        being placed in the $(DIST_SUBDIR) subdirectory of where it would
        otherwise be placed.
        """, None),

    'FINAL_TARGET': (FinalTargetValue, unicode,
        """The name of the directory to install targets to.

        The directory is relative to the top of the object directory. The
        default value is dependent on the values of XPI_NAME and DIST_SUBDIR. If
        neither are present, the result is dist/bin. If XPI_NAME is present, the
        result is dist/xpi-stage/$(XPI_NAME). If DIST_SUBDIR is present, then
        the $(DIST_SUBDIR) directory of the otherwise default value is used.
        """, None),

    'USE_EXTENSION_MANIFEST': (bool, bool,
        """Controls the name of the manifest for JAR files.

        By default, the name of the manifest is ${JAR_MANIFEST}.manifest.
        Setting this variable to ``True`` changes the name of the manifest to
        chrome.manifest.
        """, None),

    'NO_JS_MANIFEST': (bool, bool,
        """Explicitly disclaims responsibility for manifest listing in EXTRA_COMPONENTS.

        Normally, if you have .js files listed in ``EXTRA_COMPONENTS`` or
        ``EXTRA_PP_COMPONENTS``, you are expected to have a corresponding
        .manifest file to go with those .js files.  Setting ``NO_JS_MANIFEST``
        indicates that the relevant .manifest file and entries for those .js
        files are elsehwere (jar.mn, for instance) and this state of affairs
        is OK.
        """, None),

    'GYP_DIRS': (StrictOrderingOnAppendListWithFlagsFactory({
            'variables': dict,
            'input': unicode,
            'sandbox_vars': dict,
            'non_unified_sources': StrictOrderingOnAppendList,
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
            - non_unified_sources, a list containing sources files, relative to
              the current moz.build, that should be excluded from source file
              unification.

        Typical use looks like:
            GYP_DIRS += ['foo', 'bar']
            GYP_DIRS['foo'].input = 'foo/foo.gyp'
            GYP_DIRS['foo'].variables = {
                'foo': 'bar',
                (...)
            }
            (...)
        """, None),

    'SPHINX_TREES': (dict, dict,
        """Describes what the Sphinx documentation tree will look like.

        Keys are relative directories inside the final Sphinx documentation
        tree to install files into. Values are directories (relative to this
        file) whose content to copy into the Sphinx documentation tree.
        """, None),

    'SPHINX_PYTHON_PACKAGE_DIRS': (StrictOrderingOnAppendList, list,
        """Directories containing Python packages that Sphinx documents.
        """, None),

    'CFLAGS': (List, list,
        """Flags passed to the C compiler for all of the C source files
           declared in this directory.

           Note that the ordering of flags matters here, these flags will be
           added to the compiler's command line in the same order as they
           appear in the moz.build file.
        """, None),

    'CXXFLAGS': (List, list,
        """Flags passed to the C++ compiler for all of the C++ source files
           declared in this directory.

           Note that the ordering of flags matters here; these flags will be
           added to the compiler's command line in the same order as they
           appear in the moz.build file.
        """, None),

    'CMFLAGS': (List, list,
        """Flags passed to the Objective-C compiler for all of the Objective-C
           source files declared in this directory.

           Note that the ordering of flags matters here; these flags will be
           added to the compiler's command line in the same order as they
           appear in the moz.build file.
        """, None),

    'CMMFLAGS': (List, list,
        """Flags passed to the Objective-C++ compiler for all of the
           Objective-C++ source files declared in this directory.

           Note that the ordering of flags matters here; these flags will be
           added to the compiler's command line in the same order as they
           appear in the moz.build file.
        """, None),

    'ASFLAGS': (List, list,
        """Flags passed to the assembler for all of the assembly source files
           declared in this directory.

           Note that the ordering of flags matters here; these flags will be
           added to the assembler's command line in the same order as they
           appear in the moz.build file.
        """, None),

    'LDFLAGS': (List, list,
        """Flags passed to the linker when linking all of the libraries and
           executables declared in this directory.

           Note that the ordering of flags matters here; these flags will be
           added to the linker's command line in the same order as they
           appear in the moz.build file.
        """, None),

    'EXTRA_DSO_LDOPTS': (List, list,
        """Flags passed to the linker when linking a shared library.

           Note that the ordering of flags matter here, these flags will be
           added to the linker's command line in the same order as they
           appear in the moz.build file.
        """, None),

    'WIN32_EXE_LDFLAGS': (List, list,
        """Flags passed to the linker when linking a Windows .exe executable
           declared in this directory.

           Note that the ordering of flags matter here, these flags will be
           added to the linker's command line in the same order as they
           appear in the moz.build file.

           This variable only has an effect on Windows.
        """, None),

    'TEST_HARNESS_FILES': (HierarchicalStringList, list,
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
        """, 'libs'),

    'NO_EXPAND_LIBS': (bool, bool,
        """Forces to build a real static library, and no corresponding fake
           library.
        """, None),
}

# Sanity check: we don't want any variable above to have a list as storage type.
for name, (storage_type, input_types, docs, tier) in VARIABLES.items():
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
    'IS_COMPONENT',
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
    storage_type, input_types, docs, tier = VARIABLES[name]
    docs += 'This variable is only available in templates.\n'
    VARIABLES[name] = (storage_type, input_types, docs, tier)


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

    'add_android_eclipse_project': (
        lambda self: self._add_android_eclipse_project, (str, str),
        """Declare an Android Eclipse project.

        This is one of the supported ways to populate the
        ANDROID_ECLIPSE_PROJECT_TARGETS variable.

        The parameters are:
        * name - project name.
        * manifest - path to AndroidManifest.xml.

        This returns a rich Android Eclipse project type, described at
        :py:class:`mozbuild.frontend.data.AndroidEclipseProjectData`.
        """),

    'add_android_eclipse_library_project': (
        lambda self: self._add_android_eclipse_library_project, (str,),
        """Declare an Android Eclipse library project.

        This is one of the supported ways to populate the
        ANDROID_ECLIPSE_PROJECT_TARGETS variable.

        The parameters are:
        * name - project name.

        This returns a rich Android Eclipse project type, described at
        :py:class:`mozbuild.frontend.data.AndroidEclipseProjectData`.
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

    'IS_COMPONENT': '''
        Please use

            XPCOMBinaryComponent('foo')

        instead of

            Library('foo') [ or LIBRARY_NAME = 'foo' ]
            IS_COMPONENT = True
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
}

# Make sure that all template variables have a deprecation hint.
for name in TEMPLATE_VARIABLES:
    if name not in DEPRECATION_HINTS:
        raise RuntimeError('Missing deprecation hint for %s' % name)
