# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from collections import defaultdict

try:
    import urlparse
except ImportError:
    import urllib.parse as urlparse

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
