# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import ntpath
import os
import posixpath
import re
import sys

from os.path import relpath

from Preprocessor import Preprocessor

from ..util import (
    ensureParentDir,
    FileAvoidWrite,
    ReadOnlyDict,
)


if sys.version_info.major == 2:
    text_type = unicode
else:
    text_type = str


RE_SHELL_ESCAPE = re.compile('''([ \t`#$^&*(){}\\|;'"<>?\[\]])''')


def shell_escape(s):
    """Escape some characters with a backslash, and double dollar signs."""
    return RE_SHELL_ESCAPE.sub(r'\\\1', str(s)).replace('$', '$$')


class BuildConfig(object):
    """Represents the output of configure."""

    def __init__(self):
        self.topsrcdir = None
        self.topobjdir = None
        self.defines = {}
        self.non_global_defines = []
        self.substs = {}
        self.files = []

    @staticmethod
    def from_config_status(path):
        """Create an instance from a config.status file."""

        with open(path, 'rt') as fh:
            source = fh.read()
            code = compile(source, path, 'exec', dont_inherit=1)
            g = {
                '__builtins__': __builtins__,
                '__file__': path,
            }
            l = {}
            exec(code, g, l)

            config = BuildConfig()

            for name in l['__all__']:
                setattr(config, name, l[name])

            return config


class ConfigEnvironment(object):
    """Perform actions associated with a configured but bare objdir.

    The purpose of this class is to preprocess files from the source directory
    and output results in the object directory.

    There are two types of files: config files and config headers,
    each treated through a different member function.

    Creating a ConfigEnvironment requires a few arguments:
      - topsrcdir and topobjdir are, respectively, the top source and
        the top object directory.
      - defines is a list of (name, value) tuples. In autoconf, these are
        set with AC_DEFINE and AC_DEFINE_UNQUOTED
      - non_global_defines are a list of names appearing in defines above
        that are not meant to be exported in ACDEFINES and ALLDEFINES (see
        below)
      - substs is a list of (name, value) tuples. In autoconf, these are
        set with AC_SUBST.

    ConfigEnvironment automatically defines two additional substs variables
    from all the defines not appearing in non_global_defines:
      - ACDEFINES contains the defines in the form -DNAME=VALUE, for use on
        preprocessor command lines. The order in which defines were given
        when creating the ConfigEnvironment is preserved.
      - ALLDEFINES contains the defines in the form #define NAME VALUE, in
        sorted order, for use in config files, for an automatic listing of
        defines.
    and two other additional subst variables from all the other substs:
      - ALLSUBSTS contains the substs in the form NAME = VALUE, in sorted
        order, for use in autoconf.mk. It includes ACDEFINES, but doesn't
        include ALLDEFINES. Only substs with a VALUE are included, such that
        the resulting file doesn't change when new empty substs are added.
        This results in less invalidation of build dependencies in the case
        of autoconf.mk..
      - ALLEMPTYSUBSTS contains the substs with an empty value, in the form
        NAME =.

    ConfigEnvironment expects a "top_srcdir" subst to be set with the top
    source directory, in msys format on windows. It is used to derive a
    "srcdir" subst when treating config files. It can either be an absolute
    path or a path relative to the topobjdir.
    """

    def __init__(self, topsrcdir, topobjdir, defines=[], non_global_defines=[],
            substs=[]):

        self.defines = ReadOnlyDict(defines)
        self.substs = dict(substs)
        self.topsrcdir = topsrcdir
        self.topobjdir = topobjdir
        global_defines = [name for name, value in defines
            if not name in non_global_defines]
        self.substs['ACDEFINES'] = ' '.join(['-D%s=%s' % (name,
            shell_escape(self.defines[name])) for name in global_defines])
        self.substs['ALLSUBSTS'] = '\n'.join(sorted(['%s = %s' % (name,
            self.substs[name]) for name in self.substs if self.substs[name]]))
        self.substs['ALLEMPTYSUBSTS'] = '\n'.join(sorted(['%s =' % name
            for name in self.substs if not self.substs[name]]))
        self.substs['ALLDEFINES'] = '\n'.join(sorted(['#define %s %s' % (name,
            self.defines[name]) for name in global_defines]))

        self.substs = ReadOnlyDict(self.substs)

        # Populate a Unicode version of substs. This is an optimization to make
        # moz.build reading faster, since each sandbox needs a Unicode version
        # of these variables and doing it over a thousand times is a hotspot
        # during sandbox execution!
        # Bug 844509 tracks moving everything to Unicode.
        self.substs_unicode = {}
        for k, v in self.substs.items():
            if not isinstance(v, text_type):
                try:
                    v = v.decode('utf-8')
                except UnicodeDecodeError:
                    v = v.decode('utf-8', 'replace')

            self.substs_unicode[k] = v

        self.substs_unicode = ReadOnlyDict(self.substs_unicode)

    @staticmethod
    def from_config_status(path):
        config = BuildConfig.from_config_status(path)

        return ConfigEnvironment(config.topsrcdir, config.topobjdir,
            config.defines, config.non_global_defines, config.substs)

    def get_relative_srcdir(self, file):
        '''Returns the relative source directory for the given file, always
        using / as a path separator.
        '''
        assert(isinstance(file, basestring))
        dir = posixpath.dirname(relpath(file, self.topobjdir).replace(os.sep, '/'))
        if dir:
            return dir
        return '.'

    def get_top_srcdir(self, file):
        '''Returns a normalized top_srcdir for the given file: if
        substs['top_srcdir'] is a relative path, it is relative to the
        topobjdir. Adjust it to be relative to the file path.'''
        top_srcdir = self.substs['top_srcdir']
        if posixpath.isabs(top_srcdir) or ntpath.isabs(top_srcdir):
            return top_srcdir
        return posixpath.normpath(posixpath.join(self.get_depth(file), top_srcdir))

    def get_file_srcdir(self, file):
        '''Returns the srcdir for the given file, where srcdir is in msys
        format on windows, thus derived from top_srcdir.
        '''
        dir = self.get_relative_srcdir(file)
        top_srcdir = self.get_top_srcdir(file)
        return posixpath.normpath(posixpath.join(top_srcdir, dir))

    def get_depth(self, file):
        '''Returns the DEPTH for the given file, that is, the path to the
        object directory relative to the directory containing the given file.
        Always uses / as a path separator.
        '''
        return relpath(self.topobjdir, os.path.dirname(file)).replace(os.sep, '/')

    def get_input(self, file):
        '''Returns the input file path in the source tree that can be used
        to create the given config file or header.
        '''
        assert(isinstance(file, basestring))
        return os.path.normpath(os.path.join(self.topsrcdir, "%s.in" % relpath(file, self.topobjdir)))

    def create_config_file(self, path, extra=None):
        '''Creates the given config file. A config file is generated by
        taking the corresponding source file and replacing occurences of
        "@VAR@" by the value corresponding to "VAR" in the substs dict.

        Additional substs are defined according to the file being treated:
            "srcdir" for its the path to its source directory
            "relativesrcdir" for its source directory relative to the top
            "DEPTH" for the path to the top object directory
        '''
        input = self.get_input(path)
        pp = Preprocessor()
        pp.context.update(self.substs)
        pp.context.update(top_srcdir = self.get_top_srcdir(path))
        pp.context.update(srcdir = self.get_file_srcdir(path))
        pp.context.update(relativesrcdir = self.get_relative_srcdir(path))
        pp.context.update(DEPTH = self.get_depth(path))
        if extra:
            pp.context.update(extra)
        pp.do_filter('attemptSubstitution')
        pp.setMarker(None)

        pp.out = FileAvoidWrite(path)
        pp.do_include(input)
        return pp.out.close()

    def create_config_header(self, path):
        '''Creates the given config header. A config header is generated by
        taking the corresponding source file and replacing some #define/#undef
        occurences:
            "#undef NAME" is turned into "#define NAME VALUE"
            "#define NAME" is unchanged
            "#define NAME ORIGINAL_VALUE" is turned into "#define NAME VALUE"
            "#undef UNKNOWN_NAME" is turned into "/* #undef UNKNOWN_NAME */"
            Whitespaces are preserved.
        '''
        with open(self.get_input(path), 'rU') as input:
            ensureParentDir(path)
            output = FileAvoidWrite(path)
            r = re.compile('^\s*#\s*(?P<cmd>[a-z]+)(?:\s+(?P<name>\S+)(?:\s+(?P<value>\S+))?)?', re.U)
            for l in input:
                m = r.match(l)
                if m:
                    cmd = m.group('cmd')
                    name = m.group('name')
                    value = m.group('value')
                    if name:
                        if name in self.defines:
                            if cmd == 'define' and value:
                                l = l[:m.start('value')] + str(self.defines[name]) + l[m.end('value'):]
                            elif cmd == 'undef':
                                l = l[:m.start('cmd')] + 'define' + l[m.end('cmd'):m.end('name')] + ' ' + str(self.defines[name]) + l[m.end('name'):]
                        elif cmd == 'undef':
                           l = '/* ' + l[:m.end('name')] + ' */' + l[m.end('name'):]

                output.write(l)
            return output.close()
