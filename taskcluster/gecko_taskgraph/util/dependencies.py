# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from taskgraph.util.dependencies import group_by


def skip_only_or_not(config, task):
    """Return True if we should skip this task based on `only_` or `not_` config."""
    only_platforms = config.get("only-for-build-platforms")
    not_platforms = config.get("not-for-build-platforms")
    only_attributes = config.get("only-for-attributes")
    not_attributes = config.get("not-for-attributes")
    task_attrs = task.attributes
    if only_platforms or not_platforms:
        platform = task_attrs.get("build_platform")
        build_type = task_attrs.get("build_type")
        if not platform or not build_type:
            return True
        combined_platform = f"{platform}/{build_type}"
        if only_platforms and combined_platform not in only_platforms:
            return True
        if not_platforms and combined_platform in not_platforms:
            return True
    if only_attributes:
        if not set(only_attributes) & set(task_attrs):
            # make sure any attribute exists
            return True
    if not_attributes:
        if set(not_attributes) & set(task_attrs):
            return True
    return False


@group_by("single-with-filters")
def single_grouping(config, tasks):
    for task in tasks:
        if skip_only_or_not(config.config, task):
            continue
        yield [task]


@group_by("platform")
def platform_grouping(config, tasks):
    groups = {}
    for task in tasks:
        if task.kind not in config.config.get("kind-dependencies", []):
            continue
        if skip_only_or_not(config.config, task):
            continue
        platform = task.attributes.get("build_platform")
        build_type = task.attributes.get("build_type")
        product = task.attributes.get(
            "shipping_product", task.task.get("shipping-product")
        )

        groups.setdefault((platform, build_type, product), []).append(task)
    return groups.values()


@group_by("single-locale")
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
        if task.kind not in config.config.get("kind-dependencies", []):
            continue
        if skip_only_or_not(config.config, task):
            continue
        platform = task.attributes.get("build_platform")
        build_type = task.attributes.get("build_type")
        product = task.attributes.get(
            "shipping_product", task.task.get("shipping-product")
        )
        task_locale = task.attributes.get("locale")
        chunk_locales = task.attributes.get("chunk_locales")
        locales = chunk_locales or [task_locale]

        for locale in locales:
            locale_key = (platform, build_type, product, locale)
            groups.setdefault(locale_key, [])
            if task not in groups[locale_key]:
                groups[locale_key].append(task)

    return groups.values()


@group_by("chunk-locales")
def chunk_locale_grouping(config, tasks):
    """Split by a chunk_locale (but also by platform, build-type, product)

    This grouping is written for mac signing with notarization, but might also
    be useful elsewhere.

    """
    groups = {}

    for task in tasks:
        if task.kind not in config.config.get("kind-dependencies", []):
            continue
        if skip_only_or_not(config.config, task):
            continue
        platform = task.attributes.get("build_platform")
        build_type = task.attributes.get("build_type")
        product = task.attributes.get(
            "shipping_product", task.task.get("shipping-product")
        )
        chunk_locales = tuple(sorted(task.attributes.get("chunk_locales", [])))

        chunk_locale_key = (platform, build_type, product, chunk_locales)
        groups.setdefault(chunk_locale_key, [])
        if task not in groups[chunk_locale_key]:
            groups[chunk_locale_key].append(task)

    return groups.values()


@group_by("partner-repack-ids")
def partner_repack_ids_grouping(config, tasks):
    """Split by partner_repack_ids (but also by platform, build-type, product)

    This grouping is written for release-{eme-free,partner}-repack-signing.

    """
    groups = {}

    for task in tasks:
        if task.kind not in config.config.get("kind-dependencies", []):
            continue
        if skip_only_or_not(config.config, task):
            continue
        platform = task.attributes.get("build_platform")
        build_type = task.attributes.get("build_type")
        product = task.attributes.get(
            "shipping_product", task.task.get("shipping-product")
        )
        partner_repack_ids = tuple(
            sorted(task.task.get("extra", {}).get("repack_ids", []))
        )

        partner_repack_ids_key = (platform, build_type, product, partner_repack_ids)
        groups.setdefault(partner_repack_ids_key, [])
        if task not in groups[partner_repack_ids_key]:
            groups[partner_repack_ids_key].append(task)

    return groups.values()
