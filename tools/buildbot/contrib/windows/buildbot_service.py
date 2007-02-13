# Runs the build-bot as a Windows service.
# To use:
# * Install and configure buildbot as per normal (ie, running
#  'setup.py install' from the source directory).
#
# * Configure any number of build-bot directories (slaves or masters), as
#   per the buildbot instructions.  Test these directories normally by
#   using the (possibly modified) "buildbot.bat" file and ensure everything
#   is working as expected.
#
# * Install the buildbot service.  Execute the command:
#   % python buildbot_service.py
#   To see installation options.  You probably want to specify:
#   + --username and --password options to specify the user to run the
#   + --startup auto to have the service start at boot time.
#
#   For example:
#   % python buildbot_service.py --user mark --password secret \
#     --startup auto install
#   Alternatively, you could execute:
#   % python buildbot_service.py install
#   to install the service with default options, then use Control Panel
#   to configure it.
#
# * Start the service specifying the name of all buildbot directories as
#   service args.  This can be done one of 2 ways:
#   - Execute the command:
#     % python buildbot_service.py start "dir_name1" "dir_name2"
#   or:
#   - Start Control Panel->Administrative Tools->Services
#   - Locate the previously installed buildbot service.
#   - Open the "properties" for the service.
#   - Enter the directory names into the "Start Parameters" textbox.  The
#     directory names must be fully qualified, and surrounded in quotes if
#    they include spaces.
#   - Press the "Start"button.
#   Note that the service will automatically use the previously specified
#   directories if no arguments are specified. This means the directories
#   need only be specified when the directories to use have changed (and
#   therefore also the first time buildbot is configured)
#
# * The service should now be running.  You should check the Windows
#   event log.  If all goes well, you should see some information messages
#   telling you the buildbot has successfully started.
#
# * If you change the buildbot configuration, you must restart the service.
#   There is currently no way to ask a running buildbot to reload the
#   config.  You can restart by executing:
#   % python buildbot_service.py restart
#
# Troubleshooting:
# * Check the Windows event log for any errors.
# * Check the "twistd.log" file in your buildbot directories - once each
#   bot has been started it just writes to this log as normal.
# * Try executing:
#   % python buildbot_service.py debug
#   This will execute the buildbot service in "debug" mode, and allow you to
#   see all messages etc generated. If the service works in debug mode but
#   not as a real service, the error probably relates to the environment or
#   permissions of the user configured to run the service (debug mode runs as
#   the currently logged in user, not the service user)
# * Ensure you have the latest pywin32 build available, at least version 206.

# Written by Mark Hammond, 2006.

import sys, os, threading

import pywintypes
import winerror, win32con
import win32api, win32event, win32file, win32pipe, win32process, win32security
import win32service, win32serviceutil, servicemanager

# Are we running in a py2exe environment?
is_frozen = hasattr(sys, "frozen")

# Taken from the Zope service support - each "child" is run as a sub-process
# (trying to run multiple twisted apps in the same process is likely to screw
# stdout redirection etc).
# Note that unlike the Zope service, we do *not* attempt to detect a failed
# client and perform restarts - buildbot itself does a good job
# at reconnecting, and Windows itself provides restart semantics should
# everything go pear-shaped.

# We execute a new thread that captures the tail of the output from our child
# process. If the child fails, it is written to the event log.
# This process is unconditional, and the output is never written to disk
# (except obviously via the event log entry)
# Size of the blocks we read from the child process's output.
CHILDCAPTURE_BLOCK_SIZE = 80
# The number of BLOCKSIZE blocks we keep as process output.
CHILDCAPTURE_MAX_BLOCKS = 200

class BBService(win32serviceutil.ServiceFramework):    
    _svc_name_ = 'BuildBot'
    _svc_display_name_ = _svc_name_
    _svc_description_ = 'Manages local buildbot slaves and masters - ' \
                        'see http://buildbot.sourceforge.net'

    def __init__(self, args):
        win32serviceutil.ServiceFramework.__init__(self, args)

        # Create an event which we will use to wait on. The "service stop" 
        # request will set this event.
        # * We must make it inheritable so we can pass it to the child 
        #   process via the cmd-line
        # * Must be manual reset so each child process and our service
        #   all get woken from a single set of the event.
        sa = win32security.SECURITY_ATTRIBUTES()
        sa.bInheritHandle = True
        self.hWaitStop = win32event.CreateEvent(sa, True, False, None)

        self.args = args
        self.dirs = None
        self.runner_prefix = None

        # Patch up the service messages file in a frozen exe.
        # (We use the py2exe option that magically bundles the .pyd files
        # into the .zip file - so servicemanager.pyd doesn't exist.)
        if is_frozen and servicemanager.RunningAsService():
            msg_file = os.path.join(os.path.dirname(sys.executable),
                                    "buildbot.msg")
            if os.path.isfile(msg_file):
                servicemanager.Initialize("BuildBot", msg_file)
            else:
                self.warning("Strange - '%s' does not exist" % (msg_file,))

    def _checkConfig(self):
        # Locate our child process runner (but only when run from source)
        if not is_frozen:
            # Running from source
            python_exe = os.path.join(sys.prefix, "python.exe")
            if not os.path.isfile(python_exe):
                # for ppl who build Python itself from source.
                python_exe = os.path.join(sys.prefix, "PCBuild", "python.exe")
            if not os.path.isfile(python_exe):
                self.error("Can not find python.exe to spawn subprocess")
                return False

            me = __file__
            if me.endswith(".pyc") or me.endswith(".pyo"):
                me = me[:-1]

            self.runner_prefix = '"%s" "%s"' % (python_exe, me)
        else:
            # Running from a py2exe built executable - our child process is
            # us (but with the funky cmdline args!)
            self.runner_prefix = '"' + sys.executable + '"'

        # Now our arg processing - this may be better handled by a
        # twisted/buildbot style config file - but as of time of writing,
        # MarkH is clueless about such things!

        # Note that the "arguments" you type into Control Panel for the
        # service do *not* persist - they apply only when you click "start"
        # on the service. When started by Windows, args are never presented.
        # Thus, it is the responsibility of the service to persist any args.
        
        # so, when args are presented, we save them as a "custom option". If
        # they are not presented, we load them from the option.
        self.dirs = []
        if len(self.args) > 1:
            dir_string = os.pathsep.join(self.args[1:])
            save_dirs = True
        else:
            dir_string = win32serviceutil.GetServiceCustomOption(self,
                                                            "directories")
            save_dirs = False

        if not dir_string:
            self.error("You must specify the buildbot directories as "
                       "parameters to the service.\nStopping the service.")
            return False

        dirs = dir_string.split(os.pathsep)
        for d in dirs:
            d = os.path.abspath(d)
            sentinal = os.path.join(d, "buildbot.tac")
            if os.path.isfile(sentinal):
                self.dirs.append(d)
            else:
                msg = "Directory '%s' is not a buildbot dir - ignoring" \
                      % (d,)
                self.warning(msg)
        if not self.dirs:
            self.error("No valid buildbot directories were specified.\n"
                       "Stopping the service.")
            return False
        if save_dirs:
            dir_string = os.pathsep.join(self.dirs).encode("mbcs")
            win32serviceutil.SetServiceCustomOption(self, "directories",
                                                    dir_string)
        return True

    def SvcStop(self):
        # Tell the SCM we are starting the stop process.
        self.ReportServiceStatus(win32service.SERVICE_STOP_PENDING)
        # Set the stop event - the main loop takes care of termination.
        win32event.SetEvent(self.hWaitStop)

    # SvcStop only gets triggered when the user explictly stops (or restarts)
    # the service.  To shut the service down cleanly when Windows is shutting
    # down, we also need to hook SvcShutdown.
    SvcShutdown = SvcStop

    def SvcDoRun(self):
        if not self._checkConfig():
            # stopped status set by caller.
            return

        self.logmsg(servicemanager.PYS_SERVICE_STARTED)

        child_infos = []

        for bbdir in self.dirs:
            self.info("Starting BuildBot in directory '%s'" % (bbdir,))
            hstop = self.hWaitStop

            cmd = '%s --spawn %d start %s' % (self.runner_prefix, hstop, bbdir)
            #print "cmd is", cmd
            h, t, output = self.createProcess(cmd)
            child_infos.append((bbdir, h, t, output))

        while child_infos:
            handles = [self.hWaitStop] + [i[1] for i in child_infos]

            rc = win32event.WaitForMultipleObjects(handles,
                                                   0, # bWaitAll
                                                   win32event.INFINITE)
            if rc == win32event.WAIT_OBJECT_0:
                # user sent a stop service request
                break
            else:
                # A child process died.  For now, just log the output
                # and forget the process.
                index = rc - win32event.WAIT_OBJECT_0 - 1
                bbdir, dead_handle, dead_thread, output_blocks = \
                                                        child_infos[index]
                status = win32process.GetExitCodeProcess(dead_handle)
                output = "".join(output_blocks)
                if not output:
                    output = "The child process generated no output. " \
                             "Please check the twistd.log file in the " \
                             "indicated directory."

                self.warning("BuildBot for directory %r terminated with "
                             "exit code %d.\n%s" % (bbdir, status, output))

                del child_infos[index]

                if not child_infos:
                    self.warning("All BuildBot child processes have "
                                 "terminated.  Service stopping.")

        # Either no child processes left, or stop event set.
        self.ReportServiceStatus(win32service.SERVICE_STOP_PENDING)

        # The child processes should have also seen our stop signal
        # so wait for them to terminate.
        for bbdir, h, t, output in child_infos:
            for i in range(10): # 30 seconds to shutdown...
                self.ReportServiceStatus(win32service.SERVICE_STOP_PENDING)
                rc = win32event.WaitForSingleObject(h, 3000)
                if rc == win32event.WAIT_OBJECT_0:
                    break
            # Process terminated - no need to try harder.
            if rc == win32event.WAIT_OBJECT_0:
                break

            self.ReportServiceStatus(win32service.SERVICE_STOP_PENDING)
            # If necessary, kill it
            if win32process.GetExitCodeProcess(h)==win32con.STILL_ACTIVE:
                self.warning("BuildBot process at %r failed to terminate - "
                             "killing it" % (bbdir,))
                win32api.TerminateProcess(h, 3)
            self.ReportServiceStatus(win32service.SERVICE_STOP_PENDING)

            # Wait for the redirect thread - it should have died as the remote
            # process terminated.
            # As we are shutting down, we do the join with a little more care,
            # reporting progress as we wait (even though we never will <wink>)
            for i in range(5):
                t.join(1)
                self.ReportServiceStatus(win32service.SERVICE_STOP_PENDING)
                if not t.isAlive():
                    break
            else:
                self.warning("Redirect thread did not stop!")

        # All done.
        self.logmsg(servicemanager.PYS_SERVICE_STOPPED)

    #
    # Error reporting/logging functions.
    #
    def logmsg(self, event):
        # log a service event using servicemanager.LogMsg
        try:
            servicemanager.LogMsg(servicemanager.EVENTLOG_INFORMATION_TYPE,
                                  event,
                                  (self._svc_name_,
                                   " (%s)" % self._svc_display_name_))
        except win32api.error, details:
            # Failed to write a log entry - most likely problem is
            # that the event log is full.  We don't want this to kill us
            try:
                print "FAILED to write INFO event", event, ":", details
            except IOError:
                # No valid stdout!  Ignore it.
                pass

    def _dolog(self, func, msg):
        try:
            func(msg)
        except win32api.error, details:
            # Failed to write a log entry - most likely problem is
            # that the event log is full.  We don't want this to kill us
            try:
                print "FAILED to write event log entry:", details
                print msg
            except IOError:
                pass

    def info(self, s):
        self._dolog(servicemanager.LogInfoMsg, s)

    def warning(self, s):
        self._dolog(servicemanager.LogWarningMsg, s)

    def error(self, s):
        self._dolog(servicemanager.LogErrorMsg, s)
    
    # Functions that spawn a child process, redirecting any output.
    # Although builtbot itself does this, it is very handy to debug issues
    # such as ImportErrors that happen before buildbot has redirected.
    def createProcess(self, cmd):
        hInputRead, hInputWriteTemp = self.newPipe()
        hOutReadTemp, hOutWrite = self.newPipe()
        pid = win32api.GetCurrentProcess()
        # This one is duplicated as inheritable.
        hErrWrite = win32api.DuplicateHandle(pid, hOutWrite, pid, 0, 1,
                                       win32con.DUPLICATE_SAME_ACCESS)

        # These are non-inheritable duplicates.
        hOutRead = self.dup(hOutReadTemp)
        hInputWrite = self.dup(hInputWriteTemp)
        # dup() closed hOutReadTemp, hInputWriteTemp

        si = win32process.STARTUPINFO()
        si.hStdInput = hInputRead
        si.hStdOutput = hOutWrite
        si.hStdError = hErrWrite
        si.dwFlags = win32process.STARTF_USESTDHANDLES | \
                     win32process.STARTF_USESHOWWINDOW
        si.wShowWindow = win32con.SW_HIDE

        # pass True to allow handles to be inherited.  Inheritance is
        # problematic in general, but should work in the controlled
        # circumstances of a service process.
        create_flags = win32process.CREATE_NEW_CONSOLE
        # info is (hProcess, hThread, pid, tid)
        info = win32process.CreateProcess(None, cmd, None, None, True,
                                          create_flags, None, None, si)
        # (NOTE: these really aren't necessary for Python - they are closed
        # as soon as they are collected)
        hOutWrite.Close()
        hErrWrite.Close()
        hInputRead.Close()
        # We don't use stdin
        hInputWrite.Close()

        # start a thread collecting output
        blocks = []
        t = threading.Thread(target=self.redirectCaptureThread,
                             args = (hOutRead,blocks))
        t.start()
        return info[0], t, blocks

    def redirectCaptureThread(self, handle, captured_blocks):
        # One of these running per child process we are watching.  It
        # handles both stdout and stderr on a single handle. The read data is
        # never referenced until the thread dies - so no need for locks
        # around self.captured_blocks.
        #self.info("Redirect thread starting")
        while 1:
            try:
                ec, data = win32file.ReadFile(handle, CHILDCAPTURE_BLOCK_SIZE)
            except pywintypes.error, err:
                # ERROR_BROKEN_PIPE means the child process closed the
                # handle - ie, it terminated.
                if err[0] != winerror.ERROR_BROKEN_PIPE:
                    self.warning("Error reading output from process: %s" % err)
                break
            captured_blocks.append(data)
            del captured_blocks[CHILDCAPTURE_MAX_BLOCKS:]
        handle.Close()
        #self.info("Redirect capture thread terminating")

    def newPipe(self):
        sa = win32security.SECURITY_ATTRIBUTES()
        sa.bInheritHandle = True
        return win32pipe.CreatePipe(sa, 0)

    def dup(self, pipe):
        # create a duplicate handle that is not inherited, so that
        # it can be closed in the parent.  close the original pipe in
        # the process.
        pid = win32api.GetCurrentProcess()
        dup = win32api.DuplicateHandle(pid, pipe, pid, 0, 0,
                                       win32con.DUPLICATE_SAME_ACCESS)
        pipe.Close()
        return dup

# Service registration and startup
def RegisterWithFirewall(exe_name, description):
    # Register our executable as an exception with Windows Firewall.
    # taken from http://msdn.microsoft.com/library/default.asp?url=/library/en-us/ics/ics/wf_adding_an_application.asp
    from win32com.client import Dispatch
    #  Set constants
    NET_FW_PROFILE_DOMAIN = 0
    NET_FW_PROFILE_STANDARD = 1
    
    # Scope
    NET_FW_SCOPE_ALL = 0
    
    # IP Version - ANY is the only allowable setting for now
    NET_FW_IP_VERSION_ANY = 2
    
    fwMgr = Dispatch("HNetCfg.FwMgr")
    
    # Get the current profile for the local firewall policy.
    profile = fwMgr.LocalPolicy.CurrentProfile
    
    app = Dispatch("HNetCfg.FwAuthorizedApplication")
    
    app.ProcessImageFileName = exe_name
    app.Name = description
    app.Scope = NET_FW_SCOPE_ALL
    # Use either Scope or RemoteAddresses, but not both
    #app.RemoteAddresses = "*"
    app.IpVersion = NET_FW_IP_VERSION_ANY
    app.Enabled = True
    
    # Use this line if you want to add the app, but disabled.
    #app.Enabled = False
    
    profile.AuthorizedApplications.Add(app)

# A custom install function.
def CustomInstall(opts):
    # Register this process with the Windows Firewaall
    import pythoncom
    try:
        RegisterWithFirewall(sys.executable, "BuildBot")
    except pythoncom.com_error, why:
        print "FAILED to register with the Windows firewall"
        print why

#
# Magic code to allow shutdown.  Note that this code is executed in
# the *child* process, by way of the service process executing us with
# special cmdline args (which includes the service stop handle!)
def _RunChild():
    del sys.argv[1] # The --spawn arg.
    # Create a new thread that just waits for the event to be signalled.
    t = threading.Thread(target=_WaitForShutdown, 
                         args = (int(sys.argv[1]),)
                         )
    del sys.argv[1] # The stop handle
    # This child process will be sent a console handler notification as
    # users log off, or as the system shuts down.  We want to ignore these
    # signals as the service parent is responsible for our shutdown.
    def ConsoleHandler(what):
        # We can ignore *everything* - ctrl+c will never be sent as this
        # process is never attached to a console the user can press the
        # key in!
        return True
    win32api.SetConsoleCtrlHandler(ConsoleHandler, True)
    t.setDaemon(True) # we don't want to wait for this to stop!
    t.start()
    if hasattr(sys, "frozen"):
        # py2exe sets this env vars that may screw our child process - reset
        del os.environ["PYTHONPATH"]

    # Start the buildbot app
    from buildbot.scripts import runner
    runner.run()
    print "Service child process terminating normally."

def _WaitForShutdown(h):
    win32event.WaitForSingleObject(h, win32event.INFINITE)
    print "Shutdown requested"

    from twisted.internet import reactor
    reactor.callLater(0, reactor.stop)

# This function is also called by the py2exe startup code.
def HandleCommandLine():
    if len(sys.argv)>1 and sys.argv[1] == "--spawn":
        # Special command-line created by the service to execute the
        # child-process.
        # First arg is the handle to wait on
        _RunChild()
    else:
        win32serviceutil.HandleCommandLine(BBService,
                                           customOptionHandler=CustomInstall)

if __name__ == '__main__':
    HandleCommandLine()
