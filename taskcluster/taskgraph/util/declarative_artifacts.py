from __future__ import absolute_import, unicode_literals

import re

from taskgraph.util.scriptworker import generate_beetmover_upstream_artifacts


_ARTIFACT_ID_PER_PLATFORM = {
    'android-aarch64-opt': 'geckoview-default-arm64-v8a',
    'android-api-16-opt': 'geckoview-default-armeabi-v7a',
    'android-x86-opt': 'geckoview-default-x86',
    'android-x86_64-opt': 'geckoview-default-x86_64',
    'android-geckoview-fat-aar-opt': 'geckoview-default',

    'android-aarch64-nightly': 'geckoview{update_channel}-arm64-v8a',
    'android-api-16-nightly': 'geckoview{update_channel}-armeabi-v7a',
    'android-x86-nightly': 'geckoview{update_channel}-x86',
    'android-x86_64-nightly': 'geckoview{update_channel}-x86_64',
    'android-geckoview-fat-aar-nightly': 'geckoview{update_channel}',
}


def get_geckoview_upstream_artifacts(config, job, platform=''):
    if not platform:
        platform = job['attributes']['build_platform']
    upstream_artifacts = generate_beetmover_upstream_artifacts(
        config, job, platform='',
        **get_geckoview_template_vars(config, platform, job['attributes'].get('update-channel'))
    )
    return [{
        key: value for key, value in upstream_artifact.items()
        if key != 'locale'
    } for upstream_artifact in upstream_artifacts]


def get_geckoview_template_vars(config, platform, update_channel):
    version_groups = re.match(r'(\d+).(\d+).*', config.params['version'])
    if version_groups:
        major_version, minor_version = version_groups.groups()

    return {
        'artifact_id': get_geckoview_artifact_id(
            platform, update_channel
        ),
        'build_date': config.params['moz_build_date'],
        'major_version': major_version,
        'minor_version': minor_version,
    }


def get_geckoview_artifact_id(platform, update_channel=None):
    update_channel = '' if update_channel in (None, 'release') else '-{}'.format(update_channel)
    return _ARTIFACT_ID_PER_PLATFORM[platform].format(update_channel=update_channel)
