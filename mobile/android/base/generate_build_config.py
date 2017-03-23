#!/bin/python

# -*- Mode: python; indent-tabs-mode: nil; tab-width: 40 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

'''
Generate BuildConfig Java source files.

BuildConfig files are the Gradle way of configuring Java values at build time.

This is the moz.build equivalent of Gradle's `buildConfigField` (see
http://google.github.io/android-gradle-dsl/current/com.android.build.gradle.internal.dsl.ProductFlavor.html#com.android.build.gradle.internal.dsl.ProductFlavor:buildConfigField(java.lang.String,%20java.lang.String,%20java.lang.String).
This mechanism will be replaced with the Gradle native version as we
transition to Gradle.
'''

from __future__ import (
    print_function,
    unicode_literals,
)

from collections import defaultdict
import os
import sys

import buildconfig
from mozbuild.preprocessor import Preprocessor


def main(output_file, input_filename):
    # input_filename is an absolute path, so there's no need to join with __DIR__.
    pp = Preprocessor(defines=buildconfig.defines, marker='//#')

    CONFIG = defaultdict(lambda: None)
    CONFIG.update(buildconfig.substs)
    DEFINES = {}

    for var in ('MOZ_ANDROID_ANR_REPORTER',
                'MOZ_ANDROID_BEAM',
                'MOZ_ANDROID_CUSTOM_TABS',
                'MOZ_ANDROID_DOWNLOADS_INTEGRATION',
                'MOZ_ANDROID_DOWNLOAD_CONTENT_SERVICE',
                'MOZ_ANDROID_EXCLUDE_FONTS',
                'MOZ_ANDROID_GCM',
                'MOZ_ANDROID_MLS_STUMBLER',
                'MOZ_ANDROID_SEARCH_ACTIVITY',
                'MOZ_DEBUG',
                'MOZ_INSTALL_TRACKING',
                'MOZ_LOCALE_SWITCHER',
                'MOZ_NATIVE_DEVICES',
                'MOZ_SWITCHBOARD'):
        if CONFIG[var]:
            DEFINES[var] = 1

    for var in ('MOZ_ANDROID_GCM_SENDERID',
                'MOZ_PKG_SPECIAL',
                'MOZ_UPDATER'):
        if CONFIG[var]:
            DEFINES[var] = CONFIG[var]

    for var in ('ANDROID_CPU_ARCH',
                'ANDROID_PACKAGE_NAME',
                'GRE_MILESTONE',
                'MOZ_ANDROID_APPLICATION_CLASS',
                'MOZ_ANDROID_BROWSER_INTENT_CLASS',
                'MOZ_ANDROID_SEARCH_INTENT_CLASS',
                'MOZ_APP_BASENAME',
                'MOZ_APP_DISPLAYNAME',
                'MOZ_APP_ID',
                'MOZ_APP_NAME',
                'MOZ_APP_UA_NAME',
                'MOZ_APP_VENDOR',
                'MOZ_APP_VERSION',
                'MOZ_CHILD_PROCESS_NAME',
                'MOZ_CRASHREPORTER',
                'MOZ_MOZILLA_API_KEY',
                'MOZ_UPDATE_CHANNEL',
                'OMNIJAR_NAME',
                'OS_TARGET',
                'TARGET_XPCOM_ABI'):
        DEFINES[var] = CONFIG[var]

    # Mangle our package name to avoid Bug 750548.
    DEFINES['MANGLED_ANDROID_PACKAGE_NAME'] = CONFIG['ANDROID_PACKAGE_NAME'].replace('fennec', 'f3nn3c')
    DEFINES['MOZ_APP_ABI'] = CONFIG['TARGET_XPCOM_ABI']
    if not CONFIG['COMPILE_ENVIRONMENT']:
        # These should really come from the included binaries, but that's not easy.
        DEFINES['MOZ_APP_ABI'] = 'arm-eabi-gcc3'
        DEFINES['TARGET_XPCOM_ABI'] = 'arm-eabi-gcc3'

    if '-march=armv7' in CONFIG['OS_CFLAGS']:
        DEFINES['MOZ_MIN_CPU_VERSION'] = 7
    else:
        DEFINES['MOZ_MIN_CPU_VERSION'] = 5

    # It's okay to use MOZ_ADJUST_SDK_KEY here because this doesn't
    # leak the value to build logs.
    if CONFIG['MOZ_INSTALL_TRACKING']:
        DEFINES['MOZ_INSTALL_TRACKING_ADJUST_SDK_APP_TOKEN'] = CONFIG['MOZ_ADJUST_SDK_KEY']

    # TODO: mark buildid.h as a dependency?  How about the buildconfig itself?
    DEFINES['MOZ_BUILDID'] = open(os.path.join(buildconfig.topobjdir, 'buildid.h')).readline().split()[2]

    pp.context.update(DEFINES)

    with open(input_filename, 'rU') as input:
        pp.processFile(input=input, output=output_file)

    return 0


if __name__ == '__main__':
    sys.exit(main(sys.stdout, *sys.argv[1:]))
