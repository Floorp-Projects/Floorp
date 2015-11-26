# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this,
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# If we add unicode_literals, Python 2.6.1 (required for OS X 10.6) breaks.
from __future__ import print_function

import errno
import os
import stat
import subprocess
import sys

# These are the platform and build-tools versions for building
# mobile/android, respectively. Try to keep these in synch with the
# build system and Mozilla's automation.
ANDROID_TARGET_SDK = '23'
ANDROID_BUILD_TOOLS_VERSION = '23.0.1'

# These are the "Android packages" needed for building Firefox for Android.
# Use |android list sdk --extended| to see these identifiers.
ANDROID_PACKAGES = [
    'tools',
    'platform-tools',
    'build-tools-%s' % ANDROID_BUILD_TOOLS_VERSION,
    'android-%s' % ANDROID_TARGET_SDK,
    'extra-google-m2repository',
    'extra-android-m2repository',
]

ANDROID_NDK_EXISTS = '''
Looks like you have the Android NDK installed at:
%s
'''

ANDROID_SDK_EXISTS = '''
Looks like you have the Android SDK installed at:
%s
We will install all required Android packages.
'''

NOT_INSTALLING_ANDROID_PACKAGES = '''
It looks like you already have the following Android packages:
%s
No need to update!
'''

INSTALLING_ANDROID_PACKAGES = '''
We are now installing the following Android packages:
%s
You may be prompted to agree to the Android license. You may see some of
output as packages are downloaded and installed.
'''

MISSING_ANDROID_PACKAGES = '''
We tried to install the following Android packages:
%s
But it looks like we couldn't install:
%s
Install these Android packages manually and run this bootstrapper again.
'''

MOBILE_ANDROID_MOZCONFIG_TEMPLATE = '''
Paste the lines between the chevrons (>>> and <<<) into your mozconfig file:

<<<
# Build Firefox for Android:
ac_add_options --enable-application=mobile/android
ac_add_options --target=arm-linux-androideabi

# With the following Android SDK and NDK:
ac_add_options --with-android-sdk="%s"
ac_add_options --with-android-ndk="%s"
>>>
'''


def check_output(*args, **kwargs):
    """Run subprocess.check_output even if Python doesn't provide it."""
    from base import BaseBootstrapper
    fn = getattr(subprocess, 'check_output', BaseBootstrapper._check_output)

    return fn(*args, **kwargs)


def list_missing_android_packages(android_tool, packages):
    '''
    Use the given |android| tool to return the sub-list of Android
    |packages| given that are not installed.
    '''
    missing = []

    # There's no obvious way to see what's been installed already,
    # but packages that are installed don't appear in the list of
    # available packages.
    lines = check_output([android_tool,
                          'list', 'sdk', '--no-ui', '--extended']).splitlines()

    # Lines look like: 'id: 59 or "extra-google-simulators"'
    for line in lines:
        is_id_line = False
        try:
            is_id_line = line.startswith("id:")
        except:
            # Some lines contain non-ASCII characters.  Ignore them.
            pass
        if not is_id_line:
            continue

        for package in packages:
            if '"%s"' % package in line:
                # Not installed!
                missing.append(package)

    return missing


def install_mobile_android_sdk_or_ndk(url, path):
    '''
    Fetch an Android SDK or NDK from |url| and unpack it into
    the given |path|.

    We expect wget to be installed and found on the system path.

    We use, and wget respects, https.  We could also include SHAs for a
    small improvement in the integrity guarantee we give. But this script is
    bootstrapped over https anyway, so it's a really minor improvement.

    We use |wget --continue| as a cheap cache of the downloaded artifacts,
    writing into |path|/mozboot.  We don't yet clean the cache; it's better
    to waste disk and not require a long re-download than to wipe the cache
    prematurely.
    '''

    old_path = os.getcwd()
    try:
        download_path = os.path.join(path, 'mozboot')
        try:
            os.makedirs(download_path)
        except OSError as e:
            if e.errno == errno.EEXIST and os.path.isdir(download_path):
                pass
            else:
                raise

        os.chdir(download_path)
        subprocess.check_call(['wget', '--continue', url])
        file = url.split('/')[-1]

        os.chdir(path)
        abspath = os.path.join(download_path, file)
        if file.endswith('.tar.gz') or file.endswith('.tgz'):
            cmd = ['tar', 'zvxf', abspath]
        elif file.endswith('.tar.bz2'):
            cmd = ['tar', 'jvxf', abspath]
        elif file.endswith('.zip'):
            cmd = ['unzip', abspath]
        elif file.endswith('.bin'):
            # Execute the .bin file, which unpacks the content.
            mode = os.stat(path).st_mode
            os.chmod(abspath, mode | stat.S_IXUSR)
            cmd = [abspath]
        else:
            raise NotImplementedError("Don't know how to unpack file: %s" % file)

        print('Unpacking %s...' % abspath)

        with open(os.devnull, "w") as stdout:
            # These unpack commands produce a ton of output; ignore it.  The
            # .bin files are 7z archives; there's no command line flag to quiet
            # output, so we use this hammer.
            subprocess.check_call(cmd, stdout=stdout)

        print('Unpacking %s... DONE' % abspath)

    finally:
        os.chdir(old_path)


def ensure_android_sdk_and_ndk(path, sdk_path, sdk_url, ndk_path, ndk_url):
    '''
    Ensure the Android SDK and NDK are found at the given paths.  If not, fetch
    and unpack the SDK and/or NDK from the given URLs into |path|.
    '''

    # It's not particularyl bad to overwrite the NDK toolchain, but it does take
    # a while to unpack, so let's avoid the disk activity if possible.  The SDK
    # may prompt about licensing, so we do this first.
    if os.path.isdir(ndk_path):
        print(ANDROID_NDK_EXISTS % ndk_path)
    else:
        install_mobile_android_sdk_or_ndk(ndk_url, path)

    # We don't want to blindly overwrite, since we use the |android| tool to
    # install additional parts of the Android toolchain.  If we overwrite,
    # we lose whatever Android packages the user may have already installed.
    if os.path.isdir(sdk_path):
        print(ANDROID_SDK_EXISTS % sdk_path)
    else:
        install_mobile_android_sdk_or_ndk(sdk_url, path)


def ensure_android_packages(android_tool, packages=None):
    '''
    Use the given android tool (like 'android') to install required Android
    packages.
    '''

    if not packages:
        packages = ANDROID_PACKAGES

    # Bug 1171232: The |android| tool behaviour has changed; we no longer can
    # see what packages are installed easily.  Force installing everything until
    # we find a way to actually see the missing packages.
    missing = packages
    if not missing:
        print(NOT_INSTALLING_ANDROID_PACKAGES % ', '.join(packages))
        return

    # This tries to install all the required Android packages.  The user
    # may be prompted to agree to the Android license.
    print(INSTALLING_ANDROID_PACKAGES % ', '.join(missing))
    subprocess.check_call([android_tool,
                           'update', 'sdk', '--no-ui', '--all',
                           '--filter', ','.join(missing)])

    # Bug 1171232: The |android| tool behaviour has changed; we no longer can
    # see what packages are installed easily.  Don't check until we find a way
    # to actually verify.
    failing = []
    if failing:
        raise Exception(MISSING_ANDROID_PACKAGES % (', '.join(missing), ', '.join(failing)))


def suggest_mozconfig(sdk_path=None, ndk_path=None):
    print(MOBILE_ANDROID_MOZCONFIG_TEMPLATE % (sdk_path, ndk_path))


def android_ndk_url(os_name, ver='r10e'):
    # Produce a URL like 'https://dl.google.com/android/ndk/android-ndk-r10e-linux-x86_64.bin'.
    base_url = 'https://dl.google.com/android/ndk/android-ndk'

    if sys.maxsize > 2**32:
        arch = 'x86_64'
    else:
        arch = 'x86'

    return '%s-%s-%s-%s.bin' % (base_url, ver, os_name, arch)
