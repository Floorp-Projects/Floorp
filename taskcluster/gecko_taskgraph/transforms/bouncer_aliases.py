# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Add from parameters.yml into bouncer submission tasks.
"""


import logging

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.schema import resolve_keyed_by

from gecko_taskgraph.transforms.bouncer_submission import craft_bouncer_product_name
from gecko_taskgraph.transforms.bouncer_submission_partners import (
    craft_partner_bouncer_product_name,
)
from gecko_taskgraph.util.attributes import release_level
from gecko_taskgraph.util.partners import get_partners_to_be_published
from gecko_taskgraph.util.scriptworker import get_release_config

logger = logging.getLogger(__name__)

transforms = TransformSequence()


@transforms.add
def make_task_worker(config, jobs):
    for job in jobs:
        resolve_keyed_by(
            job,
            "worker-type",
            item_name=job["name"],
            **{"release-level": release_level(config.params["project"])},
        )
        resolve_keyed_by(
            job,
            "scopes",
            item_name=job["name"],
            **{"release-level": release_level(config.params["project"])},
        )
        resolve_keyed_by(
            job,
            "bouncer-products-per-alias",
            item_name=job["name"],
            **{"release-type": config.params["release_type"]},
        )
        if "partner-bouncer-products-per-alias" in job:
            resolve_keyed_by(
                job,
                "partner-bouncer-products-per-alias",
                item_name=job["name"],
                **{"release-type": config.params["release_type"]},
            )

        job["worker"]["entries"] = craft_bouncer_entries(config, job)

        del job["bouncer-products-per-alias"]
        if "partner-bouncer-products-per-alias" in job:
            del job["partner-bouncer-products-per-alias"]

        if job["worker"]["entries"]:
            yield job
        else:
            logger.warning(
                'No bouncer entries defined in bouncer submission task for "{}". \
Job deleted.'.format(
                    job["name"]
                )
            )


def craft_bouncer_entries(config, job):
    release_config = get_release_config(config)

    product = job["shipping-product"]
    current_version = release_config["version"]
    bouncer_products_per_alias = job["bouncer-products-per-alias"]

    entries = {
        bouncer_alias: craft_bouncer_product_name(
            product,
            bouncer_product,
            current_version,
        )
        for bouncer_alias, bouncer_product in bouncer_products_per_alias.items()
    }

    partner_bouncer_products_per_alias = job.get("partner-bouncer-products-per-alias")
    if partner_bouncer_products_per_alias:
        partners = get_partners_to_be_published(config)
        for partner, sub_config_name, _ in partners:
            entries.update(
                {
                    bouncer_alias.replace(
                        "PARTNER", f"{partner}-{sub_config_name}"
                    ): craft_partner_bouncer_product_name(
                        product,
                        bouncer_product,
                        current_version,
                        partner,
                        sub_config_name,
                    )
                    for bouncer_alias, bouncer_product in partner_bouncer_products_per_alias.items()  # NOQA: E501
                }
            )

    return entries
