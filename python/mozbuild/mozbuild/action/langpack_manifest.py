# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

###
# This script generates a web manifest JSON file based on the xpi-stage
# directory structure. It extracts the data from defines.inc files from
# the locale directory, chrome registry entries and other information
# necessary to produce the complete manifest file for a language pack.
###
from __future__ import absolute_import

import argparse
import sys
import os
import json
import io
import datetime
import requests
import mozversioncontrol
import mozpack.path as mozpath
from mozpack.chrome.manifest import (
    Manifest,
    ManifestLocale,
    parse_manifest,
)
from mozbuild.preprocessor import Preprocessor
import buildconfig


def write_file(path, content):
    with io.open(path, 'w', encoding='utf-8') as out:
        out.write(content + '\n')


pushlog_api_url = "{0}/json-rev/{1}"


###
# Retrievers a UTC datetime of the push for the current commit
# from a mercurial clone directory.
#
# Args:
#    path (str) - path to a directory
#
# Returns:
#    (datetime) - a datetime object
#
# Example:
#    dt = get_dt_from_hg("/var/vcs/l10n-central/pl")
#    dt == datetime(2017, 10, 11, 23, 31, 54, 0)
###
def get_dt_from_hg(path):
    with mozversioncontrol.get_repository_object(path=path) as repo:
        phase = repo._run_in_client(["log", "-r", ".", "-T" "{phase}"])
        if phase.strip() != "public":
            return datetime.datetime.utcnow()
        repo_url = repo._run_in_client(["paths", "default"])
        repo_url = repo_url.strip().replace("ssh://", "https://")
        repo_url = repo_url.replace("hg://", "https://")
        cs = repo._run_in_client(["log", "-r", ".", "-T" "{node}"])

    url = pushlog_api_url.format(repo_url, cs)
    session = requests.Session()
    try:
        response = session.get(url)
    except Exception as e:
        msg = "Failed to retrieve push timestamp using {}\nError: {}".format(url, e)
        raise Exception(msg)

    data = response.json()

    date = data['pushdate'][0]

    return datetime.datetime.utcfromtimestamp(date)


###
# Generates timestamp for a locale based on its path.
# If possible, will use the commit timestamp from HG repository,
# and if that fails, will generate the timestamp for `now`.
#
# The timestamp format is "{year}{month}{day}{hour}{minute}{second}" and
# the datetime stored in it is using UTC timezone.
#
# Args:
#    path (str) - path to the locale directory
#
# Returns:
#    (str) - a timestamp string
#
# Example:
#    ts = get_timestamp_for_locale("/var/vcs/l10n-central/pl")
#    ts == "20170914215617"
###
def get_timestamp_for_locale(path):
    dt = None
    if os.path.isdir(os.path.join(path, '.hg')):
        dt = get_dt_from_hg(path)

    if dt is None:
        dt = datetime.datetime.utcnow()

    dt = dt.replace(microsecond=0)
    return dt.strftime("%Y%m%d%H%M%S")


###
# Parses multiple defines files into a single key-value pair object.
#
# Args:
#    paths (str) - a comma separated list of paths to defines files
#
# Returns:
#    (dict) - a key-value dict with defines
#
# Example:
#    res = parse_defines('./toolkit/defines.inc,./browser/defines.inc')
#    res == {
#        'MOZ_LANG_TITLE': 'Polski',
#        'MOZ_LANGPACK_CREATOR': 'Aviary.pl',
#        'MOZ_LANGPACK_CONTRIBUTORS': 'Marek Stepien, Marek Wawoczny'
#    }
###
def parse_defines(paths):
    pp = Preprocessor()
    for path in paths:
        pp.do_include(path)

    return pp.context


###
# Converts the list of contributors from the old RDF based list
# of entries, into a comma separated list.
#
# Args:
#    str (str) - a string with an RDF list of contributors entries
#
# Returns:
#    (str) - a comma separated list of contributors
#
# Example:
#    s = convert_contributors('
#        <em:contributor>Marek Wawoczny</em:contributor>
#        <em:contributor>Marek Stepien</em:contributor>
#    ')
#    s == 'Marek Wawoczny, Marek Stepien'
###
def convert_contributors(str):
    str = str.replace('<em:contributor>', '')
    tokens = str.split('</em:contributor>')
    tokens = map(lambda t: t.strip(), tokens)
    tokens = filter(lambda t: t != '', tokens)
    return ', '.join(tokens)


###
# Build the manifest author string based on the author string
# and optionally adding the list of contributors, if provided.
#
# Args:
#    author (str)       - a string with the name of the author
#    contributors (str) - RDF based list of contributors from a chrome manifest
#
# Returns:
#    (str) - a string to be placed in the author field of the manifest.json
#
# Example:
#    s = build_author_string(
#    'Aviary.pl',
#    '
#        <em:contributor>Marek Wawoczny</em:contributor>
#        <em:contributor>Marek Stepien</em:contributor>
#    ')
#    s == 'Aviary.pl (contributors: Marek Wawoczny, Marek Stepien)'
###
def build_author_string(author, contributors):
    contrib = convert_contributors(contributors)
    if len(contrib) == 0:
        return author
    return '{0} (contributors: {1})'.format(author, contrib)


##
# Converts the list of chrome manifest entry flags to the list of platforms
# for the langpack manifest.
#
# The list of result platforms is taken from AppConstants.platform.
#
# Args:
#    flags (FlagList) - a list of Chrome Manifest entry flags
#
# Returns:
#    (list) - a list of platform the entry applies to
#
# Example:
#    str(flags) == "os==MacOS os==Windows"
#    platforms = convert_entry_flags_to_platform_codes(flags)
#    platforms == ['mac', 'win']
#
# The method supports only `os` flag name and equality operator.
# It will throw if tried with other flags or operators.
###
def convert_entry_flags_to_platform_codes(flags):
    if not flags:
        return None

    ret = []
    for key in flags:
        if key != 'os':
            raise Exception('Unknown flag name')

        for value in flags[key].values:
            if value[0] != '==':
                raise Exception('Inequality flag cannot be converted')

            if value[1] == 'Android':
                ret.append('android')
            elif value[1] == 'LikeUnix':
                ret.append('linux')
            elif value[1] == 'Darwin':
                ret.append('macosx')
            elif value[1] == 'WINNT':
                ret.append('win')
            else:
                raise Exception('Unknown flag value {0}'.format(value[1]))

    return ret


###
# Recursively parse a chrome manifest file appending new entries
# to the result list
#
# The function can handle two entry types: 'locale' and 'manifest'
#
# Args:
#    path           (str)  - a path to a chrome manifest
#    base_path      (str)  - a path to the base directory all chrome registry
#                            entries will be relative to
#    chrome_entries (list) - a list to which entries will be appended to
#
# Example:
#
#    chrome_entries = {}
#    parse_manifest('./chrome.manifest', './', chrome_entries)
#
#    chrome_entries == [
#        {
#            'type': 'locale',
#            'alias': 'devtools',
#            'locale': 'pl',
#            'platforms': null,
#            'path': 'chrome/pl/locale/pl/devtools/'
#        },
#        {
#            'type': 'locale',
#            'alias': 'autoconfig',
#            'locale': 'pl',
#            'platforms': ['win', 'mac'],
#            'path': 'chrome/pl/locale/pl/autoconfig/'
#        },
#    ]
###
def parse_chrome_manifest(path, base_path, chrome_entries):
    for entry in parse_manifest(None, path):
        if isinstance(entry, Manifest):
            parse_chrome_manifest(
                os.path.join(os.path.dirname(path), entry.relpath),
                base_path,
                chrome_entries
            )
        elif isinstance(entry, ManifestLocale):
            entry_path = os.path.join(
                os.path.relpath(
                    os.path.dirname(path),
                    base_path
                ),
                entry.relpath
            )
            chrome_entries.append({
                'type': 'locale',
                'alias': entry.name,
                'locale': entry.id,
                'platforms': convert_entry_flags_to_platform_codes(entry.flags),
                'path': mozpath.normsep(entry_path)
            })
        else:
            raise Exception('Unknown type {0}'.format(entry.name))


##
# Gets the version to use in the langpack.
#
# This uses the env variable MOZ_BUILD_DATE if it exists to expand the version to be unique
# in automation.
#
# Args:
#    min_version - Application version
#
# Returns:
#    str - Version to use, may include buildid
#
###
def get_version_maybe_buildid(min_version):
    version = str(min_version)
    buildid = os.environ.get('MOZ_BUILD_DATE')
    if buildid and len(buildid) != 14:
        print >>sys.stderr, 'Ignoring invalid MOZ_BUILD_DATE: %s' % buildid
        buildid = None
    if buildid:
        version = version + "buildid" + buildid
    return version


###
# Generates a new web manifest dict with values specific for a language pack.
#
# Args:
#    locstr         (str)  - A string with a comma separated list of locales
#                            for which resources are embedded in the
#                            language pack
#    min_app_ver    (str)  - A minimum version of the application the language
#                            resources are for
#    max_app_ver    (str)  - A maximum version of the application the language
#                            resources are for
#    app_name       (str)  - The name of the application the language
#                            resources are for
#    defines        (dict) - A dictionary of defines entries
#    chrome_entries (dict) - A dictionary of chrome registry entries
#
# Returns:
#    (dict) - a web manifest
#
# Example:
#    manifest = create_webmanifest(
#      'pl',
#      '57.0',
#      '57.0.*',
#      'Firefox',
#      '/var/vcs/l10n-central',
#      {'MOZ_LANG_TITLE': 'Polski'},
#      chrome_entries
#    )
#    manifest == {
#        'languages': {
#            'pl': {
#                'version': '201709121481',
#                'chrome_resources': {
#                    'alert': 'chrome/pl/locale/pl/alert/',
#                    'branding': 'browser/chrome/pl/locale/global/',
#                    'global-platform': {
#                      'macosx': 'chrome/pl/locale/pl/global-platform/mac/',
#                      'win': 'chrome/pl/locale/pl/global-platform/win/',
#                      'linux': 'chrome/pl/locale/pl/global-platform/unix/',
#                      'android': 'chrome/pl/locale/pl/global-platform/unix/',
#                    },
#                    'forms': 'browser/chrome/pl/locale/forms/',
#                    ...
#                }
#            }
#        },
#        'sources': {
#            'browser': {
#                'base_path': 'browser/'
#            }
#        },
#        'applications': {
#            'gecko':  {
#                'strict_min_version': '57.0',
#                'strict_max_version': '57.0.*',
#                'id': 'langpack-pl@mozilla.org',
#            }
#        },
#        'version': '57.0',
#        'name': 'Polski Language Pack',
#        ...
#    }
###
def create_webmanifest(locstr, min_app_ver, max_app_ver, app_name,
                       l10n_basedir, langpack_eid, defines, chrome_entries):
    locales = map(lambda loc: loc.strip(), locstr.split(','))
    main_locale = locales[0]

    author = build_author_string(
        defines['MOZ_LANGPACK_CREATOR'],
        defines['MOZ_LANGPACK_CONTRIBUTORS'] if 'MOZ_LANGPACK_CONTRIBUTORS' in defines else ""
    )

    manifest = {
        'langpack_id': main_locale,
        'manifest_version': 2,
        'applications': {
            'gecko': {
                'id': langpack_eid,
                'strict_min_version': min_app_ver,
                'strict_max_version': max_app_ver,
            }
        },
        'name': '{0} Language Pack'.format(defines['MOZ_LANG_TITLE']),
        'description': 'Language pack for {0} for {1}'.format(app_name, main_locale),
        'version': get_version_maybe_buildid(min_app_ver),
        'languages': {},
        'sources': {
            'browser': {
                'base_path': 'browser/'
            }
        },
        'author': author
    }

    cr = {}
    for entry in chrome_entries:
        if entry['type'] == 'locale':
            platforms = entry['platforms']
            if platforms:
                if entry['alias'] not in cr:
                    cr[entry['alias']] = {}
                for platform in platforms:
                    cr[entry['alias']][platform] = entry['path']
            else:
                assert entry['alias'] not in cr
                cr[entry['alias']] = entry['path']
        else:
            raise Exception('Unknown type {0}'.format(entry['type']))

    for loc in locales:
        manifest['languages'][loc] = {
            'version': get_timestamp_for_locale(os.path.join(l10n_basedir, loc)),
            'chrome_resources': cr
        }

    return json.dumps(manifest, indent=2, ensure_ascii=False, encoding='utf8')


def main(args):
    parser = argparse.ArgumentParser()
    parser.add_argument('--locales',
                        help='List of language codes provided by the langpack')
    parser.add_argument('--min-app-ver',
                        help='Min version of the application the langpack is for')
    parser.add_argument('--max-app-ver',
                        help='Max version of the application the langpack is for')
    parser.add_argument('--app-name',
                        help='Name of the application the langpack is for')
    parser.add_argument('--l10n-basedir',
                        help='Base directory for locales used in the language pack')
    parser.add_argument('--langpack-eid',
                        help='Language pack id to use for this locale')
    parser.add_argument('--defines', default=[], nargs='+',
                        help='List of defines files to load data from')
    parser.add_argument('--input',
                        help='Langpack directory.')

    args = parser.parse_args(args)

    chrome_entries = []
    parse_chrome_manifest(
        os.path.join(args.input, 'chrome.manifest'), args.input, chrome_entries)

    defines = parse_defines(args.defines)

    res = create_webmanifest(
        args.locales,
        args.min_app_ver,
        args.max_app_ver,
        args.app_name,
        args.l10n_basedir,
        args.langpack_eid,
        defines,
        chrome_entries
    )
    write_file(os.path.join(args.input, 'manifest.json'), res)


if __name__ == '__main__':
    main(sys.argv[1:])
