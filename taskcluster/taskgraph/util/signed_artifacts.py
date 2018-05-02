# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Defines artifacts to sign before repackage.
"""

from __future__ import absolute_import, print_function, unicode_literals
from taskgraph.util.taskcluster import get_artifact_path


def is_partner_kind(kind):
    if kind and kind.startswith(('release-partner', 'release-eme-free')):
        return True


def generate_specifications_of_artifacts_to_sign(
    task, keep_locale_template=True, kind=None, project=None
):
    build_platform = task.attributes.get('build_platform')
    is_nightly = task.attributes.get('nightly')
    if kind == 'release-source-signing':
        artifacts_specifications = [{
            'artifacts': [
                get_artifact_path(task, 'source.tar.xz')
            ],
            'formats': ['gpg'],
        }]
    elif 'android' in build_platform:
        artifacts_specifications = [{
            'artifacts': [
                get_artifact_path(task, '{locale}/target.apk'),
            ],
            'formats': ['jar'],
        }]
    # XXX: Mars aren't signed here (on any platform) because internals will be
    # signed at after this stage of the release
    elif 'macosx' in build_platform:
        if is_partner_kind(kind):
            extension = 'tar.gz'
        else:
            extension = 'dmg'
        artifacts_specifications = [{
            'artifacts': [get_artifact_path(task, '{{locale}}/target.{}'.format(extension))],
            'formats': ['macapp', 'widevine'],
        }]
    elif 'win' in build_platform:
        artifacts_specifications = [{
            'artifacts': [
                get_artifact_path(task, '{locale}/setup.exe'),
            ],
            'formats': ['sha2signcode'],
        }, {
            'artifacts': [
                get_artifact_path(task, '{locale}/target.zip'),
            ],
            'formats': ['sha2signcode', 'widevine'],
        }]
        no_stub = ("mozilla-esr60", "jamun")
        if 'win32' in build_platform and is_nightly and project not in no_stub:
            # TODO: fix the project hint to be a better design
            # We don't build stub installer on esr, so we don't want to sign it
            artifacts_specifications[0]['artifacts'] += [
                get_artifact_path(task, '{locale}/setup-stub.exe')
            ]
    elif 'linux' in build_platform:
        artifacts_specifications = [{
            'artifacts': [get_artifact_path(task, '{locale}/target.tar.bz2')],
            'formats': ['gpg', 'widevine'],
        }]
    else:
        raise Exception("Platform not implemented for signing")

    if not keep_locale_template:
        artifacts_specifications = _strip_locale_template(artifacts_specifications)

    if is_partner_kind(kind):
        artifacts_specifications = _strip_widevine_for_partners(artifacts_specifications)

    return artifacts_specifications


def _strip_locale_template(artifacts_without_locales):
    for spec in artifacts_without_locales:
        for index, artifact in enumerate(spec['artifacts']):
            stripped_artifact = artifact.format(locale='')
            stripped_artifact = stripped_artifact.replace('//', '/')
            spec['artifacts'][index] = stripped_artifact

    return artifacts_without_locales


def _strip_widevine_for_partners(artifacts_specifications):
    """ Partner repacks should not resign that's previously signed for fear of breaking partial
    updates
    """
    for spec in artifacts_specifications:
        if 'widevine' in spec['formats']:
            spec['formats'].remove('widevine')

    return artifacts_specifications
