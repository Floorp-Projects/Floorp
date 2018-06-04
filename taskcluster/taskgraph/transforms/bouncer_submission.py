# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Add from parameters.yml into bouncer submission tasks.
"""

from __future__ import absolute_import, print_function, unicode_literals

import logging

from taskgraph.transforms.base import TransformSequence
from taskgraph.transforms.l10n import parse_locales_file
from taskgraph.util.schema import resolve_keyed_by
from taskgraph.util.scriptworker import get_release_config

logger = logging.getLogger(__name__)


FTP_PLATFORMS_PER_BOUNCER_PLATFORM = {
    'android': 'android-api-16',
    'android-x86': 'android-x86',
    'linux': 'linux-i686',
    'linux64': 'linux-x86_64',
    'osx': 'mac',
    'win': 'win32',
    'win64': 'win64',
}

# :lang is interpolated by bouncer at runtime
CANDIDATES_PATH_TEMPLATE = '/{ftp_product}/candidates/{version}-candidates/build{build_number}/\
{update_folder}{ftp_platform}/:lang/{file}'
RELEASES_PATH_TEMPLATE = '/{ftp_product}/releases/{version}/\
{update_folder}{ftp_platform}/:lang/{file}'


CONFIG_PER_BOUNCER_PRODUCT = {
    'apk': {
        'path_template': RELEASES_PATH_TEMPLATE,
        'file_names': {
            'android': '{product}-{version}.:lang.android-arm.apk',
            'android-x86': '{product}-{version}.:lang.android-i386.apk',
        },
    },
    'complete-mar': {
        'path_template': RELEASES_PATH_TEMPLATE,
        'file_names': {
            'default': '{product}-{version}.complete.mar',
        },
    },
    'complete-mar-candidates': {
        'path_template': CANDIDATES_PATH_TEMPLATE,
        'file_names': {
            'default': '{product}-{version}.complete.mar',
        },
    },
    'installer': {
        'path_template': RELEASES_PATH_TEMPLATE,
        'file_names': {
            'linux': '{product}-{version}.tar.bz2',
            'linux64': '{product}-{version}.tar.bz2',
            'osx': '{pretty_product}%20{version}.dmg',
            'win': '{pretty_product}%20Setup%20{version}.exe',
            'win64': '{pretty_product}%20Setup%20{version}.exe',
        },
    },
    'partial-mar': {
        'path_template': RELEASES_PATH_TEMPLATE,
        'file_names': {
            'default': '{product}-{previous_version}-{version}.partial.mar',
        },
    },
    'partial-mar-candidates': {
        'path_template': CANDIDATES_PATH_TEMPLATE,
        'file_names': {
            'default': '{product}-{previous_version}-{version}.partial.mar',
        },
    },
    'stub-installer': {
        'path_template': RELEASES_PATH_TEMPLATE,
        'file_names': {
            'win': '{pretty_product}%20Installer.exe',
            'win64': '{pretty_product}%20Installer.exe',
        },
    },
}
CONFIG_PER_BOUNCER_PRODUCT['installer-ssl'] = CONFIG_PER_BOUNCER_PRODUCT['installer']

transforms = TransformSequence()


@transforms.add
def make_task_worker(config, jobs):
    for job in jobs:
        resolve_keyed_by(
            job, 'worker-type', item_name=job['name'], project=config.params['project']
        )
        resolve_keyed_by(
            job, 'scopes', item_name=job['name'], project=config.params['project']
        )
        resolve_keyed_by(
            job, 'bouncer-products', item_name=job['name'], project=config.params['project']
        )

        # No need to filter out ja-JP-mac, we need to upload both; but we do
        # need to filter out the platforms they come with
        all_locales = sorted([
            locale
            for locale in parse_locales_file(job['locales-file']).keys()
            if locale not in ('linux', 'win32', 'osx')
        ])

        job['worker']['locales'] = all_locales
        job['worker']['entries'] = craft_bouncer_entries(config, job)

        del job['locales-file']
        del job['bouncer-platforms']
        del job['bouncer-products']

        if job['worker']['entries']:
            # XXX Because rc jobs are defined within the same kind, we need to delete the
            # firefox-rc job at this stage, if we're not building an RC. Otherwise, even if
            # target_tasks.py filters out the rc job, it gets resurected by any kind that depends
            # on the release-bouncer-sub one (release-notify-promote as of time of this writing).
            if config.params['release_type'] == 'rc' or job['name'] != 'firefox-rc':
                yield job
        else:
            logger.warn('No bouncer entries defined in bouncer submission task for "{}". \
Job deleted.'.format(job['name']))


def craft_bouncer_entries(config, job):
    release_config = get_release_config(config)

    product = job['shipping-product']
    bouncer_platforms = job['bouncer-platforms']

    current_version = release_config['version']
    current_build_number = release_config['build_number']

    bouncer_products = job['bouncer-products']
    previous_versions_string = release_config.get('partial_versions', None)
    if previous_versions_string:
        previous_versions = previous_versions_string.split(', ')
    else:
        logger.warn('No partials defined! Bouncer submission task won\'t send any \
partial-related entry for "{}"'.format(job['name']))
        bouncer_products = [
            bouncer_product
            for bouncer_product in bouncer_products
            if 'partial' not in bouncer_product
        ]
        previous_versions = [None]

    project = config.params['project']

    return {
        craft_bouncer_product_name(
            product, bouncer_product, current_version, current_build_number, previous_version
        ): {
            'options': {
                'add_locales': craft_add_locales(product),
                'check_uptake': craft_check_uptake(bouncer_product),
                'ssl_only': craft_ssl_only(bouncer_product, project),
            },
            'paths_per_bouncer_platform': craft_paths_per_bouncer_platform(
                product, bouncer_product, bouncer_platforms, current_version,
                current_build_number, previous_version
            ),
        }
        for bouncer_product in bouncer_products
        for previous_version in previous_versions
    }


def craft_paths_per_bouncer_platform(product, bouncer_product, bouncer_platforms, current_version,
                                     current_build_number, previous_version=None):
    paths_per_bouncer_platform = {}
    for bouncer_platform in bouncer_platforms:
        file_names_per_platform = CONFIG_PER_BOUNCER_PRODUCT[bouncer_product]['file_names']
        file_name_template = file_names_per_platform.get(
            bouncer_platform, file_names_per_platform.get('default', None)
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
            previous_version=strip_build_data(previous_version),
        )

        path_template = CONFIG_PER_BOUNCER_PRODUCT[bouncer_product]['path_template']
        file_relative_location = path_template.format(
            ftp_product=_craft_ftp_product(product),
            version=current_version,
            build_number=current_build_number,
            update_folder='update/' if '-mar' in bouncer_product else '',
            ftp_platform=_craft_ftp_platform(bouncer_platform, file_name),
            file=file_name,
        )

        paths_per_bouncer_platform[bouncer_platform] = file_relative_location

    return paths_per_bouncer_platform


def _craft_ftp_product(product):
    return 'mobile' if product == 'fennec' else product.lower()


def _craft_ftp_platform(bouncer_platform, file_name):
    ftp_platform = FTP_PLATFORMS_PER_BOUNCER_PLATFORM[bouncer_platform]
    # We currently have a sole win32 stub installer that is to be used
    # in both windows platforms to toggle between full installers
    if 'Installer.exe' in file_name and ftp_platform == 'win64':
        return 'win32'

    return ftp_platform


def _craft_filename_product(product):
    return 'firefox' if product == 'devedition' else product


def craft_bouncer_product_name(product, bouncer_product, current_version,
                               current_build_number=None, previous_version=None):
    if '-ssl' in bouncer_product:
        postfix = '-SSL'
    elif 'stub-' in bouncer_product:
        postfix = '-stub'
    elif 'complete-' in bouncer_product:
        postfix = _set_current_build_number_if_needed(bouncer_product, current_build_number)
        postfix = '{}-Complete'.format(postfix)
    elif 'partial-' in bouncer_product:
        if not previous_version:
            raise Exception('Partial is being processed, but no previous version defined.')

        if '-candidates' in bouncer_product:
            postfix = _set_current_build_number_if_needed(bouncer_product, current_build_number)
            postfix = '{postfix}-Partial-{previous_version_with_build_number}'.format(
                postfix=postfix,
                build_number=current_build_number,
                previous_version_with_build_number=previous_version,
            )
        else:
            postfix = '-Partial-{previous_version}'.format(
                previous_version=strip_build_data(previous_version)
            )

    elif 'sha1-' in bouncer_product:
        postfix = '-sha1'
    else:
        postfix = ''

    return '{product}-{version}{postfix}'.format(
        product=product.capitalize(), version=current_version, postfix=postfix
    )


def _set_current_build_number_if_needed(bouncer_product, current_build_number):
    if '-candidates' in bouncer_product:
        if not current_build_number:
            raise Exception('Partial in candidates directory is being processed, \
but no current build number defined.')
        return 'build{}'.format(current_build_number)

    return ''


def craft_check_uptake(bouncer_product):
    return bouncer_product != 'complete-mar-candidates'


def craft_ssl_only(bouncer_product, project):
    # XXX ESR is the only channel where we force serve the installer over SSL
    if '-esr' in project and bouncer_product == 'installer':
        return True

    return bouncer_product not in (
        'complete-mar',
        'complete-mar-candidates',
        'installer',
        'partial-mar',
        'partial-mar-candidates',
    )


def craft_add_locales(product):
    # Do not add locales on Fennec in order to let "multi" work
    return product != 'fennec'


def strip_build_data(version):
    return version.split('build')[0] if version and 'build' in version else version
