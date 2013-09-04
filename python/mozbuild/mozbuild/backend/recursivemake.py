# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import errno
import logging
import os
import types

from mozpack.copier import FilePurger
from mozpack.manifests import (
    InstallManifest,
    PurgeManifest,
)
import mozpack.path as mozpath

from .common import CommonBackend
from ..frontend.data import (
    ConfigFileSubstitution,
    DirectoryTraversal,
    IPDLFile,
    SandboxDerived,
    VariablePassthru,
    Exports,
    Program,
    XPIDLFile,
    XpcshellManifests,
)
from ..util import FileAvoidWrite


STUB_MAKEFILE = '''
# THIS FILE WAS AUTOMATICALLY GENERATED. DO NOT MODIFY BY HAND.

DEPTH          := {depth}
topsrcdir      := {topsrc}
srcdir         := {src}
VPATH          := {src}
relativesrcdir := {relsrc}

include {topsrc}/config/rules.mk
'''.lstrip()


class BackendMakeFile(object):
    """Represents a generated backend.mk file.

    This is both a wrapper around a file handle as well as a container that
    holds accumulated state.

    It's worth taking a moment to explain the make dependencies. The
    generated backend.mk as well as the Makefile.in (if it exists) are in the
    GLOBAL_DEPS list. This means that if one of them changes, all targets
    in that Makefile are invalidated. backend.mk also depends on all of its
    input files.

    It's worth considering the effect of file mtimes on build behavior.

    Since we perform an "all or none" traversal of moz.build files (the whole
    tree is scanned as opposed to individual files), if we were to blindly
    write backend.mk files, the net effect of updating a single mozbuild file
    in the tree is all backend.mk files have new mtimes. This would in turn
    invalidate all make targets across the whole tree! This would effectively
    undermine incremental builds as any mozbuild change would cause the entire
    tree to rebuild!

    The solution is to not update the mtimes of backend.mk files unless they
    actually change. We use FileAvoidWrite to accomplish this.
    """

    def __init__(self, srcdir, objdir, environment):
        self.srcdir = srcdir
        self.objdir = objdir
        self.environment = environment
        self.path = os.path.join(objdir, 'backend.mk')

        # XPIDLFiles attached to this file.
        self.idls = []
        self.xpt_name = None

        self.fh = FileAvoidWrite(self.path)
        self.fh.write('# THIS FILE WAS AUTOMATICALLY GENERATED. DO NOT EDIT.\n')
        self.fh.write('\n')
        self.fh.write('MOZBUILD_DERIVED := 1\n')

        # The global rule to incur backend generation generates Makefiles.
        self.fh.write('NO_MAKEFILE_RULE := 1\n')

        # We can't blindly have a SUBMAKEFILES rule because some of the
        # Makefile may not have a corresponding Makefile.in. For the case
        # where a new directory is added, the mozbuild file referencing that
        # new directory will need updated. This will cause a full backend
        # scan and build, installing the new Makefile.
        self.fh.write('NO_SUBMAKEFILES_RULE := 1\n')

    def write(self, buf):
        self.fh.write(buf)

    def close(self):
        if self.xpt_name:
            self.fh.write('XPT_NAME := %s\n' % self.xpt_name)

            # We just recompile all xpidls because it's easier and less error
            # prone.
            self.fh.write('NONRECURSIVE_TARGETS += export\n')
            self.fh.write('NONRECURSIVE_TARGETS_export += xpidl\n')
            self.fh.write('NONRECURSIVE_TARGETS_export_xpidl_DIRECTORY = '
                '$(DEPTH)/config/makefiles/precompile\n')
            self.fh.write('NONRECURSIVE_TARGETS_export_xpidl_TARGETS += '
                'xpidl\n')

        return self.fh.close()


class RecursiveMakeBackend(CommonBackend):
    """Backend that integrates with the existing recursive make build system.

    This backend facilitates the transition from Makefile.in to moz.build
    files.

    This backend performs Makefile.in -> Makefile conversion. It also writes
    out .mk files containing content derived from moz.build files. Both are
    consumed by the recursive make builder.

    This backend may eventually evolve to write out non-recursive make files.
    However, as long as there are Makefile.in files in the tree, we are tied to
    recursive make and thus will need this backend.
    """

    def _init(self):
        CommonBackend._init(self)

        self._backend_files = {}
        self._ipdl_sources = set()

        def detailed(summary):
            return '{:d} total backend files. {:d} created; {:d} updated; {:d} unchanged'.format(
                summary.managed_count, summary.created_count,
                summary.updated_count, summary.unchanged_count)

        # This is a little kludgy and could be improved with a better API.
        self.summary.backend_detailed_summary = types.MethodType(detailed,
            self.summary)

        self.xpcshell_manifests = []

        self.backend_input_files.add(os.path.join(self.environment.topobjdir,
            'config', 'autoconf.mk'))

        self._purge_manifests = dict(
            dist_bin=PurgeManifest(relpath='dist/bin'),
            dist_private=PurgeManifest(relpath='dist/private'),
            dist_public=PurgeManifest(relpath='dist/public'),
            dist_sdk=PurgeManifest(relpath='dist/sdk'),
            tests=PurgeManifest(relpath='_tests'),
            xpidl=PurgeManifest(relpath='config/makefiles/xpidl'),
        )

        self._install_manifests = dict(
            dist_idl=InstallManifest(),
            dist_include=InstallManifest(),
        )

    def _update_from_avoid_write(self, result):
        existed, updated = result

        if not existed:
            self.summary.created_count += 1
        elif updated:
            self.summary.updated_count += 1
        else:
            self.summary.unchanged_count += 1

    def consume_object(self, obj):
        """Write out build files necessary to build with recursive make."""

        CommonBackend.consume_object(self, obj)

        if not isinstance(obj, SandboxDerived):
            return

        backend_file = self._backend_files.get(obj.srcdir,
            BackendMakeFile(obj.srcdir, obj.objdir, self.get_environment(obj)))

        if isinstance(obj, DirectoryTraversal):
            self._process_directory_traversal(obj, backend_file)
        elif isinstance(obj, ConfigFileSubstitution):
            self._update_from_avoid_write(
                backend_file.environment.create_config_file(obj.output_path))
            self.backend_input_files.add(obj.input_path)
            self.summary.managed_count += 1
        elif isinstance(obj, XPIDLFile):
            backend_file.idls.append(obj)
            backend_file.xpt_name = '%s.xpt' % obj.module
        elif isinstance(obj, VariablePassthru):
            # Sorted so output is consistent and we don't bump mtimes.
            for k, v in sorted(obj.variables.items()):
                if isinstance(v, list):
                    for item in v:
                        backend_file.write('%s += %s\n' % (k, item))
                elif isinstance(v, bool):
                    if v:
                        backend_file.write('%s := 1\n' % k)
                else:
                    backend_file.write('%s := %s\n' % (k, v))
        elif isinstance(obj, Exports):
            self._process_exports(obj, obj.exports, backend_file)

        elif isinstance(obj, IPDLFile):
            self._ipdl_sources.add(mozpath.join(obj.srcdir, obj.basename))

        elif isinstance(obj, Program):
            self._process_program(obj.program, backend_file)

        elif isinstance(obj, XpcshellManifests):
            self._process_xpcshell_manifests(obj, backend_file)

        self._backend_files[obj.srcdir] = backend_file

    def consume_finished(self):
        CommonBackend.consume_finished(self)

        for srcdir in sorted(self._backend_files.keys()):
            bf = self._backend_files[srcdir]

            if not os.path.exists(bf.objdir):
                try:
                    os.makedirs(bf.objdir)
                except OSError as error:
                    if error.errno != errno.EEXIST:
                        raise

            makefile_in = os.path.join(srcdir, 'Makefile.in')
            makefile = os.path.join(bf.objdir, 'Makefile')

            # If Makefile.in exists, use it as a template. Otherwise, create a
            # stub.
            if os.path.exists(makefile_in):
                self.log(logging.DEBUG, 'substitute_makefile',
                    {'path': makefile}, 'Substituting makefile: {path}')

                self._update_from_avoid_write(
                    bf.environment.create_config_file(makefile))
                self.summary.managed_count += 1

                # Adding the Makefile.in here has the desired side-effect that
                # if the Makefile.in disappears, this will force moz.build
                # traversal. This means that when we remove empty Makefile.in
                # files, the old file will get replaced with the autogenerated
                # one automatically.
                self.backend_input_files.add(makefile_in)
            else:
                self.log(logging.DEBUG, 'stub_makefile',
                    {'path': makefile}, 'Creating stub Makefile: {path}')

                params = {
                    'topsrc': bf.environment.get_top_srcdir(makefile),
                    'src': bf.environment.get_file_srcdir(makefile),
                    'depth': bf.environment.get_depth(makefile),
                    'relsrc': bf.environment.get_relative_srcdir(makefile),
                }

                aw = FileAvoidWrite(makefile)
                aw.write(STUB_MAKEFILE.format(**params))
                self._update_from_avoid_write(aw.close())
                self.summary.managed_count += 1

            self._update_from_avoid_write(bf.close())
            self.summary.managed_count += 1


        # Write out a master list of all IPDL source files.
        ipdls = FileAvoidWrite(os.path.join(self.environment.topobjdir,
            'ipc', 'ipdl', 'ipdlsrcs.mk'))
        for p in sorted(self._ipdl_sources):
            ipdls.write('ALL_IPDLSRCS += %s\n' % p)
            base = os.path.basename(p)
            root, ext = os.path.splitext(base)

            # Both .ipdl and .ipdlh become .cpp files
            ipdls.write('CPPSRCS += %s.cpp\n' % root)
            if ext == '.ipdl':
                # .ipdl also becomes Child/Parent.cpp files
                ipdls.write('CPPSRCS += %sChild.cpp\n' % root)
                ipdls.write('CPPSRCS += %sParent.cpp\n' % root)

        ipdls.write('IPDLDIRS := %s\n' % ' '.join(sorted(set(os.path.dirname(p)
            for p in self._ipdl_sources))))

        self._update_from_avoid_write(ipdls.close())
        self.summary.managed_count += 1

        # Write out a dependency file used to determine whether a config.status
        # re-run is needed.
        backend_built_path = os.path.join(self.environment.topobjdir,
            'backend.%s.built' % self.__class__.__name__).replace(os.sep, '/')
        backend_deps = FileAvoidWrite('%s.pp' % backend_built_path)
        inputs = sorted(p.replace(os.sep, '/') for p in self.backend_input_files)

        # We need to use $(DEPTH) so the target here matches what's in
        # rules.mk. If they are different, the dependencies don't get pulled in
        # properly.
        backend_deps.write('$(DEPTH)/backend.RecursiveMakeBackend.built: %s\n' %
            ' '.join(inputs))
        for path in inputs:
            backend_deps.write('%s:\n' % path)

        self._update_from_avoid_write(backend_deps.close())
        self.summary.managed_count += 1

        # Make the master xpcshell.ini file
        self.xpcshell_manifests.sort()
        if len(self.xpcshell_manifests) > 0:
            mastermanifest = FileAvoidWrite(os.path.join(
                self.environment.topobjdir, 'testing', 'xpcshell', 'xpcshell.ini'))
            mastermanifest.write(
                '; THIS FILE WAS AUTOMATICALLY GENERATED. DO NOT MODIFY BY HAND.\n\n')
            for manifest in self.xpcshell_manifests:
                mastermanifest.write("[include:%s]\n" % manifest)
            self._update_from_avoid_write(mastermanifest.close())
            self.summary.managed_count += 1

        self._write_manifests('install', self._install_manifests)
        self._write_manifests('purge', self._purge_manifests)

    def _process_directory_traversal(self, obj, backend_file):
        """Process a data.DirectoryTraversal instance."""
        fh = backend_file.fh

        for tier, dirs in obj.tier_dirs.iteritems():
            fh.write('TIERS += %s\n' % tier)

            if dirs:
                fh.write('tier_%s_dirs += %s\n' % (tier, ' '.join(dirs)))
                fh.write('DIRS += $(tier_%s_dirs)\n' % tier)

            # tier_static_dirs should have the same keys as tier_dirs.
            if obj.tier_static_dirs[tier]:
                fh.write('tier_%s_staticdirs += %s\n' % (
                    tier, ' '.join(obj.tier_static_dirs[tier])))

        if obj.dirs:
            fh.write('DIRS := %s\n' % ' '.join(obj.dirs))

        if obj.parallel_dirs:
            fh.write('PARALLEL_DIRS := %s\n' % ' '.join(obj.parallel_dirs))

        if obj.tool_dirs:
            fh.write('TOOL_DIRS := %s\n' % ' '.join(obj.tool_dirs))

        if obj.test_dirs:
            fh.write('TEST_DIRS := %s\n' % ' '.join(obj.test_dirs))

        if obj.test_tool_dirs and \
            self.environment.substs.get('ENABLE_TESTS', False):

            fh.write('TOOL_DIRS += %s\n' % ' '.join(obj.test_tool_dirs))

        if len(obj.external_make_dirs):
            fh.write('DIRS += %s\n' % ' '.join(obj.external_make_dirs))

        if len(obj.parallel_external_make_dirs):
            fh.write('PARALLEL_DIRS += %s\n' %
                ' '.join(obj.parallel_external_make_dirs))

    def _process_exports(self, obj, exports, backend_file, namespace=""):
        # This may not be needed, but is present for backwards compatibility
        # with the old make rules, just in case.
        if not obj.dist_install:
            return

        strings = exports.get_strings()
        if namespace:
            namespace += '/'

        for s in strings:
            source = os.path.normpath(os.path.join(obj.srcdir, s))
            dest = '%s%s' % (namespace, os.path.basename(s))
            self._install_manifests['dist_include'].add_symlink(source, dest)

            if not os.path.exists(source):
                raise Exception('File listed in EXPORTS does not exist: %s' % source)

        children = exports.get_children()
        for subdir in sorted(children):
            self._process_exports(obj, children[subdir], backend_file,
                namespace=namespace + subdir)

    def _handle_idl_manager(self, manager):
        build_files = self._purge_manifests['xpidl']

        for p in ('Makefile', 'backend.mk', '.deps/.mkdir.done',
            'headers/.mkdir.done', 'xpt/.mkdir.done'):
            build_files.add(p)

        for idl in manager.idls.values():
            self._install_manifests['dist_idl'].add_symlink(idl['source'],
                idl['basename'])
            self._install_manifests['dist_include'].add_optional_exists('%s.h'
                % idl['root'])
            build_files.add(mozpath.join('headers', '%s.h' % idl['root']))

        for module in manager.modules:
            build_files.add(mozpath.join('xpt', '%s.xpt' % module))
            build_files.add(mozpath.join('.deps', '%s.pp' % module))

        headers = sorted('%s.h' % idl['root'] for idl in manager.idls.values())
        modules = manager.modules
        xpt_modules = sorted(modules.keys())
        rules = []

        for module in xpt_modules:
            deps = sorted(modules[module])
            idl_deps = ['$(dist_idl_dir)/%s.idl' % dep for dep in deps]
            rules.extend([
                # It may seem strange to have the .idl files listed as
                # prerequisites both here and in the auto-generated .pp files.
                # It is necessary to list them here to handle the case where a
                # new .idl is added to an xpt. If we add a new .idl and nothing
                # else has changed, the new .idl won't be referenced anywhere
                # except in the command invocation. Therefore, the .xpt won't
                # be rebuilt because the dependencies say it is up to date. By
                # listing the .idls here, we ensure the make file has a
                # reference to the new .idl. Since the new .idl presumably has
                # an mtime newer than the .xpt, it will trigger xpt generation.
                '$(idl_xpt_dir)/%s.xpt: %s' % (module, ' '.join(idl_deps)),
                '\t@echo "$(notdir $@)"',
                '\t$(idlprocess) $(basename $(notdir $@)) %s' % ' '.join(deps),
                '',
            ])

            # Set up linkage so make knows headers come from $(idlprocess).
            h = ['$(idl_headers_dir)/%s.h' % dep for dep in deps]
            rules.extend([
                '%s: $(idl_xpt_dir)/%s.xpt' % (' '.join(h), module),
                '',
            ])

        # Create dependency for output header so we force regeneration if the
        # header was deleted. This ideally should not be necessary. However,
        # some processes (such as PGO at the time this was implemented) wipe
        # out dist/include without regard to our install/purge manifests.

        out_path = os.path.join(self.environment.topobjdir, 'config',
            'makefiles', 'xpidl', 'Makefile')
        result = self.environment.create_config_file(out_path, extra=dict(
            xpidl_rules='\n'.join(rules),
            xpidl_modules=' '.join(xpt_modules),
            xpidl_headers=' '.join(headers),
        ))
        self._update_from_avoid_write(result)
        self.summary.managed_count += 1

        # The Makefile can't regenerate itself because of custom substitution.
        # We need to list it here to ensure changes cause regeneration.
        self.backend_input_files.add(os.path.join(self.environment.topsrcdir,
            'config', 'makefiles', 'xpidl', 'Makefile.in'))

    def _process_program(self, program, backend_file):
        backend_file.write('PROGRAM = %s\n' % program)

    def _process_xpcshell_manifests(self, obj, backend_file, namespace=""):
        manifest = obj.xpcshell_manifests
        backend_file.write('XPCSHELL_TESTS += %s\n' % os.path.dirname(manifest))
        if obj.relativedir != '':
            manifest = '%s/%s' % (obj.relativedir, manifest)
        self.xpcshell_manifests.append(manifest)

    def _write_manifests(self, dest, manifests):
        man_dir = os.path.join(self.environment.topobjdir, '_build_manifests',
            dest)

        # We have a purger for the manifests themselves to ensure legacy
        # manifests are deleted.
        purger = FilePurger()

        for k, manifest in manifests.items():
            purger.add(k)

            fh = FileAvoidWrite(os.path.join(man_dir, k))
            manifest.write(fileobj=fh)
            self._update_from_avoid_write(fh.close())

        purger.purge(man_dir)
