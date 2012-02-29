#!/usr/bin/env python
#
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
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is The Mozilla Foundation
# Portions created by the Initial Developer are Copyright (C) 2010
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#  Joel Maher <joel.maher@gmail.com>
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
# ***** END LICENSE BLOCK ***** */

import re, sys, os
import subprocess
import runxpcshelltests as xpcshell
from automationutils import *
import devicemanager, devicemanagerADB, devicemanagerSUT

# A specialization of XPCShellTests that runs tests on an Android device
# via devicemanager.
class XPCShellRemote(xpcshell.XPCShellTests, object):

    def __init__(self, devmgr, options, args):
        xpcshell.XPCShellTests.__init__(self)
        self.options = options
        self.device = devmgr
        self.pathMapping = []
        self.remoteTestRoot = self.device.getTestRoot("xpcshell")
        # Terse directory names are used here ("b" for a binaries directory)
        # to minimize the length of the command line used to execute
        # xpcshell on the remote device. adb has a limit to the number
        # of characters used in a shell command, and the xpcshell command
        # line can be quite complex.
        self.remoteBinDir = self.remoteJoin(self.remoteTestRoot, "b")
        self.remoteScriptsDir = self.remoteTestRoot
        self.remoteComponentsDir = self.remoteJoin(self.remoteTestRoot, "c")
        self.profileDir = self.remoteJoin(self.remoteTestRoot, "p")
        if options.setup:
          self.setupUtilities()
          self.setupTestDir()
        self.remoteAPK = self.remoteJoin(self.remoteBinDir, os.path.basename(options.localAPK))
        self.remoteDebugger = options.debugger
        self.remoteDebuggerArgs = options.debuggerArgs  
        self.setAppRoot()

    def setAppRoot(self):
        # Determine the application root directory associated with the package 
        # name used by the Fennec APK.
        self.appRoot = None
        packageName = None
        if self.options.localAPK:
          try:
            packageName = subprocess.check_output(["unzip", "-p", self.options.localAPK, "package-name.txt"])
            if packageName:
              self.appRoot = self.device.getAppRoot(packageName.strip())
          except Exception as detail:
            print "unable to determine app root: " + detail
            pass
        return None

    def remoteJoin(self, path1, path2):
        joined = os.path.join(path1, path2)
        joined = joined.replace('\\', '/')
        return joined

    def remoteForLocal(self, local):
        for mapping in self.pathMapping:
          if (os.path.abspath(mapping.local) == os.path.abspath(local)):
            return mapping.remote
        return local

    def setupUtilities(self):
        remotePrefDir = self.remoteJoin(self.remoteBinDir, "defaults/pref")
        if (not self.device.dirExists(remotePrefDir)):
          self.device.mkDirs(self.remoteJoin(remotePrefDir, "extra"))
        if (not self.device.dirExists(self.remoteScriptsDir)):
          self.device.mkDir(self.remoteScriptsDir)
        if (not self.device.dirExists(self.remoteComponentsDir)):
          self.device.mkDir(self.remoteComponentsDir)

        local = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'head.js')
        self.device.pushFile(local, self.remoteScriptsDir)

        localBin = os.path.join(self.options.objdir, "dist/bin")
        if not os.path.exists(localBin):
          localBin = os.path.join(self.options.objdir, "bin")
          if not os.path.exists(localBin):
            print >>sys.stderr, "Error: could not find bin in objdir"
            sys.exit(1)

        local = os.path.join(localBin, "xpcshell")
        self.device.pushFile(local, self.remoteBinDir)

        local = os.path.join(localBin, "components/httpd.js")
        self.device.pushFile(local, self.remoteComponentsDir)

        local = os.path.join(localBin, "components/httpd.manifest")
        self.device.pushFile(local, self.remoteComponentsDir)

        local = os.path.join(localBin, "components/test_necko.xpt")
        self.device.pushFile(local, self.remoteComponentsDir)

        self.device.pushFile(self.options.localAPK, self.remoteBinDir)

        localLib = os.path.join(self.options.objdir, "dist/fennec")
        if not os.path.exists(localLib):
          localLib = os.path.join(self.options.objdir, "fennec/lib")
          if not os.path.exists(localLib):
            print >>sys.stderr, "Error: could not find libs in objdir"
            sys.exit(1)

        for file in os.listdir(localLib):
          if (file.endswith(".so")):
            self.device.pushFile(os.path.join(localLib, file), self.remoteBinDir)

        # Additional libraries may be found in a sub-directory such as "lib/armeabi-v7a"
        localArmLib = os.path.join(localLib, "lib")
        if os.path.exists(localArmLib):
          for root, dirs, files in os.walk(localArmLib):
            for file in files:
              if (file.endswith(".so")):
                self.device.pushFile(os.path.join(root, file), self.remoteBinDir)

    def setupTestDir(self):
        xpcDir = os.path.join(self.options.objdir, "_tests/xpcshell")
        self.device.pushDir(xpcDir, self.remoteScriptsDir)

    def buildTestList(self):
        xpcshell.XPCShellTests.buildTestList(self)
        uniqueTestPaths = set([])
        for test in self.alltests:
          uniqueTestPaths.add(test['here'])
        for testdir in uniqueTestPaths:
          xpcDir = os.path.join(self.options.objdir, "_tests/xpcshell")
          abbrevTestDir = os.path.relpath(testdir, xpcDir)
          remoteScriptDir = self.remoteJoin(self.remoteScriptsDir, abbrevTestDir)
          self.pathMapping.append(PathMapping(testdir, remoteScriptDir))

    def buildXpcsCmd(self, testdir):
        self.xpcsCmd = [
           self.remoteJoin(self.remoteBinDir, "xpcshell"),
           '-r', self.remoteJoin(self.remoteComponentsDir, 'httpd.manifest'),
           '--greomni', self.remoteAPK,
           '-s',
           '-e', 'const _HTTPD_JS_PATH = "%s";' % self.remoteJoin(self.remoteComponentsDir, 'httpd.js'),
           '-e', 'const _HEAD_JS_PATH = "%s";' % self.remoteJoin(self.remoteScriptsDir, 'head.js'),
           '-f', self.remoteScriptsDir+'/head.js']

        if self.remoteDebugger:
          # for example, "/data/local/gdbserver" "localhost:12345"
          self.xpcsCmd = [
            self.remoteDebugger, 
            self.remoteDebuggerArgs, 
            self.xpcsCmd]

    def getHeadFiles(self, test):
        self.remoteHere = self.remoteForLocal(test['here'])
        return [f.strip() for f in sorted(test['head'].split(' ')) if self.device.fileExists(self.remoteJoin(self.remoteHere, f))]
    
    def getTailFiles(self, test):
        return [f.strip() for f in sorted(test['tail'].split(' ')) if self.device.fileExists(self.remoteJoin(self.remoteHere, f))]
        
    def buildCmdTestFile(self, name):
        remoteDir = self.remoteForLocal(os.path.dirname(name))
        if remoteDir == self.remoteHere:
          remoteName = os.path.basename(name)
        else:
          remoteName = self.remoteJoin(remoteDir, os.path.basename(name))
        return ['-e', 'const _TEST_FILE = ["%s"];' %
                 replaceBackSlashes(remoteName)]

    def setupProfileDir(self):
        self.device.removeDir(self.profileDir)
        self.device.mkDir(self.profileDir)
        if self.interactive or self.singleFile:
          self.log.info("TEST-INFO | profile dir is %s" % self.profileDir)
        return self.profileDir

    def launchProcess(self, cmd, stdout, stderr, env, cwd):
        cmd[0] = self.remoteJoin(self.remoteBinDir, "xpcshell")
        env = dict()
        env["LD_LIBRARY_PATH"]=self.remoteBinDir
        env["MOZ_LINKER_CACHE"]=self.remoteBinDir
        if (self.appRoot):
          env["GRE_HOME"]=self.appRoot
        env["XPCSHELL_TEST_PROFILE_DIR"]=self.profileDir
        outputFile = "xpcshelloutput"
        f = open(outputFile, "w+")
        self.shellReturnCode = self.device.shell(cmd, f, cwd=self.remoteHere, env=env)
        f.close()
        # The device manager may have timed out waiting for xpcshell. 
        # Guard against an accumulation of hung processes by killing
        # them here. Note also that IPC tests may spawn new instances
        # of xpcshell.
        self.device.killProcess(cmd[0]);
        self.device.killProcess("xpcshell");
        return outputFile

    def communicate(self, proc):
        f = open(proc, "r")
        contents = f.read()
        f.close()
        os.remove(proc)
        return contents, ""

    def getReturnCode(self, proc):
        if self.shellReturnCode is not None:
          return int(self.shellReturnCode)
        else:
          return -1

    def removeDir(self, dirname):
        self.device.removeDir(dirname)

    #TODO: consider creating a separate log dir.  We don't have the test file structure,
    #      so we use filename.log.  Would rather see ./logs/filename.log
    def createLogFile(self, test, stdout, leakLogs):
        try:
            f = None
            filename = test.replace('\\', '/').split('/')[-1] + ".log"
            f = open(filename, "w")
            f.write(stdout)

            for leakLog in leakLogs:
              if os.path.exists(leakLog):
                leaks = open(leakLog, "r")
                f.write(leaks.read())
                leaks.close()
        finally:
            if f <> None:
                f.close()

class RemoteXPCShellOptions(xpcshell.XPCShellOptions):

    def __init__(self):
        xpcshell.XPCShellOptions.__init__(self)
        defaults = {}

        self.add_option("--deviceIP", action="store",
                        type = "string", dest = "deviceIP",
                        help = "ip address of remote device to test")
        defaults["deviceIP"] = None
 
        self.add_option("--devicePort", action="store",
                        type = "string", dest = "devicePort",
                        help = "port of remote device to test")
        defaults["devicePort"] = 20701

        self.add_option("--dm_trans", action="store",
                        type = "string", dest = "dm_trans",
                        help = "the transport to use to communicate with device: [adb|sut]; default=sut")
        defaults["dm_trans"] = "sut"
 
        self.add_option("--objdir", action="store",
                        type = "string", dest = "objdir",
                        help = "local objdir, containing xpcshell binaries")
        defaults["objdir"] = None
 
        self.add_option("--apk", action="store",
                        type = "string", dest = "localAPK",
                        help = "local path to Fennec APK")
        defaults["localAPK"] = None

        self.add_option("--noSetup", action="store_false",
                        dest = "setup",
                        help = "do not copy any files to device (to be used only if device is already setup)")
        defaults["setup"] = True

        self.set_defaults(**defaults)

class PathMapping:

    def __init__(self, localDir, remoteDir):
        self.local = localDir
        self.remote = remoteDir

def main():

    dm_none = devicemanagerADB.DeviceManagerADB(None, None)
    parser = RemoteXPCShellOptions()
    options, args = parser.parse_args()

    if len(args) < 1 and options.manifest is None:
      print >>sys.stderr, """Usage: %s <test dirs>
           or: %s --manifest=test.manifest """ % (sys.argv[0], sys.argv[0])
      sys.exit(1)

    if (options.dm_trans == "adb"):
      if (options.deviceIP):
        dm = devicemanagerADB.DeviceManagerADB(options.deviceIP, options.devicePort)
      else:
        dm = dm_none
    else:
      dm = devicemanagerSUT.DeviceManagerSUT(options.deviceIP, options.devicePort)
      if (options.deviceIP == None):
        print "Error: you must provide a device IP to connect to via the --device option"
        sys.exit(1)

    if options.interactive and not options.testPath:
      print >>sys.stderr, "Error: You must specify a test filename in interactive mode!"
      sys.exit(1)

    if not options.objdir:
      print >>sys.stderr, "Error: You must specify an objdir"
      sys.exit(1)

    if not options.localAPK:
      for file in os.listdir(os.path.join(options.objdir, "dist")):
        if (file.endswith(".apk") and file.startswith("fennec")):
          options.localAPK = os.path.join(options.objdir, "dist")
          options.localAPK = os.path.join(options.localAPK, file)
          print >>sys.stderr, "using APK: " + options.localAPK
          break
      
    if not options.localAPK:
      print >>sys.stderr, "Error: please specify an APK"
      sys.exit(1)

    xpcsh = XPCShellRemote(dm, options, args)

    if not xpcsh.runTests(xpcshell='xpcshell', 
                          testdirs=args[0:], 
                          **options.__dict__):
      sys.exit(1)


if __name__ == '__main__':
  main()

