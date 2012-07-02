# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

"""Module to handle the installation and uninstallation of Gecko based
applications across platforms.

"""
import mozinfo
from optparse import OptionParser
import os
import shutil
import subprocess
import sys
import tarfile
import time
import zipfile

if mozinfo.isMac:
    from plistlib import readPlist


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


def get_binary(path, app_name):
    """Find the binary in the specified path, and return its path. If binary is
    not found throw an InvalidBinary exception.

    Arguments:
    path -- the path within to search for the binary
    app_name -- application binary without file extension to look for

    """
    binary = None

    # On OS X we can get the real binary from the app bundle
    if mozinfo.isMac:
        plist = '%s/Contents/Info.plist' % path
        assert os.path.isfile(plist), '"%s" has not been found.' % plist

        binary = os.path.join(path, 'Contents/MacOS/',
                              readPlist(plist)['CFBundleExecutable'])

    else:
        app_name = app_name.lower()

        if mozinfo.isWin:
            app_name = app_name + '.exe'

        for root, dirs, files in os.walk(path):
            for filename in files:
                # os.access evaluates to False for some reason, so not using it
                if filename.lower() == app_name:
                    binary = os.path.realpath(os.path.join(root, filename))
                    break

    if not binary:
        # The expected binary has not been found. Make sure we clean the
        # install folder to remove any traces
        shutil.rmtree(path)

        raise InvalidBinary('"%s" does not contain a valid binary.' % path)

    return binary


def install(src, dest):
    """Install a zip, exe, tar.gz, tar.bz2 or dmg file, and return the path of
    the installation folder.

    Arguments:
    src  -- the path to the install file
    dest -- the path to install to (to ensure we do not overwrite any existent
                                    files the folder should not exist yet)

    """
    src = os.path.realpath(src)
    dest = os.path.realpath(dest)

    if not is_installer(src):
        raise InvalidSource(src + ' is not a recognized file type ' +
                                  '(zip, exe, tar.gz, tar.bz2 or dmg)')

    if not os.path.exists(dest):
        os.makedirs(dest)

    trbk = None
    try:
        install_dir = None
        if zipfile.is_zipfile(src) or tarfile.is_tarfile(src):
            install_dir = _extract(src, dest)[0]
        elif src.lower().endswith('.dmg'):
            install_dir = _install_dmg(src, dest)
        elif src.lower().endswith('.exe'):
            install_dir = _install_exe(src, dest)

        return install_dir

    except Exception, e:
        cls, exc, trbk = sys.exc_info()
        error = InstallError('Failed to install "%s"' % src)
        raise InstallError, error, trbk

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

    Arguments:
    src -- the path to the install file

    """
    src = os.path.realpath(src)

    if not os.path.isfile(src):
        return False

    if mozinfo.isLinux:
        return tarfile.is_tarfile(src)
    elif mozinfo.isMac:
        return src.lower().endswith('.dmg')
    elif mozinfo.isWin:
        return src.lower().endswith('.exe') or zipfile.is_zipfile(src)


def uninstall(install_folder):
    """Uninstalls the application in the specified path. If it has been
    installed via an installer on Windows, use the uninstaller first.

    Arguments:
    install_folder -- the path of the installation folder

    """
    install_folder = os.path.realpath(install_folder)
    assert os.path.isdir(install_folder), \
        'installation folder "%s" exists.' % install_folder

    # On Windows we have to use the uninstaller. If it's not available fallback
    # to the directory removal code
    if mozinfo.isWin:
        uninstall_folder = '%s\uninstall' % install_folder
        log_file = '%s\uninstall.log' % uninstall_folder

        if os.path.isfile(log_file):
            trbk = None
            try:
                cmdArgs = ['%s\uninstall\helper.exe' % install_folder, '/S']
                result = subprocess.call(cmdArgs)
                if not result is 0:
                    raise Exception('Execution of uninstaller failed.')

                # The uninstaller spawns another process so the subprocess call
                # returns immediately. We have to wait until the uninstall
                # folder has been removed or until we run into a timeout.
                end_time = time.time() + TIMEOUT_UNINSTALL
                while os.path.exists(uninstall_folder):
                    time.sleep(1)

                    if time.time() > end_time:
                        raise Exception('Failure removing uninstall folder.')

            except Exception, e:
                cls, exc, trbk = sys.exc_info()
                error = UninstallError('Failed to uninstall %s' % install_folder)
                raise UninstallError, error, trbk

            finally:
                # trbk won't get GC'ed due to circular reference
                # http://docs.python.org/library/sys.html#sys.exc_info
                del trbk

    # Ensure that we remove any trace of the installation. Even the uninstaller
    # on Windows leaves files behind we have to explicitely remove.
    shutil.rmtree(install_folder)


def _extract(src, dest):
    """Extract a tar or zip file into the destination folder and return the
    application folder.

    Arguments:
    src -- archive which has to be extracted
    dest -- the path to extract to

    """
    if zipfile.is_zipfile(src):
        bundle = zipfile.ZipFile(src)
        namelist = bundle.namelist()

        if hasattr(bundle, 'extractall'):
            # zipfile.extractall doesn't exist in Python 2.5
            bundle.extractall(path=dest)
        else:
            for name in namelist:
                filename = os.path.realpath(os.path.join(dest, name))
                if name.endswith('/'):
                    os.makedirs(filename)
                else:
                    path = os.path.dirname(filename)
                    if not os.path.isdir(path):
                        os.makedirs(path)
                    dest = open(filename, 'wb')
                    dest.write(bundle.read(name))
                    dest.close()

    elif tarfile.is_tarfile(src):
        bundle = tarfile.open(src)
        namelist = bundle.getnames()

        if hasattr(bundle, 'extractall'):
            # tarfile.extractall doesn't exist in Python 2.4
            bundle.extractall(path=dest)
        else:
            for name in namelist:
                bundle.extract(name, path=dest)
    else:
        return

    bundle.close()

    # namelist returns paths with forward slashes even in windows
    top_level_files = [os.path.join(dest, name) for name in namelist
                             if len(name.rstrip('/').split('/')) == 1]

    # namelist doesn't include folders, append these to the list
    for name in namelist:
        root = os.path.join(dest, name[:name.find('/')])
        if root not in top_level_files:
            top_level_files.append(root)

    return top_level_files


def _install_dmg(src, dest):
    """Extract a dmg file into the destination folder and return the
    application folder.

    Arguments:
    src -- DMG image which has to be extracted
    dest -- the path to extract to

    """
    try:
        proc = subprocess.Popen('hdiutil attach %s' % src,
                                shell=True,
                                stdout=subprocess.PIPE)

        for data in proc.communicate()[0].split():
            if data.find('/Volumes/') != -1:
                appDir = data
                break

        for appFile in os.listdir(appDir):
            if appFile.endswith('.app'):
                appName = appFile
                break

        mounted_path = os.path.join(appDir, appName)

        dest = os.path.join(dest, appName)

        # copytree() would fail if dest already exists.
        if os.path.exists(dest):
            raise InstallError('App bundle "%s" already exists.' % dest)

        shutil.copytree(mounted_path, dest, False)

    finally:
        subprocess.call('hdiutil detach %s -quiet' % appDir,
                        shell=True)

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
    dest = os.path.join(dest, filename.split('.')[0])

    # possibly gets around UAC in vista (still need to run as administrator)
    os.environ['__compat_layer'] = 'RunAsInvoker'
    cmd = [src, '/S', '/D=%s' % os.path.realpath(dest)]

    # As long as we support Python 2.4 check_call will not be available.
    result = subprocess.call(cmd)
    if not result is 0:
        raise Exception('Execution of installer failed.')

    return dest


def install_cli(argv=sys.argv[1:]):
    parser = OptionParser(usage="usage: %prog [options] installer")
    parser.add_option('-d', '--destination',
                      dest='dest',
                      default=os.getcwd(),
                      help='Directory to install application into. '
                           '[default: "%default"]')
    parser.add_option('--app', dest='app',
                      default='firefox',
                      help='Application being installed. [default: %default]')

    (options, args) = parser.parse_args(argv)
    if not len(args) == 1:
        parser.error('An installer file has to be specified.')

    src = args[0]

    # Run it
    if os.path.isdir(src):
        binary = get_binary(src, app_name=options.app)
    else:
        install_path = install(src, options.dest)
        binary = get_binary(install_path, app_name=options.app)

    print binary


def uninstall_cli(argv=sys.argv[1:]):
    parser = OptionParser(usage="usage: %prog install_path")

    (options, args) = parser.parse_args(argv)
    if not len(args) == 1:
        parser.error('An installation path has to be specified.')

    # Run it
    uninstall(argv[0])

