#! /usr/bin/python

from buildbot.twcompat import Interface

class ISlaveCommand(Interface):
    """This interface is implemented by all of the buildslave's Command
    subclasses. It specifies how the buildslave can start, interrupt, and
    query the various Commands running on behalf of the buildmaster."""

    def __init__(builder, stepId, args):
        """Create the Command. 'builder' is a reference to the parent
        buildbot.bot.SlaveBuilder instance, which will be used to send status
        updates (by calling builder.sendStatus). 'stepId' is a random string
        which helps correlate slave logs with the master. 'args' is a dict of
        arguments that comes from the master-side BuildStep, with contents
        that are specific to the individual Command subclass.

        This method is not intended to be subclassed."""

    def setup(args):
        """This method is provided for subclasses to override, to extract
        parameters from the 'args' dictionary. The default implemention does
        nothing. It will be called from __init__"""

    def start():
        """Begin the command, and return a Deferred.

        While the command runs, it should send status updates to the
        master-side BuildStep by calling self.sendStatus(status). The
        'status' argument is typically a dict with keys like 'stdout',
        'stderr', and 'rc'.

        When the step completes, it should fire the Deferred (the results are
        not used). If an exception occurs during execution, it may also
        errback the deferred, however any reasonable errors should be trapped
        and indicated with a non-zero 'rc' status rather than raising an
        exception. Exceptions should indicate problems within the buildbot
        itself, not problems in the project being tested.

        """

    def interrupt():
        """This is called to tell the Command that the build is being stopped
        and therefore the command should be terminated as quickly as
        possible. The command may continue to send status updates, up to and
        including an 'rc' end-of-command update (which should indicate an
        error condition). The Command's deferred should still be fired when
        the command has finally completed.

        If the build is being stopped because the slave it shutting down or
        because the connection to the buildmaster has been lost, the status
        updates will simply be discarded. The Command does not need to be
        aware of this.

        Child shell processes should be killed. Simple ShellCommand classes
        can just insert a header line indicating that the process will be
        killed, then os.kill() the child."""
