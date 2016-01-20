# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from collections import defaultdict
import json
import os
import urlparse

from mach.config import ConfigSettings
from mach.logging import LoggingManager
from mozbuild.backend.common import CommonBackend
from mozbuild.base import MozbuildObject
from mozbuild.frontend.data import (
    FinalTargetFiles,
    FinalTargetPreprocessedFiles,
)
from mozbuild.frontend.data import JARManifest, ChromeManifestEntry
from mozpack.chrome.manifest import (
    Manifest,
    ManifestChrome,
    ManifestOverride,
    ManifestResource,
    parse_manifest,
)
import mozpack.path as mozpath


class ChromeManifestHandler(object):
    def __init__(self):
        self.overrides = {}
        self.chrome_mapping = defaultdict(set)

    def handle_manifest_entry(self, entry):
        format_strings = {
            "content": "chrome://%s/content/",
            "resource": "resource://%s/",
            "locale": "chrome://%s/locale/",
            "skin": "chrome://%s/skin/",
        }

        if isinstance(entry, (ManifestChrome, ManifestResource)):
            if isinstance(entry, ManifestResource):
                dest = entry.target
                url = urlparse.urlparse(dest)
                if not url.scheme:
                    dest = mozpath.normpath(mozpath.join(entry.base, dest))
                if url.scheme == 'file':
                    dest = mozpath.normpath(url.path)
            else:
                dest = mozpath.normpath(entry.path)

            base_uri = format_strings[entry.type] % entry.name
            self.chrome_mapping[base_uri].add(dest)
        if isinstance(entry, ManifestOverride):
            self.overrides[entry.overloaded] = entry.overload
        if isinstance(entry, Manifest):
            for e in parse_manifest(None, entry.path):
                self.handle_manifest_entry(e)

class ChromeMapBackend(CommonBackend):
    def _init(self):
        CommonBackend._init(self)

        log_manager = LoggingManager()
        self._cmd = MozbuildObject(self.environment.topsrcdir, ConfigSettings(),
                                   log_manager, self.environment.topobjdir)
        self._install_mapping = {}
        self.manifest_handler = ChromeManifestHandler()

    def consume_object(self, obj):
        if isinstance(obj, JARManifest):
            self._consume_jar_manifest(obj)
        if isinstance(obj, ChromeManifestEntry):
            self.manifest_handler.handle_manifest_entry(obj.entry)
        if isinstance(obj, (FinalTargetFiles,
                            FinalTargetPreprocessedFiles)):
            self._handle_final_target_files(obj)
        return True

    def _handle_final_target_files(self, obj):
        for path, files in obj.files.walk():
            for f in files:
                dest = mozpath.join(obj.install_target, path, f.target_basename)
                is_pp = isinstance(obj,
                                   FinalTargetPreprocessedFiles)
                self._install_mapping[dest] = f.full_path, is_pp

    def consume_finished(self):
        # Our result has three parts:
        #  A map from url prefixes to objdir directories:
        #  { "chrome://mozapps/content/": [ "dist/bin/chrome/toolkit/content/mozapps" ], ... }
        #  A map of overrides.
        #  A map from objdir paths to sourcedir paths, and a flag for whether the source was preprocessed:
        #  { "dist/bin/browser/chrome/browser/content/browser/aboutSessionRestore.js":
        #    [ "$topsrcdir/browser/components/sessionstore/content/aboutSessionRestore.js", false ], ... }
        outputfile = os.path.join(self.environment.topobjdir, 'chrome-map.json')
        with self._write_file(outputfile) as fh:
            chrome_mapping = self.manifest_handler.chrome_mapping
            overrides = self.manifest_handler.overrides
            json.dump([
                {k: list(v) for k, v in chrome_mapping.iteritems()},
                overrides,
                self._install_mapping,
            ], fh, sort_keys=True, indent=2)
