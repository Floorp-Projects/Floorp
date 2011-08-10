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

import re, sys, os, os.path, logging, shutil, signal
from glob import glob
from optparse import OptionParser
from subprocess import Popen, PIPE, STDOUT
from tempfile import mkdtemp

import runxpcshelltests as xpcshell
from automationutils import *
import devicemanager

class XPCShellRemote(xpcshell.XPCShellTests, object):

    def __init__(self, devmgr):
        self.device = devmgr
        self.testRoot = "/tests/xpcshell"
        xpcshell.XPCShellTests.__init__(self)
        self.profileDir = self.testRoot + '/profile'
        self.device.mkDir(self.profileDir)

    #todo: figure out the remote version of this, only used for debuggerInfo
    def getcwd(self):
        return "/tests/"
        
    def readManifest(self, manifest):
        """Given a manifest file containing a list of test directories,
        return a list of absolute paths to the directories contained within."""

        manifestdir = self.testRoot + '/tests'
        testdirs = []
        try:
            f = self.device.getFile(manifest, "temp.txt")
            for line in f.split():
                dir = line.rstrip()
                path = manifestdir + '/' + dir
                testdirs.append(path)
            f.close()
        except:
            pass # just eat exceptions
        return testdirs

    def verifyFilePath(self, fileName):
        # approot - path to root of application - firefox or fennec
        # xreroot - path to xulrunner binaries - firefox or fennec/xulrunner
        # xpcshell - full or relative path to xpcshell binary
        #given fileName, returns full path of existing file
        if (self.device.fileExists(fileName)):
            return fileName
        
        fileName = self.device.getAppRoot() + '/xulrunner/' + fileName.split('/')[-1]
        if (self.device.fileExists(fileName)):
            return fileName
        
        fileName = self.device.getAppRoot() + '/' + fileName.split('/')[-1]
        if (not self.device.fileExists(fileName)):
            raise devicemanager.FileError("No File found for: " + str(fileName))

        return fileName

    def verifyDirPath(self, fileName):
        # approot - path to root of application - firefox or fennec
        # xreroot - path to xulrunner binaries - firefox or fennec/xulrunner
        # xpcshell - full or relative path to xpcshell binary
        #given fileName, returns full path of existing file
        if (self.device.dirExists(fileName)):
            return fileName
        
        fileName = self.device.getAppRoot() + '/' + fileName.split('/')[-1]
        if (self.device.dirExists(fileName)):
            return fileName
        
        fileName = self.device.getDeviceRoot() + '/' + fileName.split('/')[-1]
        if (not self.device.dirExists(fileName)):
            raise devicemanager.FileError("No Dir found for: " + str(fileName))
        return fileName

    def setAbsPath(self):
        #testharnessdir is used for head.js
        self.testharnessdir = "/tests/xpcshell/"

        # If the file exists then we have a full path (no notion of cwd)
        self.xpcshell = self.verifyFilePath(self.xpcshell)
        if self.xrePath is None:
            # If no xrePath, assume it is the directory containing xpcshell
            self.xrePath = '/'.join(self.xpcshell.split('/')[:-1])
        else:
            self.xrePath = self.verifyDirPath(self.xrePath)

        # we assume that httpd.js lives in components/ relative to xpcshell
        self.httpdJSPath = self.xrePath + '/components/httpd.js'

    def buildXpcsCmd(self, testdir):
        # <head.js> has to be loaded by xpchell: it can't load itself.
        self.env["XPCSHELL_TEST_PROFILE_DIR"] = self.profileDir
        self.xpcsCmd = [self.xpcshell, '-g', self.xrePath, '-v', '170', '-j', '-s', \
                        "--environ:CWD='" + testdir + "'", \
                        "--environ:XPCSHELL_TEST_PROFILE_DIR='" + self.env["XPCSHELL_TEST_PROFILE_DIR"] + "'", \
                        '-e', 'const _HTTPD_JS_PATH = \'%s\';' % self.httpdJSPath,
                        '-f', self.testharnessdir + '/head.js']

        if self.debuggerInfo:
            self.xpcsCmd = [self.debuggerInfo["path"]] + self.debuggerInfo["args"] + self.xpcsCmd

    def getHeadFiles(self, testdir):
        # get the list of head and tail files from the directory
        testHeadFiles = []
        for f in self.device.listFiles(testdir):
            hdmtch = re.compile("head_.*\.js")
            if (hdmtch.match(f)):
                testHeadFiles += [(testdir + '/' + f).replace('/', '//')]
                
        return sorted(testHeadFiles)
                
    def getTailFiles(self, testdir):
        testTailFiles = []
        # Tails are executed in the reverse order, to "match" heads order,
        # as in "h1-h2-h3 then t3-t2-t1".
        for f in self.device.listFiles(testdir):
            tlmtch = re.compile("tail_.*\.js")
            if (tlmtch.match(f)):
                testTailFiles += [(testdir + '/' + f).replace('/', '//')]
        return reversed(sorted(testTailFiles))
        
    def getTestFiles(self, testdir):
        testfiles = []
        # if a single test file was specified, we only want to execute that test
        for f in self.device.listFiles(testdir):
            tstmtch = re.compile("test_.*\.js")
            if (tstmtch.match(f)):
                testfiles += [(testdir + '/' + f).replace('/', '//')]
        
        for f in testfiles:
            if (self.singleFile == f.split('/')[-1]):
                return [(testdir + '/' + f).replace('/', '//')]
            else:
                pass
        return testfiles

    def setupProfileDir(self):
        self.device.removeDir(self.profileDir)
        self.device.mkDir(self.profileDir)
        self.env["XPCSHELL_TEST_PROFILE_DIR"] = self.profileDir
        return self.profileDir

    def setupLeakLogging(self):
        filename = "runxpcshelltests_leaks.log"
        
        # Enable leaks (only) detection to its own log file.
        leakLogFile = self.profileDir + '/' + filename
        self.env["XPCOM_MEM_LEAK_LOG"] = leakLogFile
        return leakLogFile

    def launchProcess(self, cmd, stdout, stderr, env, cwd):
        print "launching : " + " ".join(cmd)
        proc = self.device.launchProcess(cmd, cwd=cwd)
        return proc

    def setSignal(self, proc, sig1, sig2):
        self.device.signal(proc, sig1, sig2)

    def communicate(self, proc):
        return self.device.communicate(proc)

    def removeDir(self, dirname):
        self.device.removeDir(dirname)

    def getReturnCode(self, proc):
        return self.device.getReturnCode(proc)

    #TODO: consider creating a separate log dir.  We don't have the test file structure,
    #      so we use filename.log.  Would rather see ./logs/filename.log
    def createLogFile(self, test, stdout):
        try:
            f = None
            filename = test.replace('\\', '/').split('/')[-1] + ".log"
            f = open(filename, "w")
            f.write(stdout)

            if os.path.exists(self.leakLogFile):
                leaks = open(self.leakLogFile, "r")
                f.write(leaks.read())
                leaks.close()
        finally:
            if f <> None:
                f.close()

    #NOTE: the only difference between this and parent is the " vs ' arond the filename
    def buildCmdHead(self, headfiles, tailfiles, xpcscmd):
        cmdH = ", ".join(['\'' + f.replace('\\', '/') + '\''
                       for f in headfiles])
        cmdT = ", ".join(['\'' + f.replace('\\', '/') + '\''
                       for f in tailfiles])
        cmdH = xpcscmd + \
                ['-e', 'const _HEAD_FILES = [%s];' % cmdH] + \
                ['-e', 'const _TAIL_FILES = [%s];' % cmdT]
        return cmdH

class RemoteXPCShellOptions(xpcshell.XPCShellOptions):

  def __init__(self):
    xpcshell.XPCShellOptions.__init__(self)
    self.add_option("--device",
                    type="string", dest="device", default='',
                    help="ip address for the device")


def main():

  parser = RemoteXPCShellOptions()
  options, args = parser.parse_args()

  if len(args) < 2 and options.manifest is None or \
     (len(args) < 1 and options.manifest is not None):
     print "len(args): " + str(len(args))
     print >>sys.stderr, """Usage: %s <path to xpcshell> <test dirs>
           or: %s --manifest=test.manifest <path to xpcshell>""" % (sys.argv[0],
                                                           sys.argv[0])
     sys.exit(1)

  if (options.device == ''):
    print >>sys.stderr, "Error: Please provide an ip address for the remote device with the --device option"
    sys.exit(1)


  dm = devicemanager.DeviceManager(options.device, 20701)
  xpcsh = XPCShellRemote(dm)
  debuggerInfo = getDebuggerInfo(xpcsh.oldcwd, options.debugger, options.debuggerArgs,
    options.debuggerInteractive);

  if options.interactive and not options.testPath:
    print >>sys.stderr, "Error: You must specify a test filename in interactive mode!"
    sys.exit(1)

  # Zip up the xpcshell directory: 7z a <zipName> xpcshell/*, assuming we are in the xpcshell directory
  # TODO: ensure the system has 7z, this is adding a new dependency to the overall system
  zipName = 'xpcshell.7z'
  try:
    Popen(['7z', 'a', zipName, '../xpcshell']).wait()
  except:
    print "to run these tests remotely, we require 7z to be installed and in your path"
    sys.exit(1)

  if dm.pushFile(zipName, '/tests/xpcshell.7z') == None:
     raise devicemanager.FileError("failed to copy xpcshell.7z to device")
  if dm.unpackFile('xpcshell.7z') == None:
     raise devicemanager.FileError("failed to unpack xpcshell.7z on the device")

  if not xpcsh.runTests(args[0],
                        xrePath=options.xrePath,
                        symbolsPath=options.symbolsPath,
                        manifest=options.manifest,
                        testdirs=args[1:],
                        testPath=options.testPath,
                        interactive=options.interactive,
                        logfiles=options.logfiles,
                        debuggerInfo=debuggerInfo):
    sys.exit(1)

if __name__ == '__main__':
  main()



