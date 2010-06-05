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
# The Original Code is MozMill Test code.
#
# The Initial Developer of the Original Code is Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2009
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Henrik Skupin <hskupin@mozilla.com>
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

import glob
import os
import shutil
import subprocess
import sys
import tempfile
import time

class Installer(object):

    def install(self, package=None, destination="./"):
        # Check for a valid install package
        if not os.path.isfile(package):
            str = "%s is not valid install package." % (package)
            raise Exception(str)

        destination = os.path.join(destination, "")
        fileName = os.path.basename(package)
        fileExt = os.path.splitext(package)[1]

        # Windows
        if sys.platform == "win32":
            # If there is an existing installation remove it now
            self.uninstall(destination)

            if fileExt == ".exe":
                print "Installing %s => %s" % (fileName, destination)
                cmdArgs = [package, "-ms", "/D=%s" % destination]
                result = subprocess.call(cmdArgs)
            else:
                raise Exception("File type %s not supported." % fileExt)

        # Linux
        elif sys.platform == "linux2":
            # If there is an existing installation remove it now
            self.uninstall(destination)

            if fileExt == ".bz2":
                print "Installing %s => %s" % (fileName, destination)
                try:
                    os.mkdir(destination);
                except:
                    pass

                cmdArgs = ["tar", "xjf", package, "-C", destination,
                           "--strip-components=1"]
                result = subprocess.call(cmdArgs)
            else:
                raise Exception("File type %s not supported." % fileExt)

        elif sys.platform == "darwin":
            # Mount disk image to a temporary mount point
            mountpoint = tempfile.mkdtemp()
            print "Mounting %s => %s" % (fileName, mountpoint)
            cmdArgs = ["hdiutil", "attach", package, "-mountpoint", mountpoint]
            result = subprocess.call(cmdArgs)

            if not result:
                # Copy application to destination folder
                try:
                    bundle = glob.glob(mountpoint + '/*.app')[0]
                    targetFolder = destination + os.path.basename(bundle)

                    # If there is an existing installation remove it now
                    self.uninstall(targetFolder)
                    print "Copying %s to %s" % (bundle, targetFolder)
                    shutil.copytree(bundle, targetFolder)

                    destination = targetFolder
                except Exception, e:
                    if not os.path.exists(targetFolder):
                        print "Failure in copying the application files"
                    raise e
                else:
                    # Unmount disk image
                    print "Unmounting %s..." % fileName
                    cmdArgs = ["hdiutil", "detach", mountpoint]
                    result = subprocess.call(cmdArgs)

        # Return the real installation folder
        return destination

    def uninstall(self, folder):
        ''' Uninstall the build in the given folder '''

        if sys.platform == "win32":
            # On Windows we try to use the uninstaller first
            log_file = "%suninstall\uninstall.log" % folder
            if os.path.exists(log_file):
                try:
                    print "Uninstalling %s" % folder
                    cmdArgs = ["%suninstall\helper.exe" % folder, "/S"]
                    result = subprocess.call(cmdArgs)
            
                    # The uninstaller spawns another process so the system call returns
                    # immediately. We have to wait until the uninstall folder has been
                    # removed or until we run into a timeout.
                    timeout = 20
                    while (os.path.exists(log_file) & (timeout > 0)):
                        time.sleep(1)
                        timeout -= 1
                except Exception, e:
                    pass

        # Check for an existent application.ini file so we don't recursively delete
        # the wrong folder tree.
        contents = "Contents/MacOS/" if sys.platform == "darwin" else ""
        if os.path.exists("%s/%sapplication.ini" % (folder, contents)):
            try:
                print "Removing old installation at %s" % (folder)
                shutil.rmtree(folder)

                # Wait at maximum 20s for the removal
                timeout = 20
                while (os.path.exists(folder) & (timeout > 0)):
                    time.sleep(1)
                    timeout -= 1
            except:
                print "Folder '%s' could not be removed" % (folder)
                pass
