# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, unicode_literals

import os
import json
import sys

import mozpack.path as mozpath
from mozbuild import shellutil
from mozbuild.base import MozbuildObject
from mozbuild.backend.base import PartialBackend, HybridBackend
from mozbuild.backend.recursivemake import RecursiveMakeBackend
from mozbuild.mozconfig import MozconfigLoader
from mozbuild.shellutil import quote as shell_quote
from mozbuild.util import OrderedDefaultDict
from collections import defaultdict
import multiprocessing

from mozpack.files import (
    FileFinder,
)

from .common import CommonBackend
from ..frontend.data import (
    ChromeManifestEntry,
    ComputedFlags,
    ContextDerived,
    Defines,
    DirectoryTraversal,
    FinalTargetFiles,
    FinalTargetPreprocessedFiles,
    GeneratedFile,
    GeneratedSources,
    HostDefines,
    HostSources,
    JARManifest,
    ObjdirFiles,
    PerSourceFlag,
    Program,
    SimpleProgram,
    HostLibrary,
    HostProgram,
    HostSimpleProgram,
    SharedLibrary,
    Sources,
    StaticLibrary,
    VariablePassthru,
)
from ..util import (
    FileAvoidWrite,
    expand_variables,
)
from ..frontend.context import (
    AbsolutePath,
    ObjDirPath,
    RenamedSourcePath,
)


class BackendTupfile(object):
    """Represents a generated Tupfile.
    """

    def __init__(self, objdir, environment, topsrcdir, topobjdir, dry_run):
        self.topsrcdir = topsrcdir
        self.objdir = objdir
        self.relobjdir = mozpath.relpath(objdir, topobjdir)
        self.environment = environment
        self.name = mozpath.join(objdir, 'Tupfile')
        self.rules_included = False
        self.defines = []
        self.host_defines = []
        self.outputs = set()
        self.delayed_generated_files = []
        self.delayed_installed_files = []
        self.per_source_flags = defaultdict(list)
        self.local_flags = defaultdict(list)
        self.sources = defaultdict(list)
        self.host_sources = defaultdict(list)
        self.variables = {}
        self.static_lib = None
        self.shared_lib = None
        self.programs = []
        self.host_programs = []
        self.host_library = None
        self.exports = set()

        # These files are special, ignore anything that generates them or
        # depends on them.
        self._skip_files = [
            'signmar',
            'libxul.so',
            'libtestcrasher.so',
        ]

        self.fh = FileAvoidWrite(self.name, capture_diff=True, dry_run=dry_run)
        self.fh.write('# THIS FILE WAS AUTOMATICALLY GENERATED. DO NOT EDIT.\n')
        self.fh.write('\n')

    def write(self, buf):
        self.fh.write(buf)

    def include_rules(self):
        if not self.rules_included:
            self.write('include_rules\n')
            self.rules_included = True

    def rule(self, cmd, inputs=None, outputs=None, display=None,
             extra_inputs=None, extra_outputs=None, check_unchanged=False):
        inputs = inputs or []
        outputs = outputs or []

        for f in inputs + outputs:
            if any(f.endswith(skip_file) for skip_file in self._skip_files):
                return

        display = display or ""
        self.include_rules()
        flags = ""
        if check_unchanged:
            # This flag causes tup to compare the outputs with the previous run
            # of the command, and skip the rest of the DAG for any that are the
            # same.
            flags += "o"

        if display:
            caret_text = flags + ' ' + display
        else:
            caret_text = flags

        self.write(': %(inputs)s%(extra_inputs)s |> %(display)s%(cmd)s |> %(outputs)s%(extra_outputs)s\n' % {
            'inputs': ' '.join(inputs),
            'extra_inputs': ' | ' + ' '.join(extra_inputs) if extra_inputs else '',
            'display': '^%s^ ' % caret_text if caret_text else '',
            'cmd': ' '.join(cmd),
            'outputs': ' '.join(outputs),
            'extra_outputs': ' | ' + ' '.join(extra_outputs) if extra_outputs else '',
        })

        self.outputs.update(outputs)

    def symlink_rule(self, source, output=None, output_group=None):
        outputs = [output] if output else [mozpath.basename(source)]
        if output_group:
            outputs.append(output_group)

        # The !tup_ln macro does a symlink or file copy (depending on the
        # platform) without shelling out to a subprocess.
        self.rule(
            cmd=['!tup_ln'],
            inputs=[source],
            outputs=outputs,
        )

    def gen_sources_rules(self, extra_inputs):
        sources = self.sources
        host_sources = self.host_sources
        as_dash_c = self.variables.get('AS_DASH_C_FLAG', self.environment.substs['AS_DASH_C_FLAG'])
        compilers = [
            (sources['.S'], 'AS', 'SFLAGS', '-c', ''),
            (sources['.s'], 'AS', 'ASFLAGS', as_dash_c, ''),
            (sources['.cpp'], 'CXX', 'CXXFLAGS', '-c', ''),
            (sources['.c'], 'CC', 'CFLAGS', '-c', ''),
            (host_sources['.cpp'], 'HOST_CXX', 'HOST_CXXFLAGS', '-c', 'host_'),
            (host_sources['.c'], 'HOST_CC', 'HOST_CFLAGS', '-c', 'host_'),
        ]
        for srcs, compiler, flags, dash_c, prefix in compilers:
            for src in sorted(srcs):
                # AS can be set to $(CC), so we need to call expand_variables on
                # the compiler to get the real value.
                compiler_value = self.variables.get(compiler, self.environment.substs[compiler])
                cmd = [expand_variables(compiler_value, self.environment.substs)]
                cmd.extend(shell_quote(f) for f in self.local_flags[flags])
                cmd.extend(shell_quote(f) for f in self.per_source_flags[src])
                cmd.extend([dash_c, '%f', '-o', '%o'])
                self.rule(
                    cmd=cmd,
                    inputs=[src],
                    extra_inputs=extra_inputs,
                    outputs=[prefix + '%B.o'],
                    display='%s %%f' % compiler,
                )

    def export(self, env):
        for var in env:
            if var not in self.exports:
                self.exports.add(var)
                self.write('export %s\n' % var)

    def export_shell(self):
        # These are used by mach/mixin/process.py to determine the current
        # shell.
        self.export(['SHELL', 'MOZILLABUILD', 'COMSPEC'])

    def close(self):
        return self.fh.close()

    def requires_delay(self, inputs):
        # We need to delay the generated file rule in the Tupfile until the
        # generated inputs in the current directory are processed. We do this by
        # checking all ObjDirPaths to make sure they are in
        # self.outputs, or are in other directories.
        for f in inputs:
            if (isinstance(f, ObjDirPath) and
                f.target_basename not in self.outputs and
                mozpath.dirname(f.full_path) == self.objdir):
                return True
        return False

    @property
    def diff(self):
        return self.fh.diff


class TupBackend(CommonBackend):
    """Backend that generates Tupfiles for the tup build system.
    """

    def _init(self):
        CommonBackend._init(self)

        self._backend_files = {}
        self._cmd = MozbuildObject.from_environment()
        self._manifest_entries = OrderedDefaultDict(set)

        # These are a hack to approximate things that are needed for the
        # compile phase.
        self._compile_env_files = (
            '*.api',
            '*.c',
            '*.cfg',
            '*.cpp',
            '*.h',
            '*.inc',
            '*.msg',
            '*.py',
            '*.rs',
        )

        # These are 'group' dependencies - All rules that list these as an output
        # will be built before any rules that list this as an input.
        self._installed_idls = '$(MOZ_OBJ_ROOT)/<installed-idls>'
        self._installed_files = '$(MOZ_OBJ_ROOT)/<installed-files>'
        # The preprocessor including source-repo.h and buildid.h creates
        # dependencies that aren't specified by moz.build and cause errors
        # in Tup. Express these as a group dependency.
        self._early_generated_files = '$(MOZ_OBJ_ROOT)/<early-generated-files>'

        self._built_in_addons = set()
        self._built_in_addons_file = 'dist/bin/browser/chrome/browser/content/browser/built_in_addons.json'

    def _get_mozconfig_env(self, config):
        env = {}
        loader = MozconfigLoader(config.topsrcdir)
        mozconfig = loader.read_mozconfig(config.substs['MOZCONFIG'])
        make_extra = mozconfig['make_extra'] or []
        env = {}
        for line in make_extra:
            if line.startswith('export '):
                line = line[len('export '):]
            key, value = line.split('=')
            env[key] = value
        return env

    def build(self, config, output, jobs, verbose, what=None):
        if not what:
            what = [self.environment.topobjdir]
        args = [self.environment.substs['TUP'], 'upd'] + what
        if self.environment.substs.get('MOZ_AUTOMATION'):
            args += ['--quiet']
        if verbose:
            args += ['--verbose']
        if jobs > 0:
            args += ['-j%d' % jobs]
        else:
            args += ['-j%d' % multiprocessing.cpu_count()]
        return config.run_process(args=args,
                                  line_handler=output.on_line,
                                  ensure_exit_code=False,
                                  append_env=self._get_mozconfig_env(config))

    def _get_backend_file(self, relobjdir):
        objdir = mozpath.normpath(mozpath.join(self.environment.topobjdir, relobjdir))
        if objdir not in self._backend_files:
            self._backend_files[objdir] = \
                    BackendTupfile(objdir, self.environment,
                                   self.environment.topsrcdir, self.environment.topobjdir,
                                   self.dry_run)
        return self._backend_files[objdir]

    def _get_backend_file_for(self, obj):
        return self._get_backend_file(obj.relobjdir)

    def _py_action(self, action):
        cmd = [
            '$(PYTHON)',
            '-m',
            'mozbuild.action.%s' % action,
        ]
        return cmd

    def _lib_paths(self, objdir, libs):
        return [mozpath.relpath(mozpath.join(l.objdir, l.import_name), objdir)
                for l in libs]

    def _gen_shared_library(self, backend_file):
        shlib = backend_file.shared_lib

        if shlib.cxx_link:
            mkshlib = (
                [backend_file.environment.substs['CXX']] +
                backend_file.local_flags['CXX_LDFLAGS']
            )
        else:
            mkshlib = (
                [backend_file.environment.substs['CC']] +
                backend_file.local_flags['C_LDFLAGS']
            )

        mkshlib += (
            backend_file.environment.substs['DSO_PIC_CFLAGS'] +
            [backend_file.environment.substs['DSO_LDOPTS']] +
            ['-Wl,-h,%s' % shlib.soname] +
            ['-o', shlib.lib_name]
        )

        objs, _, shared_libs, os_libs, static_libs = self._expand_libs(shlib)
        static_libs = self._lib_paths(backend_file.objdir, static_libs)
        shared_libs = self._lib_paths(backend_file.objdir, shared_libs)

        list_file_name = '%s.list' % shlib.name.replace('.', '_')
        list_file = self._make_list_file(backend_file.objdir, objs, list_file_name)

        inputs = objs + static_libs + shared_libs

        symbols_file = []
        if shlib.symbols_file:
            inputs.append(shlib.symbols_file)
            # TODO: Assumes GNU LD
            symbols_file = ['-Wl,--version-script,%s' % shlib.symbols_file]

        cmd = (
            mkshlib +
            [list_file] +
            backend_file.local_flags['LDFLAGS'] +
            static_libs +
            shared_libs +
            symbols_file +
            [backend_file.environment.substs['OS_LIBS']] +
            os_libs
        )
        backend_file.rule(
            cmd=cmd,
            inputs=inputs,
            outputs=[shlib.lib_name],
            display='LINK %o'
        )
        backend_file.symlink_rule(mozpath.join(backend_file.objdir,
                                               shlib.lib_name),
                                  output=mozpath.join(self.environment.topobjdir,
                                                      shlib.install_target,
                                                      shlib.lib_name))

    def _gen_programs(self, backend_file):
        for p in backend_file.programs:
            self._gen_program(backend_file, p)

    def _gen_program(self, backend_file, prog):
        cc_or_cxx = 'CXX' if prog.cxx_link else 'CC'
        objs, _, shared_libs, os_libs, static_libs = self._expand_libs(prog)
        static_libs = self._lib_paths(backend_file.objdir, static_libs)
        shared_libs = self._lib_paths(backend_file.objdir, shared_libs)

        inputs = objs + static_libs + shared_libs

        list_file_name = '%s.list' % prog.name.replace('.', '_')
        list_file = self._make_list_file(backend_file.objdir, objs, list_file_name)

        if isinstance(prog, SimpleProgram):
            outputs = [prog.name]
        else:
            outputs = [mozpath.relpath(prog.output_path.full_path,
                                       backend_file.objdir)]

        cmd = (
            [backend_file.environment.substs[cc_or_cxx], '-o', '%o'] +
            backend_file.local_flags['CXX_LDFLAGS'] +
            [list_file] +
            backend_file.local_flags['LDFLAGS'] +
            static_libs +
            [backend_file.environment.substs['MOZ_PROGRAM_LDFLAGS']] +
            shared_libs +
            [backend_file.environment.substs['OS_LIBS']] +
            os_libs
        )
        backend_file.rule(
            cmd=cmd,
            inputs=inputs,
            outputs=outputs,
            display='LINK %o'
        )


    def _gen_host_library(self, backend_file):
        objs = backend_file.host_library.objs
        inputs = objs
        outputs = [backend_file.host_library.name]
        cmd = (
            [backend_file.environment.substs['HOST_AR']] +
            [backend_file.environment.substs['HOST_AR_FLAGS'].replace('$@', '%o')] +
            objs
        )
        backend_file.rule(
            cmd=cmd,
            inputs=inputs,
            outputs=outputs,
            display='AR %o'
        )


    def _gen_host_programs(self, backend_file):
        for p in backend_file.host_programs:
            self._gen_host_program(backend_file, p)


    def _gen_host_program(self, backend_file, prog):
        _, _, _, extra_libs, _ = self._expand_libs(prog)
        objs = prog.objs
        outputs = [prog.program]
        host_libs = []
        for lib in prog.linked_libraries:
            if isinstance(lib, HostLibrary):
                host_libs.append(lib)
        host_libs = self._lib_paths(backend_file.objdir, host_libs)

        inputs = objs + host_libs
        use_cxx = any(f.endswith(('.cc', '.cpp')) for f in prog.source_files())
        cc_or_cxx = 'HOST_CXX' if use_cxx else 'HOST_CC'
        cmd = (
            [backend_file.environment.substs[cc_or_cxx], '-o', '%o'] +
            backend_file.local_flags['HOST_CXX_LDFLAGS'] +
            backend_file.local_flags['HOST_LDFLAGS'] +
            objs +
            host_libs +
            extra_libs
        )
        backend_file.rule(
            cmd=cmd,
            inputs=inputs,
            outputs=outputs,
            display='LINK %o'
        )


    def _gen_static_library(self, backend_file):
        ar = [
            backend_file.environment.substs['AR'],
            backend_file.environment.substs['AR_FLAGS'].replace('$@', '%o')
        ]

        objs, _, shared_libs, _, static_libs = self._expand_libs(backend_file.static_lib)
        static_libs = self._lib_paths(backend_file.objdir, static_libs)
        shared_libs = self._lib_paths(backend_file.objdir, shared_libs)

        inputs = objs + static_libs

        cmd = (
            ar +
            inputs
        )

        backend_file.rule(
            cmd=cmd,
            inputs=inputs,
            outputs=[backend_file.static_lib.name],
            display='AR %o'
        )


    def consume_object(self, obj):
        """Write out build files necessary to build with tup."""

        if not isinstance(obj, ContextDerived):
            return False

        consumed = CommonBackend.consume_object(self, obj)
        if consumed:
            return True

        backend_file = self._get_backend_file_for(obj)

        if isinstance(obj, GeneratedFile):
            skip_files = []

            if self.environment.is_artifact_build:
                skip_files = self._compile_env_gen

            for f in obj.outputs:
                if any(mozpath.match(f, p) for p in skip_files):
                    return False

            if backend_file.requires_delay(obj.inputs):
                backend_file.delayed_generated_files.append(obj)
            else:
                self._process_generated_file(backend_file, obj)
        elif (isinstance(obj, ChromeManifestEntry) and
              obj.install_target.startswith('dist/bin')):
            top_level = mozpath.join(obj.install_target, 'chrome.manifest')
            if obj.path != top_level:
                entry = 'manifest %s' % mozpath.relpath(obj.path,
                                                        obj.install_target)
                self._manifest_entries[top_level].add(entry)
            self._manifest_entries[obj.path].add(str(obj.entry))
        elif isinstance(obj, Defines):
            self._process_defines(backend_file, obj)
        elif isinstance(obj, HostDefines):
            self._process_defines(backend_file, obj, host=True)
        elif isinstance(obj, FinalTargetFiles):
            self._process_final_target_files(obj)
        elif isinstance(obj, FinalTargetPreprocessedFiles):
            self._process_final_target_pp_files(obj, backend_file)
        elif isinstance(obj, JARManifest):
            self._consume_jar_manifest(obj)
        elif isinstance(obj, PerSourceFlag):
            backend_file.per_source_flags[obj.file_name].extend(obj.flags)
        elif isinstance(obj, ComputedFlags):
            self._process_computed_flags(obj, backend_file)
        elif isinstance(obj, (Sources, GeneratedSources)):
            backend_file.sources[obj.canonical_suffix].extend(obj.files)
        elif isinstance(obj, HostSources):
            backend_file.host_sources[obj.canonical_suffix].extend(obj.files)
        elif isinstance(obj, VariablePassthru):
            backend_file.variables = obj.variables
        elif isinstance(obj, StaticLibrary):
            backend_file.static_lib = obj
        elif isinstance(obj, SharedLibrary):
            backend_file.shared_lib = obj
        elif isinstance(obj, (HostProgram, HostSimpleProgram)):
            backend_file.host_programs.append(obj)
        elif isinstance(obj, HostLibrary):
            backend_file.host_library = obj
        elif isinstance(obj, (Program, SimpleProgram)):
            backend_file.programs.append(obj)
        elif isinstance(obj, DirectoryTraversal):
            pass

        return True

    def consume_finished(self):
        CommonBackend.consume_finished(self)

        # The approach here is similar to fastermake.py, but we
        # simply write out the resulting files here.
        for target, entries in self._manifest_entries.iteritems():
            with self._write_file(mozpath.join(self.environment.topobjdir,
                                               target)) as fh:
                fh.write(''.join('%s\n' % e for e in sorted(entries)))

        if self._built_in_addons:
            with self._write_file(mozpath.join(self.environment.topobjdir,
                                               self._built_in_addons_file)) as fh:
                json.dump({'system': sorted(list(self._built_in_addons))}, fh)

        for objdir, backend_file in sorted(self._backend_files.items()):
            backend_file.gen_sources_rules([self._installed_files])
            for var, gen_method in ((backend_file.shared_lib, self._gen_shared_library),
                                    (backend_file.static_lib and backend_file.static_lib.no_expand_lib,
                                     self._gen_static_library),
                                    (backend_file.programs, self._gen_programs),
                                    (backend_file.host_programs, self._gen_host_programs),
                                    (backend_file.host_library, self._gen_host_library)):
                if var:
                    backend_file.export_shell()
                    gen_method(backend_file)
            for obj in backend_file.delayed_generated_files:
                self._process_generated_file(backend_file, obj)
            for path, output, output_group in backend_file.delayed_installed_files:
                backend_file.symlink_rule(path, output=output, output_group=output_group)
            with self._write_file(fh=backend_file):
                pass

        with self._write_file(mozpath.join(self.environment.topobjdir, 'Tuprules.tup')) as fh:
            acdefines_flags = ' '.join(['-D%s=%s' % (name, shell_quote(value))
                for (name, value) in sorted(self.environment.acdefines.iteritems())])
            # TODO: AB_CD only exists in Makefiles at the moment.
            acdefines_flags += ' -DAB_CD=en-US'

            # TODO: BOOKMARKS_INCLUDE_DIR is used by bookmarks.html.in, and is
            # only defined in browser/locales/Makefile.in
            acdefines_flags += ' -DBOOKMARKS_INCLUDE_DIR=%s/browser/locales/en-US/profile' % self.environment.topsrcdir

            # Use BUILD_FASTER to avoid CXXFLAGS/CPPFLAGS in
            # toolkit/content/buildconfig.html
            acdefines_flags += ' -DBUILD_FASTER=1'

            fh.write('MOZ_OBJ_ROOT = $(TUP_CWD)\n')
            fh.write('DIST = $(MOZ_OBJ_ROOT)/dist\n')
            fh.write('ACDEFINES = %s\n' % acdefines_flags)
            fh.write('topsrcdir = $(MOZ_OBJ_ROOT)/%s\n' % (
                os.path.relpath(self.environment.topsrcdir, self.environment.topobjdir)
            ))
            fh.write('PYTHON = PYTHONDONTWRITEBYTECODE=1 %s\n' % self.environment.substs['PYTHON'])
            fh.write('PYTHON_PATH = $(PYTHON) $(topsrcdir)/config/pythonpath.py\n')
            fh.write('PLY_INCLUDE = -I$(topsrcdir)/other-licenses/ply\n')
            fh.write('IDL_PARSER_DIR = $(topsrcdir)/xpcom/idl-parser\n')
            fh.write('IDL_PARSER_CACHE_DIR = $(MOZ_OBJ_ROOT)/xpcom/idl-parser/xpidl\n')

        # Run 'tup init' if necessary.
        if not os.path.exists(mozpath.join(self.environment.topsrcdir, ".tup")):
            tup = self.environment.substs.get('TUP', 'tup')
            self._cmd.run_process(cwd=self.environment.topsrcdir, log_name='tup', args=[tup, 'init'])

    def _process_generated_file(self, backend_file, obj):
        # TODO: These are directories that don't work in the tup backend
        # yet, because things they depend on aren't built yet.
        skip_directories = (
            'toolkit/library', # libxul.so
        )
        if obj.script and obj.method and obj.relobjdir not in skip_directories:
            backend_file.export_shell()
            cmd = self._py_action('file_generate')
            if obj.localized:
                cmd.append('--locale=en-US')
            cmd.extend([
                obj.script,
                obj.method,
                obj.outputs[0],
                '%s.pp' % obj.outputs[0], # deps file required
                'unused', # deps target is required
            ])
            full_inputs = [f.full_path for f in obj.inputs]
            cmd.extend(full_inputs)
            cmd.extend(shell_quote(f) for f in obj.flags)

            outputs = []
            outputs.extend(obj.outputs)
            outputs.append('%s.pp' % obj.outputs[0])

            extra_exports = {
                'buildid.h': ['MOZ_BUILD_DATE'],
            }
            for f in obj.outputs:
                exports = extra_exports.get(f)
                if exports:
                    backend_file.export(exports)

            if any(f.endswith(('automation.py', 'source-repo.h', 'buildid.h'))
                   for f in obj.outputs):
                extra_outputs = [self._early_generated_files]
            else:
                extra_outputs = [self._installed_files] if obj.required_for_compile else []
                full_inputs += [self._early_generated_files]

            if len(outputs) > 3:
                display_outputs = ', '.join(outputs[0:3]) + ', ...'
            else:
                display_outputs = ', '.join(outputs)
            display = 'python {script}:{method} -> [{display_outputs}]'.format(
                script=obj.script,
                method=obj.method,
                display_outputs=display_outputs
            )
            backend_file.rule(
                display=display,
                cmd=cmd,
                inputs=full_inputs,
                outputs=outputs,
                extra_outputs=extra_outputs,
                check_unchanged=True,
            )

    def _process_defines(self, backend_file, obj, host=False):
        defines = list(obj.get_defines())
        if defines:
            if host:
                backend_file.host_defines = defines
            else:
                backend_file.defines = defines

    def _add_features(self, target, path):
        path_parts = mozpath.split(path)
        if all([target == 'dist/bin/browser', path_parts[0] == 'features',
                len(path_parts) > 1]):
            self._built_in_addons.add(path_parts[1])

    def _process_final_target_files(self, obj):
        target = obj.install_target
        if not isinstance(obj, ObjdirFiles):
            path = mozpath.basedir(target, (
                'dist/bin',
                'dist/xpi-stage',
                '_tests',
                'dist/include',
                'dist/sdk',
            ))
            if not path:
                raise Exception("Cannot install to " + target)

        for path, files in obj.files.walk():
            self._add_features(target, path)
            for f in files:
                output_group = None
                if any(mozpath.match(mozpath.basename(f), p)
                       for p in self._compile_env_files):
                    output_group = self._installed_files

                if not isinstance(f, ObjDirPath):
                    backend_file = self._get_backend_file(mozpath.join(target, path))
                    if '*' in f:
                        if f.startswith('/') or isinstance(f, AbsolutePath):
                            basepath, wild = os.path.split(f.full_path)
                            if '*' in basepath:
                                raise Exception("Wildcards are only supported in the filename part of "
                                                "srcdir-relative or absolute paths.")

                            # TODO: This is only needed for Windows, so we can
                            # skip this for now.
                            pass
                        else:
                            def _prefix(s):
                                for p in mozpath.split(s):
                                    if '*' not in p:
                                        yield p + '/'
                            prefix = ''.join(_prefix(f.full_path))
                            self.backend_input_files.add(prefix)

                            output_dir = ''
                            # If we have a RenamedSourcePath here, the common backend
                            # has generated this object from a jar manifest, and we
                            # can rely on 'path' to be our destination path relative
                            # to any wildcard match. Otherwise, the output file may
                            # contribute to our destination directory.
                            if not isinstance(f, RenamedSourcePath):
                                output_dir = ''.join(_prefix(mozpath.dirname(f)))

                            finder = FileFinder(prefix)
                            for p, _ in finder.find(f.full_path[len(prefix):]):
                                install_dir = prefix[len(obj.srcdir) + 1:]
                                output = p
                                if f.target_basename and '*' not in f.target_basename:
                                    output = mozpath.join(f.target_basename, output)
                                backend_file.symlink_rule(mozpath.join(prefix, p),
                                                          output=mozpath.join(output_dir, output),
                                                          output_group=output_group)
                    else:
                        backend_file.symlink_rule(f.full_path, output=f.target_basename, output_group=output_group)
                else:
                    if (self.environment.is_artifact_build and
                        any(mozpath.match(f.target_basename, p) for p in self._compile_env_gen_files)):
                        # If we have an artifact build we never would have generated this file,
                        # so do not attempt to install it.
                        continue

                    # We're not generating files in these directories yet, so
                    # don't attempt to install files generated from them.
                    if f.context.relobjdir not in ('toolkit/library',
                                                   'js/src/shell'):
                        output = mozpath.join('$(MOZ_OBJ_ROOT)', target, path,
                                              f.target_basename)
                        gen_backend_file = self._get_backend_file(f.context.relobjdir)
                        if gen_backend_file.requires_delay([f]):
                            gen_backend_file.delayed_installed_files.append((f.full_path, output, output_group))
                        else:
                            gen_backend_file.symlink_rule(f.full_path, output=output,
                                                          output_group=output_group)


    def _process_final_target_pp_files(self, obj, backend_file):
        for i, (path, files) in enumerate(obj.files.walk()):
            self._add_features(obj.install_target, path)
            for f in files:
                self._preprocess(backend_file, f.full_path,
                                 destdir=mozpath.join(self.environment.topobjdir, obj.install_target, path),
                                 target=f.target_basename)

    def _process_computed_flags(self, obj, backend_file):
        for var, flags in obj.get_flags():
            backend_file.local_flags[var] = flags

    def _process_unified_sources(self, obj):
        backend_file = self._get_backend_file_for(obj)
        files = [f[0] for f in obj.unified_source_mapping]
        backend_file.sources[obj.canonical_suffix].extend(files)

    def _handle_idl_manager(self, manager):
        if self.environment.is_artifact_build:
            return

        dist_idl_backend_file = self._get_backend_file('dist/idl')
        for idl in manager.idls.values():
            dist_idl_backend_file.symlink_rule(idl['source'], output_group=self._installed_idls)

        backend_file = self._get_backend_file('xpcom/xpidl')
        backend_file.export_shell()

        all_idl_directories = set()
        all_idl_directories.update(*map(lambda x: x[1], manager.modules.itervalues()))

        all_xpts = []
        for module, (idls, _) in sorted(manager.modules.iteritems()):
            cmd = [
                '$(PYTHON_PATH)',
                '$(PLY_INCLUDE)',
                '-I$(IDL_PARSER_DIR)',
                '-I$(IDL_PARSER_CACHE_DIR)',
                '$(topsrcdir)/python/mozbuild/mozbuild/action/xpidl-process.py',
                '--cache-dir', '$(IDL_PARSER_CACHE_DIR)',
                '--bindings-conf', '$(topsrcdir)/dom/bindings/Bindings.conf',
            ]

            for d in all_idl_directories:
                cmd.extend(['-I', d])

            cmd.extend([
                '$(DIST)/include',
                '$(DIST)/xpcrs',
                '.',
                module,
            ])
            cmd.extend(sorted(idls))

            all_xpts.append('$(MOZ_OBJ_ROOT)/%s/%s.xpt' % (backend_file.relobjdir, module))
            outputs = ['%s.xpt' % module]
            stems = sorted(mozpath.splitext(mozpath.basename(idl))[0] for idl in idls)
            outputs.extend(['$(MOZ_OBJ_ROOT)/dist/include/%s.h' % f for f in stems])
            outputs.extend(['$(MOZ_OBJ_ROOT)/dist/xpcrs/rt/%s.rs' % f for f in stems])
            outputs.extend(['$(MOZ_OBJ_ROOT)/dist/xpcrs/bt/%s.rs' % f for f in stems])
            backend_file.rule(
                inputs=[
                    '$(MOZ_OBJ_ROOT)/xpcom/idl-parser/xpidl/xpidllex.py',
                    '$(MOZ_OBJ_ROOT)/xpcom/idl-parser/xpidl/xpidlyacc.py',
                    self._installed_idls,
                ],
                display='XPIDL %s' % module,
                cmd=cmd,
                outputs=outputs,
                extra_outputs=[self._installed_files],
                check_unchanged=True,
            )

        cpp_backend_file = self._get_backend_file('xpcom/reflect/xptinfo')
        cpp_backend_file.export_shell()
        cpp_backend_file.rule(
            inputs=all_xpts,
            display='XPIDL xptcodegen.py %o',
            cmd=[
                '$(PYTHON_PATH)',
                '$(PLY_INCLUDE)',
                '$(topsrcdir)/xpcom/reflect/xptinfo/xptcodegen.py',
                '%o',
                '%f',
            ],
            outputs=['xptdata.cpp'],
            check_unchanged=True,
        )

    def _preprocess(self, backend_file, input_file, destdir=None, target=None):
        if target is None:
            target = mozpath.basename(input_file)
        # .css files use '%' as the preprocessor marker, which must be scaped as
        # '%%' in the Tupfile.
        marker = '%%' if target.endswith('.css') else '#'

        cmd = self._py_action('preprocessor')
        cmd.extend([shell_quote(d) for d in backend_file.defines])
        cmd.extend(['$(ACDEFINES)', '%f', '-o', '%o', '--marker=%s' % marker])

        base_input = mozpath.basename(target)
        if base_input.endswith('.in'):
            base_input = mozpath.splitext(base_input)[0]
        output = mozpath.join(destdir, base_input) if destdir else base_input

        backend_file.rule(
            inputs=[input_file],
            extra_inputs=[self._early_generated_files],
            display='Preprocess %o',
            cmd=cmd,
            outputs=[output],
            check_unchanged=True,
        )

    def _handle_ipdl_sources(self, ipdl_dir, sorted_ipdl_sources, sorted_nonstatic_ipdl_sources,
                             sorted_static_ipdl_sources, unified_ipdl_cppsrcs_mapping):
        # Preferably we wouldn't have to import ipdl, but we need to parse the
        # ast in order to determine the namespaces since they are used in the
        # header output paths.
        sys.path.append(mozpath.join(self.environment.topsrcdir, 'ipc', 'ipdl'))
        import ipdl

        backend_file = self._get_backend_file('ipc/ipdl')
        outheaderdir = '_ipdlheaders'
        srcdir = mozpath.join(self.environment.topsrcdir, 'ipc/ipdl')
        cmd = [
            '$(PYTHON_PATH)',
            '$(PLY_INCLUDE)',
            '%s/ipdl.py' % srcdir,
            '--sync-msg-list=%s/sync-messages.ini' % srcdir,
            '--msg-metadata=%s/message-metadata.ini' % srcdir,
            '--outheaders-dir=%s' % outheaderdir,
            '--outcpp-dir=.',
        ]
        ipdldirs = sorted(set(mozpath.dirname(p) for p in sorted_ipdl_sources))
        cmd.extend(['-I%s' % d for d in ipdldirs])
        cmd.extend(sorted_ipdl_sources)

        outputs = ['IPCMessageTypeName.cpp', mozpath.join(outheaderdir, 'IPCMessageStart.h'), 'ipdl_lextab.py', 'ipdl_yacctab.py']

        for filename in sorted_ipdl_sources:
            filepath, ext = os.path.splitext(filename)
            dirname, basename = os.path.split(filepath)
            dirname = mozpath.relpath(dirname, self.environment.topsrcdir)

            extensions = ['']
            if ext == '.ipdl':
                extensions.extend(['Child', 'Parent'])

            with open(filename) as f:
                ast = ipdl.parse(f.read(), filename, includedirs=ipdldirs)
                self.backend_input_files.add(filename)
            headerdir = os.path.join(outheaderdir, *([ns.name for ns in ast.namespaces]))

            for extension in extensions:
                outputs.append("%s%s.cpp" % (basename, extension))
                outputs.append(mozpath.join(headerdir, '%s%s.h' % (basename, extension)))

        backend_file.rule(
            display='IPDL code generation',
            cmd=cmd,
            outputs=outputs,
            extra_outputs=[self._installed_files],
            check_unchanged=True,
        )
        backend_file.sources['.cpp'].extend(u[0] for u in unified_ipdl_cppsrcs_mapping)

    def _handle_webidl_build(self, bindings_dir, unified_source_mapping,
                             webidls, expected_build_output_files,
                             global_define_files):
        backend_file = self._get_backend_file('dom/bindings')
        backend_file.export_shell()

        for source in sorted(webidls.all_preprocessed_sources()):
            self._preprocess(backend_file, source)

        cmd = self._py_action('webidl')
        cmd.append(mozpath.join(self.environment.topsrcdir, 'dom', 'bindings'))

        # The WebIDLCodegenManager knows all of the .cpp and .h files that will
        # be created (expected_build_output_files), but there are a few
        # additional files that are also created by the webidl py_action.
        outputs = [
            '_cache/webidlyacc.py',
            'codegen.json',
            'codegen.pp',
            'parser.out',
        ]
        outputs.extend(expected_build_output_files)

        backend_file.rule(
            display='WebIDL code generation',
            cmd=cmd,
            inputs=webidls.all_non_static_basenames(),
            outputs=outputs,
            extra_outputs=[self._installed_files],
            check_unchanged=True,
        )
        backend_file.sources['.cpp'].extend(u[0] for u in unified_source_mapping)
        backend_file.sources['.cpp'].extend(sorted(global_define_files))

        test_backend_file = self._get_backend_file('dom/bindings/test')
        test_backend_file.sources['.cpp'].extend(sorted('../%sBinding.cpp' % s for s in webidls.all_test_stems()))
