# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

import requests

from mozbuild.vendor.host_base import BaseHost


class CodebergHost(BaseHost):
    def upstream_commit(self, revision):
        """Query the codeberg api for a git commit id and timestamp."""
        codeberg_api = (
            self.repo_url.scheme + "://" + self.repo_url.netloc + "/api/v1/repos/"
        )
        codeberg_api += self.repo_url.path[1:]
        codeberg_api += "/git/commits"
        req = requests.get("/".join([codeberg_api, revision]))
        req.raise_for_status()
        info = req.json()
        return (info["sha"], info["created"])

    def upstream_snapshot(self, revision):
        codeberg_api = (
            self.repo_url.scheme + "://" + self.repo_url.netloc + "/api/v1/repos/"
        )
        codeberg_api += self.repo_url.path[1:]
        return "/".join([codeberg_api, "archive", revision + ".tar.gz"])
