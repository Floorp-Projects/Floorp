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
    HierarchicalStringListWithFlagsFactory,
    StrictOrderingOnAppendList,
    StrictOrderingOnAppendListWithFlagsFactory,
)
from .sandbox import SandboxDerivedValue
from types import StringTypes


class FinalTargetValue(SandboxDerivedValue, unicode):
    def __new__(cls, sandbox, value=""):
        if not value:
            value = 'dist/'
            if sandbox['XPI_NAME']:
                value += 'xpi-stage/' + sandbox['XPI_NAME']
            else:
                value += 'bin'
            if sandbox['DIST_SUBDIR']:
                value += '/' + sandbox['DIST_SUBDIR']
        return unicode.__new__(cls, value)


# This defines the set of mutable global variables.
#
# Each variable is a tuple of:
#
#   (storage_type, input_types, docs, tier)
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
    'ANDROID_GENERATED_RESFILES': (StrictOrderingOnAppendList, list,
        """Android resource files generated as part of the build.

        This variable contains a list of files that are expected to be
        generated (often by preprocessing) into a 'res' directory as
        part of the build process, and subsequently merged into an APK
        file.
        """, 'export'),

    'ANDROID_RES_DIRS': (list, list,
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

    'SOURCES': (StrictOrderingOnAppendListWithFlagsFactory({'no_pgo': bool, 'flags': list}), list,
        """Source code files.

        This variable contains a list of source code files to compile.
        Accepts assembler, C, C++, Objective C/C++.
        """, 'compile'),

    'GENERATED_SOURCES': (StrictOrderingOnAppendList, list,
        """Generated source code files.

        This variable contains a list of generated source code files to
        compile. Accepts assembler, C, C++, Objective C/C++.
        """, 'compile'),

    'FILES_PER_UNIFIED_FILE': (int, int,
        """The number of source files to compile into each unified source file.

        """, 'None'),

    'UNIFIED_SOURCES': (StrictOrderingOnAppendList, list,
        """Source code files that can be compiled together.

        This variable contains a list of source code files to compile,
        that can be concatenated all together and built as a single source
        file. This can help make the build faster and reduce the debug info
        size.
        """, 'compile'),

    'GENERATED_UNIFIED_SOURCES': (StrictOrderingOnAppendList, list,
        """Generated source code files that can be compiled together.

        This variable contains a list of generated source code files to
        compile, that can be concatenated all together, with UNIFIED_SOURCES,
        and built as a single source file. This can help make the build faster
        and reduce the debug info size.
        """, 'compile'),

    'GENERATED_FILES': (StrictOrderingOnAppendList, list,
        """Generic generated files.

        This variable contains a list of generate files for the build system
        to generate at export time. The rules for those files still live in
        Makefile.in.
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

    'DELAYLOAD_DLLS': (list, list,
        """Delay-loaded DLLs.

        This variable contains a list of DLL files which the module being linked
        should load lazily.  This only has an effect when building with MSVC.
        """, 'binaries'),

    'DIRS': (list, list,
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

    'DISABLE_STL_WRAPPING': (bool, bool,
        """Disable the wrappers for STL which allow it to work with C++ exceptions
        disabled.
        """, 'binaries'),

    'EXPORT_LIBRARY': (bool, bool,
        """Install the library to the static libraries folder.
        """, None),

    'EXTRA_COMPONENTS': (StrictOrderingOnAppendList, list,
        """Additional component files to distribute.

       This variable contains a list of files to copy into
       ``$(FINAL_TARGET)/components/``.
        """, 'libs'),

    'EXTRA_JS_MODULES': (StrictOrderingOnAppendList, list,
        """Additional JavaScript files to distribute.

        This variable contains a list of files to copy into
        ``$(FINAL_TARGET)/$(JS_MODULES_PATH)``. ``JS_MODULES_PATH`` defaults
        to ``modules`` if left undefined.
        """, 'libs'),

    'EXTRA_PP_JS_MODULES': (StrictOrderingOnAppendList, list,
        """Additional JavaScript files to distribute.

        This variable contains a list of files to copy into
        ``$(FINAL_TARGET)/$(JS_MODULES_PATH)``, after preprocessing.
        ``JS_MODULES_PATH`` defaults to ``modules`` if left undefined.
        """, 'libs'),

    'TESTING_JS_MODULES': (HierarchicalStringList, list,
        """JavaScript modules to install in the test-only destination.

        Some JavaScript modules (JSMs) are test-only and not distributed
        with Firefox. This variable defines them.

        To install modules in a subdirectory, use properties of this
        variable to control the final destination. e.g.

        ``TESTING_JS_MODULES.foo += ['module.jsm']``.
        """, 'libs'),

    'EXTRA_PP_COMPONENTS': (StrictOrderingOnAppendList, list,
        """Javascript XPCOM files.

       This variable contains a list of files to preprocess.  Generated
       files will be installed in the ``/components`` directory of the distribution.
        """, 'libs'),

    'FINAL_LIBRARY': (unicode, unicode,
        """Library in which the objects of the current directory will be linked.

        This variable contains the name of a library, defined elsewhere with
        ``LIBRARY_NAME``, in which the objects of the current directory will be
        linked.
        """, 'binaries'),

    'CPP_UNIT_TESTS': (StrictOrderingOnAppendList, list,
        """C++ source files for unit tests.

        This is a list of C++ unit test sources. Entries must be files that
        exist. These generally have ``.cpp`` extensions.
        """, 'binaries'),

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
        """, 'compile'),

    'IS_COMPONENT': (bool, bool,
        """Whether the library contains a binary XPCOM component manifest.
        """, None),

    'PARALLEL_DIRS': (list, list,
        """A parallel version of ``DIRS``.

        Ideally this variable does not exist. It is provided so a transition
        from recursive makefiles can be made. Once the build system has been
        converted to not use Makefile's for the build frontend, this will
        likely go away.
        """, None),

    'HOST_LIBRARY_NAME': (unicode, unicode,
        """Name of target library generated when cross compiling.
        """, 'binaries'),

    'JAVA_JAR_TARGETS': (dict, dict,
        """Defines Java JAR targets to be built.

        This variable should not be populated directly. Instead, it should
        populated by calling add_java_jar().
        """, 'binaries'),

    'JS_MODULES_PATH': (unicode, unicode,
        """Sub-directory of ``$(FINAL_TARGET)`` to install
        ``EXTRA_JS_MODULES``.

        ``EXTRA_JS_MODULES`` files are copied to
        ``$(FINAL_TARGET)/$(JS_MODULES_PATH)``. This variable does not
        need to be defined if the desired destination directory is
        ``$(FINAL_TARGET)/modules``.
        """, None),

    'LIBRARY_NAME': (unicode, unicode,
        """The name of the library generated for a directory.

        In ``example/components/moz.build``,::

           LIBRARY_NAME = 'xpcomsample'

        would generate ``example/components/libxpcomsample.so`` on Linux, or
        ``example/components/xpcomsample.lib`` on Windows.
        """, 'binaries'),

    'LIBS': (StrictOrderingOnAppendList, list,
        """Linker libraries and flags.

        A list of libraries and flags to include when linking.
        """, None),

    'LOCAL_INCLUDES': (StrictOrderingOnAppendList, list,
        """Additional directories to be searched for include files by the compiler.
        """, None),

    'MSVC_ENABLE_PGO': (bool, bool,
        """Whether profile-guided optimization is enabled for MSVC in this directory.
        """, None),

    'NO_PGO': (bool, bool,
        """Whether profile-guided optimization is disable in this directory.
        """, None),

    'NO_VISIBILITY_FLAGS': (bool, bool,
        """Build sources listed in this file without VISIBILITY_FLAGS.
        """, None),

    'OS_LIBS': (list, list,
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

    'SDK_LIBRARY': (StrictOrderingOnAppendList, list,
        """Elements of the distributed SDK.

        Files on this list will be copied into ``SDK_LIB_DIR``
        (``$DIST/sdk/lib``).
        """, None),

    'SIMPLE_PROGRAMS': (StrictOrderingOnAppendList, list,
        """Compile a list of executable names.

        Each name in this variable corresponds to an executable built from the
        corresponding source file with the same base name.

        If the configuration token ``BIN_SUFFIX`` is set, its value will be
        automatically appended to each name. If a name already ends with
        ``BIN_SUFFIX``, the name will remain unchanged.
        """, 'binaries'),

    'HOST_SIMPLE_PROGRAMS': (StrictOrderingOnAppendList, list,
        """Compile a list of host executable names.

        Each name in this variable corresponds to a hosst executable built
        from the corresponding source file with the same base name.

        If the configuration token ``HOST_BIN_SUFFIX`` is set, its value will
        be automatically appended to each name. If a name already ends with
        ``HOST_BIN_SUFFIX``, the name will remain unchanged.
        """, 'binaries'),

    'TOOL_DIRS': (list, list,
        """Like DIRS but for tools.

        Tools are for pieces of the build system that aren't required to
        produce a working binary (in theory). They provide things like test
        code and utilities.
        """, None),

    'TEST_DIRS': (list, list,
        """Like DIRS but only for directories that contain test-only code.

        If tests are not enabled, this variable will be ignored.

        This variable may go away once the transition away from Makefiles is
        complete.
        """, None),

    'TEST_TOOL_DIRS': (list, list,
        """TOOL_DIRS that is only executed if tests are enabled.
        """, None),


    'TIERS': (OrderedDict, dict,
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
        """, 'binaries'),

    'HOST_PROGRAM' : (unicode, unicode,
        """Compiled host executable name.

        If the configuration token ``HOST_BIN_SUFFIX`` is set, its value will be
        automatically appended to ``HOST_PROGRAM``. If ``HOST_PROGRAM`` already
        ends with ``HOST_BIN_SUFFIX``, ``HOST_PROGRAM`` will remain unchanged.
        """, 'binaries'),

    'NO_DIST_INSTALL': (bool, bool,
        """Disable installing certain files into the distribution directory.

        If present, some files defined by other variables won't be
        distributed/shipped with the produced build.
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
        """, 'libs'),

    'XPIDL_MODULE': (unicode, unicode,
        """XPCOM Interface Definition Module Name.

        This is the name of the ``.xpt`` file that is created by linking
        ``XPIDL_SOURCES`` together. If unspecified, it defaults to be the same
        as ``MODULE``.
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

    'CRASHTEST_MANIFESTS': (StrictOrderingOnAppendList, list,
        """List of manifest files defining crashtests.

        These are commonly named crashtests.list.
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
        """, 'libs'),

    'FINAL_TARGET': (FinalTargetValue, unicode,
        """The name of the directory to install targets to.

        The directory is relative to the top of the object directory. The
        default value is dependent on the values of XPI_NAME and DIST_SUBDIR. If
        neither are present, the result is dist/bin. If XPI_NAME is present, the
        result is dist/xpi-stage/$(XPI_NAME). If DIST_SUBDIR is present, then
        the $(DIST_SUBDIR) directory of the otherwise default value is used.
        """, 'libs'),

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

    'CFLAGS': (list, list,
        """Flags passed to the C compiler for all of the C source files
           declared in this directory.

           Note that the ordering of flags matters here, these flags will be
           added to the compiler's command line in the same order as they
           appear in the moz.build file.
        """, 'binaries'),

    'CXXFLAGS': (list, list,
        """Flags passed to the C++ compiler for all of the C++ source files
           declared in this directory.

           Note that the ordering of flags matters here; these flags will be
           added to the compiler's command line in the same order as they
           appear in the moz.build file.
        """, 'binaries'),

    'CMFLAGS': (list, list,
        """Flags passed to the Objective-C compiler for all of the Objective-C
           source files declared in this directory.

           Note that the ordering of flags matters here; these flags will be
           added to the compiler's command line in the same order as they
           appear in the moz.build file.
        """, 'binaries'),

    'CMMFLAGS': (list, list,
        """Flags passed to the Objective-C++ compiler for all of the
           Objective-C++ source files declared in this directory.

           Note that the ordering of flags matters here; these flags will be
           added to the compiler's command line in the same order as they
           appear in the moz.build file.
        """, 'binaries'),

    'LDFLAGS': (list, list,
        """Flags passed to the linker when linking all of the libraries and
           executables declared in this directory.

           Note that the ordering of flags matters here; these flags will be
           added to the linker's command line in the same order as they
           appear in the moz.build file.
        """, 'libs'),

    'EXTRA_DSO_LDOPTS': (list, list,
        """Flags passed to the linker when linking a shared library.

           Note that the ordering of flags matter here, these flags will be
           added to the linker's command line in the same order as they
           appear in the moz.build file.
        """, 'libs'),

    'WIN32_EXE_LDFLAGS': (list, list,
        """Flags passed to the linker when linking a Windows .exe executable
           declared in this directory.

           Note that the ordering of flags matter here, these flags will be
           added to the linker's command line in the same order as they
           appear in the moz.build file.

           This variable only has an effect on Windows.
        """, 'libs'),
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

    'add_java_jar': ('_add_java_jar', (str,),
        """Declare a Java JAR target to be built.

        This is the supported way to populate the JAVA_JAR_TARGETS
        variable.

        The parameters are:
        * dest - target name, without the trailing .jar. (required)

        This returns a rich Java JAR type, described at
        :py:class:`mozbuild.frontend.data.JavaJarData`.
        """),

    'add_android_eclipse_project': ('_add_android_eclipse_project', (str, str),
        """Declare an Android Eclipse project.

        This is one of the supported ways to populate the
        ANDROID_ECLIPSE_PROJECT_TARGETS variable.

        The parameters are:
        * name - project name.
        * manifest - path to AndroidManifest.xml.

        This returns a rich Android Eclipse project type, described at
        :py:class:`mozbuild.frontend.data.AndroidEclipseProjectData`.
        """),

    'add_android_eclipse_library_project': ('_add_android_eclipse_library_project', (str,),
        """Declare an Android Eclipse library project.

        This is one of the supported ways to populate the
        ANDROID_ECLIPSE_PROJECT_TARGETS variable.

        The parameters are:
        * name - project name.

        This returns a rich Android Eclipse project type, described at
        :py:class:`mozbuild.frontend.data.AndroidEclipseProjectData`.
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

    'export': ('_export', (str,),
        """Make the specified variable available to all child directories.

        The variable specified by the argument string is added to the
        environment of all directories specified in the DIRS, PARALLEL_DIRS,
        TOOL_DIRS, TEST_DIRS, and TEST_TOOL_DIRS variables. If those directories
        themselves have child directories, the variable will be exported to all
        of them.

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
