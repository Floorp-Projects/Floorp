# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

import argparse
import sys


def android_version_code(buildid, cpu_arch=None, min_sdk=0, max_sdk=0):
    base = int(str(buildid)[:10])
    # None is interpreted as arm.
    if not cpu_arch or cpu_arch in ['armeabi', 'armeabi-v7a']:
        # Increment by MIN_SDK_VERSION -- this adds 9 to every build ID as a
        # minimum.  Our split APK starts at 11.
        return base + min_sdk + 0
    elif cpu_arch in ['x86']:
        # Increment the version code by 3 for x86 builds so they are offered to
        # x86 phones that have ARM emulators, beating the 2-point advantage that
        # the v11+ ARMv7 APK has.  If we change our splits in the future, we'll
        # need to do this further still.
        return base + min_sdk + 3
    else:
        raise ValueError("Don't know how to compute android:versionCode "
                         "for CPU arch " + cpu_arch)


def main(argv):
    parser = argparse.ArgumentParser('Generate an android:versionCode',
                                     add_help=False)
    parser.add_argument('--verbose', action='store_true',
                        default=False,
                        help='Be verbose')
    parser.add_argument('--with-android-cpu-arch', dest='cpu_arch',
                        choices=['armeabi', 'armeabi-v7a', 'mips', 'x86'],
                        help='The target CPU architecture')
    parser.add_argument('--with-android-min-sdk-version', dest='min_sdk',
                        type=int, default=0,
                        help='The minimum target SDK')
    parser.add_argument('--with-android-max-sdk-version', dest='max_sdk',
                        type=int, default=0,
                        help='The maximum target SDK')
    parser.add_argument('buildid', type=int,
                        help='The input build ID')

    args = parser.parse_args(argv)
    code = android_version_code(args.buildid,
        cpu_arch=args.cpu_arch,
        min_sdk=args.min_sdk,
        max_sdk=args.max_sdk)
    print(code)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
