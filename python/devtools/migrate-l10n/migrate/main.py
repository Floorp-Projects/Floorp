import argparse
import glob
import HTMLParser
import logging
import os
import re
import sys
import urllib2


# Import compare-locales parser from parent folder.
script_path = os.path.dirname(os.path.realpath(__file__))
compare_locales_path = os.path.join(script_path, '../../../../third_party/python/compare-locales')
sys.path.insert(0, compare_locales_path)
from compare_locales import parser


# Configure logging format and level
logging.basicConfig(format='  [%(levelname)s] %(message)s', level=logging.INFO)


# License header to use when creating new properties files.
DEFAULT_HEADER = ('# This Source Code Form is subject to the terms of the '
                  'Mozilla Public\n# License, v. 2.0. If a copy of the MPL '
                  'was not distributed with this\n# file, You can obtain '
                  'one at http://mozilla.org/MPL/2.0/.\n')


# Base url to retrieve properties files on central, that will be parsed for
# localization notes.
CENTRAL_BASE_URL = ('https://hg.mozilla.org/'
                    'mozilla-central/raw-file/tip/'
                    'devtools/client/locales/en-US/')


# HTML parser to translate HTML entities in dtd files.
HTML_PARSER = HTMLParser.HTMLParser()

# Cache to store properties files retrieved over the network.
central_prop_cache = {}

# Cache the parsed entities from the existing DTD files.
dtd_entities_cache = {}


# Retrieve the content of the current version of a properties file for the
# provided filename, from devtools/client on mozilla central. Will return an
# empty array if the file can't be retrieved or read.
def get_central_prop_content(prop_filename):
    if prop_filename in central_prop_cache:
        return central_prop_cache[prop_filename]

    url = CENTRAL_BASE_URL + prop_filename
    logging.info('loading localization file from central: {%s}' % url)

    try:
        central_prop_cache[prop_filename] = urllib2.urlopen(url).readlines()
    except:
        logging.error('failed to load properties file from central: {%s}'
                      % url)
        central_prop_cache[prop_filename] = []

    return central_prop_cache[prop_filename]


# Retrieve the current en-US localization notes for the provided prop_name.
def get_localization_note(prop_name, prop_filename):
    prop_content = get_central_prop_content(prop_filename)

    comment_buffer = []
    for i, line in enumerate(prop_content):
        # Remove line breaks.
        line = line.strip('\n').strip('\r')

        if line.startswith('#'):
            # Comment line, add to the current comment buffer.
            comment_buffer.append(line)
        elif re.search('(^|\n)' + re.escape(prop_name) + '\s*=', line):
            # Property found, the current comment buffer is the localization
            # note.
            break;
        else:
            # No match, not a comment, reinitialize the comment buffer.
            comment_buffer = []

    return '\n'.join(comment_buffer)


# Retrieve the parsed DTD entities for a provided path. Results are cached by
# dtd path.
def get_dtd_entities(dtd_path):
    if dtd_path in dtd_entities_cache:
        return dtd_entities_cache[dtd_path]

    dtd_parser = parser.getParser('.dtd')
    dtd_parser.readFile(dtd_path)
    dtd_entities_cache[dtd_path] = dtd_parser.parse()
    return dtd_entities_cache[dtd_path]


# Extract the value of an entity in a dtd file.
def get_translation_from_dtd(dtd_path, entity_name):
    entities, map = get_dtd_entities(dtd_path)
    if entity_name not in map:
        # Bail out if translation is missing.
        return

    key = map[entity_name]
    entity = entities[key]
    translation = HTML_PARSER.unescape(entity.val)
    return translation.encode('utf-8')


# Extract the header and file wide comments for the provided properties file
# filename.
def get_properties_header(prop_filename):
    prop_content = get_central_prop_content(prop_filename)

    # if the file content is empty, return the default license header.
    if len(prop_content) == 0:
        return DEFAULT_HEADER

    header_buffer = []
    for i, line in enumerate(prop_content):
        # remove line breaks.
        line = line.strip('\n').strip('\r')

        # regexp matching keys extracted form parser.py.
        is_entity_line = re.search('^(\s*)'
                                   '((?:[#!].*?\n\s*)*)'
                                   '([^#!\s\n][^=:\n]*?)\s*[:=][ \t]*', line)
        is_loc_note = re.search('^(\s*)'
                                '\#\s*LOCALIZATION NOTE\s*\([^)]+\)', line)
        if is_entity_line or is_loc_note:
            # header finished, break the loop.
            break
        else:
            # header line, add to the current buffer.
            header_buffer.append(line)

    # concatenate the current buffer and return.
    return '\n'.join(header_buffer)


# Create a new properties file at the provided path.
def create_properties_file(prop_path):
    logging.info('creating new *.properties file: {%s}' % prop_path)

    prop_filename = os.path.basename(prop_path)
    header = get_properties_header(prop_filename)

    prop_file = open(prop_path, 'w+')
    prop_file.write(header)
    prop_file.close()


# Migrate a single string entry for a dtd to a properties file.
def migrate_string(dtd_path, prop_path, dtd_name, prop_name):
    if not os.path.isfile(dtd_path):
        logging.error('dtd file can not be found at: {%s}' % dtd_path)
        return

    translation = get_translation_from_dtd(dtd_path, dtd_name)
    if not translation:
        logging.error('translation could not be found for: {%s} in {%s}'
                      % (dtd_name, dtd_path))
        return

    # Create properties file if missing.
    if not os.path.isfile(prop_path):
        create_properties_file(prop_path)

    if not os.path.isfile(prop_path):
        logging.error('could not create new properties file at: {%s}'
                      % prop_path)
        return

    prop_line = prop_name + '=' + translation + '\n'

    # Skip the string if it already exists in the destination file.
    prop_file_content = open(prop_path, 'r').read()
    if prop_line in prop_file_content:
        logging.warning('string already migrated, skipping: {%s}' % prop_name)
        return

    # Skip the string and log an error if an existing entry is found, but with
    # a different value.
    if re.search('(^|\n)' + re.escape(prop_name) + '\s*=', prop_file_content):
        logging.error('existing string found, skipping: {%s}' % prop_name)
        return

    prop_filename = os.path.basename(prop_path)
    logging.info('migrating {%s} in {%s}' % (prop_name, prop_filename))
    with open(prop_path, 'a') as prop_file:
        localization_note = get_localization_note(prop_name, prop_filename)
        if len(localization_note):
            prop_file.write('\n' + localization_note)
        else:
            logging.warning('localization notes could not be found for: {%s}'
                            % prop_name)
        prop_file.write('\n' + prop_line)


# Apply the migration instructions in the provided configuration file.
def migrate_conf(conf_path, l10n_path):
    f = open(conf_path, 'r')
    lines = f.readlines()
    f.close()

    for i, line in enumerate(lines):
        # Remove line breaks.
        line = line.strip('\n').strip('\r')

        # Skip invalid lines.
        if ' = ' not in line:
            continue

        # Expected syntax: ${prop_path}:${prop_name} = ${dtd_path}:${dtd_name}.
        prop_info, dtd_info = line.split(' = ')
        prop_path, prop_name = prop_info.split(':')
        dtd_path, dtd_name = dtd_info.split(':')

        dtd_path = os.path.join(l10n_path, dtd_path)
        prop_path = os.path.join(l10n_path, prop_path)

        migrate_string(dtd_path, prop_path, dtd_name, prop_name)


def main():
    # Read command line arguments.
    arg_parser = argparse.ArgumentParser(
            description='Migrate devtools localized strings.')
    arg_parser.add_argument('path', type=str, help='path to l10n repository')
    arg_parser.add_argument('-c', '--config', type=str,
                            help='path to configuration file or folder')
    args = arg_parser.parse_args()

    # Retrieve path to devtools localization files in l10n repository.
    devtools_l10n_path = os.path.join(args.path, 'devtools/client/')
    if not os.path.exists(devtools_l10n_path):
        logging.error('l10n path is invalid: {%s}' % devtools_l10n_path)
        exit()
    logging.info('l10n path is valid: {%s}' % devtools_l10n_path)

    # Retrieve configuration files to apply.
    if os.path.isdir(args.config):
        conf_files = glob.glob(args.config + '*')
    elif os.path.isfile(args.config):
        conf_files = [args.config]
    else:
        logging.error('config path is invalid: {%s}' % args.config)
        exit()

    # Perform migration for each configuration file.
    for conf_file in conf_files:
        logging.info('performing migration for config file: {%s}' % conf_file)
        migrate_conf(conf_file, devtools_l10n_path)


if __name__ == '__main__':
    main()
