# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
import tempfile
import subprocess


class BaseHost:
    def __init__(self, manifest):
        self.manifest = manifest

    def upstream_tag(self, revision):
        """Temporarily clone the repo to get the latest tag and timestamp"""
        with tempfile.TemporaryDirectory() as temp_repo_clone:
            os.chdir(temp_repo_clone)
            subprocess.run(
                [
                    "git",
                    "clone",
                    self.manifest["origin"]["url"],
                    self.manifest["origin"]["name"],
                ],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                universal_newlines=True,
                check=True,
            )
            os.chdir("/".join([temp_repo_clone, self.manifest["origin"]["name"]]))
            latest_tag = subprocess.run(
                ["git", "--no-pager", "tag", "--sort=creatordate"],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                universal_newlines=True,
                check=True,
            ).stdout.splitlines()[-1]
            latest_tag_timestamp = subprocess.run(
                [
                    "git",
                    "log",
                    "-1",
                    "--date=iso8601-strict",
                    "--format=%ad",
                    latest_tag,
                ],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                universal_newlines=True,
                check=True,
            ).stdout.splitlines()[-1]
            return (latest_tag, latest_tag_timestamp)

    def upstream_snapshot(self, revision):
        pass
