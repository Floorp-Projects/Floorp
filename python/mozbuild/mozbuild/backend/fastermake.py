# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, unicode_literals, print_function

from mozbuild.backend.base import PartialBackend
from mozbuild.backend.common import CommonBackend
from mozbuild.frontend.context import (
    ObjDirPath,
)
from mozbuild.frontend.data import (
    ChromeManifestEntry,
    FinalTargetPreprocessedFiles,
    FinalTargetFiles,
    JARManifest,
    XPIDLFile,
)
from mozbuild.makeutil import Makefile
from mozbuild.util import OrderedDefaultDict
from mozpack.manifests import InstallManifest
import mozpack.path as mozpath


class FasterMakeBackend(CommonBackend, PartialBackend):
    def _init(self):
        super(FasterMakeBackend, self)._init()

        self._manifest_entries = OrderedDefaultDict(set)

        self._install_manifests = OrderedDefaultDict(InstallManifest)

        self._dependencies = OrderedDefaultDict(list)

        self._has_xpidl = False

    def _add_preprocess(self, obj, path, dest, target=None, **kwargs):
        if target is None:
            target = mozpath.basename(path)
        # This matches what PP_TARGETS do in config/rules.
        if target.endswith('.in'):
            target = target[:-3]
        if target.endswith('.css'):
            kwargs['marker'] = '%'
        depfile = mozpath.join(
            self.environment.topobjdir, 'faster', '.deps',
            mozpath.join(obj.install_target, dest, target).replace('/', '_'))
        self._install_manifests[obj.install_target].add_preprocess(
            mozpath.join(obj.srcdir, path),
            mozpath.join(dest, target),
            depfile,
            **kwargs)

    def consume_object(self, obj):
        if isinstance(obj, JARManifest) and \
                obj.install_target.startswith('dist/bin'):
            self._consume_jar_manifest(obj)

        elif isinstance(obj, (FinalTargetFiles,
                              FinalTargetPreprocessedFiles)) and \
                obj.install_target.startswith('dist/bin'):
            defines = obj.defines or {}
            if defines:
                defines = defines.defines
            for path, files in obj.files.walk():
                for f in files:
                    if isinstance(obj, FinalTargetPreprocessedFiles):
                        self._add_preprocess(obj, f.full_path, path,
                                             target=f.target_basename,
                                             defines=defines)
                    elif '*' in f:
                        def _prefix(s):
                            for p in mozpath.split(s):
                                if '*' not in p:
                                    yield p + '/'
                        prefix = ''.join(_prefix(f.full_path))

                        if '*' in f.target_basename:
                            target = path
                        else:
                            target = mozpath.join(path, f.target_basename)
                        mozpath.join(path, f.target_basename)
                        self._install_manifests[obj.install_target] \
                            .add_pattern_link(
                                prefix,
                                f.full_path[len(prefix):],
                                target)
                    else:
                        self._install_manifests[obj.install_target].add_link(
                            f.full_path,
                            mozpath.join(path, f.target_basename)
                        )
                    if isinstance(f, ObjDirPath):
                        dep_target = 'install-%s' % obj.install_target
                        self._dependencies[dep_target].append(
                            mozpath.relpath(f.full_path,
                                            self.environment.topobjdir))

        elif isinstance(obj, ChromeManifestEntry) and \
                obj.install_target.startswith('dist/bin'):
            top_level = mozpath.join(obj.install_target, 'chrome.manifest')
            if obj.path != top_level:
                entry = 'manifest %s' % mozpath.relpath(obj.path,
                                                        obj.install_target)
                self._manifest_entries[top_level].add(entry)
            self._manifest_entries[obj.path].add(str(obj.entry))

        elif isinstance(obj, XPIDLFile):
            self._has_xpidl = True
            # We're not actually handling XPIDL files.
            return False

        else:
            return False

        return True

    def consume_finished(self):
        mk = Makefile()
        # Add the default rule at the very beginning.
        mk.create_rule(['default'])
        mk.add_statement('TOPSRCDIR = %s' % self.environment.topsrcdir)
        mk.add_statement('TOPOBJDIR = %s' % self.environment.topobjdir)
        if not self._has_xpidl:
            mk.add_statement('NO_XPIDL = 1')

        # Add a few necessary variables inherited from configure
        for var in (
            'PYTHON',
            'ACDEFINES',
            'MOZ_BUILD_APP',
            'MOZ_WIDGET_TOOLKIT',
        ):
            value = self.environment.substs.get(var)
            if value is not None:
                mk.add_statement('%s = %s' % (var, value))

        install_manifests_bases = self._install_manifests.keys()

        # Add information for chrome manifest generation
        manifest_targets = []

        for target, entries in self._manifest_entries.iteritems():
            manifest_targets.append(target)
            install_target = mozpath.basedir(target, install_manifests_bases)
            self._install_manifests[install_target].add_content(
                ''.join('%s\n' % e for e in sorted(entries)),
                mozpath.relpath(target, install_target))

        # Add information for install manifests.
        mk.add_statement('INSTALL_MANIFESTS = %s'
                         % ' '.join(self._install_manifests.keys()))

        # Add dependencies we infered:
        for target, deps in self._dependencies.iteritems():
            mk.create_rule([target]).add_dependencies(
                '$(TOPOBJDIR)/%s' % d for d in deps)

        mk.add_statement('include $(TOPSRCDIR)/config/faster/rules.mk')

        for base, install_manifest in self._install_manifests.iteritems():
            with self._write_file(
                    mozpath.join(self.environment.topobjdir, 'faster',
                                 'install_%s' % base.replace('/', '_'))) as fh:
                install_manifest.write(fileobj=fh)

        # For artifact builds only, write a single unified manifest for consumption by |mach watch|.
        if self.environment.is_artifact_build:
            unified_manifest = InstallManifest()
            for base, install_manifest in self._install_manifests.iteritems():
                # Expect 'dist/bin/**', which includes 'dist/bin' with no trailing slash.
                assert base.startswith('dist/bin')
                base = base[len('dist/bin'):]
                if base and base[0] == '/':
                    base = base[1:]
                unified_manifest.add_entries_from(install_manifest, base=base)

            with self._write_file(
                    mozpath.join(self.environment.topobjdir, 'faster',
                                 'unified_install_dist_bin')) as fh:
                unified_manifest.write(fileobj=fh)

        with self._write_file(
                mozpath.join(self.environment.topobjdir, 'faster',
                             'Makefile')) as fh:
            mk.dump(fh, removal_guard=False)
