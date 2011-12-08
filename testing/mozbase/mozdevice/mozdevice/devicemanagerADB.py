import subprocess
from devicemanager import DeviceManager, DMError
import re
import os
import sys

class DeviceManagerADB(DeviceManager):

  def __init__(self, host = None, port = 20701, retrylimit = 5, packageName = None):
    self.host = host
    self.port = port
    self.retrylimit = retrylimit
    self.retries = 0
    self._sock = None
    self.useRunAs = False
    self.packageName = None
    if packageName == None:
      if os.getenv('USER'):
        packageName = 'org.mozilla.fennec_' + os.getenv('USER')
      else:
        packageName = 'org.mozilla.fennec_'
    self.Init(packageName)

  def Init(self, packageName):
    # Initialization code that may fail: Catch exceptions here to allow
    # successful initialization even if, for example, adb is not installed.
    try:
      self.verifyADB()
      self.verifyRunAs(packageName)
    except:
      self.useRunAs = False
      self.packageName = None
    try:
      # a test to see if we have root privs
      files = self.listFiles("/data/data")
      if (len(files) == 1):
        if (files[0].find("Permission denied") != -1):
          print "NOT running as root"
          raise Exception("not running as root")
    except:
      try:
        self.checkCmd(["root"])
      except:
        print "restarting as root failed"

  # external function
  # returns:
  #  success: True
  #  failure: False
  def pushFile(self, localname, destname):
    try:
      if (os.name == "nt"):
        destname = destname.replace('\\', '/')
      if (self.useRunAs):
        remoteTmpFile = self.tmpDir + "/" + os.path.basename(localname)
        self.checkCmd(["push", os.path.realpath(localname), remoteTmpFile])
        self.checkCmdAs(["shell", "cp", remoteTmpFile, destname])
        self.checkCmd(["shell", "rm", remoteTmpFile])
      else:
        self.checkCmd(["push", os.path.realpath(localname), destname])
      if (self.isDir(destname)):
        destname = destname + "/" + os.path.basename(localname)
      self.chmodDir(destname)
      return True
    except:
      return False

  # external function
  # returns:
  #  success: directory name
  #  failure: None
  def mkDir(self, name):
    try:
      self.checkCmdAs(["shell", "mkdir", name])
      self.chmodDir(name)
      return name
    except:
      return None

  # make directory structure on the device
  # external function
  # returns:
  #  success: directory structure that we created
  #  failure: None
  def mkDirs(self, filename):
    parts = filename.split('/')
    name = ""
    for part in parts:
      if (part == parts[-1]): break
      if (part != ""):
        name += '/' + part
        if (not self.dirExists(name)):
          if (self.mkDir(name) == None):
            print "failed making directory: " + str(name)
            return None
    return name

  # push localDir from host to remoteDir on the device
  # external function
  # returns:
  #  success: remoteDir
  #  failure: None
  def pushDir(self, localDir, remoteDir):
    # adb "push" accepts a directory as an argument, but if the directory
    # contains symbolic links, the links are pushed, rather than the linked
    # files; we push file-by-file to get around this limitation
    try:
      if (not self.dirExists(remoteDir)):
        self.mkDirs(remoteDir+"/x")
      for root, dirs, files in os.walk(localDir, followlinks='true'):
        relRoot = os.path.relpath(root, localDir)
        for file in files:
          localFile = os.path.join(root, file)
          remoteFile = remoteDir + "/"
          if (relRoot!="."):
            remoteFile = remoteFile + relRoot + "/"
          remoteFile = remoteFile + file
          self.pushFile(localFile, remoteFile)
        for dir in dirs:
          targetDir = remoteDir + "/"
          if (relRoot!="."):
            targetDir = targetDir + relRoot + "/"
          targetDir = targetDir + dir
          if (not self.dirExists(targetDir)):
            self.mkDir(targetDir)
      self.checkCmdAs(["shell", "chmod", "777", remoteDir])
      return True
    except:
      print "pushing " + localDir + " to " + remoteDir + " failed"
      return False

  # external function
  # returns:
  #  success: True
  #  failure: False
  def dirExists(self, dirname):
    return self.isDir(dirname)

  # Because we always have / style paths we make this a lot easier with some
  # assumptions
  # external function
  # returns:
  #  success: True
  #  failure: False
  def fileExists(self, filepath):
    p = self.runCmd(["shell", "ls", "-a", filepath])
    data = p.stdout.readlines()
    if (len(data) == 1):
      if (data[0].rstrip() == filepath):
        return True
    return False

  def removeFile(self, filename):
    return self.runCmd(["shell", "rm", filename]).stdout.read()

  # does a recursive delete of directory on the device: rm -Rf remoteDir
  # external function
  # returns:
  #  success: output of telnet, i.e. "removing file: /mnt/sdcard/tests/test.txt"
  #  failure: None
  def removeSingleDir(self, remoteDir):
    return self.runCmd(["shell", "rmdir", remoteDir]).stdout.read()

  # does a recursive delete of directory on the device: rm -Rf remoteDir
  # external function
  # returns:
  #  success: output of telnet, i.e. "removing file: /mnt/sdcard/tests/test.txt"
  #  failure: None
  def removeDir(self, remoteDir):
      out = ""
      if (self.isDir(remoteDir)):
          files = self.listFiles(remoteDir.strip())
          for f in files:
              if (self.isDir(remoteDir.strip() + "/" + f.strip())):
                  out += self.removeDir(remoteDir.strip() + "/" + f.strip())
              else:
                  out += self.removeFile(remoteDir.strip() + "/" + f.strip())
          out += self.removeSingleDir(remoteDir.strip())
      else:
          out += self.removeFile(remoteDir.strip())
      return out

  def isDir(self, remotePath):
      p = self.runCmd(["shell", "ls", "-a", remotePath])
      data = p.stdout.readlines()
      if (len(data) == 0):
          return True
      if (len(data) == 1):
          if (data[0].rstrip() == remotePath):
              return False
          if (data[0].find("No such file or directory") != -1):
              return False
          if (data[0].find("Not a directory") != -1):
              return False
      return True

  def listFiles(self, rootdir):
      p = self.runCmd(["shell", "ls", "-a", rootdir])
      data = p.stdout.readlines()
      if (len(data) == 1):
          if (data[0] == rootdir):
              return []
          if (data[0].find("No such file or directory") != -1):
              return []
          if (data[0].find("Not a directory") != -1):
              return []
      return data

  # external function
  # returns:
  #  success: array of process tuples
  #  failure: []
  def getProcessList(self):
    p = self.runCmd(["shell", "ps"])
      # first line is the headers
    p.stdout.readline()
    proc = p.stdout.readline()
    ret = []
    while (proc):
      els = proc.split()
      ret.append(list([els[1], els[len(els) - 1], els[0]]))
      proc =  p.stdout.readline()
    return ret

  # external function
  # returns:
  #  success: pid
  #  failure: None
  def fireProcess(self, appname, failIfRunning=False):
    #strip out env vars
    parts = appname.split('"');
    if (len(parts) > 2):
      parts = parts[2:]
    return self.launchProcess(parts, failIfRunning)

  # external function
  # returns:
  #  success: output filename
  #  failure: None
  def launchProcess(self, cmd, outputFile = "process.txt", cwd = '', env = '', failIfRunning=False):
    acmd = ["shell", "am","start"]
    cmd = ' '.join(cmd).strip()
    i = cmd.find(" ")
    acmd.append("-n")
    acmd.append(cmd[0:i] + "/.App")
    acmd.append("--es")
    acmd.append("args")
    acmd.append(cmd[i:])
    print acmd
    self.checkCmd(acmd)
    return outputFile;

  # external function
  # returns:
  #  success: output from testagent
  #  failure: None
  def killProcess(self, appname):
    procs = self.getProcessList()
    for (pid, name, user) in procs:
      if name == appname:
        p = self.runCmdAs(["shell", "kill", pid])
        return p.stdout.read()
    return None

  # external function
  # returns:
  #  success: filecontents
  #  failure: None
  def catFile(self, remoteFile):
    #p = self.runCmd(["shell", "cat", remoteFile])
    #return p.stdout.read()
    return self.getFile(remoteFile)

  # external function
  # returns:
  #  success: output of pullfile, string
  #  failure: None
  def pullFile(self, remoteFile):
    #return self.catFile(remoteFile)
    return self.getFile(remoteFile)

  # copy file from device (remoteFile) to host (localFile)
  # external function
  # returns:
  #  success: output of pullfile, string
  #  failure: None
  def getFile(self, remoteFile, localFile = 'tmpfile_dm_adb'):
    # TODO: add debug flags and allow for printing stdout
    # self.runCmd(["pull", remoteFile, localFile])
    try:
      self.runCmd(["pull",  remoteFile, localFile]).stdout.read()
      f = open(localFile)
      ret = f.read()
      f.close()
      return ret;      
    except:
      return None

  # copy directory structure from device (remoteDir) to host (localDir)
  # external function
  # checkDir exists so that we don't create local directories if the
  # remote directory doesn't exist but also so that we don't call isDir
  # twice when recursing.
  # returns:
  #  success: list of files, string
  #  failure: None
  def getDirectory(self, remoteDir, localDir, checkDir=True):
    ret = []
    p = self.runCmd(["pull", remoteDir, localDir])
    p.stderr.readline()
    line = p.stderr.readline()
    while (line):
      els = line.split()
      f = els[len(els) - 1]
      i = f.find(localDir)
      if (i != -1):
        if (localDir[len(localDir) - 1] != '/'):
          i = i + 1
        f = f[i + len(localDir):]
      i = f.find("/")
      if (i > 0):
        f = f[0:i]
      ret.append(f)
      line =  p.stderr.readline()
    #the last line is a summary
    if (len(ret) > 0):
      ret.pop()
    return ret



  # true/false check if the two files have the same md5 sum
  # external function
  # returns:
  #  success: True
  #  failure: False
  def validateFile(self, remoteFile, localFile):
    return self.getRemoteHash(remoteFile) == self.getLocalHash(localFile)

  # return the md5 sum of a remote file
  # internal function
  # returns:
  #  success: MD5 hash for given filename
  #  failure: None
  def getRemoteHash(self, filename):
    data = p = self.runCmd(["shell", "ls", "-l", filename]).stdout.read()
    return data.split()[3]

  def getLocalHash(self, filename):
    data = p = subprocess.Popen(["ls", "-l", filename], stdout=subprocess.PIPE).stdout.read()
    return data.split()[4]

  # Gets the device root for the testing area on the device
  # For all devices we will use / type slashes and depend on the device-agent
  # to sort those out.  The agent will return us the device location where we
  # should store things, we will then create our /tests structure relative to
  # that returned path.
  # Structure on the device is as follows:
  # /tests
  #       /<fennec>|<firefox>  --> approot
  #       /profile
  #       /xpcshell
  #       /reftest
  #       /mochitest
  #
  # external function
  # returns:
  #  success: path for device root
  #  failure: None
  def getDeviceRoot(self):
    # /mnt/sdcard/tests is preferred to /data/local/tests, but this can be
    # over-ridden by creating /data/local/tests
    testRoot = "/data/local/tests"
    if (self.dirExists(testRoot)):
      return testRoot
    root = "/mnt/sdcard"
    if (not self.dirExists(root)):
      root = "/data/local"
    testRoot = root + "/tests"
    if (not self.dirExists(testRoot)):
      self.mkDir(testRoot)
    return testRoot

  # Either we will have /tests/fennec or /tests/firefox but we will never have
  # both.  Return the one that exists
  # TODO: ensure we can support org.mozilla.firefox
  # external function
  # returns:
  #  success: path for app root
  #  failure: None
  def getAppRoot(self):
    devroot = self.getDeviceRoot()
    if (devroot == None):
      return None

    if (self.dirExists(devroot + '/fennec')):
      return devroot + '/fennec'
    elif (self.dirExists(devroot + '/firefox')):
      return devroot + '/firefox'
    elif (self.packageName and self.dirExists('/data/data/' + self.packageName)):
      return '/data/data/' + self.packageName

    # Failure (either not installed or not a recognized platform)
    print "devicemanagerADB: getAppRoot failed"
    return None

  # Gets the directory location on the device for a specific test type
  # Type is one of: xpcshell|reftest|mochitest
  # external function
  # returns:
  #  success: path for test root
  #  failure: None
  def getTestRoot(self, type):
    devroot = self.getDeviceRoot()
    if (devroot == None):
      return None

    if (re.search('xpcshell', type, re.I)):
      self.testRoot = devroot + '/xpcshell'
    elif (re.search('?(i)reftest', type)):
      self.testRoot = devroot + '/reftest'
    elif (re.search('?(i)mochitest', type)):
      self.testRoot = devroot + '/mochitest'
    return self.testRoot


  # external function
  # returns:
  #  success: status from test agent
  #  failure: None
  def reboot(self, wait = False):
    ret = self.runCmd(["reboot"]).stdout.read()
    if (not wait):
      return "Success"
    countdown = 40
    while (countdown > 0):
      countdown
      try:
        self.checkCmd(["wait-for-device", "shell", "ls", "/sbin"])
        return ret
      except:
        try:
          self.checkCmd(["root"])
        except:
          time.sleep(1)
          print "couldn't get root"
    return "Success"

  # external function
  # returns:
  #  success: text status from command or callback server
  #  failure: None
  def updateApp(self, appBundlePath, processName=None, destPath=None, ipAddr=None, port=30000):
    return self.runCmd(["install", "-r", appBundlePath]).stdout.read()

  # external function
  # returns:
  #  success: time in ms
  #  failure: None
  def getCurrentTime(self):
    timestr = self.runCmd(["shell", "date", "+%s"]).stdout.read().strip()
    if (not timestr or not timestr.isdigit()):
        return None
    return str(int(timestr)*1000)

  # Returns information about the device:
  # Directive indicates the information you want to get, your choices are:
  # os - name of the os
  # id - unique id of the device
  # uptime - uptime of the device
  # systime - system time of the device
  # screen - screen resolution
  # memory - memory stats
  # process - list of running processes (same as ps)
  # disk - total, free, available bytes on disk
  # power - power status (charge, battery temp)
  # all - all of them - or call it with no parameters to get all the information
  # returns:
  #   success: dict of info strings by directive name
  #   failure: {}
  def getInfo(self, directive="all"):
    ret = {}
    if (directive == "id" or directive == "all"):
      ret["id"] = self.runCmd(["get-serialno"]).stdout.read()
    if (directive == "os" or directive == "all"):
      ret["os"] = self.runCmd(["shell", "getprop", "ro.build.display.id"]).stdout.read()
    if (directive == "uptime" or directive == "all"):
      utime = self.runCmd(["shell", "uptime"]).stdout.read()
      if (not utime):
        raise DMError("error getting uptime")
      utime = utime[9:]
      hours = utime[0:utime.find(":")]
      utime = utime[utime[1:].find(":") + 2:]
      minutes = utime[0:utime.find(":")]
      utime = utime[utime[1:].find(":") +  2:]
      seconds = utime[0:utime.find(",")]
      ret["uptime"] = ["0 days " + hours + " hours " + minutes + " minutes " + seconds + " seconds"]
    if (directive == "process" or directive == "all"):
      ret["process"] = self.runCmd(["shell", "ps"]).stdout.read()
    if (directive == "systime" or directive == "all"):
      ret["systime"] = self.runCmd(["shell", "date"]).stdout.read()
    print ret
    return ret

  def runCmd(self, args):
    args.insert(0, "adb")
    return subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

  def runCmdAs(self, args):
    if self.useRunAs:
      args.insert(1, "run-as")
      args.insert(2, self.packageName)
    return self.runCmd(args)

  def checkCmd(self, args):
    args.insert(0, "adb")
    return subprocess.check_call(args)

  def checkCmdAs(self, args):
    if (self.useRunAs):
      args.insert(1, "run-as")
      args.insert(2, self.packageName)
    return self.checkCmd(args)

  def chmodDir(self, remoteDir):
    if (self.isDir(remoteDir)):
      files = self.listFiles(remoteDir.strip())
      for f in files:
        if (self.isDir(remoteDir.strip() + "/" + f.strip())):
          self.chmodDir(remoteDir.strip() + "/" + f.strip())
        else:
          self.checkCmdAs(["shell", "chmod", "777", remoteDir.strip()])
          print "chmod " + remoteDir.strip()
      self.checkCmdAs(["shell", "chmod", "777", remoteDir])
      print "chmod " + remoteDir
    else:
      self.checkCmdAs(["shell", "chmod", "777", remoteDir.strip()])
      print "chmod " + remoteDir.strip()

  def verifyADB(self):
    # Check to see if adb itself can be executed.
    try:
      self.runCmd(["version"])
    except Exception as (ex):
      print "unable to execute ADB: ensure Android SDK is installed and adb is in your $PATH"
    
  def isCpAvailable(self):
    # Some Android systems may not have a cp command installed,
    # or it may not be executable by the user. 
    data = self.runCmd(["shell", "cp"]).stdout.read()
    if (re.search('Usage', data)):
      return True
    else:
      print "unable to execute 'cp' on device; consider installing busybox from Android Market"
      return False

  def verifyRunAs(self, packageName):
    # If a valid package name is available, and certain other
    # conditions are met, devicemanagerADB can execute file operations
    # via the "run-as" command, so that pushed files and directories 
    # are created by the uid associated with the package, more closely
    # echoing conditions encountered by Fennec at run time.
    # Check to see if run-as can be used here, by verifying a 
    # file copy via run-as.
    self.useRunAs = False
    devroot = self.getDeviceRoot()
    if (packageName and self.isCpAvailable() and devroot):
      self.tmpDir = devroot + "/tmp"
      if (not self.dirExists(self.tmpDir)):
        self.mkDir(self.tmpDir)
      self.checkCmd(["shell", "run-as", packageName, "mkdir", devroot + "/sanity"])
      self.checkCmd(["push", os.path.abspath(sys.argv[0]), self.tmpDir + "/tmpfile"])
      self.checkCmd(["shell", "run-as", packageName, "cp", self.tmpDir + "/tmpfile", devroot + "/sanity"])
      if (self.fileExists(devroot + "/sanity/tmpfile")):
        print "will execute commands via run-as " + packageName
        self.packageName = packageName
        self.useRunAs = True
      self.checkCmd(["shell", "rm", devroot + "/tmp/tmpfile"])
      self.checkCmd(["shell", "run-as", packageName, "rm", "-r", devroot + "/sanity"])
      
