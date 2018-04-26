#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Generate build info files for use by other tools.
# This script assumes it is being run in a Mozilla CI build.

from __future__ import unicode_literals

from argparse import ArgumentParser
import json
import buildconfig
import os


def main():
    parser = ArgumentParser()
    parser.add_argument('output_json', help='Output JSON file')
    parser.add_argument('output_txt', help='Output text file')
    # TODO: Move package-name.mk variables into moz.configure.
    parser.add_argument('pkg_platform', help='Package platform identifier')
    args = parser.parse_args()

    important_substitutions = [
        'target_alias', 'target_cpu', 'target_os', 'target_vendor',
        'host_alias', 'host_cpu', 'host_os', 'host_vendor',
        'MOZ_UPDATE_CHANNEL', 'MOZ_APP_VENDOR', 'MOZ_APP_NAME',
        'MOZ_APP_VERSION', 'MOZ_APP_MAXVERSION', 'MOZ_APP_ID',
        'CC', 'CXX', 'AS', 'MOZ_SOURCE_REPO',
    ]

    all_key_value_pairs = {x.lower(): buildconfig.substs[x]
                           for x in important_substitutions}
    build_id = os.environ['MOZ_BUILD_DATE']
    all_key_value_pairs.update({
        'buildid': build_id,
        'moz_source_stamp': buildconfig.substs['MOZ_SOURCE_CHANGESET'],
        'moz_pkg_platform': args.pkg_platform,
    })

    with open(args.output_json, 'wb') as f:
        json.dump(all_key_value_pairs, f, indent=2, sort_keys=True)
        f.write('\n')

    with open(args.output_txt, 'wb') as f:
        f.write('buildID={}\n'.format(build_id))


if __name__ == '__main__':
    main()
