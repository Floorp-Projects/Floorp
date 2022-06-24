# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import re
from typing import AnyStr

import attr

from ..config import GraphConfig
from ..parameters import Parameters
from ..util.memoize import memoize
from ..util.schema import Schema, validate_schema


@attr.s(frozen=True)
class RepoConfig:
    prefix = attr.ib(type=str)
    name = attr.ib(type=str)
    base_repository = attr.ib(type=str)
    head_repository = attr.ib(type=str)
    head_ref = attr.ib(type=str)
    type = attr.ib(type=str)
    path = attr.ib(type=str, default="")
    head_rev = attr.ib(type=str, default=None)
    ssh_secret_name = attr.ib(type=str, default=None)


@attr.s(frozen=True, cmp=False)
class TransformConfig:
    """
    A container for configuration affecting transforms.  The `config` argument
    to transforms is an instance of this class.
    """

    # the name of the current kind
    kind = attr.ib()

    # the path to the kind configuration directory
    path = attr.ib(type=AnyStr)

    # the parsed contents of kind.yml
    config = attr.ib(type=dict)

    # the parameters for this task-graph generation run
    params = attr.ib(type=Parameters)

    # a list of all the tasks associated with the kind dependencies of the
    # current kind
    kind_dependencies_tasks = attr.ib()

    # Global configuration of the taskgraph
    graph_config = attr.ib(type=GraphConfig)

    # whether to write out artifacts for the decision task
    write_artifacts = attr.ib(type=bool)

    @property
    @memoize
    def repo_configs(self):
        repositories = self.graph_config["taskgraph"]["repositories"]
        if len(repositories) == 1:
            current_prefix = list(repositories.keys())[0]
        else:
            project = self.params["project"]
            matching_repos = {
                repo_prefix: repo
                for (repo_prefix, repo) in repositories.items()
                if re.match(repo["project-regex"], project)
            }
            if len(matching_repos) != 1:
                raise Exception(
                    f"Couldn't find repository matching project `{project}`"
                )
            current_prefix = list(matching_repos.keys())[0]

        repo_configs = {
            current_prefix: RepoConfig(
                prefix=current_prefix,
                name=repositories[current_prefix]["name"],
                base_repository=self.params["base_repository"],
                head_repository=self.params["head_repository"],
                head_ref=self.params["head_ref"],
                head_rev=self.params["head_rev"],
                type=self.params["repository_type"],
                ssh_secret_name=repositories[current_prefix].get("ssh-secret-name"),
            ),
        }
        if len(repositories) != 1:
            repo_configs.update(
                {
                    repo_prefix: RepoConfig(
                        prefix=repo_prefix,
                        name=repo["name"],
                        base_repository=repo["default-repository"],
                        head_repository=repo["default-repository"],
                        head_ref=repo["default-ref"],
                        type=repo["type"],
                        ssh_secret_name=repo.get("ssh-secret-name"),
                    )
                    for (repo_prefix, repo) in repositories.items()
                    if repo_prefix != current_prefix
                }
            )
        return repo_configs


@attr.s()
class TransformSequence:
    """
    Container for a sequence of transforms.  Each transform is represented as a
    callable taking (config, items) and returning a generator which will yield
    transformed items.  The resulting sequence has the same interface.

    This is convenient to use in a file full of transforms, as it provides a
    decorator, @transforms.add, that will add the decorated function to the
    sequence.
    """

    _transforms = attr.ib(factory=list)

    def __call__(self, config, items):
        for xform in self._transforms:
            items = xform(config, items)
            if items is None:
                raise Exception(f"Transform {xform} is not a generator")
        return items

    def add(self, func):
        self._transforms.append(func)
        return func

    def add_validate(self, schema):
        self.add(ValidateSchema(schema))


@attr.s
class ValidateSchema:
    schema = attr.ib(type=Schema)

    def __call__(self, config, tasks):
        for task in tasks:
            if "name" in task:
                error = "In {kind} kind task {name!r}:".format(
                    kind=config.kind, name=task["name"]
                )
            elif "label" in task:
                error = "In job {label!r}:".format(label=task["label"])
            elif "primary-dependency" in task:
                error = "In {kind} kind task for {dependency!r}:".format(
                    kind=config.kind, dependency=task["primary-dependency"].label
                )
            else:
                error = "In unknown task:"
            validate_schema(self.schema, task, error)
            yield task
