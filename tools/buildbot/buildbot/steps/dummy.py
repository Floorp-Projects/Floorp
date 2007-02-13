
from twisted.internet import reactor
from buildbot.process.buildstep import BuildStep, LoggingBuildStep
from buildbot.process.buildstep import LoggedRemoteCommand
from buildbot.status.builder import SUCCESS, FAILURE

# these classes are used internally by buildbot unit tests

class Dummy(BuildStep):
    """I am a dummy no-op step, which runs entirely on the master, and simply
    waits 5 seconds before finishing with SUCCESS
    """

    haltOnFailure = True
    name = "dummy"

    def __init__(self, timeout=5, **kwargs):
        """
        @type  timeout: int
        @param timeout: the number of seconds to delay before completing
        """
        BuildStep.__init__(self, **kwargs)
        self.timeout = timeout
        self.timer = None

    def start(self):
        self.step_status.setColor("yellow")
        self.step_status.setText(["delay", "%s secs" % self.timeout])
        self.timer = reactor.callLater(self.timeout, self.done)

    def interrupt(self, reason):
        if self.timer:
            self.timer.cancel()
            self.timer = None
            self.step_status.setColor("red")
            self.step_status.setText(["delay", "interrupted"])
            self.finished(FAILURE)

    def done(self):
        self.step_status.setColor("green")
        self.finished(SUCCESS)

class FailingDummy(Dummy):
    """I am a dummy no-op step that 'runs' master-side and finishes (with a
    FAILURE status) after 5 seconds."""

    name = "failing dummy"

    def start(self):
        self.step_status.setColor("yellow")
        self.step_status.setText(["boom", "%s secs" % self.timeout])
        self.timer = reactor.callLater(self.timeout, self.done)

    def done(self):
        self.step_status.setColor("red")
        self.finished(FAILURE)

class RemoteDummy(LoggingBuildStep):
    """I am a dummy no-op step that runs on the remote side and
    simply waits 5 seconds before completing with success.
    See L{buildbot.slave.commands.DummyCommand}
    """

    haltOnFailure = True
    name = "remote dummy"

    def __init__(self, timeout=5, **kwargs):
        """
        @type  timeout: int
        @param timeout: the number of seconds to delay
        """
        LoggingBuildStep.__init__(self, **kwargs)
        self.timeout = timeout
        self.description = ["remote", "delay", "%s secs" % timeout]

    def describe(self, done=False):
        return self.description

    def start(self):
        args = {'timeout': self.timeout}
        cmd = LoggedRemoteCommand("dummy", args)
        self.startCommand(cmd)

class Wait(LoggingBuildStep):
    """I start a command on the slave that waits for the unit test to
    tell it when to finish.
    """

    name = "wait"
    def __init__(self, handle, **kwargs):
        LoggingBuildStep.__init__(self, **kwargs)
        self.handle = handle

    def describe(self, done=False):
        return ["wait: %s" % self.handle]

    def start(self):
        args = {'handle': (self.handle, self.build.reason)}
        cmd = LoggedRemoteCommand("dummy.wait", args)
        self.startCommand(cmd)
