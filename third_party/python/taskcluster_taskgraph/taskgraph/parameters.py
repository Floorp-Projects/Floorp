# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import gzip
import hashlib
import json
import os
import time
from datetime import datetime
from io import BytesIO
from pprint import pformat
from subprocess import CalledProcessError
from urllib.parse import urlparse
from urllib.request import urlopen

from voluptuous import ALLOW_EXTRA, Optional, Required, Schema

from taskgraph.util import yaml
from taskgraph.util.readonlydict import ReadOnlyDict
from taskgraph.util.schema import validate_schema
from taskgraph.util.taskcluster import find_task_id, get_artifact_url
from taskgraph.util.vcs import get_repository


class ParameterMismatch(Exception):
    """Raised when a parameters.yml has extra or missing parameters."""


# Please keep this list sorted and in sync with taskcluster/docs/parameters.rst
base_schema = Schema(
    {
        Required("base_repository"): str,
        Required("build_date"): int,
        Required("do_not_optimize"): [str],
        Required("existing_tasks"): {str: str},
        Required("filters"): [str],
        Required("head_ref"): str,
        Required("head_repository"): str,
        Required("head_rev"): str,
        Required("head_tag"): str,
        Required("level"): str,
        Required("moz_build_date"): str,
        Required("optimize_target_tasks"): bool,
        Required("owner"): str,
        Required("project"): str,
        Required("pushdate"): int,
        Required("pushlog_id"): str,
        Required("repository_type"): str,
        # target-kind is not included, since it should never be
        # used at run-time
        Required("target_tasks_method"): str,
        Required("tasks_for"): str,
        Optional("code-review"): {
            Required("phabricator-build-target"): str,
        },
    }
)


def _get_defaults(repo_root=None):
    repo = get_repository(repo_root or os.getcwd())
    try:
        repo_url = repo.get_url()
        project = repo_url.rsplit("/", 1)[1]
    except (CalledProcessError, IndexError):
        # IndexError is raised if repo url doesn't have any slashes.
        repo_url = ""
        project = ""

    return {
        "base_repository": repo_url,
        "build_date": int(time.time()),
        "do_not_optimize": [],
        "existing_tasks": {},
        "filters": ["target_tasks_method"],
        "head_ref": repo.head_ref,
        "head_repository": repo_url,
        "head_rev": repo.head_ref,
        "head_tag": "",
        "level": "3",
        "moz_build_date": datetime.now().strftime("%Y%m%d%H%M%S"),
        "optimize_target_tasks": True,
        "owner": "nobody@mozilla.com",
        "project": project,
        "pushdate": int(time.time()),
        "pushlog_id": "0",
        "repository_type": repo.tool,
        "target_tasks_method": "default",
        "tasks_for": "",
    }


defaults_functions = [_get_defaults]


def extend_parameters_schema(schema, defaults_fn=None):
    """
    Extend the schema for parameters to include per-project configuration.

    This should be called by the `taskgraph.register` function in the
    graph-configuration.

    Args:
        schema (Schema): The voluptuous.Schema object used to describe extended
            parameters.
        defaults_fn (function): A function which takes no arguments and returns a
            dict mapping parameter name to default value in the
            event strict=False (optional).
    """
    global base_schema
    global defaults_functions
    base_schema = base_schema.extend(schema)
    if defaults_fn:
        defaults_functions.append(defaults_fn)


class Parameters(ReadOnlyDict):
    """An immutable dictionary with nicer KeyError messages on failure"""

    def __init__(self, strict=True, repo_root=None, **kwargs):
        self.strict = strict
        self.spec = kwargs.pop("spec", None)
        self._id = None

        if not self.strict:
            # apply defaults to missing parameters
            kwargs = Parameters._fill_defaults(repo_root=repo_root, **kwargs)

        ReadOnlyDict.__init__(self, **kwargs)

    @property
    def id(self):
        if not self._id:
            self._id = hashlib.sha256(
                json.dumps(self, sort_keys=True).encode("utf-8")
            ).hexdigest()[:12]

        return self._id

    @staticmethod
    def format_spec(spec):
        """
        Get a friendly identifier from a parameters specifier.

        Args:
            spec (str): Parameters specifier.

        Returns:
            str: Name to identify parameters by.
        """
        if spec is None:
            return "defaults"

        if any(spec.startswith(s) for s in ("task-id=", "project=")):
            return spec

        result = urlparse(spec)
        if result.scheme in ("http", "https"):
            spec = result.path

        return os.path.splitext(os.path.basename(spec))[0]

    @staticmethod
    def _fill_defaults(repo_root=None, **kwargs):
        defaults = {}
        for fn in defaults_functions:
            defaults.update(fn(repo_root))

        for name, default in defaults.items():
            if name not in kwargs:
                kwargs[name] = default
        return kwargs

    def check(self):
        schema = (
            base_schema if self.strict else base_schema.extend({}, extra=ALLOW_EXTRA)
        )
        try:
            validate_schema(schema, self.copy(), "Invalid parameters:")
        except Exception as e:
            raise ParameterMismatch(str(e))

    def __getitem__(self, k):
        try:
            return super().__getitem__(k)
        except KeyError:
            raise KeyError(f"taskgraph parameter {k!r} not found")

    def is_try(self):
        """
        Determine whether this graph is being built on a try project or for
        `mach try fuzzy`.
        """
        return "try" in self["project"] or self["tasks_for"] == "github-pull-request"

    @property
    def moz_build_date(self):
        # XXX self["moz_build_date"] is left as a string because:
        #  * of backward compatibility
        #  * parameters are output in a YAML file
        return datetime.strptime(self["moz_build_date"], "%Y%m%d%H%M%S")

    def file_url(self, path, pretty=False):
        """
        Determine the VCS URL for viewing a file in the tree, suitable for
        viewing by a human.

        :param str path: The path, relative to the root of the repository.
        :param bool pretty: Whether to return a link to a formatted version of the
            file, or the raw file version.

        :return str: The URL displaying the given path.
        """
        if self["repository_type"] == "hg":
            if path.startswith("comm/"):
                path = path[len("comm/") :]
                repo = self["comm_head_repository"]
                rev = self["comm_head_rev"]
            else:
                repo = self["head_repository"]
                rev = self["head_rev"]
            endpoint = "file" if pretty else "raw-file"
            return f"{repo}/{endpoint}/{rev}/{path}"
        elif self["repository_type"] == "git":
            # For getting the file URL for git repositories, we only support a Github HTTPS remote
            repo = self["head_repository"]
            if repo.startswith("https://github.com/"):
                if repo.endswith("/"):
                    repo = repo[:-1]

                rev = self["head_rev"]
                endpoint = "blob" if pretty else "raw"
                return f"{repo}/{endpoint}/{rev}/{path}"
            elif repo.startswith("git@github.com:"):
                if repo.endswith(".git"):
                    repo = repo[:-4]
                rev = self["head_rev"]
                endpoint = "blob" if pretty else "raw"
                return "{}/{}/{}/{}".format(
                    repo.replace("git@github.com:", "https://github.com/"),
                    endpoint,
                    rev,
                    path,
                )
            else:
                raise ParameterMismatch(
                    "Don't know how to determine file URL for non-github"
                    "repo: {}".format(repo)
                )
        else:
            raise RuntimeError(
                'Only the "git" and "hg" repository types are supported for using file_url()'
            )

    def __str__(self):
        return f"Parameters(id={self.id}) (from {self.format_spec(self.spec)})"

    def __repr__(self):
        return pformat(dict(self), indent=2)


def load_parameters_file(
    spec, strict=True, overrides=None, trust_domain=None, repo_root=None
):
    """
    Load parameters from a path, url, decision task-id or project.

    Examples:
        task-id=fdtgsD5DQUmAQZEaGMvQ4Q
        project=mozilla-central
    """

    if overrides is None:
        overrides = {}
    overrides["spec"] = spec

    if not spec:
        return Parameters(strict=strict, repo_root=repo_root, **overrides)

    try:
        # reading parameters from a local parameters.yml file
        f = open(spec)
    except OSError:
        # fetching parameters.yml using task task-id, project or supplied url
        task_id = None
        if spec.startswith("task-id="):
            task_id = spec.split("=")[1]
        elif spec.startswith("project="):
            if trust_domain is None:
                raise ValueError(
                    "Can't specify parameters by project "
                    "if trust domain isn't supplied.",
                )
            index = "{trust_domain}.v2.{project}.latest.taskgraph.decision".format(
                trust_domain=trust_domain,
                project=spec.split("=")[1],
            )
            task_id = find_task_id(index)

        if task_id:
            spec = get_artifact_url(task_id, "public/parameters.yml")
        f = urlopen(spec)

        # Decompress gzipped parameters.
        if f.info().get("Content-Encoding") == "gzip":
            buf = BytesIO(f.read())
            f = gzip.GzipFile(fileobj=buf)

    if spec.endswith(".yml"):
        kwargs = yaml.load_stream(f)
    elif spec.endswith(".json"):
        kwargs = json.load(f)
    else:
        raise TypeError(f"Parameters file `{spec}` is not JSON or YAML")

    kwargs.update(overrides)
    return Parameters(strict=strict, repo_root=repo_root, **kwargs)


def parameters_loader(spec, strict=True, overrides=None):
    def get_parameters(graph_config):
        try:
            repo_root = graph_config.vcs_root
        except Exception:
            repo_root = None

        parameters = load_parameters_file(
            spec,
            strict=strict,
            overrides=overrides,
            repo_root=repo_root,
            trust_domain=graph_config["trust-domain"],
        )
        parameters.check()
        return parameters

    return get_parameters
