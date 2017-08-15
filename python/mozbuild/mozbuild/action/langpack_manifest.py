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
from mozpack.chrome.manifest import (
    Manifest,
    ManifestLocale,
    ManifestOverride,
    ManifestResource,
    parse_manifest,
)
from mozbuild.preprocessor import Preprocessor


def write_file(path, content):
    with io.open(path, 'w', encoding='utf-8') as out:
        out.write(content + '\n')


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
# Recursively parse a chrome manifest file appending new entries
# to the result list
#
# The function can handle three entry types: 'locale', 'override' and 'resource'
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
#            'path': 'chrome/pl/locale/pl/devtools/'
#        },
#        {
#            'type': 'locale',
#            'alias': 'autoconfig',
#            'locale': 'pl',
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
            chrome_entries.append({
                'type': 'locale',
                'alias': entry.name,
                'locale': entry.id,
                'path': os.path.join(
                    os.path.relpath(
                        os.path.dirname(path),
                        base_path
                    ),
                    entry.relpath
                )
            })
        elif isinstance(entry, ManifestOverride):
            chrome_entries.append({
                'type': 'override',
                'real-path': entry.overloaded,
                'overlay-path': entry.overload
            })
        elif isinstance(entry, ManifestResource):
            chrome_entries.append({
                'type': 'resource',
                'alias': entry.name,
                'path': entry.target
            })
        else:
            raise Exception('Unknown type %s' % entry[0])


###
# Generates a new web manifest dict with values specific for a language pack.
#
# Args:
#    locstr         (str)  - A string with a comma separated list of locales
#                            for which resources are embedded in the
#                            language pack
#    appver         (str)  - A version of the application the language
#                            resources are for
#    defines        (dict) - A dictionary of defines entries
#    chrome_entries (dict) - A dictionary of chrome registry entries
#
# Returns:
#    (dict) - a web manifest
#
# Example:
#    manifest = create_webmanifest(
#      ['pl'],
#      '{ec8030f7-c20a-464f-9b0e-13a3a9e97384}',
#      '55.0a1',
#      {'MOZ_LANG_TITLE': 'Polski'},
#      chrome_entries
#    )
#    manifest == {
#        'languages': ['pl'],
#        'applications': {
#            'gecko':  {
#                'strict_min_version': '55.0a1',
#                'id': '',
#            }
#        },
#        'version': '55.0a1',
#        'name': 'Polski Language Pack',
#        ...
#    }
###
def create_webmanifest(locstr, appver, defines, chrome_entries):
    locales = map(lambda loc: loc.strip(), locstr.split(','))
    main_locale = locales[0]

    contributors = convert_contributors(defines['MOZ_LANGPACK_CONTRIBUTORS'])

    manifest = {
        'langpack-id': main_locale,
        'manifest_version': 2,
        'applications': {
            'gecko': {
                'id': "langpack-" + main_locale + "@mozilla.org",
                'strict_min_version': appver
            }
        },
        'name': defines['MOZ_LANG_TITLE'] + ' Language Pack',
        'description': 'Language pack for Firefox for ' + main_locale,
        'version': appver,
        'languages': locales,
        'author': '%s (contributors: %s)' % (defines['MOZ_LANGPACK_CREATOR'], contributors),
        'chrome_entries': [
        ]
    }

    for entry in chrome_entries:
        line = ''
        if entry['type'] == 'locale':
            line = '%s %s %s %s' % (
                entry['type'],
                entry['alias'],
                entry['locale'],
                entry['path']
            )
        elif entry['type'] == 'override':
            line = '%s %s %s' % (
                entry['type'],
                entry['real-path'],
                entry['overlay-path']
            )
        elif entry['type'] == 'resource':
            line = '%s %s %s' % (
                entry['type'],
                entry['alias'],
                entry['path']
            )
        else:
            raise Exception('Unknown type %s' % entry['type'])
        manifest['chrome_entries'].append(line)
    return json.dumps(manifest, indent=2, ensure_ascii=False, encoding='utf8')


def main(args):
    parser = argparse.ArgumentParser()
    parser.add_argument('--locales',
                        help='List of language codes provided by the langpack')
    parser.add_argument('--appver',
                        help='Version of the application the langpack is for')
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
        args.appver,
        defines,
        chrome_entries
    )
    write_file(os.path.join(args.input, 'manifest.json'), res)


if __name__ == '__main__':
    main(sys.argv[1:])
