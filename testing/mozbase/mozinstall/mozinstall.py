#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is mozinstall.
#
# The Initial Developer of the Original Code is
#  The Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2011
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#  Clint Talbert <ctalbert@mozilla.com>
#  Andrew Halberstadt <halbersa@gmail.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

from optparse import OptionParser
import mozinfo
import subprocess
import zipfile
import tarfile
import sys
import os

_default_apps = ["firefox",
                 "thunderbird",
                 "fennec"]

def install(src, dest=None, apps=_default_apps):
    """
    Installs a zip, exe, tar.gz, tar.bz2 or dmg file
    src - the path to the install file
    dest - the path to install to [default is os.path.dirname(src)]
    returns - the full path to the binary in the installed folder
              or None if the binary cannot be found
    """
    src = os.path.realpath(src)
    assert(os.path.isfile(src))
    if not dest:
        dest = os.path.dirname(src)

    trbk = None
    try:
        install_dir = None
        if zipfile.is_zipfile(src) or tarfile.is_tarfile(src):
            install_dir = _extract(src, dest)[0]
        elif mozinfo.isMac and src.lower().endswith(".dmg"):
            install_dir = _install_dmg(src, dest)
        elif mozinfo.isWin and os.access(src, os.X_OK):
            install_dir = _install_exe(src, dest)
        else:
            raise InvalidSource(src + " is not a recognized file type " +
                                      "(zip, exe, tar.gz, tar.bz2 or dmg)")
    except InvalidSource, e:
        raise
    except Exception, e:
        cls, exc, trbk = sys.exc_info()
        install_error = InstallError("Failed to install %s" % src)
        raise install_error.__class__, install_error, trbk
    finally:
        # trbk won't get GC'ed due to circular reference
        # http://docs.python.org/library/sys.html#sys.exc_info
        del trbk

    if install_dir:
        return get_binary(install_dir, apps=apps)

def get_binary(path, apps=_default_apps):
    """
    Finds the binary in the specified path
    path - the path within which to search for the binary
    returns - the full path to the binary in the folder
              or None if the binary cannot be found
    """
    if mozinfo.isWin:
        apps = [app + ".exe" for app in apps]
    for root, dirs, files in os.walk(path):
        for filename in files:
            # os.access evaluates to False for some reason, so not using it
            if filename in apps:
                return os.path.realpath(os.path.join(root, filename))

def _extract(path, extdir=None, delete=False):
    """
    Takes in a tar or zip file and extracts it to extdir
    If extdir is not specified, extracts to os.path.dirname(path)
    If delete is set to True, deletes the bundle at path
    Returns the list of top level files that were extracted
    """
    if zipfile.is_zipfile(path):
        bundle = zipfile.ZipFile(path)
        namelist = bundle.namelist()
    elif tarfile.is_tarfile(path):
        bundle = tarfile.open(path)
        namelist = bundle.getnames()
    else:
        return
    if extdir is None:
        extdir = os.path.dirname(path)
    elif not os.path.exists(extdir):
        os.makedirs(extdir)
    bundle.extractall(path=extdir)
    bundle.close()
    if delete:
        os.remove(path)
    # namelist returns paths with forward slashes even in windows
    top_level_files = [os.path.join(extdir, name) for name in namelist
                             if len(name.rstrip('/').split('/')) == 1]
    # namelist doesn't include folders in windows, append these to the list
    if mozinfo.isWin:
        for name in namelist:
            root = name[:name.find('/')]
            if root not in top_level_files:
                top_level_files.append(root)
    return top_level_files

def _install_dmg(src, dest):
    proc = subprocess.Popen("hdiutil attach " + src,
                            shell=True,
                            stdout=subprocess.PIPE)
    try:
        for data in proc.communicate()[0].split():
            if data.find("/Volumes/") != -1:
                appDir = data
                break
        for appFile in os.listdir(appDir):
            if appFile.endswith(".app"):
                 appName = appFile
                 break
        subprocess.call("cp -r " + os.path.join(appDir, appName) + " " + dest,
                        shell=True)
    finally:
        subprocess.call("hdiutil detach " + appDir + " -quiet",
                        shell=True)
    return os.path.join(dest, appName)

def _install_exe(src, dest):
    # possibly gets around UAC in vista (still need to run as administrator)
    os.environ['__compat_layer'] = "RunAsInvoker"
    cmd = [src, "/S", "/D=" + os.path.realpath(dest)]
    subprocess.call(cmd)
    return dest

def cli(argv=sys.argv[1:]):
    parser = OptionParser()
    parser.add_option("-s", "--source",
                      dest="src",
                      help="Path to installation file. "
                           "Accepts: zip, exe, tar.bz2, tar.gz, and dmg")
    parser.add_option("-d", "--destination",
                      dest="dest",
                      default=None,
                      help="[optional] Directory to install application into")
    parser.add_option("--app", dest="app",
                      action="append",
                      default=_default_apps,
                      help="[optional] Application being installed. "
                           "Should be lowercase, e.g: "
                           "firefox, fennec, thunderbird, etc.")

    (options, args) = parser.parse_args(argv)
    if not options.src or not os.path.exists(options.src):
        print "Error: must specify valid source"
        return 2

    # Run it
    if os.path.isdir(options.src):
        binary = get_binary(options.src, apps=options.app)
    else:
        binary = install(options.src, dest=options.dest, apps=options.app)
    print binary

class InvalidSource(Exception):
    """
    Thrown when the specified source is not a recognized
    file type (zip, exe, tar.gz, tar.bz2 or dmg)
    """

class InstallError(Exception):
    """
    Thrown when the installation fails. Includes traceback
    if available.
    """

if __name__ == "__main__":
    sys.exit(cli())
