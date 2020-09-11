#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

"""See USAGE for details.

Potential script improvements:
- Display usage on all errors
- AUTOMATE: a-c check out -> GV versions
- AUTOMATE: fenix check out -> a-c hash
- (later) Support more than nightly
- (later) Would this be more usable as a website?
  Would require automating extraction of a-c hash
  from Fenix apk or other mechanism

Fragile assumptions in implementation:
- Hard-coded POM XML format
"""

import os
import sys
from urllib.error import HTTPError
from urllib.request import urlopen

SCRIPT_NAME=os.path.basename(__file__)
USAGE="""usage: ./{script_name} <gv-nightly-version>
  <gv-nightly-version>: should look like 82.0.20200907094115

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

2) a-c checkout -> GV Nightly version
    Open buildSrc/src/main/java/Gecko.kt
    and read `const val nightly_version = ...`

3) GV Nightly version -> mozilla-central hash
    AUTOMATED: run this script

In the future, we hope to automate steps 1 & 2 too.""".format(script_name=SCRIPT_NAME)

TEMPLATE_NIGHTLY_POM="https://maven.mozilla.org/maven2/org/mozilla/geckoview/geckoview-nightly-arm64-v8a/{version}/geckoview-nightly-arm64-v8a-{version}.pom"
INDENT= '     '

def maybe_display_usage():
    if sys.argv[1] == '--help' or sys.argv[1] == '-h':
        print(USAGE, file=sys.stderr)
        sys.exit(1)

def validate_args(gv_nightly_version):
    major = gv_nightly_version[:gv_nightly_version.find('.')]
    if int(major) < 71:  # it may actually be some intermediate v70 that the hash was added.
        print('ERROR: GV versions below 71 do not have an m-c hash associated with their POM. Aborting...', file=sys.stderr)
        sys.exit(1)

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

def main():
    maybe_display_usage()
    gv_nightly_version = sys.argv[1]
    validate_args(gv_nightly_version)

    mc_hash = get_mc_hash_from_gv_nightly(gv_nightly_version)
    print('gv-nightly-version: ' + gv_nightly_version)
    print('mozilla-central hash: ' + mc_hash.decode('utf-8'))

if __name__ == '__main__':
    main()
