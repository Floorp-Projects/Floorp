# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

# This modules provides functionality for dealing with code completion.

import os

from mozbuild.base import MozbuildObject
from mozbuild.compilation import util
from mozbuild.backend.common import CommonBackend
from mozbuild.frontend.data import (
    Sources,
    HostSources,
    UnifiedSources,
    GeneratedSources,
)
from mozbuild.shellutil import (
    split as shell_split,
    quote as shell_quote,
)
from mach.config import ConfigSettings
from mach.logging import LoggingManager


class CompileDBBackend(CommonBackend):
    def _init(self):
        CommonBackend._init(self)
        if not util.check_top_objdir(self.environment.topobjdir):
            raise Exception()

        # The database we're going to dump out to.
        self._db = []

        # The cache for per-directory flags
        self._flags = {}

        log_manager = LoggingManager()
        self._cmd = MozbuildObject(self.environment.topsrcdir, ConfigSettings(),
                                   log_manager, self.environment.topobjdir)


    def consume_object(self, obj):
        consumed = CommonBackend.consume_object(self, obj)

        if consumed:
            return True

        if isinstance(obj, Sources) or isinstance(obj, HostSources) or \
           isinstance(obj, GeneratedSources):
            # For other sources, include each source file.
            for f in obj.files:
                flags = self._get_dir_flags(obj.objdir)
                self._build_db_line(obj.objdir, self.environment, f,
                                    obj.canonical_suffix, flags,
                                    isinstance(obj, HostSources))

        return True

    def consume_finished(self):
        CommonBackend.consume_finished(self)

        import json
        # Output the database (a JSON file) to objdir/compile_commands.json
        outputfile = os.path.join(self.environment.topobjdir, 'compile_commands.json')
        with self._write_file(outputfile) as jsonout:
            json.dump(self._db, jsonout, indent=0)

    def _process_unified_sources(self, obj):
        # For unified sources, only include the unified source file.
        # Note that unified sources are never used for host sources.
        for f in obj.unified_source_mapping:
            flags = self._get_dir_flags(obj.objdir)
            self._build_db_line(obj.objdir, self.environment, f[0],
                                obj.canonical_suffix, flags, False)

    def _handle_idl_manager(self, idl_manager):
        pass

    def _handle_ipdl_sources(self, ipdl_dir, sorted_ipdl_sources,
                             unified_ipdl_cppsrcs_mapping):
        flags = self._get_dir_flags(ipdl_dir)
        for f in unified_ipdl_cppsrcs_mapping:
            self._build_db_line(ipdl_dir, self.environment, f[0],
                                '.cpp', flags, False)

    def _handle_webidl_build(self, bindings_dir, unified_source_mapping,
                             webidls, expected_build_output_files,
                             global_define_files):
        flags = self._get_dir_flags(bindings_dir)
        for f in unified_source_mapping:
            self._build_db_line(bindings_dir, self.environment, f[0],
                                '.cpp', flags, False)

    def _get_dir_flags(self, directory):
        if directory in self._flags:
            return self._flags[directory]

        from mozbuild.util import resolve_target_to_make

        make_dir, make_target = resolve_target_to_make(self.environment.topobjdir, directory)
        if make_dir is None and make_target is None:
            raise Exception('Cannot figure out the make dir and target for ' + directory)

        build_vars = util.get_build_vars(directory, self._cmd)

        # We only care about the following build variables.
        for name in ('COMPILE_CFLAGS', 'COMPILE_CXXFLAGS',
                     'COMPILE_CMFLAGS', 'COMPILE_CMMFLAGS'):
            if name not in build_vars:
                continue

            build_vars[name] = shell_split(build_vars[name])

        self._flags[directory] = build_vars
        return self._flags[directory]

    def _build_db_line(self, objdir, cenv, filename, canonical_suffix, flags, ishost):
        # Distinguish between host and target files.
        prefix = 'HOST_' if ishost else ''
        if canonical_suffix == '.c':
            compiler = cenv.substs[prefix + 'CC']
            cflags = list(flags['COMPILE_CFLAGS'])
            # Add the Objective-C flags if needed.
            if filename.endswith('.m'):
                cflags.extend(flags['COMPILE_CMFLAGS'])
        elif canonical_suffix == '.cpp':
            compiler = cenv.substs[prefix + 'CXX']
            cflags = list(flags['COMPILE_CXXFLAGS'])
            # Add the Objective-C++ flags if needed.
            if filename.endswith('.mm'):
                cflags.extend(flags['COMPILE_CMMFLAGS'])
        else:
            return

        cmd = compiler.split() + [
          '-o', '/dev/null', '-c'
        ] + cflags + [ filename ]

        self._db.append({
            'directory': objdir,
            'command': ' '.join(shell_quote(a) for a in cmd),
            'file': filename
        })

