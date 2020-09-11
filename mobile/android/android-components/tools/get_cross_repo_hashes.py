#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

"""See USAGE for details.

Potential script improvements:
- AUTOMATE: fenix check out -> a-c hash
- (later) Support more than nightly
- (later) Would this be more usable as a website?
  Would require automating extraction of a-c hash
  from Fenix apk or other mechanism

Fragile assumptions in implementation:
- Hard-coded path: Gecko.kt
- Hard-coded format: Gecko.kt
- Hard-coded format: POM XML
"""

import os
import sys
from enum import Enum
from urllib.error import HTTPError
from urllib.request import urlopen

SCRIPT_NAME=os.path.basename(__file__)
SCRIPT_DIR=os.path.dirname(os.path.realpath(__file__))

USAGE="""usage: ./{script_name} [-h] [--help] [--gv-nightly <version>]
  (no args): maps the currently checked out ac revision to a valid mc
             revision.
  --gv-nightly: disables default behavior and maps the given GV nightly version
                to a valid mc version.
  --help, -h: prints this information and exits.


Purpose: when building across repositories, e.g. fenix with a local a-c,
sometimes the build will fail with errors unrelated to your changes because of
a version mismatch: there were breaking changes in the dependency repository
that are not represented in the dependent repository. This script aims to
address that.

At present, this script is only partially automated. Here are instructions
to get valid hashes between the major repositories:

1) fenix checkout -> a-c hash
    build & install Fenix, hit 3-dot -> Settings -> About Firefox Nightly
    and read hash at the end of the "AC:" line

2) a-c checkout (current) -> GV Nightly version
    AUTOMATED: run this script

3) GV nightly version -> mozilla-central hash
    AUTOMATED: run this script
    Use `--gv-nightly` to override with a custom version.

In the future, we hope to automate step 1.""".format(script_name=SCRIPT_NAME)

TEMPLATE_NIGHTLY_POM="https://maven.mozilla.org/maven2/org/mozilla/geckoview/geckoview-nightly-arm64-v8a/{version}/geckoview-nightly-arm64-v8a-{version}.pom"
INDENT= '  '

GV_VERSION_PATH=os.path.join('buildSrc', 'src', 'main', 'java', 'Gecko.kt')

class Mode(Enum):
    Default = 1
    Nightly_version = 2

def print_usage(exit=False):
    print(USAGE, file=sys.stderr)
    if exit: sys.exit(1)

def maybe_display_usage():
    if len(sys.argv) >= 2 and (sys.argv[1] == '--help' or sys.argv[1] == '-h'):
        print_usage(exit=True)

def validate_args():
    if len(sys.argv) <= 1:
        return Mode.Default, None

    if sys.argv[1] != '--gv-nightly':
        raise Exception('unknown argument ' + sys.argv[1])

    gv_nightly_version = sys.argv[2]
    major = gv_nightly_version[:gv_nightly_version.find('.')]
    if int(major) < 71:  # it may actually be some intermediate v70 that the hash was added.
        print('ERROR: GV versions below 71 do not have an m-c hash associated with their POM. Aborting...', file=sys.stderr)
        sys.exit(1)

    return Mode.Nightly_version, gv_nightly_version

def get_pom(gv_nightly_version):
    url = TEMPLATE_NIGHTLY_POM.format(version=gv_nightly_version)
    try:
        res = urlopen(url)  # throws if not success.
    except HTTPError as e:
        raise Exception('unable to fetch POM from...\n' + INDENT + url) from e
    return res.read(), url

def get_mc_hash_from_pom(pom_str, debug_pom_url = None):
    # XML libraries are open to vulnerabilities so we parse by hand.
    # Expected format (one line): <tag>d4e11195e39888686d843a146a893eb0ebf38224</tag>
    mc_hash = 0
    for line in pom_str.split(b'\n'):
        stripped = line.strip()
        if not stripped.startswith(b'<tag>'): continue
        close_tag_index = stripped.find(b'</tag>')
        mc_hash = stripped[5:close_tag_index]  # also remove open tag.

    if not mc_hash:
        raise Exception('mc hash could not be found in pom.xml from...\n' + INDENT + str(debug_pom_url))
    return mc_hash

def get_mc_hash_from_gv_nightly(gv_nightly_version):
    pom, debug_pom_url = get_pom(gv_nightly_version)
    return get_mc_hash_from_pom(pom, debug_pom_url)

def get_gv_versions_from_ac_checkout(ac_root):
    def get_version_from_line(line):
        # Expected format: `const val nightly_version = "82.0.20200907094115"`
        return line[line.find('"') + 1:line.rfind('"')]

    release_version = None
    beta_version = None
    nightly_version = None

    gv_version_path = os.path.join(ac_root, GV_VERSION_PATH)
    with open(gv_version_path) as f:
        for line in f:
            stripped = line.strip()
            if stripped.startswith('const val nightly_version = "'):
                nightly_version = get_version_from_line(line)
            elif stripped.startswith('const val beta_version = "'):
                beta_version = get_version_from_line(line)
            elif stripped.startswith('const val release_version = "'):
                release_version = get_version_from_line(line)
    return release_version, beta_version, nightly_version

def main_mode_default():
    # fenix checkout -> ac hash
    print('fenix checkout -> ac hash')
    print(INDENT + 'must be done manually. See --help for instructions')
    print(INDENT + 'WARNING: if a different ac revision is found in fenix than')
    print(INDENT + 'the currently checked out one, this script will likely need')
    print(INDENT + 'to be rerun after the new revision is checked out')
    print()

    # ac checkout -> GV versions
    gv_version_path = os.path.join(SCRIPT_DIR, '..')
    releasev, betav, nightlyv = get_gv_versions_from_ac_checkout(gv_version_path)
    print('ac checkout (current) -> GV versions')
    print(INDENT + 'nightly: ' + nightlyv)
    print(INDENT + 'beta:    ' + betav)
    print(INDENT + 'release: ' + releasev)
    print()

    # GV Nightly -> mc hash
    mc_hash = get_mc_hash_from_gv_nightly(nightlyv)
    print('GV nightly version -> mc hash:')
    print(INDENT + mc_hash.decode('utf-8'))

def main_mode_nightly(gv_nightly_version):
    mc_hash = get_mc_hash_from_gv_nightly(gv_nightly_version)
    print(mc_hash.decode('utf-8'))

def main():
    maybe_display_usage()
    mode, gv_nightly_version = validate_args()
    if mode == Mode.Default: main_mode_default()
    elif mode == Mode.Nightly_version: main_mode_nightly(gv_nightly_version)

if __name__ == '__main__':
    try:
        main()
    except Exception as e:
        # We display usage on all errors to make understanding how to use the
        # script easier, given that our arg parsing is not robust.
        print_usage()
        print('\n---\n')  # separate usage from Exception.
        raise e
