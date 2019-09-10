from __future__ import absolute_import
from __future__ import print_function
from lib.xul import collect_messages
from lib.dtd import get_dtds
from lib.utils import read_file, write_file
from lib.migration import build_migration
from lib.fluent import build_ftl
import json
import argparse
import sys
# import re

# To run this, you'll need to have lxml installed:
#   `pip install lxml`

# default migration directions
data = {
    'migration': 'python/l10n/fluent_migrations',
    'description': 'Migrate l10n strings',
    'prefix': ''
}


def parse_inputs():
    sys_args = sys.argv[1:]

    parser = argparse.ArgumentParser()
    parser.add_argument('bug_id', type=str,
                        help='Id number for bug tracking')
    parser.add_argument('xul', type=str,
                        help='POSIX path from mozilla-central to the XML to be updated')
    parser.add_argument('ftl', type=str,
                        help='Case sensitive POSIX path from mozilla-central to desired ftl file')
    parser.add_argument('mozilla_central', type=str,
                        help='Case sensitive absolute POSIX path to current mozilla-central repo')
    parser.add_argument('dtd', type=str,
                        help='comma delimited list of case sensitive POSIX dtd file paths')
    parser.add_argument('description', type=str,
                        help='string enclosed in quotes')
    parser.add_argument('--dry-run', action='store_true',
                        help='Tell if running dry run or not')
    parser.add_argument('--prefix', type=str,
                        help='a keyword string to be added to all l10n ids')

    parsed_args = parser.parse_args(sys_args)

    data['description'] = parsed_args.description
    data['bug_id'] = parsed_args.bug_id
    data['xul'] = parsed_args.xul
    data['ftl'] = parsed_args.ftl
    data['mozilla-central'] = parsed_args.mozilla_central
    data['dtd'] = parsed_args.dtd.split(',')
    data['dry-run'] = parsed_args.dry_run
    data['prefix'] = parsed_args.prefix
    data['recipe'] = "bug_{}_{}.py".format(data['bug_id'],
                                           data['xul'].split('/')[-1].split('.')[0])

    main()


def main():
    dry_run = data['dry-run']
    dtds = get_dtds(data['dtd'], data['mozilla-central'])

    print('======== DTDs ========')
    print(json.dumps(dtds, sort_keys=True, indent=2))

    s = read_file(data['xul'], data['mozilla-central'])

    print('======== INPUT ========')
    print(s)

    print('======== OUTPUT ========')
    (new_xul, messages) = collect_messages(s, data['prefix'])
    print(new_xul)
    if not dry_run:
        write_file(data['xul'], new_xul, data['mozilla-central'])

    print('======== L10N ========')

    print(json.dumps(messages, sort_keys=True, indent=2))

    migration = build_migration(messages, dtds, data)

    print('======== MIGRATION ========')
    print(migration)
    recipe_path = "{}/{}".format(data['migration'], data['recipe'])
    if not dry_run:
        write_file(recipe_path, migration, data['mozilla-central'])

    ftl = build_ftl(messages, dtds, data)

    print('======== Fluent ========')
    print(ftl.encode("utf-8"))
    if not dry_run:
        write_file(data['ftl'], ftl.encode("utf-8"), data['mozilla-central'])


if __name__ == '__main__':
    parse_inputs()
