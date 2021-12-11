# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

from optparse import OptionParser
import os
import plistlib
import shutil
import subprocess
import sys
import tarfile
import tempfile
import time
import zipfile

import requests

from six import PY3, reraise

import mozfile
import mozinfo

try:
    import pefile

    has_pefile = True
except ImportError:
    has_pefile = False


TIMEOUT_UNINSTALL = 60


class InstallError(Exception):
    """Thrown when installation fails. Includes traceback if available."""


class InvalidBinary(Exception):
    """Thrown when the binary cannot be found after the installation."""


class InvalidSource(Exception):
    """Thrown when the specified source is not a recognized file type.

    Supported types:
    Linux:   tar.gz, tar.bz2
    Mac:     dmg
    Windows: zip, exe

    """


class UninstallError(Exception):
    """Thrown when uninstallation fails. Includes traceback if available."""


def _readPlist(path):
    if PY3:
        with open(path, "rb") as fp:
            return plistlib.load(fp)
    return plistlib.readPlist(path)


def get_binary(path, app_name):
    """Find the binary in the specified path, and return its path. If binary is
    not found throw an InvalidBinary exception.

    :param path: Path within to search for the binary
    :param app_name: Application binary without file extension to look for
    """
    binary = None

    # On OS X we can get the real binary from the app bundle
    if mozinfo.isMac:
        plist = "%s/Contents/Info.plist" % path
        if not os.path.isfile(plist):
            raise InvalidBinary("%s/Contents/Info.plist not found" % path)

        binary = os.path.join(
            path, "Contents/MacOS/", _readPlist(plist)["CFBundleExecutable"]
        )

    else:
        app_name = app_name.lower()

        if mozinfo.isWin:
            app_name = app_name + ".exe"

        for root, dirs, files in os.walk(path):
            for filename in files:
                # os.access evaluates to False for some reason, so not using it
                if filename.lower() == app_name:
                    binary = os.path.realpath(os.path.join(root, filename))
                    break

    if not binary:
        # The expected binary has not been found.
        raise InvalidBinary('"%s" does not contain a valid binary.' % path)

    return binary


def install(src, dest):
    """Install a zip, exe, tar.gz, tar.bz2 or dmg file, and return the path of
    the installation folder.

    :param src: Path to the install file
    :param dest: Path to install to (to ensure we do not overwrite any existent
                 files the folder should not exist yet)
    """
    if not is_installer(src):
        msg = "{} is not a valid installer file".format(src)
        if "://" in src:
            try:
                return _install_url(src, dest)
            except Exception:
                exc, val, tb = sys.exc_info()
                error = InvalidSource("{} ({})".format(msg, val))
                reraise(InvalidSource, error, tb)
        raise InvalidSource(msg)

    src = os.path.realpath(src)
    dest = os.path.realpath(dest)

    did_we_create = False
    if not os.path.exists(dest):
        did_we_create = True
        os.makedirs(dest)

    trbk = None
    try:
        install_dir = None
        if src.lower().endswith(".dmg"):
            install_dir = _install_dmg(src, dest)
        elif src.lower().endswith(".exe"):
            install_dir = _install_exe(src, dest)
        elif zipfile.is_zipfile(src) or tarfile.is_tarfile(src):
            install_dir = mozfile.extract(src, dest)[0]

        return install_dir

    except BaseException:
        cls, exc, trbk = sys.exc_info()
        if did_we_create:
            try:
                # try to uninstall this properly
                uninstall(dest)
            except Exception:
                # uninstall may fail, let's just try to clean the folder
                # in this case
                try:
                    mozfile.remove(dest)
                except Exception:
                    pass
        if issubclass(cls, Exception):
            error = InstallError('Failed to install "%s (%s)"' % (src, str(exc)))
            reraise(InstallError, error, trbk)
        # any other kind of exception like KeyboardInterrupt is just re-raised.
        reraise(cls, exc, trbk)

    finally:
        # trbk won't get GC'ed due to circular reference
        # http://docs.python.org/library/sys.html#sys.exc_info
        del trbk


def is_installer(src):
    """Tests if the given file is a valid installer package.

    Supported types:
    Linux:   tar.gz, tar.bz2
    Mac:     dmg
    Windows: zip, exe

    On Windows pefile will be used to determine if the executable is the
    right type, if it is installed on the system.

    :param src: Path to the install file.
    """
    src = os.path.realpath(src)

    if not os.path.isfile(src):
        return False

    if mozinfo.isLinux:
        return tarfile.is_tarfile(src)
    elif mozinfo.isMac:
        return src.lower().endswith(".dmg")
    elif mozinfo.isWin:
        if zipfile.is_zipfile(src):
            return True

        if os.access(src, os.X_OK) and src.lower().endswith(".exe"):
            if has_pefile:
                # try to determine if binary is actually a gecko installer
                pe_data = pefile.PE(src)
                data = {}
                for info in getattr(pe_data, "FileInfo", []):
                    if info.Key == "StringFileInfo":
                        for string in info.StringTable:
                            data.update(string.entries)
                return "BuildID" not in data
            else:
                # pefile not available, just assume a proper binary was passed in
                return True

        return False


def uninstall(install_folder):
    """Uninstalls the application in the specified path. If it has been
    installed via an installer on Windows, use the uninstaller first.

    :param install_folder: Path of the installation folder

    """
    install_folder = os.path.realpath(install_folder)
    assert os.path.isdir(install_folder), (
        'installation folder "%s" exists.' % install_folder
    )

    # On Windows we have to use the uninstaller. If it's not available fallback
    # to the directory removal code
    if mozinfo.isWin:
        uninstall_folder = "%s\\uninstall" % install_folder
        log_file = "%s\\uninstall.log" % uninstall_folder

        if os.path.isfile(log_file):
            trbk = None
            try:
                cmdArgs = ["%s\\uninstall\helper.exe" % install_folder, "/S"]
                result = subprocess.call(cmdArgs)
                if result != 0:
                    raise Exception("Execution of uninstaller failed.")

                # The uninstaller spawns another process so the subprocess call
                # returns immediately. We have to wait until the uninstall
                # folder has been removed or until we run into a timeout.
                end_time = time.time() + TIMEOUT_UNINSTALL
                while os.path.exists(uninstall_folder):
                    time.sleep(1)

                    if time.time() > end_time:
                        raise Exception("Failure removing uninstall folder.")

            except Exception as ex:
                cls, exc, trbk = sys.exc_info()
                error = UninstallError(
                    "Failed to uninstall %s (%s)" % (install_folder, str(ex))
                )
                reraise(UninstallError, error, trbk)

            finally:
                # trbk won't get GC'ed due to circular reference
                # http://docs.python.org/library/sys.html#sys.exc_info
                del trbk

    # Ensure that we remove any trace of the installation. Even the uninstaller
    # on Windows leaves files behind we have to explicitely remove.
    mozfile.remove(install_folder)


def _install_url(url, dest):
    """Saves a url to a temporary file, and passes that through to the
    install function.

    :param url: Url to the install file
    :param dest: Path to install to (to ensure we do not overwrite any existent
                 files the folder should not exist yet)
    """
    r = requests.get(url, stream=True)
    name = tempfile.mkstemp()[1]
    try:
        with open(name, "w+b") as fh:
            for chunk in r.iter_content(chunk_size=16 * 1024):
                fh.write(chunk)
        result = install(name, dest)
    finally:
        mozfile.remove(name)
    return result


def _install_dmg(src, dest):
    """Extract a dmg file into the destination folder and return the
    application folder.

    src -- DMG image which has to be extracted
    dest -- the path to extract to

    """
    appDir = None
    try:
        # According to the Apple doc, the hdiutil output is stable and is based on the tab
        # separators
        # Therefor, $3 should give us the mounted path
        appDir = (
            subprocess.check_output(
                'hdiutil attach -nobrowse -noautoopen "%s"'
                "|grep /Volumes/"
                "|awk 'BEGIN{FS=\"\t\"} {print $3}'" % str(src),
                shell=True,
            )
            .strip()
            .decode("ascii")
        )

        for appFile in os.listdir(appDir):
            if appFile.endswith(".app"):
                appName = appFile
                break

        mounted_path = os.path.join(appDir, appName)

        dest = os.path.join(dest, appName)

        # copytree() would fail if dest already exists.
        if os.path.exists(dest):
            raise InstallError('App bundle "%s" already exists.' % dest)

        shutil.copytree(mounted_path, dest, False)

    finally:
        if appDir:
            subprocess.check_call('hdiutil detach "%s" -quiet' % appDir, shell=True)

    return dest


def _install_exe(src, dest):
    """Run the MSI installer to silently install the application into the
    destination folder. Return the folder path.

    Arguments:
    src -- MSI installer to be executed
    dest -- the path to install to

    """
    # The installer doesn't automatically create a sub folder. Lets guess the
    # best name from the src file name
    filename = os.path.basename(src)
    dest = os.path.join(dest, filename.split(".")[0])

    # possibly gets around UAC in vista (still need to run as administrator)
    os.environ["__compat_layer"] = "RunAsInvoker"
    cmd = '"%s" /extractdir=%s' % (src, os.path.realpath(dest))

    subprocess.check_call(cmd)

    return dest


def install_cli(argv=sys.argv[1:]):
    parser = OptionParser(usage="usage: %prog [options] installer")
    parser.add_option(
        "-d",
        "--destination",
        dest="dest",
        default=os.getcwd(),
        help="Directory to install application into. " '[default: "%default"]',
    )
    parser.add_option(
        "--app",
        dest="app",
        default="firefox",
        help="Application being installed. [default: %default]",
    )

    (options, args) = parser.parse_args(argv)
    if not len(args) == 1:
        parser.error("An installer file has to be specified.")

    src = args[0]

    # Run it
    if os.path.isdir(src):
        binary = get_binary(src, app_name=options.app)
    else:
        install_path = install(src, options.dest)
        binary = get_binary(install_path, app_name=options.app)

    print(binary)


def uninstall_cli(argv=sys.argv[1:]):
    parser = OptionParser(usage="usage: %prog install_path")

    (options, args) = parser.parse_args(argv)
    if not len(args) == 1:
        parser.error("An installation path has to be specified.")

    # Run it
    uninstall(argv[0])
