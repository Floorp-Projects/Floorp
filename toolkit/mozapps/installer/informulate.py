#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys
import json
import buildconfig


def parse_cmdline(args):
    """Take a list of strings in the format K=V and turn them into a python
    dictionary"""
    contents = {}
    for arg in args:
        key, s, value = arg.partition("=")
        if s == '':
            print "ERROR: Malformed command line key value pairing (%s)" % arg
            exit(1)
        contents[key.lower()] = value
    return contents


def main():
    if len(sys.argv) < 2:
        print "ERROR: You must specify an output file"
        exit(1)

    all_key_value_pairs = {}
    important_substitutions = [
        'target_alias', 'target_cpu', 'target_os', 'target_vendor',
        'host_alias', 'host_cpu', 'host_os', 'host_vendor',
        'MOZ_UPDATE_CHANNEL', 'MOZ_APP_VENDOR', 'MOZ_APP_NAME',
        'MOZ_APP_VERSION', 'MOZ_APP_MAXVERSION', 'MOZ_APP_ID',
        'CC', 'CXX', 'LD', 'AS']

    all_key_value_pairs = dict([(x.lower(), buildconfig.substs[x]) for x in important_substitutions])
    all_key_value_pairs.update(parse_cmdline(sys.argv[2:]))

    with open(sys.argv[1], "w+") as f:
        json.dump(all_key_value_pairs, f, indent=2, sort_keys=True)
        f.write('\n')


if __name__=="__main__":
    main()
