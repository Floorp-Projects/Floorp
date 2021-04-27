#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

"""A script to help build fenix with a local ac or ac with a local GV without
build errors. See USAGE and script output for details.

This script has some fragile hard-coded bits: these are documented in the
test file.

Potential script improvements:
- (later) Support more than AC nightly builds and GV nightly builds
- (later) Would this be more usable as a website?
"""

import os
import sys
from urllib.error import HTTPError
from urllib.request import urlopen

SCRIPT_NAME=os.path.basename(__file__)
SCRIPT_DIR=os.path.dirname(os.path.realpath(__file__))

USAGE="""usage: ./{script_name} path-to-fenix-repository

For secondary use cases:
  --no-fenix: pass instead of path-to-fenix-repository to disable fenix functionality
  --help, -h: prints this information and exits.

When building across repositories, e.g. fenix with a local a-c, sometimes the
build will fail with errors unrelated to your changes because of a version
mismatch: e.g. there were breaking changes in ac that are not represented in
fenix. This script will print changeset information to help fix these build errors.

Does the script work correctly? Is it intuitive?
Please send feedback to @mcomella.""".format(script_name=SCRIPT_NAME)

INDENT= '  '
INDENT2=INDENT * 2

# For a-c, we deliberately choose a component that's unlikely to be renamed or go away.
TEMPLATE_POM_URL_AC_NIGHTLY="https://nightly.maven.mozilla.org/maven2/org/mozilla/components/support-base/{version}/support-base-{version}.pom"
TEMPLATE_POM_URL_GV_NIGHTLY="https://maven.mozilla.org/maven2/org/mozilla/geckoview/geckoview-nightly-arm64-v8a/{version}/geckoview-nightly-arm64-v8a-{version}.pom"

# We get the ac version from a published POM.xml; the ac hash wasn't (roughly) available before this version.
VERSION_MIN_AC_NIGHTLY='59.0.20200914093656'
VERSION_MIN_AC_NIGHTLY_BUGFIX=VERSION_MIN_AC_NIGHTLY.split('.')[2]
VERSION_MIN_GV_NIGHTLY_MAJOR='71' # it may actually be some intermediate v70 that the hash was added.

PATH_AC_ROOT=os.path.join(SCRIPT_DIR, '..')
PATH_AC_VERSION=os.path.join('buildSrc', 'src', 'main', 'java', 'AndroidComponents.kt')
PATH_GV_VERSION=os.path.join('buildSrc', 'src', 'main', 'java', 'Gecko.kt')

### SECTION: USAGE AND ARGS ###
def print_usage(exit=False):
    print(USAGE, file=sys.stderr)
    if exit: sys.exit(1)

def maybe_display_usage():
    if '--help' in sys.argv or '-h' in sys.argv:
        print_usage(exit=True)

def validate_args():
    if len(sys.argv) != 2: # argv[0] == script invocation.
        # We intentionally require one or the other to ensure folks are opting in to the chosen behavior.
        raise Exception('expected one argument: path-to-fenix-repository or --no-fenix. See usage above.')

    if sys.argv[1] == '--no-fenix':
        return None, True
    else:
        return sys.argv[1], False

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
    ac_hash = get_hash_from_pom(pom, debug_pom_url)
    return ac_hash, ac_version

### SECTION: ac -> mc ###
def validate_gv_nightly_version(version):
    major = version.split('.')[0]
    if int(major) < int(VERSION_MIN_GV_NIGHTLY_MAJOR):
        raise Exception('Found GV nightly version {} is too old: use major version {} or newer (e.g. use a newer version of ac).'.format(version, VERSION_MIN_GV_NIGHTLY_MAJOR))

def gv_nightly_version_to_mc_hash(gv_nightly_version):
    pom_url = TEMPLATE_POM_URL_GV_NIGHTLY.format(version=gv_nightly_version)
    pom, debug_pom_url = fetch_pom(pom_url)
    return get_hash_from_pom(pom, debug_pom_url)

def ac_checkout_to_gv_version(ac_root):
    version = None

    gv_version_path = os.path.join(ac_root, PATH_GV_VERSION)
    with open(gv_version_path) as f:
        for line in f:
            stripped = line.strip()

            # Expected format: `const val nightly_version = "82.0.20200907094115"`
            if stripped.startswith('const val version = "'):
                version = extract_str_inside_quotes(line)

        if not version:
            raise Exception('Unable to find gv versions from ac repository file "{}". This is likely a bug where the file path or format has changed.'.format(gv_version_path))
    return version

def ac_checkout_to_mc_hash(ac_root):
    version = ac_checkout_to_gv_version(ac_root)

    validate_gv_nightly_version(version)
    mc_hash = gv_nightly_version_to_mc_hash(version)
    return mc_hash, version

### SECTION: MAIN ###
def main_repo_to_hash(fenix_path, is_no_fenix_passed):
    header = 'Building fenix with a local ac?'
    if is_no_fenix_passed:
        print('Skipping "{}"'.format(header))
    else:
        ac_hash, ac_version = fenix_checkout_to_ac_hash(fenix_path)
        print(header)
        print(INDENT + 'The last known ac nightly version that cleanly builds with your fenix checkout is...')
        print(INDENT2 + ac_version)
        print(INDENT + 'To build with this version, checkout ac commit...')
        print(INDENT2 + ac_hash.decode('utf-8'))
    print()

    mc_hash, nightlyv, = ac_checkout_to_mc_hash(PATH_AC_ROOT)
    print('Building ac with a local GeckoView?')
    print(INDENT + 'The last known GV nightly version that cleanly builds with your ac checkout is...')
    print(INDENT2 + nightlyv)
    print(INDENT + 'To build with this version, checkout mozilla-central commit...')
    print(INDENT2 + mc_hash.decode('utf-8'))
    print()

    print('Building fenix with a local GeckoView?')
    print(INDENT + 'To ensure a clean build, 1) checkout the suggested ac commit,')
    print(INDENT + '2) re-run this script, and 3) checkout the suggested m-c commit')
    print()

    print('Building fenix with a local ac that has a local GeckoView?')
    print(INDENT + 'This functionality is currently broken. See:')
    print(INDENT + 'https://github.com/mozilla-mobile/android-components/issues/8386')

def main():
    maybe_display_usage()
    fenix_path, is_no_fenix_passed = validate_args()
    main_repo_to_hash(fenix_path, is_no_fenix_passed)

if __name__ == '__main__':
    try:
        main()
    except Exception as e:
        # We display usage on all errors to make understanding how to use the
        # script easier, given that our arg parsing is not robust.
        print_usage()
        print('\n---\n')  # separate usage from Exception.
        raise e
