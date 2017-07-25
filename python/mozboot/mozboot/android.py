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

ANDROID_NDK_EXISTS = '''
Looks like you have the Android NDK installed at:
%s
'''

ANDROID_SDK_EXISTS = '''
Looks like you have the Android SDK installed at:
%s
We will install all required Android packages.
'''

ANDROID_SDK_TOO_OLD = '''
Looks like you have an outdated Android SDK installed at:
%s
I can't update outdated Android SDKs to have the required 'sdkmanager'
tool.  Move it out of the way (or remove it entirely) and then run
bootstrap again.
'''

INSTALLING_ANDROID_PACKAGES = '''
We are now installing the following Android packages:
%s
You may be prompted to agree to the Android license. You may see some of
output as packages are downloaded and installed.
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

MOBILE_ANDROID_ARTIFACT_MODE_MOZCONFIG_TEMPLATE = '''
Paste the lines between the chevrons (>>> and <<<) into your mozconfig file:

<<<
# Build Firefox for Android Artifact Mode:
ac_add_options --enable-application=mobile/android
ac_add_options --target=arm-linux-androideabi
ac_add_options --enable-artifact-builds

# With the following Android SDK:
ac_add_options --with-android-sdk="%s"

# Write build artifacts to:
mk_add_options MOZ_OBJDIR=./objdir-frontend
>>>
'''


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
            cmd = ['tar', 'zxf', abspath]
        elif file.endswith('.tar.bz2'):
            cmd = ['tar', 'jxf', abspath]
        elif file.endswith('.zip'):
            cmd = ['unzip', '-q', abspath]
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


def get_paths(os_name):
    mozbuild_path = os.environ.get('MOZBUILD_STATE_PATH',
                                   os.path.expanduser(os.path.join('~', '.mozbuild')))
    sdk_path = os.environ.get('ANDROID_SDK_HOME',
                              os.path.join(mozbuild_path, 'android-sdk-{0}'.format(os_name)))
    ndk_path = os.environ.get('ANDROID_NDK_HOME',
                              os.path.join(mozbuild_path, 'android-ndk-r11c'))
    return (mozbuild_path, sdk_path, ndk_path)


def ensure_dir(dir):
    '''Ensures the given directory exists'''
    if dir and not os.path.exists(dir):
        try:
            os.makedirs(dir)
        except OSError as error:
            if error.errno != errno.EEXIST:
                raise


def ensure_android(os_name, artifact_mode=False, no_interactive=False):
    '''
    Ensure the Android SDK (and NDK, if `artifact_mode` is falsy) are
    installed.  If not, fetch and unpack the SDK and/or NDK from the
    given URLs.  Ensure the required Android SDK packages are
    installed.

    `os_name` can be 'linux' or 'macosx'.
    '''
    # The user may have an external Android SDK (in which case we
    # save them a lengthy download), or they may have already
    # completed the download. We unpack to
    # ~/.mozbuild/{android-sdk-$OS_NAME, android-ndk-$VER}.
    mozbuild_path, sdk_path, ndk_path = get_paths(os_name)
    os_tag = 'darwin' if os_name == 'macosx' else os_name
    sdk_url = 'https://dl.google.com/android/repository/sdk-tools-{0}-3859397.zip'.format(os_tag)
    ndk_url = android_ndk_url(os_name)

    ensure_android_sdk_and_ndk(mozbuild_path, os_name,
                               sdk_path=sdk_path, sdk_url=sdk_url,
                               ndk_path=ndk_path, ndk_url=ndk_url,
                               artifact_mode=artifact_mode)

    if no_interactive:
        # Cribbed from observation and https://stackoverflow.com/a/38381577.
        path = os.path.join(mozbuild_path, 'android-sdk-{0}'.format(os_name), 'licenses')
        ensure_dir(path)

        licenses = {
            'android-sdk-license': '8933bad161af4178b1185d1a37fbf41ea5269c55',
            'android-sdk-preview-license': '84831b9409646a918e30573bab4c9c91346d8abd',
        }
        for license, tag in licenses.items():
            lname = os.path.join(path, license)
            if not os.path.isfile(lname):
                open(lname, 'w').write('\n{0}\n'.format(tag))


    # We expect the |sdkmanager| tool to be at
    # ~/.mozbuild/android-sdk-$OS_NAME/tools/bin/sdkmanager.
    sdkmanager_tool = os.path.join(sdk_path, 'tools', 'bin', 'sdkmanager')
    ensure_android_packages(sdkmanager_tool=sdkmanager_tool)


def ensure_android_sdk_and_ndk(mozbuild_path, os_name, sdk_path, sdk_url, ndk_path, ndk_url, artifact_mode):
    '''
    Ensure the Android SDK and NDK are found at the given paths.  If not, fetch
    and unpack the SDK and/or NDK from the given URLs into |mozbuild_path/{android-sdk-$OS_NAME,android-ndk-$VER}|.
    '''

    # It's not particularly bad to overwrite the NDK toolchain, but it does take
    # a while to unpack, so let's avoid the disk activity if possible.  The SDK
    # may prompt about licensing, so we do this first.
    # Check for Android NDK only if we are not in artifact mode.
    if not artifact_mode:
        if os.path.isdir(ndk_path):
            print(ANDROID_NDK_EXISTS % ndk_path)
        else:
            # The NDK archive unpacks into a top-level android-ndk-$VER directory.
            install_mobile_android_sdk_or_ndk(ndk_url, mozbuild_path)

    # We don't want to blindly overwrite, since we use the
    # |sdkmanager| tool to install additional parts of the Android
    # toolchain.  If we overwrite, we lose whatever Android packages
    # the user may have already installed.
    if os.path.isfile(os.path.join(sdk_path, 'tools', 'bin', 'sdkmanager')):
        print(ANDROID_SDK_EXISTS % sdk_path)
    elif os.path.isdir(sdk_path):
        raise NotImplementedError(ANDROID_SDK_TOO_OLD % sdk_path)
    else:
        # The SDK archive used to include a top-level
        # android-sdk-$OS_NAME directory; it no longer does so.  We
        # preserve the old convention to smooth detecting existing SDK
        # installations.
        install_mobile_android_sdk_or_ndk(sdk_url, os.path.join(mozbuild_path, 'android-sdk-{0}'.format(os_name)))


def ensure_android_packages(sdkmanager_tool, packages=None):
    '''
    Use the given sdkmanager tool (like 'sdkmanager') to install required
    Android packages.
    '''

    # This tries to install all the required Android packages.  The user
    # may be prompted to agree to the Android license.
    package_file_name = os.path.abspath(os.path.join(os.path.dirname(__file__), 'android-packages.txt'))
    print(INSTALLING_ANDROID_PACKAGES % open(package_file_name, 'rt').read())
    subprocess.check_call([sdkmanager_tool,
                           '--package_file={0}'.format(package_file_name)])


def suggest_mozconfig(os_name, artifact_mode=False):
    _mozbuild_path, sdk_path, ndk_path = get_paths(os_name)
    if artifact_mode:
        print(MOBILE_ANDROID_ARTIFACT_MODE_MOZCONFIG_TEMPLATE % (sdk_path))
    else:
        print(MOBILE_ANDROID_MOZCONFIG_TEMPLATE % (sdk_path, ndk_path))


def android_ndk_url(os_name, ver='r11c'):
    # Produce a URL like
    # 'https://dl.google.com/android/repository/android-ndk-$VER-linux-x86_64.zip
    base_url = 'https://dl.google.com/android/repository/android-ndk'

    if os_name == 'macosx':
        # |mach bootstrap| uses 'macosx', but Google uses 'darwin'.
        os_name = 'darwin'

    if sys.maxsize > 2**32:
        arch = 'x86_64'
    else:
        arch = 'x86'

    return '%s-%s-%s-%s.zip' % (base_url, ver, os_name, arch)


def main(argv):
    import optparse # No argparse, which is new in Python 2.7.
    import platform

    parser = optparse.OptionParser()
    parser.add_option('-a', '--artifact-mode', dest='artifact_mode', action='store_true',
                      help='If true, install only the Android SDK (and not the Android NDK).')
    parser.add_option('--no-interactive', dest='no_interactive', action='store_true',
                      help='Accept the Android SDK licenses without user interaction.')

    options, _ = parser.parse_args(argv)

    os_name = None
    if platform.system() == 'Darwin':
        os_name = 'macosx'
    elif platform.system() == 'Linux':
        os_name = 'linux'
    elif platform.system() == 'Windows':
        os_name = 'windows'
    else:
        raise NotImplementedError("We don't support bootstrapping the Android SDK (or Android NDK) "
                                  "on {0} yet!".format(platform.system()))

    ensure_android(os_name, artifact_mode=options.artifact_mode, no_interactive=options.no_interactive)
    suggest_mozconfig(os_name, options.artifact_mode)

    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
