# -*- test-case-name: buildbot.test.test_steps,buildbot.test.test_properties -*-

import types, re
from twisted.python import log
from buildbot import util
from buildbot.process.buildstep import LoggingBuildStep, RemoteShellCommand
from buildbot.status.builder import SUCCESS, WARNINGS, FAILURE

class _BuildPropertyDictionary:
    def __init__(self, build):
        self.build = build
    def __getitem__(self, name):
        p = self.build.getProperty(name)
        if p is None:
            p = ""
        return p

class WithProperties(util.ComparableMixin):
    """This is a marker class, used in ShellCommand's command= argument to
    indicate that we want to interpolate a build property.
    """

    compare_attrs = ('fmtstring', 'args')

    def __init__(self, fmtstring, *args):
        self.fmtstring = fmtstring
        self.args = args

    def render(self, build):
        if self.args:
            strings = []
            for name in self.args:
                p = build.getProperty(name)
                if p is None:
                    p = ""
                strings.append(p)
            s = self.fmtstring % tuple(strings)
        else:
            s = self.fmtstring % _BuildPropertyDictionary(build)
        return s

class ShellCommand(LoggingBuildStep):
    """I run a single shell command on the buildslave. I return FAILURE if
    the exit code of that command is non-zero, SUCCESS otherwise. To change
    this behavior, override my .evaluateCommand method.

    By default, a failure of this step will mark the whole build as FAILURE.
    To override this, give me an argument of flunkOnFailure=False .

    I create a single Log named 'log' which contains the output of the
    command. To create additional summary Logs, override my .createSummary
    method.

    The shell command I run (a list of argv strings) can be provided in
    several ways:
      - a class-level .command attribute
      - a command= parameter to my constructor (overrides .command)
      - set explicitly with my .setCommand() method (overrides both)

    @ivar command: a list of argv strings (or WithProperties instances).
                   This will be used by start() to create a
                   RemoteShellCommand instance.

    @ivar logfiles: a dict mapping log NAMEs to workdir-relative FILENAMEs
                    of their corresponding logfiles. The contents of the file
                    named FILENAME will be put into a LogFile named NAME, ina
                    something approximating real-time. (note that logfiles=
                    is actually handled by our parent class LoggingBuildStep)

    """

    name = "shell"
    description = None # set this to a list of short strings to override
    descriptionDone = None # alternate description when the step is complete
    command = None # set this to a command, or set in kwargs
    # logfiles={} # you can also set 'logfiles' to a dictionary, and it
    #               will be merged with any logfiles= argument passed in
    #               to __init__

    # override this on a specific ShellCommand if you want to let it fail
    # without dooming the entire build to a status of FAILURE
    flunkOnFailure = True

    def __init__(self, workdir,
                 description=None, descriptionDone=None,
                 command=None,
                 **kwargs):
        # most of our arguments get passed through to the RemoteShellCommand
        # that we create, but first strip out the ones that we pass to
        # BuildStep (like haltOnFailure and friends), and a couple that we
        # consume ourselves.
        self.workdir = workdir # required by RemoteShellCommand
        if description:
            self.description = description
        if isinstance(self.description, str):
            self.description = [self.description]
        if descriptionDone:
            self.descriptionDone = descriptionDone
        if isinstance(self.descriptionDone, str):
            self.descriptionDone = [self.descriptionDone]
        if command:
            self.command = command

        # pull out the ones that LoggingBuildStep wants, then upcall
        buildstep_kwargs = {}
        for k in kwargs.keys()[:]:
            if k in self.__class__.parms:
                buildstep_kwargs[k] = kwargs[k]
                del kwargs[k]
        LoggingBuildStep.__init__(self, **buildstep_kwargs)

        # everything left over goes to the RemoteShellCommand
        kwargs['workdir'] = workdir # including a copy of 'workdir'
        self.remote_kwargs = kwargs


    def setCommand(self, command):
        self.command = command

    def describe(self, done=False):
        """Return a list of short strings to describe this step, for the
        status display. This uses the first few words of the shell command.
        You can replace this by setting .description in your subclass, or by
        overriding this method to describe the step better.

        @type  done: boolean
        @param done: whether the command is complete or not, to improve the
                     way the command is described. C{done=False} is used
                     while the command is still running, so a single
                     imperfect-tense verb is appropriate ('compiling',
                     'testing', ...) C{done=True} is used when the command
                     has finished, and the default getText() method adds some
                     text, so a simple noun is appropriate ('compile',
                     'tests' ...)
        """

        if done and self.descriptionDone is not None:
            return self.descriptionDone
        if self.description is not None:
            return self.description

        words = self.command
        # TODO: handle WithProperties here
        if isinstance(words, types.StringTypes):
            words = words.split()
        if len(words) < 1:
            return ["???"]
        if len(words) == 1:
            return ["'%s'" % words[0]]
        if len(words) == 2:
            return ["'%s" % words[0], "%s'" % words[1]]
        return ["'%s" % words[0], "%s" % words[1], "...'"]

    def _interpolateProperties(self, command):
        # interpolate any build properties into our command
        if not isinstance(command, (list, tuple)):
            return command
        command_argv = []
        for argv in command:
            if isinstance(argv, WithProperties):
                command_argv.append(argv.render(self.build))
            else:
                command_argv.append(argv)
        return command_argv

    def setupEnvironment(self, cmd):
        # merge in anything from Build.slaveEnvironment . Earlier steps
        # (perhaps ones which compile libraries or sub-projects that need to
        # be referenced by later steps) can add keys to
        # self.build.slaveEnvironment to affect later steps.
        slaveEnv = self.build.slaveEnvironment
        if slaveEnv:
            if cmd.args['env'] is None:
                cmd.args['env'] = {}
            cmd.args['env'].update(slaveEnv)
            # note that each RemoteShellCommand gets its own copy of the
            # dictionary, so we shouldn't be affecting anyone but ourselves.

    def checkForOldSlaveAndLogfiles(self):
        if not self.logfiles:
            return # doesn't matter
        if not self.slaveVersionIsOlderThan("shell", "2.1"):
            return # slave is new enough
        # this buildslave is too old and will ignore the 'logfiles'
        # argument. You'll either have to pull the logfiles manually
        # (say, by using 'cat' in a separate RemoteShellCommand) or
        # upgrade the buildslave.
        msg1 = ("Warning: buildslave %s is too old "
                "to understand logfiles=, ignoring it."
               % self.getSlaveName())
        msg2 = "You will have to pull this logfile (%s) manually."
        log.msg(msg1)
        for logname,remotefilename in self.logfiles.items():
            newlog = self.addLog(logname)
            newlog.addHeader(msg1 + "\n")
            newlog.addHeader(msg2 % remotefilename + "\n")
            newlog.finish()
        # now prevent setupLogfiles() from adding them
        self.logfiles = {}

    def start(self):
        # this block is specific to ShellCommands. subclasses that don't need
        # to set up an argv array, an environment, or extra logfiles= (like
        # the Source subclasses) can just skip straight to startCommand()
        command = self._interpolateProperties(self.command)
        assert isinstance(command, (list, tuple, str))
        # create the actual RemoteShellCommand instance now
        kwargs = self.remote_kwargs
        kwargs['command'] = command
        kwargs['logfiles'] = self.logfiles
        cmd = RemoteShellCommand(**kwargs)
        self.setupEnvironment(cmd)
        self.checkForOldSlaveAndLogfiles()

        self.startCommand(cmd)



class TreeSize(ShellCommand):
    name = "treesize"
    command = ["du", "-s", "."]
    kb = None

    def commandComplete(self, cmd):
        out = cmd.log.getText()
        m = re.search(r'^(\d+)', out)
        if m:
            self.kb = int(m.group(1))

    def evaluateCommand(self, cmd):
        if cmd.rc != 0:
            return FAILURE
        if self.kb is None:
            return WARNINGS # not sure how 'du' could fail, but whatever
        return SUCCESS

    def getText(self, cmd, results):
        if self.kb is not None:
            return ["treesize", "%d kb" % self.kb]
        return ["treesize", "unknown"]

class Configure(ShellCommand):

    name = "configure"
    haltOnFailure = 1
    description = ["configuring"]
    descriptionDone = ["configure"]
    command = ["./configure"]

class Compile(ShellCommand):

    name = "compile"
    haltOnFailure = 1
    description = ["compiling"]
    descriptionDone = ["compile"]
    command = ["make", "all"]

    OFFprogressMetrics = ('output',)
    # things to track: number of files compiled, number of directories
    # traversed (assuming 'make' is being used)

    def createSummary(self, cmd):
        # TODO: grep for the characteristic GCC warning/error lines and
        # assemble them into a pair of buffers
        pass

class Test(ShellCommand):

    name = "test"
    warnOnFailure = 1
    description = ["testing"]
    descriptionDone = ["test"]
    command = ["make", "test"]
