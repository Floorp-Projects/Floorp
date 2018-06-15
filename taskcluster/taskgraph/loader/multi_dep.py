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


def loader(kind, path, config, params, loaded_tasks):
    """
    Load tasks based on the jobs dependant kinds, designed for use as
    multiple-dependent needs.

    Required ``group-by-fn`` is used to define how we coalesce the
    multiple deps together to pass to transforms, e.g. all kinds specified get
    collapsed by platform with `platform`

    The `only-for-build-platforms` kind configuration, if specified, will limit
    the build platforms for which a job will be created. Alternatively there is
    'not-for-build-platforms' kind configuration which will be consulted only after
    'only-for-build-platforms' is checked (if present), and omit any jobs where the
    build platform matches.

    Optional ``job-template`` kind configuration value, if specified, will be used to
    pass configuration down to the specified transforms used.
    """
    job_template = config.get('job-template')

    for dep_tasks in group_tasks(config, loaded_tasks):
        job = {'dependent-tasks': dep_tasks}
        if job_template:
            job.update(copy.deepcopy(job_template))
        # copy shipping_product from upstream
        product = dep_tasks[dep_tasks.keys()[0]].attributes.get(
            'shipping_product',
            dep_tasks[dep_tasks.keys()[0]].task.get('shipping-product')
        )
        if product:
            job.setdefault('shipping-product', product)

        yield job


def group_tasks(config, tasks):
    group_by_fn = GROUP_BY_MAP[config['group-by']]

    groups = group_by_fn(config, tasks)

    for combinations in groups.itervalues():
        kinds = [f.kind for f in combinations]
        assert_unique_members(kinds, error_msg=(
            "Multi_dep.py should have filtered down to one task per kind"))
        dependencies = {t.kind: copy.deepcopy(t) for t in combinations}
        yield dependencies


@group_by('platform')
def platform_grouping(config, tasks):
    only_platforms = config.get('only-for-build-platforms')
    not_platforms = config.get('not-for-build-platforms')

    groups = {}
    for task in tasks:
        if task.kind not in config.get('kind-dependencies', []):
            continue
        platform = task.attributes.get('build_platform')
        build_type = task.attributes.get('build_type')
        product = task.attributes.get('shipping_product',
                                      task.task.get('shipping-product'))

        # Skip only_ and not_ platforms that don't match
        if only_platforms or not_platforms:
            if not platform or not build_type:
                continue
            combined_platform = "{}/{}".format(platform, build_type)
            if only_platforms and combined_platform not in only_platforms:
                continue
            elif not_platforms and combined_platform in not_platforms:
                continue

        groups.setdefault((platform, build_type, product), []).append(task)
    return groups


def assert_unique_members(kinds, error_msg=None):
    if len(kinds) != len(set(kinds)):
        raise Exception(error_msg)
