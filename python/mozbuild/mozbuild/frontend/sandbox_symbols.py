# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

######################################################################
# DO NOT UPDATE THIS FILE WITHOUT SIGN-OFF FROM A BUILD MODULE PEER. #
######################################################################

r"""Defines the global config variables.

This module contains data structures defining the global symbols that have
special meaning in the frontend files for the build system.

If you are looking for the absolute authority on what the global namespace in
the Sandbox consists of, you've come to the right place.
"""

from __future__ import unicode_literals

from collections import OrderedDict
from mozbuild.util import (
    HierarchicalStringList,
    StrictOrderingOnAppendList,
)


# This defines the set of mutable global variables.
#
# Each variable is a tuple of:
#
#   (storage_type, input_types, default_value, docs, tier)
#
# Tier says for which specific tier the variable has an effect.
# Valid tiers are:
# - 'export'
# - 'compile': everything in relation with compiling objects.
# - 'binaries': everything in relation with linking objects, producing
#      programs and libraries.
# - 'libs': everything that is not compile or binaries and that has
#      traditionally been in the libs tier.
# - 'tools'
# A value of None means the variable has no direct effect on any tier.

VARIABLES = {
    # Variables controlling reading of other frontend files.
    'ASFILES': (StrictOrderingOnAppendList, list, [],
        """Assembly file sources.

        This variable contains a list of files to invoke the assembler on.
        """, 'compile'),

    'CMMSRCS': (StrictOrderingOnAppendList, list, [],
        """Sources to compile with the Objective C/C++ compiler.

        This variable contains a list of objective-C++ sources to compile.
        """, 'compile'),

    'CSRCS': (StrictOrderingOnAppendList, list, [],
        """C code source files.

        This variable contains a list of C source files to compile.
        """, 'compile'),

    'DEFINES': (OrderedDict, dict, OrderedDict(),
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

    'DIRS': (list, list, [],
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

    'EXPORT_LIBRARY': (bool, bool, False,
        """Install the library to the static libraries folder.
        """, None),

    'EXTRA_COMPONENTS': (StrictOrderingOnAppendList, list, [],
        """Additional component files to distribute.

       This variable contains a list of files to copy into
       ``$(FINAL_TARGET)/components/``.
        """, 'libs'),

    'EXTRA_JS_MODULES': (StrictOrderingOnAppendList, list, [],
        """Additional JavaScript files to distribute.

        This variable contains a list of files to copy into
        ``$(FINAL_TARGET)/$(JS_MODULES_PATH)``. ``JS_MODULES_PATH`` defaults
        to ``modules`` if left undefined.
        """, 'libs'),

    'EXTRA_PP_JS_MODULES': (StrictOrderingOnAppendList, list, [],
        """Additional JavaScript files to distribute.

        This variable contains a list of files to copy into
        ``$(FINAL_TARGET)/$(JS_MODULES_PATH)``, after preprocessing.
        ``JS_MODULES_PATH`` defaults to ``modules`` if left undefined.
        """, 'libs'),

    'EXTRA_PP_COMPONENTS': (StrictOrderingOnAppendList, list, [],
        """Javascript XPCOM files.

       This variable contains a list of files to preprocess.  Generated
       files will be installed in the ``/components`` directory of the distribution.
        """, 'libs'),

    'CPP_UNIT_TESTS': (StrictOrderingOnAppendList, list, [],
        """C++ source files for unit tests.

        This is a list of C++ unit test sources. Entries must be files that
        exist. These generally have ``.cpp`` extensions.
        """, 'binaries'),

    'FAIL_ON_WARNINGS': (bool, bool, False,
        """Whether to treat warnings as errors.
        """, None),

    'FORCE_SHARED_LIB': (bool, bool, False,
        """Whether the library in this directory is a shared library.
        """, None),

    'FORCE_STATIC_LIB': (bool, bool, False,
        """Whether the library in this directory is a static library.
        """, None),

    'GTEST_C_SOURCES': (StrictOrderingOnAppendList, list, [],
        """C code source files for GTest unit tests.

        This variable contains a list of C GTEST unit test source files to
        compile.
        """, 'compile'),

    'GTEST_CMM_SOURCES': (StrictOrderingOnAppendList, list, [],
        """Sources for GTest unit tests to compile with the Objective C/C++ compiler.

        This variable contains a list of objective-C++ GTest unit test sources
        to compile.
        """, 'compile'),

    'GTEST_CPP_SOURCES': (list, list, [],
        """C++ source files for GTest unit tests.

        This is a list of C++ GTest unit test sources. Entries must be files
        that exist. These generally have ``.cpp``, ``.cc``, or ``.cxx``
        extensions.
        """, 'compile'),

    'HOST_CPPSRCS': (StrictOrderingOnAppendList, list, [],
        """C++ source files to compile with the host compiler.

        This variable contains a list of C++ source files to compile.
        """, 'compile'),

    'HOST_CSRCS': (StrictOrderingOnAppendList, list, [],
        """C source files to compile with the host compiler.

        This variable contains a list of C source files to compile.
        """, 'compile'),

    'IS_COMPONENT': (bool, bool, False,
        """Whether the library contains a binary XPCOM component manifest.
        """, None),

    'PARALLEL_DIRS': (list, list, [],
        """A parallel version of ``DIRS``.

        Ideally this variable does not exist. It is provided so a transition
        from recursive makefiles can be made. Once the build system has been
        converted to not use Makefile's for the build frontend, this will
        likely go away.
        """, None),

    'HOST_LIBRARY_NAME': (unicode, unicode, "",
        """Name of target library generated when cross compiling.
        """, 'binaries'),

    'JS_MODULES_PATH': (unicode, unicode, "",
        """Sub-directory of ``$(FINAL_TARGET)`` to install
        ``EXTRA_JS_MODULES``.

        ``EXTRA_JS_MODULES`` files are copied to
        ``$(FINAL_TARGET)/$(JS_MODULES_PATH)``. This variable does not
        need to be defined if the desired destination directory is
        ``$(FINAL_TARGET)/modules``.
        """, None),

    'LIBRARY_NAME': (unicode, unicode, "",
        """The name of the library generated for a directory.

        In ``example/components/moz.build``,::

           LIBRARY_NAME = 'xpcomsample'

        would generate ``example/components/libxpcomsample.so`` on Linux, or
        ``example/components/xpcomsample.lib`` on Windows.
        """, 'binaries'),

    'LIBS': (StrictOrderingOnAppendList, list, [],
        """Linker libraries and flags.

        A list of libraries and flags to include when linking.
        """, None),

    'LIBXUL_LIBRARY': (bool, bool, False,
        """Whether the library in this directory is linked into libxul.

        Implies ``MOZILLA_INTERNAL_API`` and ``FORCE_STATIC_LIB``.
        """, None),

    'LOCAL_INCLUDES': (StrictOrderingOnAppendList, list, [],
        """Additional directories to be searched for include files by the compiler.
        """, None),

    'MSVC_ENABLE_PGO': (bool, bool, False,
        """Whether profile-guided optimization is enabled in this directory.
        """, None),

    'OS_LIBS': (list, list, [],
        """System link libraries.

        This variable contains a list of system libaries to link against.
        """, None),

    'SDK_LIBRARY': (StrictOrderingOnAppendList, list, [],
        """Elements of the distributed SDK.

        Files on this list will be copied into ``SDK_LIB_DIR``
        (``$DIST/sdk/lib``).
        """, None),

    'SHARED_LIBRARY_LIBS': (StrictOrderingOnAppendList, list, [],
        """Libraries linked into a shared library.

        A list of static library paths which should be linked into the
        current shared library.
        """, None),

    'SIMPLE_PROGRAMS': (StrictOrderingOnAppendList, list, [],
        """Generate a list of binaries from source.

        A list of sources, one per program, to compile & link with libs
        into standalone programs.
        """, 'binaries'),

    'SSRCS': (StrictOrderingOnAppendList, list, [],
        """Assembly source files.

        This variable contains a list of files to invoke the assembler on.
        """, 'compile'),

    'TOOL_DIRS': (list, list, [],
        """Like DIRS but for tools.

        Tools are for pieces of the build system that aren't required to
        produce a working binary (in theory). They provide things like test
        code and utilities.
        """, None),

    'TEST_DIRS': (list, list, [],
        """Like DIRS but only for directories that contain test-only code.

        If tests are not enabled, this variable will be ignored.

        This variable may go away once the transition away from Makefiles is
        complete.
        """, None),

    'TEST_TOOL_DIRS': (list, list, [],
        """TOOL_DIRS that is only executed if tests are enabled.
        """, None),


    'TIERS': (OrderedDict, dict, OrderedDict(),
        """Defines directories constituting the tier traversal mechanism.

        The recursive make backend iteration is organized into tiers. There are
        major tiers (keys in this dict) that correspond roughly to applications
        or libraries being built. e.g. base, nspr, js, platform, app. Within
        each tier are phases like export, libs, and tools. The recursive make
        backend iterates over each phase in the first tier then proceeds to the
        next tier until all tiers are exhausted.

        Tiers are a way of working around deficiencies in recursive make. These
        will probably disappear once we no longer rely on recursive make for
        the build backend. They will likely be replaced by ``DIRS``.

        This variable is typically not populated directly. Instead, it is
        populated by calling add_tier_dir().
        """, None),

    'EXTERNAL_MAKE_DIRS': (list, list, [],
        """Directories that build with make but don't use moz.build files.

        This is like ``DIRS`` except it implies that ``make`` is used to build the
        directory and that the directory does not define itself with moz.build
        files.
        """, None),

    'PARALLEL_EXTERNAL_MAKE_DIRS': (list, list, [],
        """Parallel version of ``EXTERNAL_MAKE_DIRS``.
        """, None),

    'CONFIGURE_SUBST_FILES': (StrictOrderingOnAppendList, list, [],
        """Output files that will be generated using configure-like substitution.

        This is a substitute for ``AC_OUTPUT`` in autoconf. For each path in this
        list, we will search for a file in the srcdir having the name
        ``{path}.in``. The contents of this file will be read and variable
        patterns like ``@foo@`` will be substituted with the values of the
        ``AC_SUBST`` variables declared during configure.
        """, None),

    'MODULE': (unicode, unicode, "",
        """Module name.

        Historically, this variable was used to describe where to install header
        files, but that feature is now handled by ``EXPORTS_NAMESPACES``.
        ``MODULE`` will likely be removed in the future.
        """, None),

    'EXPORTS': (HierarchicalStringList, list, HierarchicalStringList(),
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

    'PROGRAM' : (unicode, unicode, "",
        """Compiled executable name.

        If the configuration token ``BIN_SUFFIX`` is set, its value will be
        automatically appended to ``PROGRAM``. If ``PROGRAM`` already ends with
        ``BIN_SUFFIX``, ``PROGRAM`` will remain unchanged.
        """, 'binaries'),

    'CPP_SOURCES': (list, list, [],
        """C++ source file list.

        This is a list of C++ files to be compiled. Entries must be files that
        exist. These generally have ``.cpp``, ``.cc``, or ``.cxx`` extensions.
        """, 'compile'),

    'NO_DIST_INSTALL': (bool, bool, False,
        """Disable installing certain files into the distribution directory.

        If present, some files defined by other variables won't be
        distributed/shipped with the produced build.
        """, None),

    # IDL Generation.
    'XPIDL_SOURCES': (StrictOrderingOnAppendList, list, [],
        """XPCOM Interface Definition Files (xpidl).

        This is a list of files that define XPCOM interface definitions.
        Entries must be files that exist. Entries are almost certainly ``.idl``
        files.
        """, 'libs'),

    'XPIDL_MODULE': (unicode, unicode, "",
        """XPCOM Interface Definition Module Name.

        This is the name of the ``.xpt`` file that is created by linking
        ``XPIDL_SOURCES`` together. If unspecified, it defaults to be the same
        as ``MODULE``.
        """, None),

    'IPDL_SOURCES': (StrictOrderingOnAppendList, list, [],
        """IPDL source files.

        These are ``.ipdl`` files that will be parsed and converted to
        ``.cpp`` files.
        """, 'export'),

    'WEBIDL_FILES': (StrictOrderingOnAppendList, list, [],
        """WebIDL source files.

        These will be parsed and converted to ``.cpp`` and ``.h`` files.
        """, 'export'),

    'GENERATED_EVENTS_WEBIDL_FILES': (StrictOrderingOnAppendList, list, [],
        """WebIDL source files for generated events.

        These will be parsed and converted to ``.cpp`` and ``.h`` files.
        """, 'export'),

    'TEST_WEBIDL_FILES': (StrictOrderingOnAppendList, list, [],
         """Test WebIDL source files.

         These will be parsed and converted to ``.cpp`` and ``.h`` files
         if tests are enabled.
         """, 'export'),

    'GENERATED_WEBIDL_FILES': (StrictOrderingOnAppendList, list, [],
         """Generated WebIDL source files.

         These will be generated from some other files.
         """, 'export'),

    'PREPROCESSED_TEST_WEBIDL_FILES': (StrictOrderingOnAppendList, list, [],
         """Preprocessed test WebIDL source files.

         These will be preprocessed, then parsed and converted to .cpp
         and ``.h`` files if tests are enabled.
         """, 'export'),

    'PREPROCESSED_WEBIDL_FILES': (StrictOrderingOnAppendList, list, [],
         """Preprocessed WebIDL source files.

         These will be preprocessed before being parsed and converted.
         """, 'export'),

    # Test declaration.
    'A11Y_MANIFESTS': (StrictOrderingOnAppendList, list, [],
        """List of manifest files defining a11y tests.
        """, None),

    'BROWSER_CHROME_MANIFESTS': (StrictOrderingOnAppendList, list, [],
        """List of manifest files defining browser chrome tests.
        """, None),

    'METRO_CHROME_MANIFESTS': (StrictOrderingOnAppendList, list, [],
        """List of manifest files defining metro browser chrome tests.
        """, None),

    'MOCHITEST_CHROME_MANIFESTS': (StrictOrderingOnAppendList, list, [],
        """List of manifest files defining mochitest chrome tests.
        """, None),

    'MOCHITEST_MANIFESTS': (StrictOrderingOnAppendList, list, [],
        """List of manifest files defining mochitest tests.
        """, None),

    'MOCHITEST_WEBAPPRT_CHROME_MANIFESTS': (StrictOrderingOnAppendList, list, [],
        """List of manifest files defining webapprt mochitest chrome tests.
        """, None),

    'WEBRTC_SIGNALLING_TEST_MANIFESTS': (StrictOrderingOnAppendList, list, [],
        """List of manifest files defining WebRTC signalling tests.
        """, None),

    'XPCSHELL_TESTS_MANIFESTS': (StrictOrderingOnAppendList, list, [],
        """List of manifest files defining xpcshell tests.
        """, None),
}

# The set of functions exposed to the sandbox.
#
# Each entry is a tuple of:
#
#  (method attribute, (argument types), docs)
#
# The first element is an attribute on Sandbox that should be a function type.
#
FUNCTIONS = {
    'include': ('_include', (str,),
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

    'add_tier_dir': ('_add_tier_directory', (str, [str, list], bool, bool),
        """Register a directory for tier traversal.

        This is the preferred way to populate the TIERS variable.

        Tiers are how the build system is organized. The build process is
        divided into major phases called tiers. The most important tiers are
        "platform" and "apps." The platform tier builds the Gecko platform
        (typically outputting libxul). The apps tier builds the configured
        application (browser, mobile/android, b2g, etc).

        This function is typically only called by the main moz.build file or a
        file directly included by the main moz.build file. An error will be
        raised if it is called when it shouldn't be.

        An error will also occur if you attempt to add the same directory to
        the same tier multiple times.

        Example usage
        ^^^^^^^^^^^^^

        Register a single directory with the 'platform' tier::

           add_tier_dir('platform', 'xul')

        Register multiple directories with the 'app' tier.::

           add_tier_dir('app', ['components', 'base'])

        Register a directory as having static content (no dependencies)::

           add_tier_dir('base', 'foo', static=True)

        Register a directory as having external content (same as static
        content, but traversed with export, libs, and tools subtiers::

           add_tier_dir('base', 'bar', external=True)
        """),

    'warning': ('_warning', (str,),
        """Issue a warning.

        Warnings are string messages that are printed during execution.

        Warnings are ignored during execution.
        """),

    'error': ('_error', (str,),
        """Issue a fatal error.

        If this function is called, processing is aborted immediately.
        """),
}

# Special variables. These complement VARIABLES.
SPECIAL_VARIABLES = {
    'TOPSRCDIR': (str,
        """Constant defining the top source directory.

        The top source directory is the parent directory containing the source
        code and all build files. It is typically the root directory of a
        cloned repository.
        """),

    'TOPOBJDIR': (str,
        """Constant defining the top object directory.

        The top object directory is the parent directory which will contain
        the output of the build. This is commonly referred to as "the object
        directory."
        """),

    'RELATIVEDIR': (str,
        """Constant defining the relative path of this file.

        The relative path is from ``TOPSRCDIR``. This is defined as relative
        to the main file being executed, regardless of whether additional
        files have been included using ``include()``.
        """),

    'SRCDIR': (str,
        """Constant defining the source directory of this file.

        This is the path inside ``TOPSRCDIR`` where this file is located. It
        is the same as ``TOPSRCDIR + RELATIVEDIR``.
        """),

    'OBJDIR': (str,
        """The path to the object directory for this file.

        Is is the same as ``TOPOBJDIR + RELATIVEDIR``.
        """),

    'CONFIG': (dict,
        """Dictionary containing the current configuration variables.

        All the variables defined by the configuration system are available
        through this object. e.g. ``ENABLE_TESTS``, ``CFLAGS``, etc.

        Values in this container are read-only. Attempts at changing values
        will result in a run-time error.

        Access to an unknown variable will return None.
        """),

    '__builtins__': (dict,
        """Exposes Python built-in types.

        The set of exposed Python built-ins is currently:

        - True
        - False
        - None
        """),
}
