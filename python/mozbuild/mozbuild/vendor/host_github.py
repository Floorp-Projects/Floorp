# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

import requests

from mozbuild.vendor.host_base import BaseHost


class GitHubHost(BaseHost):
    def api_get(self, path):
        """Generic Github API get."""
        repo = self.repo_url.path[1:].strip("/")
        github_api = f"https://api.github.com/repos/{repo}/{path}"
        req = requests.get(github_api)
        req.raise_for_status()
        return req.json()

    def upstream_commit(self, revision):
        """Query the github api for a git commit id and timestamp."""
        info = self.api_get(f"commits/{revision}")
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
