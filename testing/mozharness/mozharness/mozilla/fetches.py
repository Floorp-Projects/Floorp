# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
from distutils.spawn import find_executable
import json

import mozfile

ARTIFACT_URL = 'https://queue.taskcluster.net/v1/task/{task}/artifacts/{artifact}'


class FetchesMixin(object):
    """Utility class to download artifacts via `MOZ_FETCHES` and the
    `fetch-content` script."""

    @property
    def fetch_script(self):
        if getattr(self, '_fetch_script', None):
            return self._fetch_script

        self._fetch_script = find_executable('fetch-content')
        if not self._fetch_script and 'GECKO_PATH' in os.environ:
            self._fetch_script = os.path.join(os.environ['GECKO_PATH'],
                'taskcluster', 'script', 'misc', 'fetch-content')
        return self._fetch_script

    def fetch_content(self):
        if not os.environ.get('MOZ_FETCHES'):
            self.warning('no fetches to download')
            return

        if not self.fetch_script or not os.path.isfile(self.fetch_script):
            self.warning("fetch-content script not found, downloading manually")
            self._download_fetches()
            return

        cmd = [self.fetch_script, 'task-artifacts']
        self.run_command(cmd, env=os.environ, throw_exception=True)

    def _download_fetches(self):
        # TODO: make sure fetch-content script is available everywhere
        #       so this isn't needed
        fetches_dir = os.environ['MOZ_FETCHES_DIR']

        fetches = json.loads(os.environ.get('MOZ_FETCHES', '{}'))
        for fetch in fetches:
            extdir = fetches_dir
            if 'dest' in 'fetch':
                extdir = os.path.join(extdir, fetch['dest'])
            artifact = fetch['artifact']
            if not artifact.startswith('public/'):
                raise Exception('Private artifacts in `MOZ_FETCHES` not supported.')
            url = ARTIFACT_URL.format(artifact=artifact, task=fetch['task'])

            path = self.download_file(url, parent_dir=extdir)

            if fetch['extract']:
                mozfile.extract(path, extdir)
                os.remove(path)
