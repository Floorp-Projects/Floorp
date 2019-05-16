# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

import sys

import buildconfig

from mozbuild import preprocessor


def main(output, strings_xml, android_strings_dtd, sync_strings_dtd, locale=None):
    if not locale:
        raise ValueError('locale must be specified!')

    CONFIG = buildconfig.substs

    defines = {}
    defines['ANDROID_PACKAGE_NAME'] = CONFIG['ANDROID_PACKAGE_NAME']
    defines['MOZ_APP_DISPLAYNAME'] = CONFIG['MOZ_APP_DISPLAYNAME']
    # Includes.
    defines['STRINGSPATH'] = android_strings_dtd
    defines['SYNCSTRINGSPATH'] = sync_strings_dtd
    # Fennec branding is en-US only: see
    # $(MOZ_BRANDING_DIRECTORY)/locales/jar.mn.
    defines['BRANDPATH'] = '{}/{}/locales/en-US/brand.dtd'.format(
        buildconfig.topsrcdir, CONFIG['MOZ_BRANDING_DIRECTORY'])

    includes = preprocessor.preprocess(includes=[strings_xml],
                                       defines=defines,
                                       output=output)
    return includes


if __name__ == "__main__":
    main(sys.argv[1:])
