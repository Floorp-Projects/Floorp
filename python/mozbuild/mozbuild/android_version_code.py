# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

import argparse
import math
import sys
import time

# Builds before this build ID use the v0 version scheme.  Builds after this
# build ID use the v1 version scheme.
V1_CUTOFF = 20150801000000 # YYYYmmddHHMMSS

def android_version_code_v0(buildid, cpu_arch=None, min_sdk=0, max_sdk=0):
    base = int(str(buildid)[:10])
    # None is interpreted as arm.
    if not cpu_arch or cpu_arch in ['armeabi', 'armeabi-v7a']:
        # Increment by MIN_SDK_VERSION -- this adds 9 to every build ID as a
        # minimum.  Our split APK starts at 15.
        return base + min_sdk + 0
    elif cpu_arch in ['x86']:
        # Increment the version code by 3 for x86 builds so they are offered to
        # x86 phones that have ARM emulators, beating the 2-point advantage that
        # the v15+ ARMv7 APK has.  If we change our splits in the future, we'll
        # need to do this further still.
        return base + min_sdk + 3
    else:
        raise ValueError("Don't know how to compute android:versionCode "
                         "for CPU arch %s" % cpu_arch)

def android_version_code_v1(buildid, cpu_arch=None, min_sdk=0, max_sdk=0):
    '''Generate a v1 android:versionCode.

    The important consideration is that version codes be monotonically
    increasing (per Android package name) for all published builds.  The input
    build IDs are based on timestamps and hence are always monotonically
    increasing.

    The generated v1 version codes look like (in binary):

    0111 1000 0010 tttt tttt tttt tttt txpg

    The 17 bits labelled 't' represent the number of hours since midnight on
    September 1, 2015.  (2015090100 in YYYYMMMDDHH format.)  This yields a
    little under 15 years worth of hourly build identifiers, since 2**17 / (366
    * 24) =~ 14.92.

    The bits labelled 'x', 'p', and 'g' are feature flags.

    The bit labelled 'x' is 1 if the build is for an x86 architecture and 0
    otherwise, which means the build is for an ARM architecture.  (Fennec no
    longer supports ARMv6, so ARM is equivalent to ARMv7 and above.)

    The bit labelled 'p' is a placeholder that is always 0 (for now).

    Firefox no longer supports API 14 or earlier.

    This version code computation allows for a split on API levels that allowed
    us to ship builds specifically for Gingerbread (API 9-10); we preserve
    that functionality for sanity's sake, and to allow us to reintroduce a
    split in the future.

    At present, the bit labelled 'g' is 1 if the build is an ARM build
    targeting API 15+, which will always be the case.

    We throw an explanatory exception when we are within one calendar year of
    running out of build events.  This gives lots of time to update the version
    scheme.  The responsible individual should then bump the range (to allow
    builds to continue) and use the time remaining to update the version scheme
    via the reserved high order bits.

    N.B.: the reserved 0 bit to the left of the highest order 't' bit can,
    sometimes, be used to bump the version scheme.  In addition, by reducing the
    granularity of the build identifiers (for example, moving to identifying
    builds every 2 or 4 hours), the version scheme may be adjusted further still
    without losing a (valuable) high order bit.
    '''
    def hours_since_cutoff(buildid):
        # The ID is formatted like YYYYMMDDHHMMSS (using
        # datetime.now().strftime('%Y%m%d%H%M%S'); see build/variables.py).
        # The inverse function is time.strptime.
        # N.B.: the time module expresses time as decimal seconds since the
        # epoch.
        fmt = '%Y%m%d%H%M%S'
        build = time.strptime(str(buildid), fmt)
        cutoff = time.strptime(str(V1_CUTOFF), fmt)
        return int(math.floor((time.mktime(build) - time.mktime(cutoff)) / (60.0 * 60.0)))

    # Of the 21 low order bits, we take 17 bits for builds.
    base = hours_since_cutoff(buildid)
    if base < 0:
        raise ValueError("Something has gone horribly wrong: cannot calculate "
                         "android:versionCode from build ID %s: hours underflow "
                         "bits allotted!" % buildid)
    if base > 2**17:
        raise ValueError("Something has gone horribly wrong: cannot calculate "
                         "android:versionCode from build ID %s: hours overflow "
                         "bits allotted!" % buildid)
    if base > 2**17 - 366 * 24:
        raise ValueError("Running out of low order bits calculating "
                         "android:versionCode from build ID %s: "
                         "; YOU HAVE ONE YEAR TO UPDATE THE VERSION SCHEME." % buildid)

    version = 0b1111000001000000000000000000000
    # We reserve 1 "middle" high order bit for the future, and 3 low order bits
    # for architecture and APK splits.
    version |= base << 3

    # None is interpreted as arm.
    if not cpu_arch or cpu_arch in ['armeabi', 'armeabi-v7a']:
        # 0 is interpreted as SDK 9.
        if not min_sdk or min_sdk == 9:
            pass
        # This used to compare to 11. The 15+ APK directly supersedes 11+, so
        # we reuse this check.
        elif min_sdk == 15:
            version |= 1 << 0
        else:
            raise ValueError("Don't know how to compute android:versionCode "
                             "for CPU arch %s and min SDK %s" % (cpu_arch, min_sdk))
    elif cpu_arch in ['x86']:
        version |= 1 << 2
    else:
        raise ValueError("Don't know how to compute android:versionCode "
                         "for CPU arch %s" % cpu_arch)

    return version

def android_version_code(buildid, *args, **kwargs):
    base = int(str(buildid))
    if base < V1_CUTOFF:
        return android_version_code_v0(buildid, *args, **kwargs)
    else:
        return android_version_code_v1(buildid, *args, **kwargs)


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
