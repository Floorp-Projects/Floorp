#!/usr/bin/env python
# coding=utf8

import os
import sys
import json
import logging
import argparse
import importlib

import hglib
from hglib.util import b

from fluent.migrate import (
    MergeContext, MigrationError, convert_blame_to_changesets
)
from blame import Blame


def main(lang, reference_dir, localization_dir, blame, migrations, dry_run):
    """Run migrations and commit files with the result."""
    changesets = convert_blame_to_changesets(blame)
    client = hglib.open(localization_dir)

    for migration in migrations:

        print('Running migration {}'.format(migration.__name__))

        # For each migration create a new context.
        ctx = MergeContext(lang, reference_dir, localization_dir)

        try:
            # Add the migration spec.
            migration.migrate(ctx)
        except MigrationError as err:
            sys.exit(err.message)

        # Keep track of how many changesets we're committing.
        index = 0

        for changeset in changesets:
            # Run the migration for the changeset.
            snapshot = ctx.serialize_changeset(changeset['changes'])

            # Did it change any files?
            if not snapshot:
                continue

            # Write serialized FTL files to disk.
            for path, content in snapshot.iteritems():
                fullpath = os.path.join(localization_dir, path)
                print('  Writing to {}'.format(fullpath))
                if not dry_run:
                    fulldir = os.path.dirname(fullpath)
                    if not os.path.isdir(fulldir):
                        os.makedirs(fulldir)
                    with open(fullpath, 'w') as f:
                        f.write(content.encode('utf8'))
                        f.close()

            index += 1
            author = changeset['author'].encode('utf8')
            message = migration.migrate.__doc__.format(
                index=index,
                author=author
            )

            print('    Committing changeset: {}'.format(message))
            if not dry_run:
                client.commit(
                    b(message), user=b(author), addremove=True
                )

if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description='Migrate translations to FTL.'
    )
    parser.add_argument(
        'migrations', metavar='MIGRATION', type=str, nargs='+',
        help='migrations to run (Python modules)'
    )
    parser.add_argument(
        '--lang', type=str,
        help='target language code'
    )
    parser.add_argument(
        '--reference-dir', type=str,
        help='directory with reference FTL files'
    )
    parser.add_argument(
        '--localization-dir', type=str,
        help='directory for localization files'
    )
    parser.add_argument(
        '--blame', type=argparse.FileType(), default=None,
        help='path to a JSON with blame information'
    )
    parser.add_argument(
        '--dry-run', action='store_true',
        help='do not write to disk nor commit any changes'
    )
    parser.set_defaults(dry_run=False)

    logger = logging.getLogger('migrate')
    logger.setLevel(logging.INFO)

    args = parser.parse_args()

    if args.blame:
        # Load pre-computed blame from a JSON file.
        blame = json.load(args.blame)
    else:
        # Compute blame right now.
        print('Annotating {}'.format(args.localization_dir))
        blame = Blame(args.localization_dir).main()

    main(
        lang=args.lang,
        reference_dir=args.reference_dir,
        localization_dir=args.localization_dir,
        blame=blame,
        migrations=map(importlib.import_module, args.migrations),
        dry_run=args.dry_run
    )
