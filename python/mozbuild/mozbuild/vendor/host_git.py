# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import subprocess
import tempfile

from mozbuild.vendor.host_base import BaseHost


class GitHost(BaseHost):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.workdir = tempfile.TemporaryDirectory(suffix="." + self.repo_url.netloc)
        subprocess.check_call(
            ["git", "clone", self.repo_url.geturl(), self.workdir.name]
        )

    def upstream_commit(self, revision):
        sha = subprocess.check_output(
            ["git", "rev-parse", revision], cwd=self.workdir.name
        )
        created = subprocess.check_output(
            ["git", "show", "--no-patch", "--format=%ci", revision],
            cwd=self.workdir.name,
        )
        return sha.strip(), created.strip()

    def upstream_snapshot(self, revision):
        tarball = os.path.join(self.workdir.name, revision.decode() + ".tar")
        subprocess.check_call(
            ["git", "archive", "--format=tar", "-o", tarball, revision],
            cwd=self.workdir.name,
        )
        return "file://" + tarball
