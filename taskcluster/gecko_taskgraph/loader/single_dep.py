# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import copy

from taskgraph.task import Task
from taskgraph.util.schema import Schema
from voluptuous import Required

schema = Schema(
    {
        Required("primary-dependency", "primary dependency task"): Task,
    }
)


def loader(kind, path, config, params, loaded_tasks):
    """
    Load tasks based on the jobs dependant kinds.

    The `only-for-build-platforms` kind configuration, if specified, will limit
    the build platforms for which a job will be created. Alternatively there is
    'not-for-build-platforms' kind configuration which will be consulted only after
    'only-for-build-platforms' is checked (if present), and omit any jobs where the
    build platform matches.

    Optional `only-for-attributes` kind configuration, if specified, will limit
    the jobs chosen to ones which have the specified attribute, with the specified
    value.

    Optional `job-template` kind configuration value, if specified, will be used to
    pass configuration down to the specified transforms used.
    """
    only_platforms = config.get("only-for-build-platforms")
    not_platforms = config.get("not-for-build-platforms")
    only_attributes = config.get("only-for-attributes")
    job_template = config.get("job-template")

    for task in loaded_tasks:
        if task.kind not in config.get("kind-dependencies", []):
            continue

        if only_platforms or not_platforms:
            build_platform = task.attributes.get("build_platform")
            build_type = task.attributes.get("build_type")
            if not build_platform or not build_type:
                continue
            platform = f"{build_platform}/{build_type}"
            if only_platforms and platform not in only_platforms:
                continue
            elif not_platforms and platform in not_platforms:
                continue

        if only_attributes:
            config_attrs = set(only_attributes)
            if not config_attrs & set(task.attributes):
                # make sure any attribute exists
                continue

        job = {
            "primary-dependency": task,
        }

        if job_template:
            job.update(copy.deepcopy(job_template))

        # copy shipping_product from upstream
        product = task.attributes.get(
            "shipping_product", task.task.get("shipping-product")
        )
        if product:
            job.setdefault("shipping-product", product)

        yield job
