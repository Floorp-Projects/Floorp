# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Add from parameters.yml into bouncer submission tasks.
"""


import copy
import logging

import attr
from taskgraph.util.schema import resolve_keyed_by

from gecko_taskgraph.transforms.base import TransformSequence
from gecko_taskgraph.transforms.l10n import parse_locales_file
from gecko_taskgraph.util.attributes import release_level
from gecko_taskgraph.util.scriptworker import get_release_config

logger = logging.getLogger(__name__)


FTP_PLATFORMS_PER_BOUNCER_PLATFORM = {
    "linux": "linux-i686",
    "linux64": "linux-x86_64",
    "osx": "mac",
    "win": "win32",
    "win64": "win64",
    "win64-aarch64": "win64-aarch64",
}

# :lang is interpolated by bouncer at runtime
CANDIDATES_PATH_TEMPLATE = "/{ftp_product}/candidates/{version}-candidates/build{build_number}/\
{update_folder}{ftp_platform}/:lang/{file}"
RELEASES_PATH_TEMPLATE = "/{ftp_product}/releases/{version}/\
{update_folder}{ftp_platform}/:lang/{file}"


CONFIG_PER_BOUNCER_PRODUCT = {
    "complete-mar": {
        "name_postfix": "-Complete",
        "path_template": RELEASES_PATH_TEMPLATE,
        "file_names": {
            "default": "{product}-{version}.complete.mar",
        },
    },
    "complete-mar-candidates": {
        "name_postfix": "build{build_number}-Complete",
        "path_template": CANDIDATES_PATH_TEMPLATE,
        "file_names": {
            "default": "{product}-{version}.complete.mar",
        },
    },
    "installer": {
        "path_template": RELEASES_PATH_TEMPLATE,
        "file_names": {
            "linux": "{product}-{version}.tar.bz2",
            "linux64": "{product}-{version}.tar.bz2",
            "osx": "{pretty_product}%20{version}.dmg",
            "win": "{pretty_product}%20Setup%20{version}.exe",
            "win64": "{pretty_product}%20Setup%20{version}.exe",
            "win64-aarch64": "{pretty_product}%20Setup%20{version}.exe",
        },
    },
    "partial-mar": {
        "name_postfix": "-Partial-{previous_version}",
        "path_template": RELEASES_PATH_TEMPLATE,
        "file_names": {
            "default": "{product}-{previous_version}-{version}.partial.mar",
        },
    },
    "partial-mar-candidates": {
        "name_postfix": "build{build_number}-Partial-{previous_version}build{previous_build}",
        "path_template": CANDIDATES_PATH_TEMPLATE,
        "file_names": {
            "default": "{product}-{previous_version}-{version}.partial.mar",
        },
    },
    "stub-installer": {
        "name_postfix": "-stub",
        # We currently have a sole win32 stub installer that is to be used
        # in all windows platforms to toggle between full installers
        "path_template": RELEASES_PATH_TEMPLATE.replace("{ftp_platform}", "win32"),
        "file_names": {
            "win": "{pretty_product}%20Installer.exe",
            "win64": "{pretty_product}%20Installer.exe",
            "win64-aarch64": "{pretty_product}%20Installer.exe",
        },
    },
    "msi": {
        "name_postfix": "-msi-SSL",
        "path_template": RELEASES_PATH_TEMPLATE,
        "file_names": {
            "win": "{pretty_product}%20Setup%20{version}.msi",
            "win64": "{pretty_product}%20Setup%20{version}.msi",
        },
    },
    "msix": {
        "name_postfix": "-msix-SSL",
        "path_template": RELEASES_PATH_TEMPLATE.replace(":lang", "multi"),
        "file_names": {
            "win": "{pretty_product}%20Setup%20{version}.msix",
            "win64": "{pretty_product}%20Setup%20{version}.msix",
        },
    },
    "pkg": {
        "name_postfix": "-pkg-SSL",
        "path_template": RELEASES_PATH_TEMPLATE,
        "file_names": {
            "osx": "{pretty_product}%20{version}.pkg",
        },
    },
}
CONFIG_PER_BOUNCER_PRODUCT["installer-ssl"] = copy.deepcopy(
    CONFIG_PER_BOUNCER_PRODUCT["installer"]
)
CONFIG_PER_BOUNCER_PRODUCT["installer-ssl"]["name_postfix"] = "-SSL"

transforms = TransformSequence()


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

        # No need to filter out ja-JP-mac, we need to upload both; but we do
        # need to filter out the platforms they come with
        all_locales = sorted(
            locale
            for locale in parse_locales_file(job["locales-file"]).keys()
            if locale not in ("linux", "win32", "osx")
        )

        job["worker"]["locales"] = all_locales
        job["worker"]["entries"] = craft_bouncer_entries(config, job)

        del job["locales-file"]
        del job["bouncer-platforms"]
        del job["bouncer-products"]

        if job["worker"]["entries"]:
            yield job
        else:
            logger.warn(
                'No bouncer entries defined in bouncer submission task for "{}". \
Job deleted.'.format(
                    job["name"]
                )
            )


def craft_bouncer_entries(config, job):
    release_config = get_release_config(config)

    product = job["shipping-product"]
    bouncer_platforms = job["bouncer-platforms"]

    current_version = release_config["version"]
    current_build_number = release_config["build_number"]

    bouncer_products = job["bouncer-products"]
    previous_versions_string = release_config.get("partial_versions", None)
    if previous_versions_string:
        previous_versions = previous_versions_string.split(", ")
    else:
        logger.warn(
            'No partials defined! Bouncer submission task won\'t send any \
partial-related entry for "{}"'.format(
                job["name"]
            )
        )
        bouncer_products = [
            bouncer_product
            for bouncer_product in bouncer_products
            if "partial" not in bouncer_product
        ]
        previous_versions = [None]

    project = config.params["project"]

    return {
        craft_bouncer_product_name(
            product,
            bouncer_product,
            current_version,
            current_build_number,
            previous_version,
        ): {
            "options": {
                "add_locales": False if "msix" in bouncer_product else True,
                "ssl_only": craft_ssl_only(bouncer_product, project),
            },
            "paths_per_bouncer_platform": craft_paths_per_bouncer_platform(
                product,
                bouncer_product,
                bouncer_platforms,
                current_version,
                current_build_number,
                previous_version,
            ),
        }
        for bouncer_product in bouncer_products
        for previous_version in previous_versions
    }


def craft_paths_per_bouncer_platform(
    product,
    bouncer_product,
    bouncer_platforms,
    current_version,
    current_build_number,
    previous_version=None,
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
            previous_version=split_build_data(previous_version)[0],
        )

        path_template = CONFIG_PER_BOUNCER_PRODUCT[bouncer_product]["path_template"]
        file_relative_location = path_template.format(
            ftp_product=_craft_ftp_product(product),
            version=current_version,
            build_number=current_build_number,
            update_folder="update/" if "-mar" in bouncer_product else "",
            ftp_platform=FTP_PLATFORMS_PER_BOUNCER_PLATFORM[bouncer_platform],
            file=file_name,
        )

        paths_per_bouncer_platform[bouncer_platform] = file_relative_location

    return paths_per_bouncer_platform


def _craft_ftp_product(product):
    return product.lower()


def _craft_filename_product(product):
    return "firefox" if product == "devedition" else product


@attr.s
class InvalidSubstitution:
    error = attr.ib(type=str)

    def __str__(self):
        raise Exception("Partial is being processed, but no previous version defined.")


def craft_bouncer_product_name(
    product,
    bouncer_product,
    current_version,
    current_build_number=None,
    previous_version=None,
):
    if previous_version is None:
        previous_version = previous_build = InvalidSubstitution(
            "Partial is being processed, but no previous version defined."
        )
    else:
        previous_version, previous_build = split_build_data(previous_version)
    postfix = (
        CONFIG_PER_BOUNCER_PRODUCT[bouncer_product]
        .get("name_postfix", "")
        .format(
            build_number=current_build_number,
            previous_version=previous_version,
            previous_build=previous_build,
        )
    )

    return "{product}-{version}{postfix}".format(
        product=product.capitalize(), version=current_version, postfix=postfix
    )


def craft_ssl_only(bouncer_product, project):
    # XXX ESR is the only channel where we force serve the installer over SSL
    if "-esr" in project and bouncer_product == "installer":
        return True

    return bouncer_product not in (
        "complete-mar",
        "complete-mar-candidates",
        "installer",
        "partial-mar",
        "partial-mar-candidates",
    )


def split_build_data(version):
    if version and "build" in version:
        return version.split("build")
    else:
        return version, InvalidSubstitution("k")
