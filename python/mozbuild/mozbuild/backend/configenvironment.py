# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import os
import sys

from collections import Iterable
from types import StringTypes

import mozpack.path as mozpath

from mozbuild.util import ReadOnlyDict
from mozbuild.shellutil import quote as shell_quote


if sys.version_info.major == 2:
    text_type = unicode
else:
    text_type = str


class BuildConfig(object):
    """Represents the output of configure."""

    _CODE_CACHE = {}

    def __init__(self):
        self.topsrcdir = None
        self.topobjdir = None
        self.defines = {}
        self.non_global_defines = []
        self.substs = {}
        self.files = []

    @classmethod
    def from_config_status(cls, path):
        """Create an instance from a config.status file."""
        code_cache = cls._CODE_CACHE
        mtime = os.path.getmtime(path)

        # cache the compiled code as it can be reused
        # we cache it the first time, or if the file changed
        if not path in code_cache or code_cache[path][0] != mtime:
            with open(path, 'rt') as fh:
                source = fh.read()
                code_cache[path] = (
                    mtime,
                    compile(source, path, 'exec', dont_inherit=1)
                )

        g = {
            '__builtins__': __builtins__,
            '__file__': path,
        }
        l = {}
        exec(code_cache[path][1], g, l)

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
        that are not meant to be exported in ACDEFINES (see below)
      - substs is a list of (name, value) tuples. In autoconf, these are
        set with AC_SUBST.

    ConfigEnvironment automatically defines one additional substs variable
    from all the defines not appearing in non_global_defines:
      - ACDEFINES contains the defines in the form -DNAME=VALUE, for use on
        preprocessor command lines. The order in which defines were given
        when creating the ConfigEnvironment is preserved.
    and two other additional subst variables from all the other substs:
      - ALLSUBSTS contains the substs in the form NAME = VALUE, in sorted
        order, for use in autoconf.mk. It includes ACDEFINES
        Only substs with a VALUE are included, such that the resulting file
        doesn't change when new empty substs are added.
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
        substs=[], source=None):

        if not source:
            source = mozpath.join(topobjdir, 'config.status')
        self.source = source
        self.defines = ReadOnlyDict(defines)
        self.non_global_defines = non_global_defines
        self.substs = dict(substs)
        self.topsrcdir = mozpath.abspath(topsrcdir)
        self.topobjdir = mozpath.abspath(topobjdir)
        self.lib_prefix = self.substs.get('LIB_PREFIX', '')
        if 'LIB_SUFFIX' in self.substs:
            self.lib_suffix = '.%s' % self.substs['LIB_SUFFIX']
        self.dll_prefix = self.substs.get('DLL_PREFIX', '')
        self.dll_suffix = self.substs.get('DLL_SUFFIX', '')
        if self.substs.get('IMPORT_LIB_SUFFIX'):
            self.import_prefix = self.lib_prefix
            self.import_suffix = '.%s' % self.substs['IMPORT_LIB_SUFFIX']
        else:
            self.import_prefix = self.dll_prefix
            self.import_suffix = self.dll_suffix

        global_defines = [name for name, value in defines
            if not name in non_global_defines]
        self.substs['ACDEFINES'] = ' '.join(['-D%s=%s' % (name,
            shell_quote(self.defines[name]).replace('$', '$$')) for name in global_defines])
        def serialize(obj):
            if isinstance(obj, StringTypes):
                return obj
            if isinstance(obj, Iterable):
                return ' '.join(obj)
            raise Exception('Unhandled type %s', type(obj))
        self.substs['ALLSUBSTS'] = '\n'.join(sorted(['%s = %s' % (name,
            serialize(self.substs[name])) for name in self.substs if self.substs[name]]))
        self.substs['ALLEMPTYSUBSTS'] = '\n'.join(sorted(['%s =' % name
            for name in self.substs if not self.substs[name]]))

        self.substs = ReadOnlyDict(self.substs)

        self.external_source_dir = None
        external = self.substs.get('EXTERNAL_SOURCE_DIR', '')
        if external:
            external = mozpath.normpath(external)
            if not os.path.isabs(external):
                external = mozpath.join(self.topsrcdir, external)
            self.external_source_dir = mozpath.normpath(external)

        # Populate a Unicode version of substs. This is an optimization to make
        # moz.build reading faster, since each sandbox needs a Unicode version
        # of these variables and doing it over a thousand times is a hotspot
        # during sandbox execution!
        # Bug 844509 tracks moving everything to Unicode.
        self.substs_unicode = {}

        def decode(v):
            if not isinstance(v, text_type):
                try:
                    return v.decode('utf-8')
                except UnicodeDecodeError:
                    return v.decode('utf-8', 'replace')

        for k, v in self.substs.items():
            if not isinstance(v, StringTypes):
                if isinstance(v, Iterable):
                    type(v)(decode(i) for i in v)
            elif not isinstance(v, text_type):
                v = decode(v)

            self.substs_unicode[k] = v

        self.substs_unicode = ReadOnlyDict(self.substs_unicode)

    @staticmethod
    def from_config_status(path):
        config = BuildConfig.from_config_status(path)

        return ConfigEnvironment(config.topsrcdir, config.topobjdir,
            config.defines, config.non_global_defines, config.substs, path)
