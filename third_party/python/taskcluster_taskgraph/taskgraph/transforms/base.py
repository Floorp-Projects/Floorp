# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import functools
import re
from dataclasses import dataclass, field
from typing import Dict, List, Union

from taskgraph.task import Task

from ..config import GraphConfig
from ..parameters import Parameters
from ..util.schema import Schema, validate_schema


@dataclass(frozen=True)
class RepoConfig:
    prefix: str
    name: str
    base_repository: str
    head_repository: str
    head_ref: str
    type: str
    path: str = ""
    head_rev: Union[str, None] = None
    ssh_secret_name: Union[str, None] = None


@dataclass(frozen=True, eq=False)
class TransformConfig:
    """
    A container for configuration affecting transforms.  The `config` argument
    to transforms is an instance of this class.
    """

    # the name of the current kind
    kind: str

    # the path to the kind configuration directory
    path: str

    # the parsed contents of kind.yml
    config: Dict

    # the parameters for this task-graph generation run
    params: Parameters

    # a dict of all the tasks associated with the kind dependencies of the
    # current kind
    kind_dependencies_tasks: Dict[str, Task]

    # Global configuration of the taskgraph
    graph_config: GraphConfig

    # whether to write out artifacts for the decision task
    write_artifacts: bool

    @property
    @functools.lru_cache(maxsize=None)
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


@dataclass()
class TransformSequence:
    """
    Container for a sequence of transforms.  Each transform is represented as a
    callable taking (config, items) and returning a generator which will yield
    transformed items.  The resulting sequence has the same interface.

    This is convenient to use in a file full of transforms, as it provides a
    decorator, @transforms.add, that will add the decorated function to the
    sequence.
    """

    _transforms: List = field(default_factory=list)

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


@dataclass
class ValidateSchema:
    schema: Schema

    def __call__(self, config, tasks):
        for task in tasks:
            if "name" in task:
                error = "In {kind} kind task {name!r}:".format(
                    kind=config.kind, name=task["name"]
                )
            elif "label" in task:
                error = "In task {label!r}:".format(label=task["label"])
            elif "primary-dependency" in task:
                error = "In {kind} kind task for {dependency!r}:".format(
                    kind=config.kind, dependency=task["primary-dependency"].label
                )
            else:
                error = "In unknown task:"
            validate_schema(self.schema, task, error)
            yield task
