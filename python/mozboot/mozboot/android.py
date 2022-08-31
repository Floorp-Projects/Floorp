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
import requests
from typing import Optional, Union
from pathlib import Path
from tqdm import tqdm

# We need the NDK version in multiple different places, and it's inconvenient
# to pass down the NDK version to all relevant places, so we have this global
# variable.
from mozboot.bootstrap import MOZCONFIG_SUGGESTION_TEMPLATE

NDK_VERSION = "r21d"
CMDLINE_TOOLS_VERSION_STRING = "7.0"
CMDLINE_TOOLS_VERSION = "8512546"

BUNDLETOOL_VERSION = "1.11.0"

# We expect the emulator AVD definitions to be platform agnostic
LINUX_X86_64_ANDROID_AVD = "linux64-android-avd-x86_64-repack"
LINUX_ARM_ANDROID_AVD = "linux64-android-avd-arm-repack"

MACOS_X86_64_ANDROID_AVD = "linux64-android-avd-x86_64-repack"
MACOS_ARM_ANDROID_AVD = "linux64-android-avd-arm-repack"
MACOS_ARM64_ANDROID_AVD = "linux64-android-avd-arm64-repack"

WINDOWS_X86_64_ANDROID_AVD = "linux64-android-avd-x86_64-repack"
WINDOWS_ARM_ANDROID_AVD = "linux64-android-avd-arm-repack"

AVD_MANIFEST_X86_64 = Path(__file__).resolve().parent / "android-avds/x86_64.json"
AVD_MANIFEST_ARM = Path(__file__).resolve().parent / "android-avds/arm.json"
AVD_MANIFEST_ARM64 = Path(__file__).resolve().parent / "android-avds/arm64.json"

JAVA_VERSION_MAJOR = "17"
JAVA_VERSION_MINOR = "0.4.1"
JAVA_VERSION_PATCH = "1"

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
# For newer phones or Apple silicon
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


def install_bundletool(url, path: Path):
    """
    Fetch bundletool to the desired directory.
    """
    try:
        subprocess.check_call(
            ["wget", "--continue", url, "--output-document", "bundletool.jar"],
            cwd=str(path),
        )
    finally:
        pass


def install_mobile_android_sdk_or_ndk(url, path: Path):
    """
    Fetch an Android SDK or NDK from |url| and unpack it into the given |path|.

    We use, and 'requests' respects, https. We could also include SHAs for a
    small improvement in the integrity guarantee we give. But this script is
    bootstrapped over https anyway, so it's a really minor improvement.

    We keep a cache of the downloaded artifacts, writing into |path|/mozboot.
    We don't yet clean the cache; it's better to waste some disk space and
    not require a long re-download than to wipe the cache prematurely.
    """

    download_path = path / "mozboot"
    try:
        download_path.mkdir(parents=True)
    except OSError as e:
        if e.errno == errno.EEXIST and download_path.is_dir():
            pass
        else:
            raise

    file_name = url.split("/")[-1]
    download_file_path = download_path / file_name

    with requests.Session() as session:
        request = session.head(url)
        remote_file_size = int(request.headers["content-length"])

        if download_file_path.is_file():
            local_file_size = download_file_path.stat().st_size

            if local_file_size == remote_file_size:
                print(f"{download_file_path} already downloaded. Skipping download...")
            else:
                print(
                    f"Partial download detected. Resuming download of {download_file_path}..."
                )
                download(
                    download_file_path,
                    session,
                    url,
                    remote_file_size,
                    local_file_size,
                )
        else:
            print(f"Downloading {download_file_path}...")
            download(download_file_path, session, url, remote_file_size)

    if file_name.endswith(".tar.gz") or file_name.endswith(".tgz"):
        cmd = ["tar", "zxf", str(download_file_path)]
    elif file_name.endswith(".tar.bz2"):
        cmd = ["tar", "jxf", str(download_file_path)]
    elif file_name.endswith(".zip"):
        cmd = ["unzip", "-q", str(download_file_path)]
    elif file_name.endswith(".bin"):
        # Execute the .bin file, which unpacks the content.
        mode = os.stat(path).st_mode
        download_file_path.chmod(mode | stat.S_IXUSR)
        cmd = [str(download_file_path)]
    else:
        raise NotImplementedError(f"Don't know how to unpack file: {file_name}")

    print(f"Unpacking {download_file_path}...")

    with open(os.devnull, "w") as stdout:
        # These unpack commands produce a ton of output; ignore it.  The
        # .bin files are 7z archives; there's no command line flag to quiet
        # output, so we use this hammer.
        subprocess.check_call(cmd, stdout=stdout, cwd=str(path))

    print(f"Unpacking {download_file_path}... DONE")
    # Now delete the archive
    download_file_path.unlink()


def download(
    download_file_path: Path,
    session,
    url,
    remote_file_size,
    resume_from_byte_pos: int = None,
):
    """
    Handles both a fresh SDK/NDK download, as well as resuming a partial one
    """
    # "ab" will behave same as "wb" if file does not exist
    with open(download_file_path, "ab") as file:
        # 64 KB/s should be fine on even the slowest internet connections
        chunk_size = 1024 * 64
        # https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Range#directives
        resume_header = (
            {"Range": f"bytes={resume_from_byte_pos}-"}
            if resume_from_byte_pos
            else None
        )

        request = session.get(
            url, stream=True, allow_redirects=True, headers=resume_header
        )

        with tqdm(
            total=int(remote_file_size),
            unit="B",
            unit_scale=True,
            unit_divisor=1024,
            desc=download_file_path.name,
            initial=resume_from_byte_pos if resume_from_byte_pos else 0,
        ) as progress_bar:
            for chunk in request.iter_content(chunk_size):
                file.write(chunk)
                progress_bar.update(len(chunk))


def get_ndk_version(ndk_path: Union[str, Path]):
    """Given the path to the NDK, return the version as a 3-tuple of (major,
    minor, human).
    """
    ndk_path = Path(ndk_path)
    with open(ndk_path / "source.properties", "r") as f:
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
    mozbuild_path = Path(
        os.environ.get("MOZBUILD_STATE_PATH", Path("~/.mozbuild").expanduser())
    )
    sdk_path = Path(
        os.environ.get("ANDROID_SDK_HOME", mozbuild_path / f"android-sdk-{os_name}"),
    )
    ndk_path = Path(
        os.environ.get(
            "ANDROID_NDK_HOME", mozbuild_path / f"android-ndk-{NDK_VERSION}"
        ),
    )
    avd_home_path = Path(
        os.environ.get("ANDROID_AVD_HOME", mozbuild_path / "android-device" / "avd")
    )
    return mozbuild_path, sdk_path, ndk_path, avd_home_path


def sdkmanager_tool(sdk_path: Path):
    # sys.platform is win32 even if Python/Win64.
    sdkmanager = "sdkmanager.bat" if sys.platform.startswith("win") else "sdkmanager"
    return (
        sdk_path / "cmdline-tools" / CMDLINE_TOOLS_VERSION_STRING / "bin" / sdkmanager
    )


def avdmanager_tool(sdk_path: Path):
    # sys.platform is win32 even if Python/Win64.
    sdkmanager = "avdmanager.bat" if sys.platform.startswith("win") else "avdmanager"
    return (
        sdk_path / "cmdline-tools" / CMDLINE_TOOLS_VERSION_STRING / "bin" / sdkmanager
    )


def adb_tool(sdk_path: Path):
    adb = "adb.bat" if sys.platform.startswith("win") else "adb"
    return sdk_path / "platform-tools" / adb


def emulator_tool(sdk_path: Path):
    emulator = "emulator.bat" if sys.platform.startswith("win") else "emulator"
    return sdk_path / "emulator" / emulator


def ensure_android(
    os_name,
    os_arch,
    artifact_mode=False,
    ndk_only=False,
    system_images_only=False,
    emulator_only=False,
    avd_manifest_path: Optional[Path] = None,
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
    mozbuild_path, sdk_path, ndk_path, avd_home_path = get_paths(os_name)

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
    bundletool_url = "https://github.com/google/bundletool/releases/download/{v}/bundletool-all-{v}.jar".format(  # NOQA: E501
        v=BUNDLETOOL_VERSION
    )

    ensure_android_sdk_and_ndk(
        mozbuild_path,
        os_name,
        sdk_path=sdk_path,
        sdk_url=sdk_url,
        ndk_path=ndk_path,
        ndk_url=ndk_url,
        bundletool_url=bundletool_url,
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
        # Some AVDs cannot be prewarmed in CI because they cannot run on linux64
        # (like the arm64 AVD).
        if "emulator_prewarm" in avd_manifest:
            prewarm_avd = prewarm_avd and avd_manifest["emulator_prewarm"]

    # We expect the |sdkmanager| tool to be at
    # ~/.mozbuild/android-sdk-$OS_NAME/tools/cmdline-tools/$CMDLINE_TOOLS_VERSION_STRING/bin/sdkmanager. # NOQA: E501
    ensure_android_packages(
        os_name,
        os_arch,
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
        no_interactive=no_interactive,
        avd_manifest=avd_manifest,
        prewarm_avd=prewarm_avd,
    )


def ensure_android_sdk_and_ndk(
    mozbuild_path: Path,
    os_name,
    sdk_path: Path,
    sdk_url,
    ndk_path: Path,
    ndk_url,
    bundletool_url,
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
        if ndk_path.is_dir():
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
    if sdkmanager_tool(sdk_path).is_file():
        print(ANDROID_SDK_EXISTS % sdk_path)
    elif sdk_path.is_dir():
        raise NotImplementedError(ANDROID_SDK_TOO_OLD % sdk_path)
    else:
        # The SDK archive used to include a top-level
        # android-sdk-$OS_NAME directory; it no longer does so.  We
        # preserve the old convention to smooth detecting existing SDK
        # installations.
        cmdline_tools_path = mozbuild_path / f"android-sdk-{os_name}" / "cmdline-tools"
        install_mobile_android_sdk_or_ndk(sdk_url, cmdline_tools_path)
        # The tools package *really* wants to be in
        # <sdk>/cmdline-tools/$CMDLINE_TOOLS_VERSION_STRING
        (cmdline_tools_path / "cmdline-tools").rename(
            cmdline_tools_path / CMDLINE_TOOLS_VERSION_STRING
        )
        install_bundletool(bundletool_url, mozbuild_path)


def get_packages_to_install(packages_file_content, avd_manifest):
    packages = []
    packages += map(lambda package: package.strip(), packages_file_content)
    if avd_manifest is not None:
        packages += [avd_manifest["emulator_package"]]
    return packages


def ensure_android_avd(
    avdmanager_tool: Path,
    adb_tool: Path,
    emulator_tool: Path,
    avd_home_path: Path,
    sdk_path: Path,
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

    avd_home_path.mkdir(parents=True, exist_ok=True)
    # The AVD needs this folder to boot, so make sure it exists here.
    (sdk_path / "platforms").mkdir(parents=True, exist_ok=True)

    avd_name = avd_manifest["emulator_avd_name"]
    args = [
        str(avdmanager_tool),
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
    env["ANDROID_AVD_HOME"] = str(avd_home_path)
    proc = subprocess.Popen(args, stdin=subprocess.PIPE, env=env)
    proc.communicate("no\n".encode("UTF-8"))

    retcode = proc.poll()
    if retcode:
        cmd = args[0]
        e = subprocess.CalledProcessError(retcode, cmd)
        raise e

    avd_path = avd_home_path / (str(avd_name) + ".avd")
    config_file_name = avd_path / "config.ini"

    print(f"Writing config at {config_file_name}")

    if config_file_name.is_file():
        with open(config_file_name, "a") as config:
            for key, value in avd_manifest["emulator_extra_config"].items():
                config.write("%s=%s\n" % (key, value))
    else:
        raise NotImplementedError(
            f"Could not find config file at {config_file_name}, something went wrong"
        )
    if prewarm_avd:
        run_prewarm_avd(adb_tool, emulator_tool, env, avd_name, avd_manifest)
    # When running in headless mode, the emulator does not run the cleanup
    # step, and thus doesn't delete lock files. On some platforms, left-over
    # lock files can cause the emulator to not start, so we remove them here.
    for lock_file in ["hardware-qemu.ini.lock", "multiinstance.lock"]:
        lock_file_path = avd_path / lock_file
        try:
            lock_file_path.unlink()
            print(f"Removed lock file {lock_file_path}")
        except OSError:
            # The lock file is not there, nothing to do.
            pass


def run_prewarm_avd(
    adb_tool: Path,
    emulator_tool: Path,
    env,
    avd_name,
    avd_manifest,
):
    """
    Ensures the emulator is fully booted to save time on future iterations.
    """
    args = [str(emulator_tool), "-avd", avd_name] + avd_manifest["emulator_extra_args"]

    # Flush outputs before running emulator.
    sys.stdout.flush()
    proc = subprocess.Popen(args, env=env)

    booted = False
    for i in range(100):
        boot_completed_cmd = [str(adb_tool), "shell", "getprop", "sys.boot_completed"]
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
    subprocess.Popen([str(adb_tool), "emu", "kill"], env=env).wait()
    proc.wait()


def ensure_android_packages(
    os_name,
    os_arch,
    sdkmanager_tool: Path,
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

    packages_file_path = (Path(__file__).parent / packages_file_name).resolve()

    with open(packages_file_path) as packages_file:
        packages_file_content = packages_file.readlines()

    packages = get_packages_to_install(packages_file_content, avd_manifest)
    print(INSTALLING_ANDROID_PACKAGES % "\n".join(packages))

    args = [str(sdkmanager_tool)]
    if os_name == "macosx" and os_arch == "arm64":
        # Support for Apple Silicon is still in nightly
        args.append("--channel=3")
    args.extend(packages)

    # sdkmanager needs JAVA_HOME
    java_bin_path = ensure_java(os_name, os_arch)
    env = os.environ.copy()
    env["JAVA_HOME"] = str(java_bin_path.parent)

    if not no_interactive:
        subprocess.check_call(args, env=env)
        return

    # Flush outputs before running sdkmanager.
    sys.stdout.flush()
    sys.stderr.flush()
    # Emulate yes.  For a discussion of passing input to check_output,
    # see https://stackoverflow.com/q/10103551.
    yes = "\n".join(["y"] * 100).encode("UTF-8")
    proc = subprocess.Popen(args, stdin=subprocess.PIPE, env=env)
    proc.communicate(yes)

    retcode = proc.poll()
    if retcode:
        cmd = args[0]
        e = subprocess.CalledProcessError(retcode, cmd)
        raise e
    if list_packages:
        subprocess.check_call([str(sdkmanager_tool), "--list"])


def generate_mozconfig(os_name, artifact_mode=False):
    moz_state_dir, sdk_path, ndk_path, avd_home_path = get_paths(os_name)

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
        "--jdk-only",
        dest="jdk_only",
        action="store_true",
        help="If true, install only the Java JDK.",
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

    os_arch = platform.machine()

    if options.jdk_only:
        ensure_java(os_name, os_arch)
        return 0

    avd_manifest_path = (
        Path(options.avd_manifest_path) if options.avd_manifest_path else None
    )

    ensure_android(
        os_name,
        os_arch,
        artifact_mode=options.artifact_mode,
        ndk_only=options.ndk_only,
        system_images_only=options.system_images_only,
        emulator_only=options.emulator_only,
        avd_manifest_path=avd_manifest_path,
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


def ensure_java(os_name, os_arch):
    mozbuild_path, _, _, _ = get_paths(os_name)

    if os_name == "macosx":
        os_tag = "mac"
    else:
        os_tag = os_name

    if os_arch == "x86_64":
        arch = "x64"
    elif os_arch == "arm64":
        arch = "aarch64"
    else:
        arch = os_arch

    ext = "zip" if os_name == "windows" else "tar.gz"

    java_path = java_bin_path(os_name, mozbuild_path)
    if not java_path:
        raise NotImplementedError(f"Could not bootstrap java for {os_name}.")

    if not java_path.exists():
        # e.g. https://github.com/adoptium/temurin17-binaries/releases/
        #      download/jdk-17.0.4.1%2B1/OpenJDK17U-jdk_x64_linux_hotspot_17.0.4.1_1.tar.gz
        java_url = (
            "https://github.com/adoptium/temurin{major}-binaries/releases/"
            "download/jdk-{major}.{minor}%2B{patch}/"
            "OpenJDK{major}U-jdk_{arch}_{os}_hotspot_{major}.{minor}_{patch}.{ext}"
        ).format(
            major=JAVA_VERSION_MAJOR,
            minor=JAVA_VERSION_MINOR,
            patch=JAVA_VERSION_PATCH,
            os=os_tag,
            arch=arch,
            ext=ext,
        )
        install_mobile_android_sdk_or_ndk(java_url, mozbuild_path / "jdk")
    return java_path


def java_bin_path(os_name, toolchain_path: Path):
    # Like jdk-17.0.4.1+1
    jdk_folder = "jdk-{major}.{minor}+{patch}".format(
        major=JAVA_VERSION_MAJOR, minor=JAVA_VERSION_MINOR, patch=JAVA_VERSION_PATCH
    )

    java_path = toolchain_path / "jdk" / jdk_folder

    if os_name == "macosx":
        return java_path / "Contents" / "Home" / "bin"
    elif os_name == "linux":
        return java_path / "bin"
    elif os_name == "windows":
        return java_path / "bin"
    else:
        return None


def locate_java_bin_path(host_kernel, toolchain_path: Union[str, Path]):
    if host_kernel == "WINNT":
        os_name = "windows"
    elif host_kernel == "Darwin":
        os_name = "macosx"
    elif host_kernel == "Linux":
        os_name = "linux"
    else:
        # Default to Linux
        os_name = "linux"
    path = java_bin_path(os_name, Path(toolchain_path))
    if not path.is_dir():
        raise JavaLocationFailedException(
            f"Could not locate Java at {path}, please run "
            "./mach bootstrap --no-system-changes"
        )
    return str(path)


class JavaLocationFailedException(Exception):
    pass


if __name__ == "__main__":
    sys.exit(main(sys.argv))
