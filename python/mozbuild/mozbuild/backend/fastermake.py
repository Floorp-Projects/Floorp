# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, unicode_literals

from mozbuild.backend.common import CommonBackend
from mozbuild.frontend.data import (
    ContextDerived,
    Defines,
    DistFiles,
    FinalTargetFiles,
    JARManifest,
    JavaScriptModules,
    JsPreferenceFile,
    Resources,
    VariablePassthru,
)
from mozbuild.makeutil import Makefile
from mozbuild.util import OrderedDefaultDict
from mozpack.manifests import InstallManifest
import mozpack.path as mozpath
from collections import OrderedDict
from itertools import chain


class FasterMakeBackend(CommonBackend):
    def _init(self):
        super(FasterMakeBackend, self)._init()

        self._seen_directories = set()
        self._defines = dict()
        self._jar_manifests = OrderedDict()

        self._preprocess_files = OrderedDict()

        self._manifest_entries = OrderedDefaultDict(list)

        self._install_manifests = OrderedDefaultDict(InstallManifest)

    def consume_object(self, obj):
        if not isinstance(obj, Defines) and isinstance(obj, ContextDerived):
            defines = self._defines.get(obj.objdir, [])
            if defines:
                defines = list(defines.get_defines())

        if isinstance(obj, Defines):
            self._defines[obj.objdir] = obj

            # We're assuming below that Defines come first for a given objdir,
            # which is kind of set in stone from the order things are treated
            # in emitter.py.
            assert obj.objdir not in self._seen_directories

        elif isinstance(obj, JARManifest) and \
                obj.install_target.startswith('dist/bin'):
            self._jar_manifests[obj.path] = (obj.objdir,
                                             obj.install_target,
                                             defines)

        elif isinstance(obj, VariablePassthru) and \
                obj.install_target.startswith('dist/bin'):
            for f in obj.variables.get('EXTRA_COMPONENTS', {}):
                path = mozpath.join(obj.install_target, 'components',
                                    mozpath.basename(f))
                self._install_manifests[obj.install_target].add_symlink(
                    mozpath.join(obj.srcdir, f),
                    mozpath.join('components', mozpath.basename(f))
                )
                if f.endswith('.manifest'):
                    manifest = mozpath.join(obj.install_target,
                                            'chrome.manifest')
                    self._manifest_entries[manifest].append(
                        'manifest components/%s' % mozpath.basename(f))

            for f in obj.variables.get('EXTRA_PP_COMPONENTS', {}):
                path = mozpath.join(obj.install_target, 'components',
                                    mozpath.basename(f))
                self._preprocess_files[path] = (obj.srcdir, f, defines)
                if f.endswith('.manifest'):
                    manifest = mozpath.join(obj.install_target,
                                            'chrome.manifest')
                    self._manifest_entries[manifest].append(
                        'manifest components/%s' % mozpath.basename(f))

        elif isinstance(obj, JavaScriptModules) and \
                obj.install_target.startswith('dist/bin'):
            for path, strings in obj.modules.walk():
                base = mozpath.join(obj.install_target, 'modules', path)
                for f in strings:
                    if obj.flavor == 'extra':
                        self._install_manifests[obj.install_target].add_symlink(
                            mozpath.join(obj.srcdir, f),
                            mozpath.join('modules', path, mozpath.basename(f))
                        )
                    elif obj.flavor == 'extra_pp':
                        dest = mozpath.join(base, mozpath.basename(f))
                        self._preprocess_files[dest] = (obj.srcdir, f, defines)

        elif isinstance(obj, JsPreferenceFile) and \
                obj.install_target.startswith('dist/bin'):
            # The condition for the directory value in config/rules.mk is:
            # ifneq (,$(DIST_SUBDIR)$(XPI_NAME)$(LIBXUL_SDK))
            # - LIBXUL_SDK is not supported (it likely doesn't work in the
            # recursive backend anyways
            # - when XPI_NAME is set, obj.install_target will start with
            # dist/xpi-stage
            # - when DIST_SUBDIR is set, obj.install_target will start with
            # dist/bin/$(DIST_SUBDIR)
            # So an equivalent condition that is not cumbersome for us and that
            # is enough at least for now is checking if obj.install_target is
            # different from dist/bin.
            if obj.install_target == 'dist/bin':
                pref_dir = 'defaults/pref'
            else:
                pref_dir = 'defaults/preferences'

            dest = mozpath.join(obj.install_target, pref_dir,
                                mozpath.basename(obj.path))
            # on win32, pref files need CRLF line endings... see bug 206029
            if self.environment.substs['OS_ARCH'] == 'WINNT':
                defines.append('--line-endings=crlf')
            # We preprocess these, but they don't necessarily have preprocessor
            # directives, so tell the preprocessor to not complain about that.
            defines.append('--silence-missing-directive-warnings')
            self._preprocess_files[dest] = (obj.srcdir, obj.path, defines)

        elif isinstance(obj, Resources) and \
                obj.install_target.startswith('dist/bin'):
            for path, strings in obj.resources.walk():
                base = mozpath.join(obj.install_target, 'res', path)
                for f in strings:
                    flags = strings.flags_for(f)
                    if flags and flags.preprocess:
                        dest = mozpath.join(base, mozpath.basename(f))
                        defines = Defines(obj._context, obj.defines)
                        defines = list(defines.get_defines())
                        defines.extend(['--marker', '%'])
                        self._preprocess_files[dest] = (obj.srcdir, f, defines)
                    else:
                        self._install_manifests[obj.install_target].add_symlink(
                            mozpath.join(obj.srcdir, f),
                            mozpath.join('res', path, mozpath.basename(f))
                        )

        elif isinstance(obj, FinalTargetFiles) and \
                obj.install_target.startswith('dist/bin'):
            for path, strings in obj.files.walk():
                base = mozpath.join(obj.install_target, path)
                for f in strings:
                    self._install_manifests[obj.install_target].add_symlink(
                        mozpath.join(obj.srcdir, f),
                        mozpath.join(path, mozpath.basename(f))
                    )

        elif isinstance(obj, DistFiles) and \
                obj.install_target.startswith('dist/bin'):
            # We preprocess these, but they don't necessarily have preprocessor
            # directives, so tell the preprocessor to not complain about that.
            defines.append('--silence-missing-directive-warnings')
            for f in obj.files:
                dest = mozpath.join(obj.install_target, mozpath.basename(f))
                self._preprocess_files[dest] = (obj.srcdir, f, defines)

        else:
            # We currently ignore a lot of object types, so just acknowledge
            # everything.
            return True

        self._seen_directories.add(obj.objdir)
        return True

    def consume_finished(self):
        mk = Makefile()
        # Add the default rule at the very beginning.
        mk.create_rule(['default'])
        mk.add_statement('TOPSRCDIR = %s' % self.environment.topsrcdir)
        mk.add_statement('TOPOBJDIR = %s' % self.environment.topobjdir)

        # Add a few necessary variables inherited from configure
        for var in (
            'PYTHON',
            'ACDEFINES',
            'MOZ_CHROME_FILE_FORMAT',
        ):
            mk.add_statement('%s = %s' % (var, self.environment.substs[var]))

        # Add all necessary information for jar manifest processing
        jar_mn_targets = []

        for path, (objdir, install_target, defines) in \
                self._jar_manifests.iteritems():
            rel_manifest = mozpath.relpath(path, self.environment.topsrcdir)
            target = rel_manifest.replace('/', '-')
            assert target not in jar_mn_targets
            jar_mn_targets.append(target)
            target = 'jar-%s' % target
            mk.create_rule([target]).add_dependencies([path])
            if objdir != mozpath.join(self.environment.topobjdir,
                                      mozpath.dirname(rel_manifest)):
                mk.create_rule([target]).add_dependencies(
                    ['objdir = %s' % objdir])
            if install_target != 'dist/bin':
                mk.create_rule([target]).add_dependencies(
                    ['install_target = %s' % install_target])
            if defines:
                mk.create_rule([target]).add_dependencies(
                    ['defines = %s' % ' '.join(defines)])

        mk.add_statement('JAR_MN_TARGETS = %s' % ' '.join(jar_mn_targets))

        # Add information for chrome manifest generation
        manifest_targets = []

        for target, entries in self._manifest_entries.iteritems():
            manifest_targets.append(target)
            target = '$(TOPOBJDIR)/%s' % target
            mk.create_rule([target]).add_dependencies(
                ['content = %s' % ' '.join('"%s"' % e for e in entries)])

        mk.add_statement('MANIFEST_TARGETS = %s' % ' '.join(manifest_targets))

        # Add information for preprocessed files.
        preprocess_targets = []

        for target, (srcdir, f, defines) in self._preprocess_files.iteritems():
            # This matches what PP_TARGETS do in config/rules.
            if target.endswith('.in'):
                target = target[:-3]
                # PP_TARGETS assumes this is true, but doesn't enforce it.
                assert target not in self._preprocess_files
            preprocess_targets.append(target)
            target = '$(TOPOBJDIR)/%s' % target
            mk.create_rule([target]).add_dependencies(
                [mozpath.join(srcdir, f)])
            if defines:
                mk.create_rule([target]).add_dependencies(
                        ['defines = %s' % ' '.join(defines)])

        mk.add_statement('PP_TARGETS = %s' % ' '.join(preprocess_targets))

        # Add information for install manifests.
        mk.add_statement('INSTALL_MANIFESTS = %s'
                         % ' '.join(self._install_manifests.keys()))

        mk.add_statement('include $(TOPSRCDIR)/config/faster/rules.mk')

        for base, install_manifest in self._install_manifests.iteritems():
            with self._write_file(
                    mozpath.join(self.environment.topobjdir, 'faster',
                                 'install_%s' % base.replace('/', '_'))) as fh:
                install_manifest.write(fileobj=fh)

        with self._write_file(
                mozpath.join(self.environment.topobjdir, 'faster',
                             'Makefile')) as fh:
            mk.dump(fh, removal_guard=False)
