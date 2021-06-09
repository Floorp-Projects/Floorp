# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import copy


# Define a collection of group_by functions
GROUP_BY_MAP = {}


def group_by(name):
    def wrapper(func):
        GROUP_BY_MAP[name] = func
        return func
    return wrapper



def group_tasks(config, tasks):
    group_by_fn = GROUP_BY_MAP[config['group-by']]

    groups = group_by_fn(config, tasks)

    for combinations in groups.values():
        dependencies = [copy.deepcopy(t) for t in combinations]
        yield dependencies


@group_by('build-type')
def build_type_grouping(config, tasks):
    groups = {}
    kind_dependencies = config.get('kind-dependencies')
    only_build_type = config.get('only-for-build-types')

    for task in tasks:
        if task.kind not in kind_dependencies:
            continue

        if only_build_type:
            build_type = task.attributes.get('build-type')
            if build_type not in only_build_type:
                continue

        build_type = task.attributes.get('build-type')

        groups.setdefault(build_type, []).append(task)

    return groups


@group_by('attributes')
def attributes_grouping(config, tasks):
    groups = {}
    kind_dependencies = config.get('kind-dependencies')
    only_attributes = config.get('only-for-attributes')

    for task in tasks:
        if task.kind not in kind_dependencies:
            continue

        group_attr = None
        if only_attributes:
            if not any(attr in task.attributes for attr in only_attributes):
                continue
        else:
            continue

        groups.setdefault(task.label, []).append(task)

    return groups
