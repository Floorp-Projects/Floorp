# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import errno
import logging
import os
import types

from .base import BuildBackend
from ..frontend.data import (
    ConfigFileSubstitution,
    DirectoryTraversal,
    SandboxDerived,
    VariablePassthru,
    Exports,
    Program,
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
    actually change. We use FileAvoidWrite to accomplish this. However, this
    puts us in a somewhat complicated position when it comes to tree recursion.
    As you are recursing the tree, the first time you come across a backend.mk
    that is out of date, a full tree build will be incurred. In typical make
    build systems, we would touch the out-of-date target (backend.mk) to ensure
    its mtime is newer than all its dependencies - even if the contents did
    not change. However, we can't rely on just this approach. During recursion,
    the first trigger of backend generation will cause only that backend.mk to
    update. If there is another backend.mk that is also out of date according
    to mtime but whose contents were not changed, when we recurse to that
    directory, make will trigger another full backend generation! This would
    be completely redundant and would slow down builds! This is not acceptable.

    We work around this problem by having backend generation update the mtime
    of backend.mk if they are older than their inputs - even if the file
    contents did not change. This is essentially a middle ground between
    always updating backend.mk and only updating the backend.mk that was out
    of date during recursion.
    """

    def __init__(self, srcdir, objdir, environment):
        self.srcdir = srcdir
        self.objdir = objdir
        self.environment = environment
        self.path = os.path.join(objdir, 'backend.mk')

        # Filenames that influenced the content of this file.
        self.inputs = set()

        self.fh = FileAvoidWrite(self.path)
        self.fh.write('# THIS FILE WAS AUTOMATICALLY GENERATED. DO NOT EDIT.\n')
        self.fh.write('\n')
        self.fh.write('MOZBUILD_DERIVED := 1\n')

        # SUBSTITUTE_FILES handles Makefile.in -> Makefile conversion. This
        # also doubles to handle the case where there is no Makefile.in.
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
        if self.inputs:
            l = ' '.join(sorted(self.inputs))
            self.fh.write('BACKEND_INPUT_FILES += %s\n' % l)

        result = self.fh.close()

        if not self.inputs:
            return result

        # Update mtime iff any of its input files are newer. See class notes
        # for why we do this.
        existing_mtime = os.path.getmtime(self.path)

        def mtime(path):
            try:
                return os.path.getmtime(path)
            except OSError as e:
                if e.errno == errno.ENOENT:
                    return 0

                raise

        input_mtime = max(mtime(path) for path in self.inputs)

        if input_mtime > existing_mtime:
            os.utime(self.path, None)

        return result


class RecursiveMakeBackend(BuildBackend):
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
        self._backend_files = {}

        self.summary.managed_count = 0
        self.summary.created_count = 0
        self.summary.updated_count = 0
        self.summary.unchanged_count = 0

        def detailed(summary):
            return '{:d} total backend files. {:d} created; {:d} updated; {:d} unchanged'.format(
                summary.managed_count, summary.created_count,
                summary.updated_count, summary.unchanged_count)

        # This is a little kludgy and could be improved with a better API.
        self.summary.backend_detailed_summary = types.MethodType(detailed,
            self.summary)

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

        if not isinstance(obj, SandboxDerived):
            return

        backend_file = self._backend_files.get(obj.srcdir,
            BackendMakeFile(obj.srcdir, obj.objdir, self.get_environment(obj)))

        # Define the paths that will trigger a backend rebuild. We always
        # add autoconf.mk because that is proxy for CONFIG. We can't use
        # config.status because there is no make target for that!
        autoconf_path = os.path.join(obj.topobjdir, 'config', 'autoconf.mk')
        backend_file.inputs.add(autoconf_path)
        backend_file.inputs |= obj.sandbox_all_paths

        if isinstance(obj, DirectoryTraversal):
            self._process_directory_traversal(obj, backend_file)
        elif isinstance(obj, ConfigFileSubstitution):
            backend_file.write('SUBSTITUTE_FILES += %s\n' % obj.relpath)
            self._update_from_avoid_write(
                backend_file.environment.create_config_file(obj.output_path))
            self.summary.managed_count += 1
        elif isinstance(obj, VariablePassthru):
            # Sorted so output is consistent and we don't bump mtimes.
            for k, v in sorted(obj.variables.items()):
                if isinstance(v, list):
                    for item in v:
                        backend_file.write('%s += %s\n' % (k, item))

                else:
                    backend_file.write('%s := %s\n' % (k, v))
        elif isinstance(obj, Exports):
            self._process_exports(obj.exports, backend_file)

        elif isinstance(obj, Program):
            self._process_program(obj.program, backend_file)

        elif isinstance(obj, XpcshellManifests):
            self._process_xpcshell_manifests(obj.xpcshell_manifests, backend_file)

        self._backend_files[obj.srcdir] = backend_file

    def consume_finished(self):
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

                bf.write('SUBSTITUTE_FILES += Makefile\n')
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

    def _process_directory_traversal(self, obj, backend_file):
        """Process a data.DirectoryTraversal instance."""
        fh = backend_file.fh

        for tier, dirs in obj.tier_dirs.iteritems():
            fh.write('TIERS += %s\n' % tier)

            if dirs:
                fh.write('tier_%s_dirs += %s\n' % (tier, ' '.join(dirs)))

            # tier_static_dirs should have the same keys as tier_dirs.
            if obj.tier_static_dirs[tier]:
                fh.write('tier_%s_staticdirs += %s\n' % (
                    tier, ' '.join(obj.tier_static_dirs[tier])))

                static = ' '.join(obj.tier_static_dirs[tier])
                fh.write('EXTERNAL_DIRS += %s\n' % static)

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

    def _process_exports(self, exports, backend_file, namespace=""):
        strings = exports.get_strings()
        if namespace:
            if strings:
                backend_file.write('EXPORTS_NAMESPACES += %s\n' % namespace)
            export_name = 'EXPORTS_%s' % namespace
            namespace += '/'
        else:
            export_name = 'EXPORTS'

        # Iterate over the list of export filenames, printing out an EXPORTS
        # declaration for each.
        if strings:
            backend_file.write('%s += %s\n' % (export_name, ' '.join(strings)))

        children = exports.get_children()
        for subdir in sorted(children):
            self._process_exports(children[subdir], backend_file,
                                  namespace=namespace + subdir)

    def _process_program(self, program, backend_file):
        backend_file.write('PROGRAM = %s\n' % program)

    def _process_xpcshell_manifests(self, manifest, backend_file, namespace=""):
        backend_file.write('XPCSHELL_TESTS += %s\n' % os.path.dirname(manifest))
