# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import urllib
import requests

from mozbuild.vendor.host_base import BaseHost


class GitLabHost(BaseHost):
    def upstream_commit(self, revision):
        """Query the gitlab api for a git commit id and timestamp."""
        repo_url = urllib.parse.urlparse(self.manifest["origin"]["url"])
        gitlab_api = repo_url.scheme + "://" + repo_url.netloc + "/api/v4/projects/"
        gitlab_api += repo_url.path[1:].replace("/", "%2F")
        gitlab_api += "/repository/commits"
        req = requests.get("/".join([gitlab_api, revision]))
        req.raise_for_status()
        info = req.json()
        return (info["id"], info["committed_date"])

    def upstream_snapshot(self, revision):
        return "/".join(
            [self.manifest["origin"]["url"], "-", "archive", revision + ".tar.gz"]
        )
