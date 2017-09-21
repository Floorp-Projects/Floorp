# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import re

# platform family is extracted from build platform by taking the alphabetic prefix
# and then translating win -> windows
_platform_re = re.compile(r'^[a-z]*')
_renames = {
    'win': 'windows'
}


def platform_family(build_platform):
    """Given a build platform, return the platform family (linux, macosx, etc.)"""
    family = _platform_re.match(build_platform).group(0)
    return _renames.get(family, family)
