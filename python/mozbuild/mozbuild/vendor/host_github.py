# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import urllib
import requests

from mozbuild.vendor.host_base import BaseHost


class GitHubHost(BaseHost):
    def upstream_commit(self, revision):
        """Query the github api for a git commit id and timestamp."""
        github_api = "https://api.github.com"
        repo_url = urllib.parse.urlparse(self.manifest["origin"]["url"])
        repo = repo_url.path[1:]
        req = requests.get("/".join([github_api, "repos", repo, "commits", revision]))
        req.raise_for_status()
        info = req.json()
        return (info["sha"], info["commit"]["committer"]["date"])

    def upstream_snapshot(self, revision):
        return "/".join(
            [self.manifest["origin"]["url"], "archive", revision + ".tar.gz"]
        )
