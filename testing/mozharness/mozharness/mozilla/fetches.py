# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
from distutils.spawn import find_executable

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

        fetches = os.environ['MOZ_FETCHES'].split()

        if not self.fetch_script or not os.path.isfile(self.fetch_script):
            self.warning("fetch-content script not found, downloading manually")
            self._download_fetches(fetches)
            return

        cmd = [self.fetch_script, 'task-artifacts'] + fetches
        self.run_command(cmd, env=os.environ, throw_exception=True)

    def _download_fetches(self, fetches):
        # TODO: make sure fetch-content script is available everywhere
        #       so this isn't needed
        for word in fetches:
            artifact, task = word.split('@', 1)
            extdir = os.environ['MOZ_FETCHES_DIR']

            if '>' in artifact:
                artifact, subdir = artifact.rsplit('>', 1)
                extdir = os.path.join(extdir, subdir)

            url = ARTIFACT_URL.format(artifact=artifact, task=task)
            self.download_file(url)

            filename = os.path.basename(artifact)
            mozfile.extract(filename, extdir)
            os.remove(filename)
