# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Defines artifacts to sign before repackage.
"""

from __future__ import absolute_import, print_function, unicode_literals


def generate_specifications_of_artifacts_to_sign(
    build_platform, is_nightly=False, keep_locale_template=True, kind=None
):
    if kind == 'release-source-signing':
        artifacts_specifications = [{
            'artifacts': [
                'public/build/SOURCE'
            ],
            'formats': ['gpg'],
        }]
    elif 'android' in build_platform:
        artifacts_specifications = [{
            'artifacts': [
                'public/build/{locale}/target.apk',
            ],
            'formats': ['jar'],
        }]
    # XXX: Mars aren't signed here (on any platform) because internals will be
    # signed at after this stage of the release
    elif 'macosx' in build_platform:
        artifacts_specifications = [{
            'artifacts': ['public/build/{locale}/target.dmg'],
            'formats': ['macapp', 'widevine'],
        }]
    elif 'win' in build_platform:
        artifacts_specifications = [{
            'artifacts': [
                'public/build/{locale}/setup.exe',
            ],
            'formats': ['sha2signcode'],
        }, {
            'artifacts': [
                'public/build/{locale}/target.zip',
            ],
            'formats': ['sha2signcode', 'widevine'],
        }]
        if 'win32' in build_platform and is_nightly:
            artifacts_specifications[0]['artifacts'] += ['public/build/{locale}/setup-stub.exe']
    elif 'linux' in build_platform:
        artifacts_specifications = [{
            'artifacts': ['public/build/{locale}/target.tar.bz2'],
            'formats': ['gpg', 'widevine'],
        }]
    else:
        raise Exception("Platform not implemented for signing")

    if not keep_locale_template:
        artifacts_specifications = _strip_locale_template(artifacts_specifications)

    return artifacts_specifications


def _strip_locale_template(artifacts_without_locales):
    for spec in artifacts_without_locales:
        for index, artifact in enumerate(spec['artifacts']):
            stripped_artifact = artifact.format(locale='')
            stripped_artifact = stripped_artifact.replace('//', '/')
            spec['artifacts'][index] = stripped_artifact

    return artifacts_without_locales
