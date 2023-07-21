# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from itertools import islice

from taskgraph import MAX_DEPENDENCIES
from taskgraph.transforms.base import TransformSequence
from taskgraph.util.dependencies import get_primary_dependency

from gecko_taskgraph.util.platforms import architecture
from gecko_taskgraph.util.scriptworker import (
    generate_artifact_registry_gcs_sources,
    get_beetmover_apt_repo_scope,
    get_beetmover_repo_action_scope,
)

transforms = TransformSequence()


@transforms.add
def beetmover_apt(config, tasks):
    product = (
        "firefox"
        if not config.params["release_type"]  # try
        or config.params["release_type"] == "nightly"
        else config.params["release_product"]
    )
    filtered_tasks = filter_beetmover_apt_tasks(config, tasks, product)
    # There are too many beetmover-repackage dependencies for a single task
    # and we hit the taskgraph dependencies limit.
    # To work around this limitation, we chunk the would be task
    # into tasks dependendent on, at most, half of MAX_DEPENDENCIES.
    batches = batched(filtered_tasks, MAX_DEPENDENCIES // 2)
    for index, batch in enumerate(batches):
        dependencies = {}
        gcs_sources = []
        for task in batch:
            dep = get_primary_dependency(config, task)
            assert dep

            dependencies[dep.label] = dep.label
            gcs_sources.extend(generate_artifact_registry_gcs_sources(dep))
        description = f"Batch {index + 1} of beetmover APT submissions for the {config.params['release_type']} .deb packages"
        platform = "firefox-release/opt"
        treeherder = {
            "platform": platform,
            "tier": 1,
            "kind": "other",
            "symbol": f"BM-apt(batch-{index + 1})",
        }
        apt_repo_scope = get_beetmover_apt_repo_scope(config)
        repo_action_scope = get_beetmover_repo_action_scope(config)
        attributes = {
            "required_signoffs": ["mar-signing"],
            "shippable": True,
            "shipping_product": product,
        }
        task = {
            "label": f"{config.kind}-{index + 1}-{platform}",
            "description": description,
            "worker-type": "beetmover",
            "treeherder": treeherder,
            "scopes": [apt_repo_scope, repo_action_scope],
            "attributes": attributes,
            "shipping-phase": "ship",
            "shipping-product": product,
            "dependencies": dependencies,
        }
        worker = {
            "implementation": "beetmover-import-from-gcs-to-artifact-registry",
            "product": product,
            "gcs-sources": gcs_sources,
        }
        task["worker"] = worker
        yield task


def batched(iterable, n):
    "Batch data into tuples of length n. The last batch may be shorter."
    # batched('ABCDEFG', 3) --> ABC DEF G
    if n < 1:
        raise ValueError("n must be at least one")
    it = iter(iterable)
    batch = tuple(islice(it, n))
    while batch:
        yield batch
        batch = tuple(islice(it, n))


def filter_beetmover_apt_tasks(config, tasks, product):
    for task in tasks:
        task["primary-dependency"] = get_primary_dependency(config, task)
        if filter_beetmover_apt_task(task, product):
            yield task


def filter_beetmover_apt_task(task, product):
    # We only create beetmover-apt tasks for l10n beetmover-repackage tasks that
    # beetmove langpack .deb packages. The langpack .deb packages support all
    # architectures, so we generate them only on x86_64 tasks.
    return (
        is_x86_64_l10n_task(task) or is_not_l10n_task(task)
    ) and is_task_for_product(task, product)


def is_x86_64_l10n_task(task):
    dep = task["primary-dependency"]
    locale = dep.attributes.get("locale")
    return locale and architecture(dep.attributes["build_platform"]) == "x86_64"


def is_not_l10n_task(task):
    dep = task["primary-dependency"]
    locale = dep.attributes.get("locale")
    return not locale


def is_task_for_product(task, product):
    dep = task["primary-dependency"]
    return dep.attributes.get("shipping_product") == product
