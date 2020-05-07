# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import copy
import six
from six import text_type

from voluptuous import Required

from ..task import Task
from ..util.attributes import sorted_unique_list
from ..util.schema import Schema

schema = Schema({
    Required('primary-dependency', 'primary dependency task'): Task,
    Required(
        'dependent-tasks',
        'dictionary of dependent tasks, keyed by kind',
    ): {text_type: Task},
})


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

    Optional ``primary-dependency`` (ordered list or string) is used to determine
    which upstream kind to inherit attrs from. See ``get_primary_dep``.

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
        job['primary-dependency'] = get_primary_dep(config, dep_tasks)
        if job_template:
            job.update(copy.deepcopy(job_template))
        # copy shipping_product from upstream
        product = job['primary-dependency'].attributes.get(
            'shipping_product',
            job['primary-dependency'].task.get('shipping-product')
        )
        if product:
            job.setdefault('shipping-product', product)
        job.setdefault('attributes', {})['required_signoffs'] = sorted_unique_list(
            *[task.attributes.get('required_signoffs', [])
              for task in dep_tasks.values()
              ]
        )

        yield job


def skip_only_or_not(config, task):
    """Return True if we should skip this task based on only_ or not_ config."""
    only_platforms = config.get('only-for-build-platforms')
    not_platforms = config.get('not-for-build-platforms')
    only_attributes = config.get('only-for-attributes')
    not_attributes = config.get('not-for-attributes')
    task_attrs = task.attributes
    if only_platforms or not_platforms:
        platform = task_attrs.get('build_platform')
        build_type = task_attrs.get('build_type')
        if not platform or not build_type:
            return True
        combined_platform = "{}/{}".format(platform, build_type)
        if only_platforms and combined_platform not in only_platforms:
            return True
        elif not_platforms and combined_platform in not_platforms:
            return True
    if only_attributes:
        if not set(only_attributes) & set(task_attrs):
            # make sure any attribute exists
            return True
    if not_attributes:
        if set(not_attributes) & set(task_attrs):
            return True
    return False


def group_tasks(config, tasks):
    group_by_fn = GROUP_BY_MAP[config['group-by']]

    groups = group_by_fn(config, tasks)

    for combinations in six.itervalues(groups):
        kinds = [f.kind for f in combinations]
        assert_unique_members(kinds, error_msg=(
            "Multi_dep.py should have filtered down to one task per kind"))
        dependencies = {t.kind: copy.deepcopy(t) for t in combinations}
        yield dependencies


@group_by('platform')
def platform_grouping(config, tasks):
    groups = {}
    for task in tasks:
        if task.kind not in config.get('kind-dependencies', []):
            continue
        if skip_only_or_not(config, task):
            continue
        platform = task.attributes.get('build_platform')
        build_type = task.attributes.get('build_type')
        product = task.attributes.get('shipping_product',
                                      task.task.get('shipping-product'))

        groups.setdefault((platform, build_type, product), []).append(task)
    return groups


@group_by('single-locale')
def single_locale_grouping(config, tasks):
    """Split by a single locale (but also by platform, build-type, product)

    The locale can be `None` (en-US build/signing/repackage), a single locale,
    or multiple locales per task, e.g. for l10n chunking. In the case of a task
    with, say, five locales, the task will show up in all five locale groupings.

    This grouping is written for non-partner-repack beetmover, but might also
    be useful elsewhere.

    """
    groups = {}

    for task in tasks:
        if task.kind not in config.get('kind-dependencies', []):
            continue
        if skip_only_or_not(config, task):
            continue
        platform = task.attributes.get('build_platform')
        build_type = task.attributes.get('build_type')
        product = task.attributes.get('shipping_product',
                                      task.task.get('shipping-product'))
        task_locale = task.attributes.get('locale')
        chunk_locales = task.attributes.get('chunk_locales')
        locales = chunk_locales or [task_locale]

        for locale in locales:
            locale_key = (platform, build_type, product, locale)
            groups.setdefault(locale_key, [])
            if task not in groups[locale_key]:
                groups[locale_key].append(task)

    return groups


@group_by('chunk-locales')
def chunk_locale_grouping(config, tasks):
    """Split by a chunk_locale (but also by platform, build-type, product)

    This grouping is written for mac signing with notarization, but might also
    be useful elsewhere.

    """
    groups = {}

    for task in tasks:
        if task.kind not in config.get('kind-dependencies', []):
            continue
        if skip_only_or_not(config, task):
            continue
        platform = task.attributes.get('build_platform')
        build_type = task.attributes.get('build_type')
        product = task.attributes.get('shipping_product',
                                      task.task.get('shipping-product'))
        chunk_locales = tuple(sorted(task.attributes.get('chunk_locales', [])))

        chunk_locale_key = (platform, build_type, product, chunk_locales)
        groups.setdefault(chunk_locale_key, [])
        if task not in groups[chunk_locale_key]:
            groups[chunk_locale_key].append(task)

    return groups


@group_by('partner-repack-ids')
def partner_repack_ids_grouping(config, tasks):
    """Split by partner_repack_ids (but also by platform, build-type, product)

    This grouping is written for release-{eme-free,partner}-repack-signing.

    """
    groups = {}

    for task in tasks:
        if task.kind not in config.get('kind-dependencies', []):
            continue
        if skip_only_or_not(config, task):
            continue
        platform = task.attributes.get('build_platform')
        build_type = task.attributes.get('build_type')
        product = task.attributes.get('shipping_product',
                                      task.task.get('shipping-product'))
        partner_repack_ids = tuple(sorted(task.task.get('extra', {}).get('repack_ids', [])))

        partner_repack_ids_key = (platform, build_type, product, partner_repack_ids)
        groups.setdefault(partner_repack_ids_key, [])
        if task not in groups[partner_repack_ids_key]:
            groups[partner_repack_ids_key].append(task)

    return groups


def assert_unique_members(kinds, error_msg=None):
    if len(kinds) != len(set(kinds)):
        raise Exception(error_msg)


def get_primary_dep(config, dep_tasks):
    """Find the dependent task to inherit attributes from.

    If ``primary-dependency`` is defined in ``kind.yml`` and is a string,
    then find the first dep with that task kind and return it. If it is
    defined and is a list, the first kind in that list with a matching dep
    is the primary dependency. If it's undefined, return the first dep.

    """
    primary_dependencies = config.get('primary-dependency')
    if isinstance(primary_dependencies, text_type):
        primary_dependencies = [primary_dependencies]
    if not primary_dependencies:
        assert len(dep_tasks) == 1, "Must define a primary-dependency!"
        return list(dep_tasks.values())[0]
    primary_dep = None
    for primary_kind in primary_dependencies:
        for dep_kind in dep_tasks:
            if dep_kind == primary_kind:
                assert primary_dep is None, \
                    "Too many primary dependent tasks in dep_tasks: {}!".format(
                        [t.label for t in dep_tasks]
                    )
                primary_dep = dep_tasks[dep_kind]
    if primary_dep is None:
        raise Exception("Can't find dependency of {}: {}".format(
            config['primary-dependency'], config
        ))
    return primary_dep
