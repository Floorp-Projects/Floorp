# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import requests
import redo

import logging
logger = logging.getLogger(__name__)

PLATFORM_RENAMES = {
    'windows2012-32': 'win32',
    'windows2012-64': 'win64',
    'osx-cross': 'macosx64',
}

BALROG_PLATFORM_MAP = {
    "linux": [
        "Linux_x86-gcc3"
    ],
    "linux32": [
        "Linux_x86-gcc3"
    ],
    "linux64": [
        "Linux_x86_64-gcc3"
    ],
    "linux64-asan-reporter": [
        "Linux_x86_64-gcc3-asan"
    ],
    "macosx64": [
        "Darwin_x86_64-gcc3-u-i386-x86_64",
        "Darwin_x86-gcc3-u-i386-x86_64",
        "Darwin_x86-gcc3",
        "Darwin_x86_64-gcc3"
    ],
    "win32": [
        "WINNT_x86-msvc",
        "WINNT_x86-msvc-x86",
        "WINNT_x86-msvc-x64"
    ],
    "win64": [
        "WINNT_x86_64-msvc",
        "WINNT_x86_64-msvc-x64"
    ]
}

FTP_PLATFORM_MAP = {
    "Darwin_x86-gcc3": "mac",
    "Darwin_x86-gcc3-u-i386-x86_64": "mac",
    "Darwin_x86_64-gcc3": "mac",
    "Darwin_x86_64-gcc3-u-i386-x86_64": "mac",
    "Linux_x86-gcc3": "linux-i686",
    "Linux_x86_64-gcc3": "linux-x86_64",
    "Linux_x86_64-gcc3-asan": "linux-x86_64-asan-reporter",
    "WINNT_x86-msvc": "win32",
    "WINNT_x86-msvc-x64": "win32",
    "WINNT_x86-msvc-x86": "win32",
    "WINNT_x86_64-msvc": "win64",
    "WINNT_x86_64-msvc-x64": "win64",
}


def get_balrog_platform_name(platform):
    """Convert build platform names into balrog platform names"""
    if '-nightly' in platform:
        platform = platform.replace('-nightly', '')
    if '-devedition' in platform:
        platform = platform.replace('-devedition', '')
    return PLATFORM_RENAMES.get(platform, platform)


def _sanitize_platform(platform):
    platform = get_balrog_platform_name(platform)
    if platform not in BALROG_PLATFORM_MAP:
        return platform
    return BALROG_PLATFORM_MAP[platform][0]


def get_builds(release_history, platform, locale):
    """Examine cached balrog release history and return the list of
    builds we need to generate diffs from"""
    platform = _sanitize_platform(platform)
    return release_history.get(platform, {}).get(locale, {})


def get_partials_artifacts(release_history, platform, locale):
    platform = _sanitize_platform(platform)
    return release_history.get(platform, {}).get(locale, {}).keys()


def get_partials_artifact_map(release_history, platform, locale):
    platform = _sanitize_platform(platform)

    artifact_map = {}
    for k in release_history.get(platform, {}).get(locale, {}):
        details = release_history[platform][locale][k]
        attributes = ('buildid',
                      'previousBuildNumber',
                      'previousVersion')
        artifact_map[k] = {attr: details[attr] for attr in attributes
                           if attr in details}
    return artifact_map


def _retry_on_http_errors(url, verify, params, errors):
    if params:
        params_str = "&".join("=".join([k, str(v)])
                              for k, v in params.iteritems())
    else:
        params_str = ''
    logger.info("Connecting to %s?%s", url, params_str)
    for _ in redo.retrier(sleeptime=5, max_sleeptime=30, attempts=10):
        try:
            req = requests.get(url, verify=verify, params=params, timeout=4)
            req.raise_for_status()
            return req
        except requests.HTTPError as e:
            if e.response.status_code in errors:
                logger.exception("Got HTTP %s trying to reach %s",
                                 e.response.status_code, url)
            else:
                raise
    else:
        raise


def get_sorted_releases(product, branch):
    """Returns a list of release names from Balrog.
    :param product: product name, AKA appName
    :param branch: branch name, e.g. mozilla-central
    :return: a sorted list of release names, most recent first.
    """
    url = "{}/releases".format(_get_balrog_api_root(branch))
    params = {
        "product": product,
        # Adding -nightly-2 (2 stands for the beginning of build ID
        # based on date) should filter out release and latest blobs.
        # This should be changed to -nightly-3 in 3000 ;)
        "name_prefix": "{}-{}-nightly-2".format(product, branch),
        "names_only": True
    }
    req = _retry_on_http_errors(
        url=url, verify=True, params=params,
        errors=[500])
    releases = req.json()["names"]
    releases = sorted(releases, reverse=True)
    return releases


def get_release_builds(release, branch):
    url = "{}/releases/{}".format(_get_balrog_api_root(branch), release)
    req = _retry_on_http_errors(
        url=url, verify=True, params=None,
        errors=[500])
    return req.json()


def _get_balrog_api_root(branch):
    if branch in ('mozilla-central', 'mozilla-beta', 'mozilla-release') or 'mozilla-esr' in branch:
        return 'https://aus5.mozilla.org/api/v1'
    else:
        return 'https://aus5.stage.mozaws.net/api/v1'


def find_localtest(fileUrls):
    for channel in fileUrls:
        if "-localtest" in channel:
            return channel


def populate_release_history(product, branch, maxbuilds=4, maxsearch=10,
                             partial_updates=None):
    # Assuming we are using release branches when we know the list of previous
    # releases in advance
    if partial_updates:
        return _populate_release_history(
            product, branch, partial_updates=partial_updates)
    else:
        return _populate_nightly_history(
            product, branch, maxbuilds=maxbuilds, maxsearch=maxsearch)


def _populate_nightly_history(product, branch, maxbuilds=4, maxsearch=10):
    """Find relevant releases in Balrog
    Not all releases have all platforms and locales, due
    to Taskcluster migration.

        Args:
            product (str): capitalized product name, AKA appName, e.g. Firefox
            branch (str): branch name (mozilla-central)
            maxbuilds (int): Maximum number of historical releases to populate
            maxsearch(int): Traverse at most this many releases, to avoid
                working through the entire history.
        Returns:
            json object based on data from balrog api

            results = {
                'platform1': {
                    'locale1': {
                        'buildid1': mar_url,
                        'buildid2': mar_url,
                        'buildid3': mar_url,
                    },
                    'locale2': {
                        'target.partial-1.mar': {'buildid1': 'mar_url'},
                    }
                },
                'platform2': {
                }
            }
        """
    last_releases = get_sorted_releases(product, branch)

    partial_mar_tmpl = 'target.partial-{}.mar'

    builds = dict()
    for release in last_releases[:maxsearch]:
        # maxbuilds in all categories, don't make any more queries
        full = len(builds) > 0 and all(
            len(builds[platform][locale]) >= maxbuilds
            for platform in builds for locale in builds[platform])
        if full:
            break
        history = get_release_builds(release, branch)

        for platform in history['platforms']:
            if 'alias' in history['platforms'][platform]:
                continue
            if platform not in builds:
                builds[platform] = dict()
            for locale in history['platforms'][platform]['locales']:
                if locale not in builds[platform]:
                    builds[platform][locale] = dict()
                if len(builds[platform][locale]) >= maxbuilds:
                    continue
                buildid = history['platforms'][platform]['locales'][locale]['buildID']
                url = history['platforms'][platform]['locales'][locale]['completes'][0]['fileUrl']
                nextkey = len(builds[platform][locale]) + 1
                builds[platform][locale][partial_mar_tmpl.format(nextkey)] = {
                    'buildid': buildid,
                    'mar_url': url,
                }
    return builds


def _populate_release_history(product, branch, partial_updates):
    builds = dict()
    for version, release in partial_updates.iteritems():
        prev_release_blob = '{product}-{version}-build{build_number}'.format(
            product=product, version=version, build_number=release['buildNumber']
        )
        partial_mar_key = 'target-{version}.partial.mar'.format(version=version)
        history = get_release_builds(prev_release_blob, branch)
        # use one of the localtest channels to avoid relying on bouncer
        localtest = find_localtest(history['fileUrls'])
        url_pattern = history['fileUrls'][localtest]['completes']['*']

        for platform in history['platforms']:
            if 'alias' in history['platforms'][platform]:
                continue
            if platform not in builds:
                builds[platform] = dict()
            for locale in history['platforms'][platform]['locales']:
                if locale not in builds[platform]:
                    builds[platform][locale] = dict()
                buildid = history['platforms'][platform]['locales'][locale]['buildID']
                url = url_pattern.replace(
                    '%OS_FTP%', FTP_PLATFORM_MAP[platform]).replace(
                    '%LOCALE%', locale
                )
                builds[platform][locale][partial_mar_key] = {
                    'buildid': buildid,
                    'mar_url': url,
                    'previousVersion': version,
                    'previousBuildNumber': release['buildNumber'],
                    'product': product,
                }
    return builds
