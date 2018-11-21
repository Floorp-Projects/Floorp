# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import attr

from ..util.schema import Schema, validate_schema


class TransformConfig(object):
    """A container for configuration affecting transforms.  The `config`
    argument to transforms is an instance of this class, possibly with
    additional kind-specific attributes beyond those set here."""
    def __init__(self, kind, path, config, params,
                 kind_dependencies_tasks=None, graph_config=None):
        # the name of the current kind
        self.kind = kind

        # the path to the kind configuration directory
        self.path = path

        # the parsed contents of kind.yml
        self.config = config

        # the parameters for this task-graph generation run
        self.params = params

        # a list of all the tasks associated with the kind dependencies of the
        # current kind
        self.kind_dependencies_tasks = kind_dependencies_tasks

        # Global configuration of the taskgraph
        self.graph_config = graph_config or {}


class TransformSequence(object):
    """
    Container for a sequence of transforms.  Each transform is represented as a
    callable taking (config, items) and returning a generator which will yield
    transformed items.  The resulting sequence has the same interface.

    This is convenient to use in a file full of transforms, as it provides a
    decorator, @transforms.add, that will add the decorated function to the
    sequence.
    """

    def __init__(self, transforms=None):
        self.transforms = transforms or []

    def __call__(self, config, items):
        for xform in self.transforms:
            items = xform(config, items)
            if items is None:
                raise Exception("Transform {} is not a generator".format(xform))
        return items

    def __repr__(self):
        return '\n'.join(
            ['TransformSequence(['] +
            [repr(x) for x in self.transforms] +
            ['])'])

    def add(self, func):
        self.transforms.append(func)
        return func

    def add_validate(self, schema):
        self.add(ValidateSchema(schema))


@attr.s
class ValidateSchema(object):
    schema = attr.ib(type=Schema)

    def __call__(self, config, tasks):
        for task in tasks:
            if 'name' in task:
                error = "In {kind} kind task {name!r}:".format(
                    kind=config.kind, name=task['name'])
            elif 'label' in task:
                error = "In job {label!r}:".format(label=task['label'])
            elif 'primary-dependency' in task:
                error = "In {kind} kind task for {dependency!r}:".format(
                    kind=config.kind, dependency=task['primary-dependency'].label)
            else:
                error = "In unknown task:"
            validate_schema(self.schema, task, error)
            yield task
