# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Add from parameters.yml into bouncer submission tasks.
"""


import logging

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.schema import resolve_keyed_by

from gecko_taskgraph.transforms.bouncer_submission import (
    FTP_PLATFORMS_PER_BOUNCER_PLATFORM,
    CONFIG_PER_BOUNCER_PRODUCT as CONFIG_PER_BOUNCER_PRODUCT_VANILLA,
    _craft_ftp_product,
    _craft_filename_product,
)
from gecko_taskgraph.util.attributes import release_level
from gecko_taskgraph.util.partners import (
    check_if_partners_enabled,
    get_partners_to_be_published,
)
from gecko_taskgraph.util.scriptworker import get_release_config

logger = logging.getLogger(__name__)


PARTNER_PLATFORMS_TO_BOUNCER = {
    "linux-shippable": "linux",
    "linux64-shippable": "linux64",
    "macosx64-shippable": "osx",
    "win32-shippable": "win",
    "win64-shippable": "win64",
    "win64-aarch64-shippable": "win64-aarch64",
}

# :lang is interpolated by bouncer at runtime
RELEASES_PARTNERS_PATH_TEMPLATE = "/{ftp_product}/releases/partners/{partner}/{sub_config}/\
{version}/{ftp_platform}/:lang/{file}"

CONFIG_PER_BOUNCER_PRODUCT = {
    "installer": {
        "name_postfix": "-{partner}-{sub_config}",
        "path_template": RELEASES_PARTNERS_PATH_TEMPLATE,
        "file_names": CONFIG_PER_BOUNCER_PRODUCT_VANILLA["installer"]["file_names"],
    },
    "stub-installer": {
        "name_postfix": "-{partner}-{sub_config}-stub",
        # We currently have a sole win32 stub installer that is to be used
        # in all windows platforms to toggle between full installers
        "path_template": RELEASES_PARTNERS_PATH_TEMPLATE.replace(
            "{ftp_platform}", "win32"
        ),
        "file_names": CONFIG_PER_BOUNCER_PRODUCT_VANILLA["stub-installer"][
            "file_names"
        ],
    },
}

transforms = TransformSequence()
transforms.add(check_if_partners_enabled)


@transforms.add
def make_task_worker(config, jobs):
    for job in jobs:
        resolve_keyed_by(
            job,
            "worker-type",
            item_name=job["name"],
            **{"release-level": release_level(config.params["project"])}
        )
        resolve_keyed_by(
            job,
            "scopes",
            item_name=job["name"],
            **{"release-level": release_level(config.params["project"])}
        )
        resolve_keyed_by(
            job,
            "bouncer-products",
            item_name=job["name"],
            **{"release-type": config.params["release_type"]}
        )

        # the schema requires at least one locale but this will not be used
        job["worker"]["locales"] = ["fake"]
        job["worker"]["entries"] = craft_bouncer_entries(config, job)

        del job["locales-file"]
        del job["bouncer-platforms"]
        del job["bouncer-products"]

        if job["worker"]["entries"]:
            yield job


def craft_bouncer_entries(config, job):
    release_config = get_release_config(config)

    product = job["shipping-product"]
    current_version = release_config["version"]
    bouncer_products = job["bouncer-products"]

    partners = get_partners_to_be_published(config)
    entries = {}
    for partner, sub_config_name, platforms in partners:
        platforms = [PARTNER_PLATFORMS_TO_BOUNCER[p] for p in platforms]
        entries.update(
            {
                craft_partner_bouncer_product_name(
                    product, bouncer_product, current_version, partner, sub_config_name
                ): {
                    "options": {
                        "add_locales": False,  # partners may use different sets of locales
                        "ssl_only": craft_ssl_only(bouncer_product),
                    },
                    "paths_per_bouncer_platform": craft_paths_per_bouncer_platform(
                        product,
                        bouncer_product,
                        platforms,
                        current_version,
                        partner,
                        sub_config_name,
                    ),
                }
                for bouncer_product in bouncer_products
            }
        )
    return entries


def craft_paths_per_bouncer_platform(
    product, bouncer_product, bouncer_platforms, current_version, partner, sub_config
):
    paths_per_bouncer_platform = {}
    for bouncer_platform in bouncer_platforms:
        file_names_per_platform = CONFIG_PER_BOUNCER_PRODUCT[bouncer_product][
            "file_names"
        ]
        file_name_template = file_names_per_platform.get(
            bouncer_platform, file_names_per_platform.get("default", None)
        )
        if not file_name_template:
            # Some bouncer product like stub-installer are only meant to be on Windows.
            # Thus no default value is defined there
            continue

        file_name_product = _craft_filename_product(product)
        file_name = file_name_template.format(
            product=file_name_product,
            pretty_product=file_name_product.capitalize(),
            version=current_version,
        )

        path_template = CONFIG_PER_BOUNCER_PRODUCT[bouncer_product]["path_template"]
        file_relative_location = path_template.format(
            ftp_product=_craft_ftp_product(product),
            version=current_version,
            ftp_platform=FTP_PLATFORMS_PER_BOUNCER_PLATFORM[bouncer_platform],
            partner=partner,
            sub_config=sub_config,
            file=file_name,
        )

        paths_per_bouncer_platform[bouncer_platform] = file_relative_location

    return paths_per_bouncer_platform


def craft_partner_bouncer_product_name(
    product, bouncer_product, current_version, partner, sub_config
):
    postfix = (
        CONFIG_PER_BOUNCER_PRODUCT[bouncer_product]
        .get("name_postfix", "")
        .format(
            partner=partner,
            sub_config=sub_config,
        )
    )

    return "{product}-{version}{postfix}".format(
        product=product.capitalize(), version=current_version, postfix=postfix
    )


def craft_ssl_only(bouncer_product):
    return bouncer_product == "stub-installer"
