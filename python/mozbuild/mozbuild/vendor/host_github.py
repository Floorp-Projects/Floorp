# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

import requests

from mozbuild.vendor.host_base import BaseHost


class GitHubHost(BaseHost):
    def upstream_commit(self, revision):
        """Query the github api for a git commit id and timestamp."""
        github_api = "https://api.github.com"
        repo = self.repo_url.path[1:].strip("/")
        req = requests.get("/".join([github_api, "repos", repo, "commits", revision]))
        req.raise_for_status()
        info = req.json()
        return (info["sha"], info["commit"]["committer"]["date"])

    def upstream_snapshot(self, revision):
        return "/".join(
            [self.manifest["vendoring"]["url"], "archive", revision + ".tar.gz"]
        )

    def upstream_path_to_file(self, revision, filepath):
        repo = self.repo_url.path[1:]
        return "/".join(["https://raw.githubusercontent.com", repo, revision, filepath])

    def upstream_release_artifact(self, revision, release_artifact):
        repo = self.repo_url.path[1:]
        release_artifact = release_artifact.format(tag=revision)
        return (
            f"https://github.com/{repo}/releases/download/{revision}/{release_artifact}"
        )
