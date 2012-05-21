#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys
import os
import re
import types
from optparse import OptionValueError
from subprocess import PIPE
from time import sleep
from tempfile import mkstemp

sys.path.insert(0, os.path.abspath(os.path.realpath(
  os.path.dirname(sys.argv[0]))))

from automation import Automation
from runtests import Mochitest, MochitestOptions

class VMwareOptions(MochitestOptions):
  def __init__(self, automation, mochitest, **kwargs):
    defaults = {}
    MochitestOptions.__init__(self, automation, mochitest.SCRIPT_DIRECTORY)

    def checkPathCallback(option, opt_str, value, parser):
      path = mochitest.getFullPath(value)
      if not os.path.exists(path):
        raise OptionValueError("Path %s does not exist for %s option"
                               % (path, opt_str))
      setattr(parser.values, option.dest, path)

    self.add_option("--with-vmware-vm",
                    action = "callback", type = "string", dest = "vmx",
                    callback = checkPathCallback,
                    help = "launches the given VM and runs mochitests inside")
    defaults["vmx"] = None

    self.add_option("--with-vmrun-executable",
                    action = "callback", type = "string", dest = "vmrun",
                    callback = checkPathCallback,
                    help = "specifies the vmrun.exe to use for VMware control")
    defaults["vmrun"] = None
 
    self.add_option("--shutdown-vm-when-done",
                    action = "store_true", dest = "shutdownVM",
                    help = "shuts down the VM when mochitests complete")
    defaults["shutdownVM"] = False

    self.add_option("--repeat-until-failure",
                    action = "store_true", dest = "repeatUntilFailure",
                    help = "Runs tests continuously until failure")
    defaults["repeatUntilFailure"] = False

    self.set_defaults(**defaults)

class VMwareMochitest(Mochitest):
  _pathFixRegEx = re.compile(r'^[cC](\:[\\\/]+)')

  def convertHostPathsToGuestPaths(self, string):
    """ converts a path on the host machine to a path on the guest machine """
    # XXXbent Lame!
    return self._pathFixRegEx.sub(r'z\1', string)

  def prepareGuestArguments(self, parser, options):
    """ returns an array of command line arguments needed to replicate the
        current set of options in the guest """
    args = []
    for key in options.__dict__.keys():
      # Don't send these args to the vm test runner!
      if key == "vmrun" or key == "vmx" or key == "repeatUntilFailure":
        continue

      value = options.__dict__[key]
      valueType = type(value)

      # Find the option in the parser's list.
      option = None
      for index in range(len(parser.option_list)):
        if str(parser.option_list[index].dest) == key:
          option = parser.option_list[index]
          break
      if not option:
        continue

      # No need to pass args on the command line if they're just going to set
      # default values. The exception is list values... For some reason the
      # option parser modifies the defaults as well as the values when using the
      # "append" action.
      if value == parser.defaults[option.dest]:
        if valueType == types.StringType and \
           value == self.convertHostPathsToGuestPaths(value):
          continue
        if valueType != types.ListType:
          continue

      def getArgString(arg, option):
        if option.action == "store_true" or option.action == "store_false":
          return str(option)
        return "%s=%s" % (str(option),
                          self.convertHostPathsToGuestPaths(str(arg)))

      if valueType == types.ListType:
        # Expand lists into separate args.
        for item in value:
          args.append(getArgString(item, option))
      else:
        args.append(getArgString(value, option))

    return tuple(args)

  def launchVM(self, options):
    """ launches the VM and enables shared folders """
    # Launch VM first.
    self.automation.log.info("INFO | runtests.py | Launching the VM.")
    (result, stdout) = self.runVMCommand(self.vmrunargs + ("start", self.vmx))
    if result:
      return result

    # Make sure that shared folders are enabled.
    self.automation.log.info("INFO | runtests.py | Enabling shared folders in "
                             "the VM.")
    (result, stdout) = self.runVMCommand(self.vmrunargs + \
                                         ("enableSharedFolders", self.vmx))
    if result:
      return result

  def shutdownVM(self):
    """ shuts down the VM """
    self.automation.log.info("INFO | runtests.py | Shutting down the VM.")
    command = self.vmrunargs + ("runProgramInGuest", self.vmx,
              "c:\\windows\\system32\\shutdown.exe", "/s", "/t", "1")
    (result, stdout) = self.runVMCommand(command)
    return result

  def runVMCommand(self, command, expectedErrors=[], silent=False):
    """ runs a command in the VM using the vmrun.exe helper """
    commandString = ""
    for part in command:
      commandString += str(part) + " "
    if not silent:
      self.automation.log.info("INFO | runtests.py | Running command: %s"
                               % commandString)

    commonErrors = ["Error: Invalid user name or password for the guest OS",
                    "Unable to connect to host."]
    expectedErrors.extend(commonErrors)

    # VMware can't run commands until the VM has fully loaded so keep running
    # this command in a loop until it succeeds or we try 100 times.
    errorString = ""
    for i in range(100):
      process = Automation.Process(command, stdout=PIPE)
      result = process.wait()
      if result == 0:
        break

      for line in process.stdout.readlines():
        line = line.strip()
        if not line:
          continue
        errorString = line
        break

      expected = False
      for error in expectedErrors:
        if errorString.startswith(error):
          expected = True

      if not expected:
        self.automation.log.warning("WARNING | runtests.py | Command \"%s\" "
                                    "failed with result %d, : %s"
                                    % (commandString, result, errorString))
        break

      if not silent:
        self.automation.log.info("INFO | runtests.py | Running command again.")

    return (result, process.stdout.readlines())

  def monitorVMExecution(self, appname, logfilepath):
    """ monitors test execution in the VM. Waits for the test process to start,
        then watches the log file for test failures and checks the status of the
        process to catch crashes. Returns True if mochitests ran successfully.
    """
    success = True

    self.automation.log.info("INFO | runtests.py | Waiting for test process to "
                             "start.")

    listProcessesCommand = self.vmrunargs + ("listProcessesInGuest", self.vmx)
    expectedErrors = [ "Error: The virtual machine is not powered on" ]

    running  = False
    for i in range(100):
      (result, stdout) = self.runVMCommand(listProcessesCommand, expectedErrors,
                                           silent=True)
      if result:
        self.automation.log.warning("WARNING | runtests.py | Failed to get "
                                    "list of processes in VM!")
        return False
      for line in stdout:
        line = line.strip()
        if line.find(appname) != -1:
          running = True
          break
      if running:
        break
      sleep(1)

    self.automation.log.info("INFO | runtests.py | Found test process, "
                             "monitoring log.")

    completed = False
    nextLine = 0
    while running:
      log = open(logfilepath, "rb")
      lines = log.readlines()
      if len(lines) > nextLine:
        linesToPrint = lines[nextLine:]
        for line in linesToPrint:
          line = line.strip()
          if line.find("INFO SimpleTest FINISHED") != -1:
            completed = True
            continue
          if line.find("ERROR TEST-UNEXPECTED-FAIL") != -1:
            self.automation.log.info("INFO | runtests.py | Detected test "
                                     "failure: \"%s\"" % line)
            success = False
        nextLine = len(lines)
      log.close()

      (result, stdout) = self.runVMCommand(listProcessesCommand, expectedErrors,
                                           silent=True)
      if result:
        self.automation.log.warning("WARNING | runtests.py | Failed to get "
                                    "list of processes in VM!")
        return False

      stillRunning = False
      for line in stdout:
        line = line.strip()
        if line.find(appname) != -1:
          stillRunning = True
          break
      if stillRunning:
        sleep(5)
      else:
        if not completed:
          self.automation.log.info("INFO | runtests.py | Test process exited "
                                   "without finishing tests, maybe crashed.")
          success = False
        running = stillRunning

    return success

  def getCurentSnapshotList(self):
    """ gets a list of snapshots from the VM """
    (result, stdout) = self.runVMCommand(self.vmrunargs + ("listSnapshots",
                                                           self.vmx))
    snapshots = []
    if result != 0:
      self.automation.log.warning("WARNING | runtests.py | Failed to get list "
                                  "of snapshots in VM!")
      return snapshots
    for line in stdout:
      if line.startswith("Total snapshots:"):
        continue
      snapshots.append(line.strip())
    return snapshots

  def runTests(self, parser, options):
    """ runs mochitests in the VM """
    # Base args that must always be passed to vmrun.
    self.vmrunargs = (options.vmrun, "-T", "ws", "-gu", "Replay", "-gp",
                      "mozilla")
    self.vmrun = options.vmrun
    self.vmx = options.vmx

    result = self.launchVM(options)
    if result:
      return result

    if options.vmwareRecording:
      snapshots = self.getCurentSnapshotList()

    def innerRun():
      """ subset of the function that must run every time if we're running until
          failure """
      # Make a new shared file for the log file.
      (logfile, logfilepath) = mkstemp(suffix=".log")
      os.close(logfile)
      # Get args to pass to VM process. Make sure we autorun and autoclose.
      options.autorun = True
      options.closeWhenDone = True
      options.logFile = logfilepath
      self.automation.log.info("INFO | runtests.py | Determining guest "
                               "arguments.")
      runtestsArgs = self.prepareGuestArguments(parser, options)
      runtestsPath = self.convertHostPathsToGuestPaths(self.SCRIPT_DIRECTORY)
      runtestsPath = os.path.join(runtestsPath, "runtests.py")
      runtestsCommand = self.vmrunargs + ("runProgramInGuest", self.vmx,
                        "-activeWindow", "-interactive", "-noWait",
                        "c:\\mozilla-build\\python25\\python.exe",
                        runtestsPath) + runtestsArgs
      expectedErrors = [ "Unable to connect to host.",
                         "Error: The virtual machine is not powered on" ]
      self.automation.log.info("INFO | runtests.py | Launching guest test "
                               "runner.")
      (result, stdout) = self.runVMCommand(runtestsCommand, expectedErrors)
      if result:
        return (result, False)
      self.automation.log.info("INFO | runtests.py | Waiting for guest test "
                               "runner to complete.")
      mochitestsSucceeded = self.monitorVMExecution(
        os.path.basename(options.app), logfilepath)
      if mochitestsSucceeded:
        self.automation.log.info("INFO | runtests.py | Guest tests passed!")
      else:
        self.automation.log.info("INFO | runtests.py | Guest tests failed.")
      if mochitestsSucceeded and options.vmwareRecording:
        newSnapshots = self.getCurentSnapshotList()
        if len(newSnapshots) > len(snapshots):
          self.automation.log.info("INFO | runtests.py | Removing last "
                                   "recording.")
          (result, stdout) = self.runVMCommand(self.vmrunargs + \
                                               ("deleteSnapshot", self.vmx,
                                                newSnapshots[-1]))
      self.automation.log.info("INFO | runtests.py | Removing guest log file.")
      for i in range(30):
        try:
          os.remove(logfilepath)
          break
        except:
          sleep(1)
          self.automation.log.warning("WARNING | runtests.py | Couldn't remove "
                                      "guest log file, trying again.")
      return (result, mochitestsSucceeded)

    if options.repeatUntilFailure:
      succeeded = True
      result = 0
      count = 1
      while result == 0 and succeeded:
        self.automation.log.info("INFO | runtests.py | Beginning mochitest run "
                                 "(%d)." % count)
        count += 1
        (result, succeeded) = innerRun()
    else:
      self.automation.log.info("INFO | runtests.py | Beginning mochitest run.")
      (result, succeeded) = innerRun()

    if not succeeded and options.vmwareRecording:
      newSnapshots = self.getCurentSnapshotList()
      if len(newSnapshots) > len(snapshots):
        self.automation.log.info("INFO | runtests.py | Failed recording saved "
                                 "as '%s'." % newSnapshots[-1])

    if result:
      return result

    if options.shutdownVM:
      result = self.shutdownVM()
      if result:
        return result

    return 0

def main():
  automation = Automation()
  mochitest = VMwareMochitest(automation)

  parser = VMwareOptions(automation, mochitest)
  options, args = parser.parse_args()
  options = parser.verifyOptions(options, mochitest)
  if (options == None):
    sys.exit(1)

  if options.vmx is None:
    parser.error("A virtual machine must be specified with " +
                 "--with-vmware-vm")

  if options.vmrun is None:
    options.vmrun = os.path.join("c:\\", "Program Files", "VMware",
                                 "VMware VIX", "vmrun.exe")
    if not os.path.exists(options.vmrun):
      options.vmrun = os.path.join("c:\\", "Program Files (x86)", "VMware",
                                   "VMware VIX", "vmrun.exe")
      if not os.path.exists(options.vmrun):
        parser.error("Could not locate vmrun.exe, use --with-vmrun-executable" +
                     " to identify its location")

  sys.exit(mochitest.runTests(parser, options))

if __name__ == "__main__":
  main()
