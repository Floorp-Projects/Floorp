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
    OR
    after building a release build, visit `mozilla.components.Build` from your
    Fenix repo in Android Studio and go-to-definition on `gitHash`

2) a-c checkout (current) -> GV Nightly version
    AUTOMATED: run this script

3) GV nightly version -> mozilla-central hash
    AUTOMATED: run this script
    Use `--gv-nightly` to override with a custom version.

In the future, we hope to automate step 1.""".format(script_name=SCRIPT_NAME)

INDENT= '  '
INDENT2=INDENT * 2

# For a-c, we deliberately choose a component that's unlikely to be renamed or go away.
TEMPLATE_POM_URL_AC_NIGHTLY="https://nightly.maven.mozilla.org/maven2/org/mozilla/components/support-base/{version}/support-base-{version}.pom"
TEMPLATE_POM_URL_GV_NIGHTLY="https://maven.mozilla.org/maven2/org/mozilla/geckoview/geckoview-nightly-arm64-v8a/{version}/geckoview-nightly-arm64-v8a-{version}.pom"

# We get the ac version from a published POM.xml; the ac hash wasn't (roughly) available before this version.
VERSION_MIN_AC_NIGHTLY='59.0.20200914093656'
VERSION_MIN_AC_NIGHTLY_BUGFIX=VERSION_MIN_AC_NIGHTLY.split('.')[2]
VERSION_MIN_GV_NIGHTLY_MAJOR='71' # it may actually be some intermediate v70 that the hash was added.

PATH_AC_VERSION=os.path.join('buildSrc', 'src', 'main', 'java', 'AndroidComponents.kt')
PATH_GV_VERSION=os.path.join('buildSrc', 'src', 'main', 'java', 'Gecko.kt')

class Mode(Enum):
    Default = 1
    Nightly_version = 2

### SECTION: USAGE AND ARGS ###
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
    validate_gv_nightly_version(gv_nightly_version)
    return Mode.Nightly_version, gv_nightly_version

### SECTION: UTILITIES ###
def extract_str_inside_quotes(str_with_quotes):
    """Takes a line like 'const val x = "abc"' and returns abc"""
    s = str_with_quotes
    return s[s.find('"') + 1:s.rfind('"')]

def fetch_pom(url):
    try:
        res = urlopen(url)  # throws if not success.
    except HTTPError as e:
        raise Exception('unable to fetch POM from...\n' + INDENT + url) from e
    return res.read(), url

def get_hash_from_pom(pom_str, debug_pom_url = None):
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

### SECTION: fenix -> ac ###
def validate_ac_version(ac_version):
    bugfix_version = ac_version.split('.')[2]
    is_nightly_url = len(bugfix_version) > 4 # we assume we'll never exceed 9k bug fix releases for a release version 🤞
    if not is_nightly_url:
        raise NotImplementedError('AC release versions not yet implemented: try using a nightly ac. Attempted to fetch version {}.'.format(ac_version))

    if int(bugfix_version) < int(VERSION_MIN_AC_NIGHTLY_BUGFIX):
        raise Exception('Found AC nightly version {} is too old: use version {} or newer (e.g. use a newer version of fenix).'.format(ac_version, VERSION_MIN_AC_NIGHTLY))

def fenix_checkout_to_ac_version(fenix_path):
    ac_version_path = os.path.join(fenix_path, PATH_AC_VERSION)
    with open(ac_version_path) as f:
        for line in f:
            stripped = line.strip()

            # Expected format: const val VERSION = "58.0.20200910190642"
            if not stripped.startswith('const val VERSION = "'): continue
            return extract_str_inside_quotes(stripped)
    raise Exception('Unable to find ac version from fenix repository file "{}". This is likely a bug where the file path or format has changed.'.format(ac_version_path))

def fenix_checkout_to_ac_hash(fenix_path):
    ac_version = fenix_checkout_to_ac_version(fenix_path)
    validate_ac_version(ac_version)

    pom_url = TEMPLATE_POM_URL_AC_NIGHTLY.format(version=ac_version)
    pom, debug_pom_url = fetch_pom(pom_url)
    return get_hash_from_pom(pom, debug_pom_url)

### SECTION: ac -> mc ###
def validate_gv_nightly_version(version):
    major = version.split('.')[0]
    if int(major) < int(VERSION_MIN_GV_NIGHTLY_MAJOR):
        raise Exception('Found GV nightly version {} is too old: use major version {} or newer (e.g. use a newer version of ac).'.format(version, VERSION_MIN_GV_NIGHTLY_MAJOR))

def gv_nightly_version_to_mc_hash(gv_nightly_version):
    pom_url = TEMPLATE_POM_URL_GV_NIGHTLY.format(version=gv_nightly_version)
    pom, debug_pom_url = fetch_pom(pom_url)
    return get_hash_from_pom(pom, debug_pom_url)

def ac_checkout_to_gv_versions(ac_root):
    release_version = None
    beta_version = None
    nightly_version = None

    gv_version_path = os.path.join(ac_root, PATH_GV_VERSION)
    with open(gv_version_path) as f:
        for line in f:
            stripped = line.strip()

            # Expected format: `const val nightly_version = "82.0.20200907094115"`
            if stripped.startswith('const val nightly_version = "'):
                nightly_version = extract_str_inside_quotes(line)
            elif stripped.startswith('const val beta_version = "'):
                beta_version = extract_str_inside_quotes(line)
            elif stripped.startswith('const val release_version = "'):
                release_version = extract_str_inside_quotes(line)
    return release_version, beta_version, nightly_version

def ac_checkout_to_mc_hash(ac_root):
    gv_version_path = os.path.join(SCRIPT_DIR, '..')
    releasev, betav, nightlyv = ac_checkout_to_gv_versions(gv_version_path)

    validate_gv_nightly_version(nightlyv)
    mc_hash = gv_nightly_version_to_mc_hash(nightlyv)
    return mc_hash, releasev, betav, nightlyv

### SECTION: MAIN ###
def main_mode_default():
    # fenix checkout -> ac hash
    print('fenix checkout -> ac hash')
    print(INDENT + 'must be done manually. See --help for instructions')
    print(INDENT + 'WARNING: if a different ac revision is found in fenix than')
    print(INDENT + 'the currently checked out one, this script will likely need')
    print(INDENT + 'to be rerun after the new revision is checked out')
    print()

    # ac checkout -> mc hash
    mc_hash, releasev, betav, nightlyv, = ac_checkout_to_mc_hash('.')
    print('ac checkout -> mc hash')
    print(INDENT + mc_hash.decode('utf-8'))
    print(INDENT + 'derived from:')
    print(INDENT2 + 'GV nightly: ' + nightlyv)
    print(INDENT2 + 'GV beta:    ' + betav)
    print(INDENT2 + 'GV release: ' + releasev)

def main_mode_nightly(gv_nightly_version):
    mc_hash = gv_nightly_version_to_mc_hash(gv_nightly_version)
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
