# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, unicode_literals, print_function

from mozbuild.backend.common import CommonBackend
from mozbuild.frontend.context import (
    Context,
    Path,
)
from mozbuild.frontend.data import (
    ChromeManifestEntry,
    ContextDerived,
    Defines,
    FinalTargetPreprocessedFiles,
    FinalTargetFiles,
    JARManifest,
    XPIDLFile,
)
from mozbuild.jar import JarManifestParser
from mozbuild.makeutil import Makefile
from mozbuild.preprocessor import Preprocessor
from mozbuild.util import OrderedDefaultDict
from mozpack.manifests import InstallManifest
import mozpack.path as mozpath
from collections import OrderedDict
from itertools import chain
import os
import sys


class FasterMakeBackend(CommonBackend):
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
        if isinstance(obj, (JARManifest, FinalTargetPreprocessedFiles)):
            defines = obj.defines or {}
            if defines:
                defines = defines.defines

        if isinstance(obj, JARManifest) and \
                obj.install_target.startswith('dist/bin'):
            self._consume_jar_manifest(obj, defines)

        elif isinstance(obj, (FinalTargetFiles,
                              FinalTargetPreprocessedFiles)) and \
                obj.install_target.startswith('dist/bin'):
            for path, files in obj.files.walk():
                for f in files:
                    if isinstance(obj, FinalTargetPreprocessedFiles):
                        self._add_preprocess(obj, f.full_path, path,
                                             target=f.target_basename,
                                             defines=defines)
                    else:
                        self._install_manifests[obj.install_target].add_symlink(
                            f.full_path,
                            mozpath.join(path, f.target_basename)
                        )

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

        # We currently ignore a lot of object types, so just acknowledge
        # everything.
        return True

    def _consume_jar_manifest(self, obj, defines):
        # Ideally, this would all be handled somehow in the emitter, but
        # this would require all the magic surrounding l10n and addons in
        # the recursive make backend to die, which is not going to happen
        # any time soon enough.
        # Notably missing:
        # - DEFINES from config/config.mk
        # - L10n support
        # - The equivalent of -e when USE_EXTENSION_MANIFEST is set in
        #   moz.build, but it doesn't matter in dist/bin.
        pp = Preprocessor()
        pp.context.update(defines)
        pp.context.update(self.environment.defines)
        pp.context.update(
            AB_CD='en-US',
            BUILD_FASTER=1,
        )
        pp.out = JarManifestParser()
        pp.do_include(obj.path.full_path)
        self.backend_input_files |= pp.includes

        jar_context = Context(config=obj._context.config)
        jar_context.add_source(obj.path.full_path)

        for jarinfo in pp.out:
            install_target = obj.install_target
            if jarinfo.base:
                install_target = mozpath.normpath(
                    mozpath.join(install_target, jarinfo.base))
            for e in jarinfo.entries:
                if e.is_locale:
                    if jarinfo.relativesrcdir:
                        src = '/%s' % jarinfo.relativesrcdir
                    else:
                        src = ''
                    src = mozpath.join(src, 'en-US', e.source)
                else:
                    src = e.source

                src = Path(jar_context, src)

                if '*' in e.source:
                    if e.preprocess:
                        raise Exception('%s: Wildcards are not supported with '
                                        'preprocessing' % obj.path)
                    def _prefix(s):
                        for p in mozpath.split(s):
                            if '*' not in p:
                                yield p + '/'
                    prefix = ''.join(_prefix(src.full_path))

                    self._install_manifests[install_target] \
                        .add_pattern_symlink(
                        prefix,
                        src.full_path[len(prefix):],
                        mozpath.join(jarinfo.name, e.output))
                    continue

                if not os.path.exists(src.full_path):
                    if e.is_locale:
                        raise Exception(
                            '%s: Cannot find %s' % (obj.path, e.source))
                    if e.source.startswith('/'):
                        src = Path(jar_context, '!' + e.source)
                    else:
                        # This actually gets awkward if the jar.mn is not
                        # in the same directory as the moz.build declaring
                        # it, but it's how it works in the recursive make,
                        # not that anything relies on that, but it's simpler.
                        src = Path(obj._context, '!' + e.source)
                    self._dependencies['install-%s' % install_target] \
                        .append(mozpath.relpath(
                        src.full_path, self.environment.topobjdir))

                if e.preprocess:
                    self._add_preprocess(
                        obj,
                        src.full_path,
                        mozpath.join(jarinfo.name, mozpath.dirname(e.output)),
                        mozpath.basename(e.output),
                        defines=defines)
                else:
                    self._install_manifests[install_target].add_symlink(
                        src.full_path,
                        mozpath.join(jarinfo.name, e.output))

            manifest = mozpath.normpath(mozpath.join(install_target,
                                                     jarinfo.name))
            manifest += '.manifest'
            for m in jarinfo.chrome_manifests:
                self._manifest_entries[manifest].add(
                    m.replace('%', mozpath.basename(jarinfo.name) + '/'))

            if jarinfo.name != 'chrome':
                manifest = mozpath.normpath(mozpath.join(install_target,
                                                         'chrome.manifest'))
                entry = 'manifest %s.manifest' % jarinfo.name
                self._manifest_entries[manifest].add(entry)

    def consume_finished(self):
        mk = Makefile()
        # Add the default rule at the very beginning.
        mk.create_rule(['default'])
        mk.add_statement('TOPSRCDIR = %s' % self.environment.topsrcdir)
        mk.add_statement('TOPOBJDIR = %s' % self.environment.topobjdir)
        mk.add_statement('BACKEND = %s' % self._backend_output_list_file)
        if not self._has_xpidl:
            mk.add_statement('NO_XPIDL = 1')

        # Add a few necessary variables inherited from configure
        for var in (
            'PYTHON',
            'ACDEFINES',
            'MOZ_BUILD_APP',
            'MOZ_WIDGET_TOOLKIT',
        ):
            mk.add_statement('%s = %s' % (var, self.environment.substs[var]))

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

        # Add backend dependencies:
        mk.create_rule([self._backend_output_list_file]).add_dependencies(
            self.backend_input_files)

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
