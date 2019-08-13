# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this,
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import errno
import os
import stat
import subprocess
import sys

# We need the NDK version in multiple different places, and it's inconvenient
# to pass down the NDK version to all relevant places, so we have this global
# variable.
NDK_VERSION = 'r17b'

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
Paste the lines between the chevrons (>>> and <<<) into your
$topsrcdir/mozconfig file, or create the file if it does not exist:

>>>
# Build GeckoView/Firefox for Android:
ac_add_options --enable-application=mobile/android

# Targeting the following architecture.
# For regular phones, no --target is needed.
# For x86 emulators (and x86 devices, which are uncommon):
# ac_add_options --target=i686
# For newer phones.
# ac_add_options --target=aarch64
# For x86_64 emulators (and x86_64 devices, which are even less common):
# ac_add_options --target=x86_64

{extra_lines}
<<<
'''

MOBILE_ANDROID_ARTIFACT_MODE_MOZCONFIG_TEMPLATE = '''
Paste the lines between the chevrons (>>> and <<<) into your
$topsrcdir/mozconfig file, or create the file if it does not exist:

>>>
# Build GeckoView/Firefox for Android Artifact Mode:
ac_add_options --enable-application=mobile/android
ac_add_options --target=arm-linux-androideabi
ac_add_options --enable-artifact-builds

{extra_lines}
# Write build artifacts to:
mk_add_options MOZ_OBJDIR=./objdir-frontend
<<<
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
                              os.path.join(mozbuild_path, 'android-ndk-{0}'.format(NDK_VERSION)))
    return (mozbuild_path, sdk_path, ndk_path)


def sdkmanager_tool(sdk_path):
    # sys.platform is win32 even if Python/Win64.
    sdkmanager = 'sdkmanager.bat' if sys.platform.startswith('win') else 'sdkmanager'
    return os.path.join(sdk_path, 'tools', 'bin', sdkmanager)


def ensure_dir(dir):
    '''Ensures the given directory exists'''
    if dir and not os.path.exists(dir):
        try:
            os.makedirs(dir)
        except OSError as error:
            if error.errno != errno.EEXIST:
                raise


def ensure_android(os_name, artifact_mode=False, ndk_only=False, no_interactive=False):
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
    sdk_url = 'https://dl.google.com/android/repository/sdk-tools-{0}-4333796.zip'.format(os_tag)
    ndk_url = android_ndk_url(os_name)

    ensure_android_sdk_and_ndk(mozbuild_path, os_name,
                               sdk_path=sdk_path, sdk_url=sdk_url,
                               ndk_path=ndk_path, ndk_url=ndk_url,
                               artifact_mode=artifact_mode,
                               ndk_only=ndk_only)

    if ndk_only:
        return

    # We expect the |sdkmanager| tool to be at
    # ~/.mozbuild/android-sdk-$OS_NAME/tools/bin/sdkmanager.
    ensure_android_packages(sdkmanager_tool=sdkmanager_tool(sdk_path),
                            no_interactive=no_interactive)


def ensure_android_sdk_and_ndk(mozbuild_path, os_name, sdk_path, sdk_url, ndk_path, ndk_url,
                               artifact_mode, ndk_only):
    '''
    Ensure the Android SDK and NDK are found at the given paths.  If not, fetch
    and unpack the SDK and/or NDK from the given URLs into
    |mozbuild_path/{android-sdk-$OS_NAME,android-ndk-$VER}|.
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

    if ndk_only:
        return

    # We don't want to blindly overwrite, since we use the
    # |sdkmanager| tool to install additional parts of the Android
    # toolchain.  If we overwrite, we lose whatever Android packages
    # the user may have already installed.
    if os.path.isfile(sdkmanager_tool(sdk_path)):
        print(ANDROID_SDK_EXISTS % sdk_path)
    elif os.path.isdir(sdk_path):
        raise NotImplementedError(ANDROID_SDK_TOO_OLD % sdk_path)
    else:
        # The SDK archive used to include a top-level
        # android-sdk-$OS_NAME directory; it no longer does so.  We
        # preserve the old convention to smooth detecting existing SDK
        # installations.
        install_mobile_android_sdk_or_ndk(sdk_url, os.path.join(mozbuild_path,
                                                                'android-sdk-{0}'.format(os_name)))


def get_packages_to_install(packages_file_name):
    """
    sdkmanager version 26.1.1 (current) and some versions below have a bug that makes
    the following command fail:
        args = [sdkmanager_tool, '--package_file={0}'.format(package_file_name)]
        subprocess.check_call(args)
    The error is in the sdkmanager, where the --package_file param isn't recognized.
        The error is being tracked here https://issuetracker.google.com/issues/66465833
    Meanwhile, this workaround achives installing all required Android packages by reading
    them out of the same file that --package_file would have used, and passing them as strings.
    So from here: https://developer.android.com/studio/command-line/sdkmanager
    Instead of:
        sdkmanager --package_file=package_file [options]
    We're doing:
        sdkmanager "platform-tools" "platforms;android-26"
    """
    with open(packages_file_name) as package_file:
        return map(lambda package: package.strip(), package_file.readlines())


def ensure_android_packages(sdkmanager_tool, packages=None, no_interactive=False):
    '''
    Use the given sdkmanager tool (like 'sdkmanager') to install required
    Android packages.
    '''

    # This tries to install all the required Android packages.  The user
    # may be prompted to agree to the Android license.
    package_file_name = os.path.abspath(os.path.join(os.path.dirname(__file__),
                                                     'android-packages.txt'))
    print(INSTALLING_ANDROID_PACKAGES % open(package_file_name, 'rt').read())

    args = [sdkmanager_tool]
    args.extend(get_packages_to_install(package_file_name))

    if not no_interactive:
        subprocess.check_call(args)
        return

    # Emulate yes.  For a discussion of passing input to check_output,
    # see https://stackoverflow.com/q/10103551.
    yes = '\n'.join(['y']*100)
    proc = subprocess.Popen(args,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT,
                            stdin=subprocess.PIPE)
    output, unused_err = proc.communicate(yes)

    retcode = proc.poll()
    if retcode:
        cmd = args[0]
        e = subprocess.CalledProcessError(retcode, cmd)
        e.output = output
        raise e

    print(output)


def suggest_mozconfig(os_name, artifact_mode=False):
    moz_state_dir, sdk_path, ndk_path = get_paths(os_name)

    extra_lines = []
    if extra_lines:
        extra_lines.append('')

    if artifact_mode:
        template = MOBILE_ANDROID_ARTIFACT_MODE_MOZCONFIG_TEMPLATE
    else:
        template = MOBILE_ANDROID_MOZCONFIG_TEMPLATE

    kwargs = dict(
        sdk_path=sdk_path,
        ndk_path=ndk_path,
        moz_state_dir=moz_state_dir,
        extra_lines='\n'.join(extra_lines),
    )
    print(template.format(**kwargs))


def android_ndk_url(os_name, ver=NDK_VERSION):
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
    import optparse  # No argparse, which is new in Python 2.7.
    import platform

    parser = optparse.OptionParser()
    parser.add_option('-a', '--artifact-mode', dest='artifact_mode', action='store_true',
                      help='If true, install only the Android SDK (and not the Android NDK).')
    parser.add_option('--ndk-only', dest='ndk_only', action='store_true',
                      help='If true, install only the Android NDK (and not the Android SDK).')
    parser.add_option('--no-interactive', dest='no_interactive', action='store_true',
                      help='Accept the Android SDK licenses without user interaction.')

    options, _ = parser.parse_args(argv)

    if options.artifact_mode and options.ndk_only:
        raise NotImplementedError('Use no options to install the NDK and the SDK.')

    os_name = None
    if platform.system() == 'Darwin':
        os_name = 'macosx'
    elif platform.system() == 'Linux':
        os_name = 'linux'
    elif platform.system() == 'Windows':
        os_name = 'windows'
    else:
        raise NotImplementedError("We don't support bootstrapping the Android SDK (or Android "
                                  "NDK) on {0} yet!".format(platform.system()))

    ensure_android(os_name, artifact_mode=options.artifact_mode,
                   ndk_only=options.ndk_only,
                   no_interactive=options.no_interactive)
    suggest_mozconfig(os_name, options.artifact_mode)

    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
