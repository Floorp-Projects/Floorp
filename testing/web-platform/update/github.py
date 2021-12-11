# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
from six.moves.urllib.parse import urljoin

requests = None


class GitHubError(Exception):
    def __init__(self, status, data):
        self.status = status
        self.data = data


class GitHub(object):
    url_base = "https://api.github.com"

    def __init__(self, token):
        # Defer the import of requests since it isn't installed by default
        global requests
        if requests is None:
            import requests

        self.headers = {"Accept": "application/vnd.github.v3+json"}
        self.auth = (token, "x-oauth-basic")

    def get(self, path):
        return self._request("GET", path)

    def post(self, path, data):
        return self._request("POST", path, data=data)

    def put(self, path, data, headers=None):
        return self._request("PUT", path, data=data, headers=headers)

    def _request(self, method, path, data=None, headers=None):
        url = urljoin(self.url_base, path)

        headers_ = self.headers
        if headers is not None:
            headers_.update(headers)
        kwargs = {"headers": headers_, "auth": self.auth}
        if data is not None:
            kwargs["data"] = json.dumps(data)

        resp = requests.request(method, url, **kwargs)

        if 200 <= resp.status_code < 300:
            return resp.json()
        else:
            print(method, path, resp.status_code, resp.json())
            raise GitHubError(resp.status_code, resp.json())

    def repo(self, owner, name):
        """GitHubRepo for a particular repository.

        :param owner: String repository owner
        :param name: String repository name
        """
        return GitHubRepo.from_name(self, owner, name)


class GitHubRepo(object):
    def __init__(self, github, data):
        """Object respresenting a GitHub respoitory"""
        self.gh = github
        self.owner = data["owner"]
        self.name = data["name"]
        self.url = data["ssh_url"]
        self._data = data

    @classmethod
    def from_name(cls, github, owner, name):
        data = github.get("/repos/%s/%s" % (owner, name))
        return cls(github, data)

    @property
    def url_base(self):
        return "/repos/%s/" % (self._data["full_name"])

    def create_pr(self, title, head, base, body):
        """Create a Pull Request in the repository

        :param title: Pull Request title
        :param head: ref to the HEAD of the PR branch.
        :param base: ref to the base branch for the Pull Request
        :param body: Description of the PR
        """
        return PullRequest.create(self, title, head, base, body)

    def load_pr(self, number):
        """Load an existing Pull Request by number.

        :param number: Pull Request number
        """
        return PullRequest.from_number(self, number)

    def path(self, suffix):
        return urljoin(self.url_base, suffix)


class PullRequest(object):
    def __init__(self, repo, data):
        """Object representing a Pull Request"""

        self.repo = repo
        self._data = data
        self.number = data["number"]
        self.title = data["title"]
        self.base = data["base"]["ref"]
        self.base = data["head"]["ref"]
        self._issue = None

    @classmethod
    def from_number(cls, repo, number):
        data = repo.gh.get(repo.path("pulls/%i" % number))
        return cls(repo, data)

    @classmethod
    def create(cls, repo, title, head, base, body):
        data = repo.gh.post(
            repo.path("pulls"),
            {"title": title, "head": head, "base": base, "body": body},
        )
        return cls(repo, data)

    def path(self, suffix):
        return urljoin(self.repo.path("pulls/%i/" % self.number), suffix)

    @property
    def issue(self):
        """Issue related to the Pull Request"""
        if self._issue is None:
            self._issue = Issue.from_number(self.repo, self.number)
        return self._issue

    def merge(self):
        """Merge the Pull Request into its base branch."""
        self.repo.gh.put(
            self.path("merge"),
            {"merge_method": "merge"},
            headers={"Accept": "application/vnd.github.polaris-preview+json"},
        )


class Issue(object):
    def __init__(self, repo, data):
        """Object representing a GitHub Issue"""
        self.repo = repo
        self._data = data
        self.number = data["number"]

    @classmethod
    def from_number(cls, repo, number):
        data = repo.gh.get(repo.path("issues/%i" % number))
        return cls(repo, data)

    def path(self, suffix):
        return urljoin(self.repo.path("issues/%i/" % self.number), suffix)

    def add_comment(self, message):
        """Add a comment to the issue

        :param message: The text of the comment
        """
        self.repo.gh.post(self.path("comments"), {"body": message})
