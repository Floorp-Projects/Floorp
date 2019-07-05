from __future__ import absolute_import, unicode_literals

import re

from taskgraph.util.scriptworker import generate_beetmover_upstream_artifacts


_ARTIFACT_ID_PER_PLATFORM = {
    'android-aarch64-nightly': 'geckoview{update_channel}-arm64-v8a',
    'android-api-16-nightly': 'geckoview{update_channel}-armeabi-v7a',
    'android-x86-nightly': 'geckoview{update_channel}-x86',
    'android-x86_64-nightly': 'geckoview{update_channel}-x86_64',
    'android-geckoview-fat-aar-nightly': 'geckoview{update_channel}',
}

_MOZ_UPDATE_CHANNEL_PER_PROJECT = {
    'mozilla-release': '',
    'mozilla-beta': '-beta',
    'mozilla-central': '-nightly',
    'try': '-nightly-try',
    'maple': '-nightly-maple',
}


def get_geckoview_upstream_artifacts(config, job):
    upstream_artifacts = generate_beetmover_upstream_artifacts(
        config, job, platform='',
        **get_geckoview_template_vars(config, job['attributes']['build_platform'])
    )
    return [{
        key: value for key, value in upstream_artifact.items()
        if key != 'locale'
    } for upstream_artifact in upstream_artifacts]


def get_geckoview_template_vars(config, platform):
    version_groups = re.match(r'(\d+).(\d+).*', config.params['version'])
    if version_groups:
        major_version, minor_version = version_groups.groups()

    return {
        'artifact_id': get_geckoview_artifact_id(platform, config.params['project']),
        'build_date': config.params['moz_build_date'],
        'major_version': major_version,
        'minor_version': minor_version,
    }


def get_geckoview_artifact_id(platform, project):
    update_channel = _MOZ_UPDATE_CHANNEL_PER_PROJECT.get(project, '-UNKNOWN_MOZ_UPDATE_CHANNEL')
    return _ARTIFACT_ID_PER_PLATFORM[platform].format(update_channel=update_channel)
