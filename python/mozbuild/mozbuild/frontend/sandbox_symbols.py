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


def doc_to_paragraphs(doc):
    """Take a documentation string and converts it to paragraphs.

    This normalizes the inline strings in VARIABLES and elsewhere in this file.

    It returns a list of paragraphs. It is up to the caller to insert newlines
    or to wrap long lines (e.g. by using textwrap.wrap()).
    """
    lines = [line.strip() for line in doc.split('\n')]

    paragraphs = []
    current = []
    for line in lines:
        if not len(line):
            if len(current):
                paragraphs.append(' '.join(current))
                current = []

            continue

        current.append(line)

    if len(current):
        paragraphs.append(' '.join(current))

    return paragraphs


# This defines the set of mutable global variables.
#
# Each variable is a tuple of:
#
#   (type, default_value, docs)
#
VARIABLES = {
    # Variables controlling reading of other frontend files.
    'DIRS': (list, [],
        """Child directories to descend into looking for build frontend files.

        This works similarly to the DIRS variable in make files. Each str value
        in the list is the name of a child directory. When this file is done
        parsing, the build reader will descend into each listed directory and
        read the frontend file there. If there is no frontend file, an error
        is raised.

        Values are relative paths. They can be multiple directory levels
        above or below. Use ".." for parent directories and "/" for path
        delimiters.
        """),

    'PARALLEL_DIRS': (list, [],
        """A parallel version of DIRS.

        Ideally this variable does not exist. It is provided so a transition
        from recursive makefiles can be made. Once the build system has been
        converted to not use Makefile's for the build frontend, this will
        likely go away.
        """),

    'TOOL_DIRS': (list, [],
        """Like DIRS but for tools.

        Tools are for pieces of the build system that aren't required to
        produce a working binary (in theory). They provide things like test
        code and utilities.
        """),

    'TEST_DIRS': (list, [],
        """Like DIRS but only for directories that contain test-only code.

        If tests are not enabled, this variable will be ignored.

        This variable may go away once the transition away from Makefiles is
        complete.
        """),

    'TEST_TOOL_DIRS': (list, [],
        """TOOL_DIRS that is only executed if tests are enabled.
        """),


    'TIERS': (OrderedDict, OrderedDict(),
        """Defines directories constituting the tier traversal mechanism.

        The recursive make backend iteration is organized into tiers. There are
        major tiers (keys in this dict) that correspond roughly to applications
        or libraries being built. e.g. base, nspr, js, platform, app. Within
        each tier are phases like export, libs, and tools. The recursive make
        backend iterates over each phase in the first tier then proceeds to the
        next tier until all tiers are exhausted.

        Tiers are a way of working around deficiencies in recursive make. These
        will probably disappear once we no longer rely on recursive make for
        the build backend. They will likely be replaced by DIRS.

        This variable is typically not populated directly. Instead, it is
        populated by calling add_tier_dir().
        """),

    'EXTERNAL_MAKE_DIRS': (list, [],
        """Directories that build with make but don't use moz.build files.

        This is like DIRS except it implies that |make| is used to build the
        directory and that the directory does not define itself with moz.build
        files.
        """),

    'PARALLEL_EXTERNAL_MAKE_DIRS': (list, [],
        """Parallel version of EXTERNAL_MAKE_DIRS.
        """),

    'CONFIGURE_SUBST_FILES': (list, [],
        """Output files that will be generated using configure-like substitution.

        This is a substitute for AC_OUTPUT in autoconf. For each path in this
        list, we will search for a file in the srcdir having the name
        {path}.in. The contents of this file will be read and variable patterns
        like @foo@ will be substituted with the values of the AC_SUBST
        variables declared during configure.
        """),

    # IDL Generation.
    'XPIDL_SOURCES': (list, [],
        """XPCOM Interface Definition Files (xpidl).

        This is a list of files that define XPCOM interface definitions.
        Entries must be files that exist. Entries are almost certainly .idl
        files.
        """),

    'XPIDL_MODULE': (unicode, "",
        """XPCOM Interface Definition Module Name.

        This is the name of the .xpt file that is created by linking
        XPIDL_SOURCES together. If unspecified, it defaults to be the same as
        MODULE.
        """),

    'XPIDL_FLAGS': (list, [],
        """XPCOM Interface Definition Module Flags.

        This is a list of extra flags that are passed to the IDL compiler.
        Typically this is a set of -I flags that denote extra include
        directories to search for included .idl files.
        """),
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

        This is similar to a #include in C languages. The filename passed to
        the function will be read and its contents will be evaluated within the
        context of the calling file.

        If a relative path is given, it is evaluated as relative to the file
        currently being processed. If there is a chain of multiple include(),
        the relative path computation is from the most recent/active file.

        If an absolute path is given, it is evaluated from TOPSRCDIR. In other
        words, include('/foo') references the path TOPSRCDIR + '/foo'.

        Example usage
        -------------

        # Include "sibling.build" from the current directory.
        include('sibling.build')

        # Include "foo.build" from a path within the top source directory.
        include('/elsewhere/foo.build')
        """),

    'add_tier_dir': ('_add_tier_directory', (str, [str, list], bool),
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
        -------------

        # Register a single directory with the 'platform' tier.
        add_tier_dir('platform', 'xul')

        # Register multiple directories with the 'app' tier.
        add_tier_dir('app', ['components', 'base'])

        # Register a directory as having static content (no dependencies).
        add_tier_dir('base', 'foo', static=True)
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

        The relative path is from TOPSRCDIR. This is defined as relative to the
        main file being executed, regardless of whether additional files have
        been included using include().
        """),

    'SRCDIR': (str,
        """Constant defining the source directory of this file.

        This is the path inside TOPSRCDIR where this file is located. It is the
        same as TOPSRCDIR + RELATIVEDIR.
        """),

    'OBJDIR': (str,
        """The path to the object directory for this file.

        Is is the same as TOPOBJDIR + RELATIVEDIR.
        """),

    'CONFIG': (dict,
        """Dictionary containing the current configuration variables.

        All the variables defined by the configuration system are available
        through this object. e.g. ENABLE_TESTS, CFLAGS, etc.

        Values in this container are read-only. Attempts at changing values
        will result in a run-time error.

        Access to an unknown variable will return None.
        """),

    '__builtins__': (dict,
        """Exposes Python built-in types.

        The set of exposed Python built-ins is currently:

            True
            False
            None
        """),
}
