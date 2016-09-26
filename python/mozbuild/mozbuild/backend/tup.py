# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, unicode_literals

import os

import mozpack.path as mozpath
from mozbuild.base import MozbuildObject
from mozbuild.backend.base import PartialBackend, HybridBackend
from mozbuild.backend.recursivemake import RecursiveMakeBackend
from mozbuild.shellutil import quote as shell_quote

from .common import CommonBackend
from ..frontend.data import (
    ContextDerived,
    Defines,
    GeneratedFile,
    HostDefines,
)
from ..util import (
    FileAvoidWrite,
)


class BackendTupfile(object):
    """Represents a generated Tupfile.
    """

    def __init__(self, srcdir, objdir, environment, topsrcdir, topobjdir):
        self.topsrcdir = topsrcdir
        self.srcdir = srcdir
        self.objdir = objdir
        self.relobjdir = mozpath.relpath(objdir, topobjdir)
        self.environment = environment
        self.name = mozpath.join(objdir, 'Tupfile')
        self.rules_included = False
        self.shell_exported = False
        self.defines = []
        self.host_defines = []

        self.fh = FileAvoidWrite(self.name, capture_diff=True)
        self.fh.write('# THIS FILE WAS AUTOMATICALLY GENERATED. DO NOT EDIT.\n')
        self.fh.write('\n')

    def write(self, buf):
        self.fh.write(buf)

    def include_rules(self):
        if not self.rules_included:
            self.write('include_rules\n')
            self.rules_included = True

    def rule(self, cmd, inputs=None, outputs=None, display=None, extra_outputs=None, check_unchanged=False):
        inputs = inputs or []
        outputs = outputs or []
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

        self.write(': %(inputs)s |> %(display)s%(cmd)s |> %(outputs)s%(extra_outputs)s\n' % {
            'inputs': ' '.join(inputs),
            'display': '^%s^ ' % caret_text if caret_text else '',
            'cmd': ' '.join(cmd),
            'outputs': ' '.join(outputs),
            'extra_outputs': ' | ' + ' '.join(extra_outputs) if extra_outputs else '',
        })

    def export_shell(self):
        if not self.shell_exported:
            # These are used by mach/mixin/process.py to determine the current
            # shell.
            for var in ('SHELL', 'MOZILLABUILD', 'COMSPEC'):
                self.write('export %s\n' % var)
            self.shell_exported = True

    def close(self):
        return self.fh.close()

    @property
    def diff(self):
        return self.fh.diff


class TupOnly(CommonBackend, PartialBackend):
    """Backend that generates Tupfiles for the tup build system.
    """

    def _init(self):
        CommonBackend._init(self)

        self._backend_files = {}
        self._cmd = MozbuildObject.from_environment()

    def _get_backend_file(self, relativedir):
        objdir = mozpath.join(self.environment.topobjdir, relativedir)
        srcdir = mozpath.join(self.environment.topsrcdir, relativedir)
        if objdir not in self._backend_files:
            self._backend_files[objdir] = \
                    BackendTupfile(srcdir, objdir, self.environment,
                                   self.environment.topsrcdir, self.environment.topobjdir)
        return self._backend_files[objdir]

    def _get_backend_file_for(self, obj):
        return self._get_backend_file(obj.relativedir)

    def _py_action(self, action):
        cmd = [
            '$(PYTHON)',
            '-m',
            'mozbuild.action.%s' % action,
        ]
        return cmd

    def consume_object(self, obj):
        """Write out build files necessary to build with tup."""

        if not isinstance(obj, ContextDerived):
            return False

        consumed = CommonBackend.consume_object(self, obj)

        # Even if CommonBackend acknowledged the object, we still need to let
        # the RecursiveMake backend also handle these objects.
        if consumed:
            return False

        backend_file = self._get_backend_file_for(obj)

        if isinstance(obj, GeneratedFile):
            # TODO: These are directories that don't work in the tup backend
            # yet, because things they depend on aren't built yet.
            skip_directories = (
                'build', # FinalTargetPreprocessedFiles
                'layout/style/test', # HostSimplePrograms
                'toolkit/library', # libxul.so
            )
            if obj.script and obj.method and obj.relobjdir not in skip_directories:
                backend_file.export_shell()
                cmd = self._py_action('file_generate')
                cmd.extend([
                    obj.script,
                    obj.method,
                    obj.outputs[0],
                    '%s.pp' % obj.outputs[0], # deps file required
                ])
                full_inputs = [f.full_path for f in obj.inputs]
                cmd.extend(full_inputs)

                outputs = []
                outputs.extend(obj.outputs)
                outputs.append('%s.pp' % obj.outputs[0])

                backend_file.rule(
                    display='python {script}:{method} -> [%o]'.format(script=obj.script, method=obj.method),
                    cmd=cmd,
                    inputs=full_inputs,
                    outputs=outputs,
                )
        elif isinstance(obj, Defines):
            self._process_defines(backend_file, obj)
        elif isinstance(obj, HostDefines):
            self._process_defines(backend_file, obj, host=True)

        return True

    def consume_finished(self):
        CommonBackend.consume_finished(self)

        for objdir, backend_file in sorted(self._backend_files.items()):
            with self._write_file(fh=backend_file):
                pass

        with self._write_file(mozpath.join(self.environment.topobjdir, 'Tuprules.tup')) as fh:
            acdefines = [name for name in self.environment.defines
                if not name in self.environment.non_global_defines]
            acdefines_flags = ' '.join(['-D%s=%s' % (name,
                shell_quote(self.environment.defines[name]))
                for name in sorted(acdefines)])
            fh.write('MOZ_OBJ_ROOT = $(TUP_CWD)\n')
            fh.write('DIST = $(MOZ_OBJ_ROOT)/dist\n')
            fh.write('ACDEFINES = %s\n' % acdefines_flags)
            fh.write('topsrcdir = $(MOZ_OBJ_ROOT)/%s\n' % (
                os.path.relpath(self.environment.topsrcdir, self.environment.topobjdir)
            ))
            fh.write('PYTHON = $(MOZ_OBJ_ROOT)/_virtualenv/bin/python -B\n')
            fh.write('PYTHON_PATH = $(PYTHON) $(topsrcdir)/config/pythonpath.py\n')
            fh.write('PLY_INCLUDE = -I$(topsrcdir)/other-licenses/ply\n')
            fh.write('IDL_PARSER_DIR = $(topsrcdir)/xpcom/idl-parser\n')
            fh.write('IDL_PARSER_CACHE_DIR = $(MOZ_OBJ_ROOT)/xpcom/idl-parser/xpidl\n')

        # Run 'tup init' if necessary.
        if not os.path.exists(mozpath.join(self.environment.topsrcdir, ".tup")):
            tup = self.environment.substs.get('TUP', 'tup')
            self._cmd.run_process(cwd=self.environment.topsrcdir, log_name='tup', args=[tup, 'init'])

    def _process_defines(self, backend_file, obj, host=False):
        defines = list(obj.get_defines())
        if defines:
            if host:
                backend_file.host_defines = defines
            else:
                backend_file.defines = defines

    def _handle_idl_manager(self, manager):
        backend_file = self._get_backend_file('xpcom/xpidl')
        backend_file.export_shell()

        for module, data in sorted(manager.modules.iteritems()):
            dest, idls = data
            cmd = [
                '$(PYTHON_PATH)',
                '$(PLY_INCLUDE)',
                '-I$(IDL_PARSER_DIR)',
                '-I$(IDL_PARSER_CACHE_DIR)',
                '$(topsrcdir)/python/mozbuild/mozbuild/action/xpidl-process.py',
                '--cache-dir', '$(IDL_PARSER_CACHE_DIR)',
                '$(DIST)/idl',
                '$(DIST)/include',
                '$(MOZ_OBJ_ROOT)/%s/components' % dest,
                module,
            ]
            cmd.extend(sorted(idls))

            outputs = ['$(MOZ_OBJ_ROOT)/%s/components/%s.xpt' % (dest, module)]
            outputs.extend(['$(MOZ_OBJ_ROOT)/dist/include/%s.h' % f for f in sorted(idls)])
            backend_file.rule(
                inputs=[
                    '$(MOZ_OBJ_ROOT)/xpcom/idl-parser/xpidl/xpidllex.py',
                    '$(MOZ_OBJ_ROOT)/xpcom/idl-parser/xpidl/xpidlyacc.py',
                ],
                display='XPIDL %s' % module,
                cmd=cmd,
                outputs=outputs,
            )

    def _preprocess(self, backend_file, input_file):
        cmd = self._py_action('preprocessor')
        cmd.extend(backend_file.defines)
        cmd.extend(['$(ACDEFINES)', '%f', '-o', '%o'])

        backend_file.rule(
            inputs=[input_file],
            display='Preprocess %o',
            cmd=cmd,
            outputs=[mozpath.basename(input_file)],
        )

    def _handle_ipdl_sources(self, ipdl_dir, sorted_ipdl_sources,
                             unified_ipdl_cppsrcs_mapping):
        # TODO: This isn't implemented yet in the tup backend, but it is called
        # by the CommonBackend.
        pass

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
            check_unchanged=True,
        )


class TupBackend(HybridBackend(TupOnly, RecursiveMakeBackend)):
    def build(self, config, output, jobs, verbose):
        status = config._run_make(directory=self.environment.topobjdir, target='tup',
                                  line_handler=output.on_line, log=False, print_directory=False,
                                  ensure_exit_code=False, num_jobs=jobs, silent=not verbose,
                                  append_env={b'NO_BUILDSTATUS_MESSAGES': b'1'})
        return status
