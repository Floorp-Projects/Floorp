# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this,
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import errno
import json
import os
import stat
import subprocess
import sys
import time

# We need the NDK version in multiple different places, and it's inconvenient
# to pass down the NDK version to all relevant places, so we have this global
# variable.
from mozboot.bootstrap import MOZCONFIG_SUGGESTION_TEMPLATE

NDK_VERSION = "r21d"
CMDLINE_TOOLS_VERSION_STRING = "4.0"
CMDLINE_TOOLS_VERSION = "7302050"

# We expect the emulator AVD definitions to be platform agnostic
LINUX_X86_64_ANDROID_AVD = "linux64-android-avd-x86_64-repack"
LINUX_ARM_ANDROID_AVD = "linux64-android-avd-arm-repack"

MACOS_X86_64_ANDROID_AVD = "linux64-android-avd-x86_64-repack"
MACOS_ARM_ANDROID_AVD = "linux64-android-avd-arm-repack"

# We don't currently support bootstrapping on Windows yet.
# WINDOWS_X86_64_ANDROID_AVD = "linux64-android-avd-x86_64-repack"
# WINDOWS_ARM_ANDROID_AVD = "linux64-android-avd-arm-repack"

AVD_MANIFEST_X86_64 = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "android-avds/x86_64.json")
)
AVD_MANIFEST_ARM = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "android-avds/arm.json")
)

ANDROID_NDK_EXISTS = """
Looks like you have the correct version of the Android NDK installed at:
%s
"""

ANDROID_SDK_EXISTS = """
Looks like you have the Android SDK installed at:
%s
We will install all required Android packages.
"""

ANDROID_SDK_TOO_OLD = """
Looks like you have an outdated Android SDK installed at:
%s
I can't update outdated Android SDKs to have the required 'sdkmanager'
tool.  Move it out of the way (or remove it entirely) and then run
bootstrap again.
"""

INSTALLING_ANDROID_PACKAGES = """
We are now installing the following Android packages:
%s
You may be prompted to agree to the Android license. You may see some of
output as packages are downloaded and installed.
"""

MOBILE_ANDROID_MOZCONFIG_TEMPLATE = """
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
"""

MOBILE_ANDROID_ARTIFACT_MODE_MOZCONFIG_TEMPLATE = """
# Build GeckoView/Firefox for Android Artifact Mode:
ac_add_options --enable-application=mobile/android
ac_add_options --target=arm-linux-androideabi
ac_add_options --enable-artifact-builds

{extra_lines}
# Write build artifacts to:
mk_add_options MOZ_OBJDIR=./objdir-frontend
"""


class GetNdkVersionError(Exception):
    pass


def install_mobile_android_sdk_or_ndk(url, path):
    """
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
    """

    old_path = os.getcwd()
    try:
        download_path = os.path.join(path, "mozboot")
        try:
            os.makedirs(download_path)
        except OSError as e:
            if e.errno == errno.EEXIST and os.path.isdir(download_path):
                pass
            else:
                raise

        os.chdir(download_path)
        subprocess.check_call(["wget", "--continue", url])
        file = url.split("/")[-1]

        os.chdir(path)
        abspath = os.path.join(download_path, file)
        if file.endswith(".tar.gz") or file.endswith(".tgz"):
            cmd = ["tar", "zxf", abspath]
        elif file.endswith(".tar.bz2"):
            cmd = ["tar", "jxf", abspath]
        elif file.endswith(".zip"):
            cmd = ["unzip", "-q", abspath]
        elif file.endswith(".bin"):
            # Execute the .bin file, which unpacks the content.
            mode = os.stat(path).st_mode
            os.chmod(abspath, mode | stat.S_IXUSR)
            cmd = [abspath]
        else:
            raise NotImplementedError("Don't know how to unpack file: %s" % file)

        print("Unpacking %s..." % abspath)

        with open(os.devnull, "w") as stdout:
            # These unpack commands produce a ton of output; ignore it.  The
            # .bin files are 7z archives; there's no command line flag to quiet
            # output, so we use this hammer.
            subprocess.check_call(cmd, stdout=stdout)

        print("Unpacking %s... DONE" % abspath)
        # Now delete the archive
        os.unlink(abspath)
    finally:
        os.chdir(old_path)


def get_ndk_version(ndk_path):
    """Given the path to the NDK, return the version as a 3-tuple of (major,
    minor, human).
    """
    with open(os.path.join(ndk_path, "source.properties"), "r") as f:
        revision = [line for line in f if line.startswith("Pkg.Revision")]
        if not revision:
            raise GetNdkVersionError(
                "Cannot determine NDK version from source.properties"
            )
        if len(revision) != 1:
            raise GetNdkVersionError("Too many Pkg.Revision lines in source.properties")

        (_, version) = revision[0].split("=")
        if not version:
            raise GetNdkVersionError(
                "Unexpected Pkg.Revision line in source.properties"
            )

        (major, minor, revision) = version.strip().split(".")
        if not major or not minor:
            raise GetNdkVersionError("Unexpected NDK version string: " + version)

        # source.properties contains a $MAJOR.$MINOR.$PATCH revision number,
        # but the more common nomenclature that Google uses is alphanumeric
        # version strings like "r20" or "r19c".  Convert the source.properties
        # notation into an alphanumeric string.
        int_minor = int(minor)
        alphas = "abcdefghijklmnop"
        ascii_minor = alphas[int_minor] if int_minor > 0 else ""
        human = "r%s%s" % (major, ascii_minor)
        return (major, minor, human)


def get_paths(os_name):
    mozbuild_path = os.environ.get(
        "MOZBUILD_STATE_PATH", os.path.expanduser(os.path.join("~", ".mozbuild"))
    )
    sdk_path = os.environ.get(
        "ANDROID_SDK_HOME",
        os.path.join(mozbuild_path, "android-sdk-{0}".format(os_name)),
    )
    ndk_path = os.environ.get(
        "ANDROID_NDK_HOME",
        os.path.join(mozbuild_path, "android-ndk-{0}".format(NDK_VERSION)),
    )
    avd_home_path = os.environ.get(
        "ANDROID_AVD_HOME", os.path.join(mozbuild_path, "android-device", "avd")
    )
    emulator_path = os.environ.get(
        "ANDROID_EMULATOR_HOME", os.path.join(mozbuild_path, "android-device")
    )
    return (mozbuild_path, sdk_path, ndk_path, avd_home_path, emulator_path)


def sdkmanager_tool(sdk_path):
    # sys.platform is win32 even if Python/Win64.
    sdkmanager = "sdkmanager.bat" if sys.platform.startswith("win") else "sdkmanager"
    return os.path.join(
        sdk_path, "cmdline-tools", CMDLINE_TOOLS_VERSION_STRING, "bin", sdkmanager
    )


def avdmanager_tool(sdk_path):
    # sys.platform is win32 even if Python/Win64.
    sdkmanager = "avdmanager.bat" if sys.platform.startswith("win") else "avdmanager"
    return os.path.join(
        sdk_path, "cmdline-tools", CMDLINE_TOOLS_VERSION_STRING, "bin", sdkmanager
    )


def adb_tool(sdk_path):
    adb = "adb.bat" if sys.platform.startswith("win") else "adb"
    return os.path.join(sdk_path, "platform-tools", adb)


def emulator_tool(sdk_path):
    emulator = "emulator.bat" if sys.platform.startswith("win") else "emulator"
    return os.path.join(sdk_path, "emulator", emulator)


def ensure_dir(dir):
    """Ensures the given directory exists"""
    if dir and not os.path.exists(dir):
        try:
            os.makedirs(dir)
        except OSError as error:
            if error.errno != errno.EEXIST:
                raise


def ensure_android(
    os_name,
    artifact_mode=False,
    ndk_only=False,
    system_images_only=False,
    emulator_only=False,
    avd_manifest_path=None,
    prewarm_avd=False,
    no_interactive=False,
    list_packages=False,
):
    """
    Ensure the Android SDK (and NDK, if `artifact_mode` is falsy) are
    installed.  If not, fetch and unpack the SDK and/or NDK from the
    given URLs.  Ensure the required Android SDK packages are
    installed.

    `os_name` can be 'linux', 'macosx' or 'windows'.
    """
    # The user may have an external Android SDK (in which case we
    # save them a lengthy download), or they may have already
    # completed the download. We unpack to
    # ~/.mozbuild/{android-sdk-$OS_NAME, android-ndk-$VER}.
    mozbuild_path, sdk_path, ndk_path, avd_home_path, emulator_path = get_paths(os_name)

    if os_name == "macosx":
        os_tag = "mac"
    elif os_name == "windows":
        os_tag = "win"
    else:
        os_tag = os_name

    sdk_url = "https://dl.google.com/android/repository/commandlinetools-{0}-{1}_latest.zip".format(  # NOQA: E501
        os_tag, CMDLINE_TOOLS_VERSION
    )
    ndk_url = android_ndk_url(os_name)

    ensure_android_sdk_and_ndk(
        mozbuild_path,
        os_name,
        sdk_path=sdk_path,
        sdk_url=sdk_url,
        ndk_path=ndk_path,
        ndk_url=ndk_url,
        artifact_mode=artifact_mode,
        ndk_only=ndk_only,
        emulator_only=emulator_only,
    )

    if ndk_only:
        return

    avd_manifest = None
    if avd_manifest_path is not None:
        with open(avd_manifest_path) as f:
            avd_manifest = json.load(f)

    # We expect the |sdkmanager| tool to be at
    # ~/.mozbuild/android-sdk-$OS_NAME/tools/cmdline-tools/$CMDLINE_TOOLS_VERSION_STRING/bin/sdkmanager. # NOQA: E501
    ensure_android_packages(
        sdkmanager_tool=sdkmanager_tool(sdk_path),
        emulator_only=emulator_only,
        system_images_only=system_images_only,
        avd_manifest=avd_manifest,
        no_interactive=no_interactive,
        list_packages=list_packages,
    )

    if emulator_only or system_images_only:
        return

    ensure_android_avd(
        avdmanager_tool=avdmanager_tool(sdk_path),
        adb_tool=adb_tool(sdk_path),
        emulator_tool=emulator_tool(sdk_path),
        avd_home_path=avd_home_path,
        sdk_path=sdk_path,
        emulator_path=emulator_path,
        no_interactive=no_interactive,
        avd_manifest=avd_manifest,
        prewarm_avd=prewarm_avd,
    )


def ensure_android_sdk_and_ndk(
    mozbuild_path,
    os_name,
    sdk_path,
    sdk_url,
    ndk_path,
    ndk_url,
    artifact_mode,
    ndk_only,
    emulator_only,
):
    """
    Ensure the Android SDK and NDK are found at the given paths.  If not, fetch
    and unpack the SDK and/or NDK from the given URLs into
    |mozbuild_path/{android-sdk-$OS_NAME,android-ndk-$VER}|.
    """

    # It's not particularly bad to overwrite the NDK toolchain, but it does take
    # a while to unpack, so let's avoid the disk activity if possible.  The SDK
    # may prompt about licensing, so we do this first.
    # Check for Android NDK only if we are not in artifact mode.
    if not artifact_mode and not emulator_only:
        install_ndk = True
        if os.path.isdir(ndk_path):
            try:
                _, _, human = get_ndk_version(ndk_path)
                if human == NDK_VERSION:
                    print(ANDROID_NDK_EXISTS % ndk_path)
                    install_ndk = False
            except GetNdkVersionError:
                pass  # Just do the install.
        if install_ndk:
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
        cmdline_tools_path = os.path.join(
            mozbuild_path, "android-sdk-{0}".format(os_name), "cmdline-tools"
        )
        install_mobile_android_sdk_or_ndk(sdk_url, cmdline_tools_path)
        # The tools package *really* wants to be in
        # <sdk>/cmdline-tools/$CMDLINE_TOOLS_VERSION_STRING
        os.rename(
            os.path.join(cmdline_tools_path, "cmdline-tools"),
            os.path.join(cmdline_tools_path, CMDLINE_TOOLS_VERSION_STRING),
        )


def get_packages_to_install(packages_file_content, avd_manifest):
    packages = []
    packages += map(lambda package: package.strip(), packages_file_content)
    if avd_manifest is not None:
        packages += [avd_manifest["emulator_package"]]
    return packages


def ensure_android_avd(
    avdmanager_tool,
    adb_tool,
    emulator_tool,
    avd_home_path,
    sdk_path,
    emulator_path,
    no_interactive=False,
    avd_manifest=None,
    prewarm_avd=False,
):
    """
    Use the given sdkmanager tool (like 'sdkmanager') to install required
    Android packages.
    """
    if avd_manifest is None:
        return

    ensure_dir(avd_home_path)
    # The AVD needs this folder to boot, so make sure it exists here.
    ensure_dir(os.path.join(sdk_path, "platforms"))

    avd_name = avd_manifest["emulator_avd_name"]
    args = [
        avdmanager_tool,
        "--verbose",
        "create",
        "avd",
        "--force",
        "--name",
        avd_name,
        "--package",
        avd_manifest["emulator_package"],
    ]

    if not no_interactive:
        subprocess.check_call(args)
        return

    # Flush outputs before running sdkmanager.
    sys.stdout.flush()
    env = os.environ.copy()
    env["ANDROID_AVD_HOME"] = avd_home_path
    proc = subprocess.Popen(args, stdin=subprocess.PIPE, env=env)
    proc.communicate("no\n".encode("UTF-8"))

    retcode = proc.poll()
    if retcode:
        cmd = args[0]
        e = subprocess.CalledProcessError(retcode, cmd)
        raise e

    avd_path = os.path.join(avd_home_path, avd_name + ".avd")
    config_file_name = os.path.join(avd_path, "config.ini")

    print("Writing config at %s" % config_file_name)

    if os.path.isfile(config_file_name):
        with open(config_file_name, "a") as config:
            for key, value in avd_manifest["emulator_extra_config"].items():
                config.write("%s=%s\n" % (key, value))
    else:
        raise NotImplementedError(
            "Could not find config file at %s, something went wrong" % config_file_name
        )
    if prewarm_avd:
        run_prewarm_avd(
            adb_tool, emulator_tool, env, avd_name, avd_manifest, no_interactive
        )
    # When running in headless mode, the emulator does not run the cleanup
    # step, and thus doesn't delete lock files. On some platforms, left-over
    # lock files can cause the emulator to not start, so we remove them here.
    for lock_file in ["hardware-qemu.ini.lock", "multiinstance.lock"]:
        lock_file_path = os.path.join(avd_path, lock_file)
        try:
            os.remove(lock_file_path)
            print("Removed lock file %s" % lock_file_path)
        except OSError:
            # The lock file is not there, nothing to do.
            pass


def run_prewarm_avd(
    adb_tool, emulator_tool, env, avd_name, avd_manifest, no_interactive=False
):
    """
    Ensures the emulator is fully booted to save time on future iterations.
    """
    args = [emulator_tool, "-avd", avd_name] + avd_manifest["emulator_extra_args"]

    # Flush outputs before running emulator.
    sys.stdout.flush()
    proc = subprocess.Popen(args, env=env)

    booted = False
    for i in range(100):
        boot_completed_cmd = [adb_tool, "shell", "getprop", "sys.boot_completed"]
        completed_proc = subprocess.Popen(
            boot_completed_cmd, env=env, stdout=subprocess.PIPE
        )
        try:
            out, err = completed_proc.communicate(timeout=30)
            boot_completed = out.decode("UTF-8").strip()
            print("sys.boot_completed = %s" % boot_completed)
            time.sleep(30)
            if boot_completed == "1":
                booted = True
                break
        except subprocess.TimeoutExpired:
            # Sometimes the adb command hangs, that's ok
            print("sys.boot_completed = Timeout")

    if not booted:
        raise NotImplementedError("Could not prewarm emulator")

    # Wait until the emulator completely shuts down
    subprocess.Popen([adb_tool, "emu", "kill"], env=env).wait()
    proc.wait()


def ensure_android_packages(
    sdkmanager_tool,
    emulator_only=False,
    system_images_only=False,
    avd_manifest=None,
    no_interactive=False,
    list_packages=False,
):
    """
    Use the given sdkmanager tool (like 'sdkmanager') to install required
    Android packages.
    """

    # This tries to install all the required Android packages.  The user
    # may be prompted to agree to the Android license.
    if system_images_only:
        packages_file_name = "android-system-images-packages.txt"
    elif emulator_only:
        packages_file_name = "android-emulator-packages.txt"
    else:
        packages_file_name = "android-packages.txt"

    packages_file_path = os.path.abspath(
        os.path.join(os.path.dirname(__file__), packages_file_name)
    )
    with open(packages_file_path) as packages_file:
        packages_file_content = packages_file.readlines()

    packages = get_packages_to_install(packages_file_content, avd_manifest)
    print(INSTALLING_ANDROID_PACKAGES % "\n".join(packages))

    args = [sdkmanager_tool]
    args.extend(packages)

    if not no_interactive:
        subprocess.check_call(args)
        return

    # Flush outputs before running sdkmanager.
    sys.stdout.flush()
    sys.stderr.flush()
    # Emulate yes.  For a discussion of passing input to check_output,
    # see https://stackoverflow.com/q/10103551.
    yes = "\n".join(["y"] * 100).encode("UTF-8")
    proc = subprocess.Popen(args, stdin=subprocess.PIPE)
    proc.communicate(yes)

    retcode = proc.poll()
    if retcode:
        cmd = args[0]
        e = subprocess.CalledProcessError(retcode, cmd)
        raise e
    if list_packages:
        subprocess.check_call([sdkmanager_tool, "--list"])


def generate_mozconfig(os_name, artifact_mode=False):
    moz_state_dir, sdk_path, ndk_path, avd_home_path, emulator_path = get_paths(os_name)

    extra_lines = []
    if extra_lines:
        extra_lines.append("")

    if artifact_mode:
        template = MOBILE_ANDROID_ARTIFACT_MODE_MOZCONFIG_TEMPLATE
    else:
        template = MOBILE_ANDROID_MOZCONFIG_TEMPLATE

    kwargs = dict(
        sdk_path=sdk_path,
        ndk_path=ndk_path,
        avd_home_path=avd_home_path,
        moz_state_dir=moz_state_dir,
        extra_lines="\n".join(extra_lines),
    )
    return template.format(**kwargs).strip()


def android_ndk_url(os_name, ver=NDK_VERSION):
    # Produce a URL like
    # 'https://dl.google.com/android/repository/android-ndk-$VER-linux-x86_64.zip
    base_url = "https://dl.google.com/android/repository/android-ndk"

    if os_name == "macosx":
        # |mach bootstrap| uses 'macosx', but Google uses 'darwin'.
        os_name = "darwin"

    if sys.maxsize > 2 ** 32:
        arch = "x86_64"
    else:
        arch = "x86"

    return "%s-%s-%s-%s.zip" % (base_url, ver, os_name, arch)


def main(argv):
    import optparse  # No argparse, which is new in Python 2.7.
    import platform

    parser = optparse.OptionParser()
    parser.add_option(
        "-a",
        "--artifact-mode",
        dest="artifact_mode",
        action="store_true",
        help="If true, install only the Android SDK (and not the Android NDK).",
    )
    parser.add_option(
        "--ndk-only",
        dest="ndk_only",
        action="store_true",
        help="If true, install only the Android NDK (and not the Android SDK).",
    )
    parser.add_option(
        "--system-images-only",
        dest="system_images_only",
        action="store_true",
        help="If true, install only the system images for the AVDs.",
    )
    parser.add_option(
        "--no-interactive",
        dest="no_interactive",
        action="store_true",
        help="Accept the Android SDK licenses without user interaction.",
    )
    parser.add_option(
        "--emulator-only",
        dest="emulator_only",
        action="store_true",
        help="If true, install only the Android emulator (and not the SDK or NDK).",
    )
    parser.add_option(
        "--avd-manifest",
        dest="avd_manifest_path",
        help="If present, generate AVD from the manifest pointed by this argument.",
    )
    parser.add_option(
        "--prewarm-avd",
        dest="prewarm_avd",
        action="store_true",
        help="If true, boot the AVD and wait until completed to speed up subsequent boots.",
    )
    parser.add_option(
        "--list-packages",
        dest="list_packages",
        action="store_true",
        help="If true, list installed packages.",
    )

    options, _ = parser.parse_args(argv)

    if options.artifact_mode and options.ndk_only:
        raise NotImplementedError("Use no options to install the NDK and the SDK.")

    if options.artifact_mode and options.emulator_only:
        raise NotImplementedError("Use no options to install the SDK and emulators.")

    os_name = None
    if platform.system() == "Darwin":
        os_name = "macosx"
    elif platform.system() == "Linux":
        os_name = "linux"
    elif platform.system() == "Windows":
        os_name = "windows"
    else:
        raise NotImplementedError(
            "We don't support bootstrapping the Android SDK (or Android "
            "NDK) on {0} yet!".format(platform.system())
        )

    ensure_android(
        os_name,
        artifact_mode=options.artifact_mode,
        ndk_only=options.ndk_only,
        system_images_only=options.system_images_only,
        emulator_only=options.emulator_only,
        avd_manifest_path=options.avd_manifest_path,
        prewarm_avd=options.prewarm_avd,
        no_interactive=options.no_interactive,
        list_packages=options.list_packages,
    )
    mozconfig = generate_mozconfig(os_name, options.artifact_mode)

    # |./mach bootstrap| automatically creates a mozconfig file for you if it doesn't
    # exist. However, here, we don't know where the "topsrcdir" is, and it's not worth
    # pulling in CommandContext (and its dependencies) to find out.
    # So, instead, we'll politely ask users to create (or update) the file themselves.
    suggestion = MOZCONFIG_SUGGESTION_TEMPLATE % ("$topsrcdir/mozconfig", mozconfig)
    print("\n" + suggestion)

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
