#! /usr/bin/python

"""Interface documentation.

Define the interfaces that are implemented by various buildbot classes.
"""

from buildbot.twcompat import Interface

# exceptions that can be raised while trying to start a build
class NoSlaveError(Exception):
    pass
class BuilderInUseError(Exception):
    pass
class BuildSlaveTooOldError(Exception):
    pass

class IChangeSource(Interface):
    """Object which feeds Change objects to the changemaster. When files or
    directories are changed and the version control system provides some
    kind of notification, this object should turn it into a Change object
    and pass it through::

      self.changemaster.addChange(change)
    """

    def start():
        """Called when the buildmaster starts. Can be used to establish
        connections to VC daemons or begin polling."""

    def stop():
        """Called when the buildmaster shuts down. Connections should be
        terminated, polling timers should be canceled."""

    def describe():
        """Should return a string which briefly describes this source. This
        string will be displayed in an HTML status page."""

class IScheduler(Interface):
    """I watch for Changes in the source tree and decide when to trigger
    Builds. I create BuildSet objects and submit them to the BuildMaster. I
    am a service, and the BuildMaster is always my parent."""

    def addChange(change):
        """A Change has just been dispatched by one of the ChangeSources.
        Each Scheduler will receive this Change. I may decide to start a
        build as a result, or I might choose to ignore it."""

    def listBuilderNames():
        """Return a list of strings indicating the Builders that this
        Scheduler might feed."""

    def getPendingBuildTimes():
        """Return a list of timestamps for any builds that are waiting in the
        tree-stable-timer queue. This is only relevant for Change-based
        schedulers, all others can just return an empty list."""
        # TODO: it might be nice to make this into getPendingBuildSets, which
        # would let someone subscribe to the buildset being finished.
        # However, the Scheduler doesn't actually create the buildset until
        # it gets submitted, so doing this would require some major rework.

class IUpstreamScheduler(Interface):
    """This marks an IScheduler as being eligible for use as the 'upstream='
    argument to a buildbot.scheduler.Dependent instance."""

    def subscribeToSuccessfulBuilds(target):
        """Request that the target callbable be invoked after every
        successful buildset. The target will be called with a single
        argument: the SourceStamp used by the successful builds."""

    def listBuilderNames():
        """Return a list of strings indicating the Builders that this
        Scheduler might feed."""

class ISourceStamp(Interface):
    pass

class IEmailSender(Interface):
    """I know how to send email, and can be used by other parts of the
    Buildbot to contact developers."""
    pass

class IEmailLookup(Interface):
    def getAddress(user):
        """Turn a User-name string into a valid email address. Either return
        a string (with an @ in it), None (to indicate that the user cannot
        be reached by email), or a Deferred which will fire with the same."""

class IStatus(Interface):
    """I am an object, obtainable from the buildmaster, which can provide
    status information."""

    def getProjectName():
        """Return the name of the project that this Buildbot is working
        for."""
    def getProjectURL():
        """Return the URL of this Buildbot's project."""
    def getBuildbotURL():
        """Return the URL of the top-most Buildbot status page, or None if
        this Buildbot does not provide a web status page."""
    def getURLForThing(thing):
        """Return the URL of a page which provides information on 'thing',
        which should be an object that implements one of the status
        interfaces defined in L{buildbot.interfaces}. Returns None if no
        suitable page is available (or if no Waterfall is running)."""

    def getSchedulers():
        """Return a list of ISchedulerStatus objects for all
        currently-registered Schedulers."""

    def getBuilderNames(categories=None):
        """Return a list of the names of all current Builders."""
    def getBuilder(name):
        """Return the IBuilderStatus object for a given named Builder."""
    def getSlave(name):
        """Return the ISlaveStatus object for a given named buildslave."""

    def getBuildSets():
        """Return a list of active (non-finished) IBuildSetStatus objects."""

    def subscribe(receiver):
        """Register an IStatusReceiver to receive new status events. The
        receiver will immediately be sent a set of 'builderAdded' messages
        for all current builders. It will receive further 'builderAdded' and
        'builderRemoved' messages as the config file is reloaded and builders
        come and go. It will also receive 'buildsetSubmitted' messages for
        all outstanding BuildSets (and each new BuildSet that gets
        submitted). No additional messages will be sent unless the receiver
        asks for them by calling .subscribe on the IBuilderStatus objects
        which accompany the addedBuilder message."""

    def unsubscribe(receiver):
        """Unregister an IStatusReceiver. No further status messgaes will be
        delivered."""

class IBuildSetStatus(Interface):
    """I represent a set of Builds, each run on a separate Builder but all
    using the same source tree."""

    def getSourceStamp():
        pass
    def getReason():
        pass
    def getID():
        """Return the BuildSet's ID string, if any. The 'try' feature uses a
        random string as a BuildSetID to relate submitted jobs with the
        resulting BuildSet."""
    def getResponsibleUsers():
        pass # not implemented
    def getInterestedUsers():
        pass # not implemented
    def getBuilderNames():
        """Return a list of the names of all Builders on which this set will
        do builds."""
    def getBuildRequests():
        """Return a list of IBuildRequestStatus objects that represent my
        component Builds. This list might correspond to the Builders named by
        getBuilderNames(), but if builder categories are used, or 'Builder
        Aliases' are implemented, then they may not."""
    def isFinished():
        pass
    def waitUntilSuccess():
        """Return a Deferred that fires (with this IBuildSetStatus object)
        when the outcome of the BuildSet is known, i.e., upon the first
        failure, or after all builds complete successfully."""
    def waitUntilFinished():
        """Return a Deferred that fires (with this IBuildSetStatus object)
        when all builds have finished."""
    def getResults():
        pass

class IBuildRequestStatus(Interface):
    """I represent a request to build a particular set of source code on a
    particular Builder. These requests may be merged by the time they are
    finally turned into a Build."""

    def getSourceStamp():
        pass
    def getBuilderName():
        pass
    def getBuilds():
        """Return a list of IBuildStatus objects for each Build that has been
        started in an attempt to satify this BuildRequest."""

    def subscribe(observer):
        """Register a callable that will be invoked (with a single
        IBuildStatus object) for each Build that is created to satisfy this
        request. There may be multiple Builds created in an attempt to handle
        the request: they may be interrupted by the user or abandoned due to
        a lost slave. The last Build (the one which actually gets to run to
        completion) is said to 'satisfy' the BuildRequest. The observer will
        be called once for each of these Builds, both old and new."""
    def unsubscribe(observer):
        """Unregister the callable that was registered with subscribe()."""


class ISlaveStatus(Interface):
    def getName():
        """Return the name of the build slave."""

    def getAdmin():
        """Return a string with the slave admin's contact data."""

    def getHost():
        """Return a string with the slave host info."""

    def isConnected():
        """Return True if the slave is currently online, False if not."""

class ISchedulerStatus(Interface):
    def getName():
        """Return the name of this Scheduler (a string)."""

    def getPendingBuildsets():
        """Return an IBuildSet for all BuildSets that are pending. These
        BuildSets are waiting for their tree-stable-timers to expire."""
        # TODO: this is not implemented anywhere


class IBuilderStatus(Interface):
    def getName():
        """Return the name of this Builder (a string)."""

    def getState():
        # TODO: this isn't nearly as meaningful as it used to be
        """Return a tuple (state, builds) for this Builder. 'state' is the
        so-called 'big-status', indicating overall status (as opposed to
        which step is currently running). It is a string, one of 'offline',
        'idle', or 'building'. 'builds' is a list of IBuildStatus objects
        (possibly empty) representing the currently active builds."""

    def getSlaves():
        """Return a list of ISlaveStatus objects for the buildslaves that are
        used by this builder."""

    def getPendingBuilds():
        """Return an IBuildRequestStatus object for all upcoming builds
        (those which are ready to go but which are waiting for a buildslave
        to be available."""

    def getCurrentBuilds():
        """Return a list containing an IBuildStatus object for each build
        currently in progress."""
        # again, we could probably provide an object for 'waiting' and
        # 'interlocked' too, but things like the Change list might still be
        # subject to change

    def getLastFinishedBuild():
        """Return the IBuildStatus object representing the last finished
        build, which may be None if the builder has not yet finished any
        builds."""

    def getBuild(number):
        """Return an IBuildStatus object for a historical build. Each build
        is numbered (starting at 0 when the Builder is first added),
        getBuild(n) will retrieve the Nth such build. getBuild(-n) will
        retrieve a recent build, with -1 being the most recent build
        started. If the Builder is idle, this will be the same as
        getLastFinishedBuild(). If the Builder is active, it will be an
        unfinished build. This method will return None if the build is no
        longer available. Older builds are likely to have less information
        stored: Logs are the first to go, then Steps."""

    def getEvent(number):
        """Return an IStatusEvent object for a recent Event. Builders
        connecting and disconnecting are events, as are ping attempts.
        getEvent(-1) will return the most recent event. Events are numbered,
        but it probably doesn't make sense to ever do getEvent(+n)."""

    def subscribe(receiver):
        """Register an IStatusReceiver to receive new status events. The
        receiver will be given builderChangedState, buildStarted, and
        buildFinished messages."""

    def unsubscribe(receiver):
        """Unregister an IStatusReceiver. No further status messgaes will be
        delivered."""

class IBuildStatus(Interface):
    """I represent the status of a single Build/BuildRequest. It could be
    in-progress or finished."""

    def getBuilder():
        """
        Return the BuilderStatus that owns this build.
        
        @rtype: implementor of L{IBuilderStatus}
        """

    def isFinished():
        """Return a boolean. True means the build has finished, False means
        it is still running."""

    def waitUntilFinished():
        """Return a Deferred that will fire when the build finishes. If the
        build has already finished, this deferred will fire right away. The
        callback is given this IBuildStatus instance as an argument."""

    def getProperty(propname):
        """Return the value of the build property with the given name."""

    def getReason():
        """Return a string that indicates why the build was run. 'changes',
        'forced', and 'periodic' are the most likely values. 'try' will be
        added in the future."""

    def getSourceStamp():
        """Return a tuple of (branch, revision, patch) which can be used to
        re-create the source tree that this build used. 'branch' is a string
        with a VC-specific meaning, or None to indicate that the checkout
        step used its default branch. 'revision' is a string, the sort you
        would pass to 'cvs co -r REVISION'. 'patch' is either None, or a
        (level, diff) tuple which represents a patch that should be applied
        with 'patch -pLEVEL < DIFF' from the directory created by the
        checkout operation.

        This method will return None if the source information is no longer
        available."""
        # TODO: it should be possible to expire the patch but still remember
        # that the build was r123+something.

        # TODO: change this to return the actual SourceStamp instance, and
        # remove getChanges()

    def getChanges():
        """Return a list of Change objects which represent which source
        changes went into the build."""

    def getResponsibleUsers():
        """Return a list of Users who are to blame for the changes that went
        into this build. If anything breaks (at least anything that wasn't
        already broken), blame them. Specifically, this is the set of users
        who were responsible for the Changes that went into this build. Each
        User is a string, corresponding to their name as known by the VC
        repository."""

    def getInterestedUsers():
        """Return a list of Users who will want to know about the results of
        this build. This is a superset of getResponsibleUsers(): it adds
        people who are interested in this build but who did not actually
        make the Changes that went into it (build sheriffs, code-domain
        owners)."""

    def getNumber():
        """Within each builder, each Build has a number. Return it."""

    def getPreviousBuild():
        """Convenience method. Returns None if the previous build is
        unavailable."""

    def getSteps():
        """Return a list of IBuildStepStatus objects. For invariant builds
        (those which always use the same set of Steps), this should always
        return the complete list, however some of the steps may not have
        started yet (step.getTimes()[0] will be None). For variant builds,
        this may not be complete (asking again later may give you more of
        them)."""

    def getTimes():
        """Returns a tuple of (start, end). 'start' and 'end' are the times
        (seconds since the epoch) when the Build started and finished. If
        the build is still running, 'end' will be None."""

    # while the build is running, the following methods make sense.
    # Afterwards they return None

    def getETA():
        """Returns the number of seconds from now in which the build is
        expected to finish, or None if we can't make a guess. This guess will
        be refined over time."""

    def getCurrentStep():
        """Return an IBuildStepStatus object representing the currently
        active step."""

    # Once you know the build has finished, the following methods are legal.
    # Before ths build has finished, they all return None.

    def getSlavename():
        """Return the name of the buildslave which handled this build."""

    def getText():
        """Returns a list of strings to describe the build. These are
        intended to be displayed in a narrow column. If more space is
        available, the caller should join them together with spaces before
        presenting them to the user."""

    def getColor():
        """Returns a single string with the color that should be used to
        display the build. 'green', 'orange', or 'red' are the most likely
        ones."""

    def getResults():
        """Return a constant describing the results of the build: one of the
        constants in buildbot.status.builder: SUCCESS, WARNINGS, or
        FAILURE."""

    def getLogs():
        """Return a list of logs that describe the build as a whole. Some
        steps will contribute their logs, while others are are less important
        and will only be accessible through the IBuildStepStatus objects.
        Each log is an object which implements the IStatusLog interface."""

    def getTestResults():
        """Return a dictionary that maps test-name tuples to ITestResult
        objects. This may return an empty or partially-filled dictionary
        until the build has completed."""

    # subscription interface

    def subscribe(receiver, updateInterval=None):
        """Register an IStatusReceiver to receive new status events. The
        receiver will be given stepStarted and stepFinished messages. If
        'updateInterval' is non-None, buildETAUpdate messages will be sent
        every 'updateInterval' seconds."""

    def unsubscribe(receiver):
        """Unregister an IStatusReceiver. No further status messgaes will be
        delivered."""

class ITestResult(Interface):
    """I describe the results of a single unit test."""

    def getName():
        """Returns a tuple of strings which make up the test name. Tests may
        be arranged in a hierarchy, so looking for common prefixes may be
        useful."""

    def getResults():
        """Returns a constant describing the results of the test: SUCCESS,
        WARNINGS, FAILURE."""

    def getText():
        """Returns a list of short strings which describe the results of the
        test in slightly more detail. Suggested components include
        'failure', 'error', 'passed', 'timeout'."""

    def getLogs():
        # in flux, it may be possible to provide more structured information
        # like python Failure instances
        """Returns a dictionary of test logs. The keys are strings like
        'stdout', 'log', 'exceptions'. The values are strings."""


class IBuildStepStatus(Interface):
    """I hold status for a single BuildStep."""

    def getName():
        """Returns a short string with the name of this step. This string
        may have spaces in it."""

    def getBuild():
        """Returns the IBuildStatus object which contains this step."""

    def getTimes():
        """Returns a tuple of (start, end). 'start' and 'end' are the times
        (seconds since the epoch) when the Step started and finished. If the
        step has not yet started, 'start' will be None. If the step is still
        running, 'end' will be None."""

    def getExpectations():
        """Returns a list of tuples (name, current, target). Each tuple
        describes a single axis along which the step's progress can be
        measured. 'name' is a string which describes the axis itself, like
        'filesCompiled' or 'tests run' or 'bytes of output'. 'current' is a
        number with the progress made so far, while 'target' is the value
        that we expect (based upon past experience) to get to when the build
        is finished.

        'current' will change over time until the step is finished. It is
        'None' until the step starts. When the build is finished, 'current'
        may or may not equal 'target' (which is merely the expectation based
        upon previous builds)."""

    def getURLs():
        """Returns a dictionary of URLs. Each key is a link name (a short
        string, like 'results' or 'coverage'), and each value is a URL. These
        links will be displayed along with the LogFiles.
        """

    def getLogs():
        """Returns a list of IStatusLog objects. If the step has not yet
        finished, this list may be incomplete (asking again later may give
        you more of them)."""


    def isFinished():
        """Return a boolean. True means the step has finished, False means it
        is still running."""

    def waitUntilFinished():
        """Return a Deferred that will fire when the step finishes. If the
        step has already finished, this deferred will fire right away. The
        callback is given this IBuildStepStatus instance as an argument."""

    # while the step is running, the following methods make sense.
    # Afterwards they return None

    def getETA():
        """Returns the number of seconds from now in which the step is
        expected to finish, or None if we can't make a guess. This guess will
        be refined over time."""

    # Once you know the step has finished, the following methods are legal.
    # Before ths step has finished, they all return None.

    def getText():
        """Returns a list of strings which describe the step. These are
        intended to be displayed in a narrow column. If more space is
        available, the caller should join them together with spaces before
        presenting them to the user."""

    def getColor():
        """Returns a single string with the color that should be used to
        display this step. 'green', 'orange', 'red' and 'yellow' are the
        most likely ones."""

    def getResults():
        """Return a tuple describing the results of the step: (result,
        strings). 'result' is one of the constants in
        buildbot.status.builder: SUCCESS, WARNINGS, FAILURE, or SKIPPED.
        'strings' is an optional list of strings that the step wants to
        append to the overall build's results. These strings are usually
        more terse than the ones returned by getText(): in particular,
        successful Steps do not usually contribute any text to the overall
        build."""

    # subscription interface

    def subscribe(receiver, updateInterval=10):
        """Register an IStatusReceiver to receive new status events. The
        receiver will be given logStarted and logFinished messages. It will
        also be given a ETAUpdate message every 'updateInterval' seconds."""

    def unsubscribe(receiver):
        """Unregister an IStatusReceiver. No further status messgaes will be
        delivered."""

class IStatusEvent(Interface):
    """I represent a Builder Event, something non-Build related that can
    happen to a Builder."""

    def getTimes():
        """Returns a tuple of (start, end) like IBuildStepStatus, but end==0
        indicates that this is a 'point event', which has no duration.
        SlaveConnect/Disconnect are point events. Ping is not: it starts
        when requested and ends when the response (positive or negative) is
        returned"""

    def getText():
        """Returns a list of strings which describe the event. These are
        intended to be displayed in a narrow column. If more space is
        available, the caller should join them together with spaces before
        presenting them to the user."""

    def getColor():
        """Returns a single string with the color that should be used to
        display this event. 'red' and 'yellow' are the most likely ones."""


LOG_CHANNEL_STDOUT = 0
LOG_CHANNEL_STDERR = 1
LOG_CHANNEL_HEADER = 2

class IStatusLog(Interface):
    """I represent a single Log, which is a growing list of text items that
    contains some kind of output for a single BuildStep. I might be finished,
    in which case this list has stopped growing.

    Each Log has a name, usually something boring like 'log' or 'output'.
    These names are not guaranteed to be unique, however they are usually
    chosen to be useful within the scope of a single step (i.e. the Compile
    step might produce both 'log' and 'warnings'). The name may also have
    spaces. If you want something more globally meaningful, at least within a
    given Build, try::

      '%s.%s' % (log.getStep.getName(), log.getName())

    The Log can be presented as plain text, or it can be accessed as a list
    of items, each of which has a channel indicator (header, stdout, stderr)
    and a text chunk. An HTML display might represent the interleaved
    channels with different styles, while a straight download-the-text
    interface would just want to retrieve a big string.

    The 'header' channel is used by ShellCommands to prepend a note about
    which command is about to be run ('running command FOO in directory
    DIR'), and append another note giving the exit code of the process.

    Logs can be streaming: if the Log has not yet finished, you can
    subscribe to receive new chunks as they are added.

    A ShellCommand will have a Log associated with it that gathers stdout
    and stderr. Logs may also be created by parsing command output or
    through other synthetic means (grepping for all the warnings in a
    compile log, or listing all the test cases that are going to be run).
    Such synthetic Logs are usually finished as soon as they are created."""


    def getName():
        """Returns a short string with the name of this log, probably 'log'.
        """

    def getStep():
        """Returns the IBuildStepStatus which owns this log."""
        # TODO: can there be non-Step logs?

    def isFinished():
        """Return a boolean. True means the log has finished and is closed,
        False means it is still open and new chunks may be added to it."""

    def waitUntilFinished():
        """Return a Deferred that will fire when the log is closed. If the
        log has already finished, this deferred will fire right away. The
        callback is given this IStatusLog instance as an argument."""

    def subscribe(receiver, catchup):
        """Register an IStatusReceiver to receive chunks (with logChunk) as
        data is added to the Log. If you use this, you will also want to use
        waitUntilFinished to find out when the listener can be retired.
        Subscribing to a closed Log is a no-op.

        If 'catchup' is True, the receiver will immediately be sent a series
        of logChunk messages to bring it up to date with the partially-filled
        log. This allows a status client to join a Log already in progress
        without missing any data. If the Log has already finished, it is too
        late to catch up: just do getText() instead.

        If the Log is very large, the receiver will be called many times with
        a lot of data. There is no way to throttle this data. If the receiver
        is planning on sending the data on to somewhere else, over a narrow
        connection, you can get a throttleable subscription by using
        C{subscribeConsumer} instead."""

    def unsubscribe(receiver):
        """Remove a receiver previously registered with subscribe(). Attempts
        to remove a receiver which was not previously registered is a no-op.
        """

    def subscribeConsumer(consumer):
        """Register an L{IStatusLogConsumer} to receive all chunks of the
        logfile, including all the old entries and any that will arrive in
        the future. The consumer will first have their C{registerProducer}
        method invoked with a reference to an object that can be told
        C{pauseProducing}, C{resumeProducing}, and C{stopProducing}. Then the
        consumer's C{writeChunk} method will be called repeatedly with each
        (channel, text) tuple in the log, starting with the very first. The
        consumer will be notified with C{finish} when the log has been
        exhausted (which can only happen when the log is finished). Note that
        a small amount of data could be written via C{writeChunk} even after
        C{pauseProducing} has been called.

        To unsubscribe the consumer, use C{producer.stopProducing}."""

    # once the log has finished, the following methods make sense. They can
    # be called earlier, but they will only return the contents of the log up
    # to the point at which they were called. You will lose items that are
    # added later. Use C{subscribe} or C{subscribeConsumer} to avoid missing
    # anything.

    def hasContents():
        """Returns True if the LogFile still has contents available. Returns
        False for logs that have been pruned. Clients should test this before
        offering to show the contents of any log."""

    def getText():
        """Return one big string with the contents of the Log. This merges
        all non-header chunks together."""

    def readlines(channel=LOG_CHANNEL_STDOUT):
        """Read lines from one channel of the logfile. This returns an
        iterator that will provide single lines of text (including the
        trailing newline).
        """

    def getTextWithHeaders():
        """Return one big string with the contents of the Log. This merges
        all chunks (including headers) together."""

    def getChunks():
        """Generate a list of (channel, text) tuples. 'channel' is a number,
        0 for stdout, 1 for stderr, 2 for header. (note that stderr is merged
        into stdout if PTYs are in use)."""

class IStatusLogConsumer(Interface):
    """I am an object which can be passed to IStatusLog.subscribeConsumer().
    I represent a target for writing the contents of an IStatusLog. This
    differs from a regular IStatusReceiver in that it can pause the producer.
    This makes it more suitable for use in streaming data over network
    sockets, such as an HTTP request. Note that the consumer can only pause
    the producer until it has caught up with all the old data. After that
    point, C{pauseProducing} is ignored and all new output from the log is
    sent directoy to the consumer."""

    def registerProducer(producer, streaming):
        """A producer is being hooked up to this consumer. The consumer only
        has to handle a single producer. It should send .pauseProducing and
        .resumeProducing messages to the producer when it wants to stop or
        resume the flow of data. 'streaming' will be set to True because the
        producer is always a PushProducer.
        """

    def unregisterProducer():
        """The previously-registered producer has been removed. No further
        pauseProducing or resumeProducing calls should be made. The consumer
        should delete its reference to the Producer so it can be released."""

    def writeChunk(chunk):
        """A chunk (i.e. a tuple of (channel, text)) is being written to the
        consumer."""

    def finish():
        """The log has finished sending chunks to the consumer."""

class IStatusReceiver(Interface):
    """I am an object which can receive build status updates. I may be
    subscribed to an IStatus, an IBuilderStatus, or an IBuildStatus."""

    def buildsetSubmitted(buildset):
        """A new BuildSet has been submitted to the buildmaster.

        @type buildset: implementor of L{IBuildSetStatus}
        """

    def builderAdded(builderName, builder):
        """
        A new Builder has just been added. This method may return an
        IStatusReceiver (probably 'self') which will be subscribed to receive
        builderChangedState and buildStarted/Finished events.

        @type  builderName: string
        @type  builder:     L{buildbot.status.builder.BuilderStatus}
        @rtype: implementor of L{IStatusReceiver}
        """

    def builderChangedState(builderName, state):
        """Builder 'builderName' has changed state. The possible values for
        'state' are 'offline', 'idle', and 'building'."""

    def buildStarted(builderName, build):
        """Builder 'builderName' has just started a build. The build is an
        object which implements IBuildStatus, and can be queried for more
        information.

        This method may return an IStatusReceiver (it could even return
        'self'). If it does so, stepStarted and stepFinished methods will be
        invoked on the object for the steps of this one build. This is a
        convenient way to subscribe to all build steps without missing any.
        This receiver will automatically be unsubscribed when the build
        finishes.

        It can also return a tuple of (IStatusReceiver, interval), in which
        case buildETAUpdate messages are sent ever 'interval' seconds, in
        addition to the stepStarted and stepFinished messages."""

    def buildETAUpdate(build, ETA):
        """This is a periodic update on the progress this Build has made
        towards completion."""

    def stepStarted(build, step):
        """A step has just started. 'step' is the IBuildStepStatus which
        represents the step: it can be queried for more information.

        This method may return an IStatusReceiver (it could even return
        'self'). If it does so, logStarted and logFinished methods will be
        invoked on the object for logs created by this one step. This
        receiver will be automatically unsubscribed when the step finishes.

        Alternatively, the method may return a tuple of an IStatusReceiver
        and an integer named 'updateInterval'. In addition to
        logStarted/logFinished messages, it will also receive stepETAUpdate
        messages about every updateInterval seconds."""

    def stepETAUpdate(build, step, ETA, expectations):
        """This is a periodic update on the progress this Step has made
        towards completion. It gets an ETA (in seconds from the present) of
        when the step ought to be complete, and a list of expectation tuples
        (as returned by IBuildStepStatus.getExpectations) with more detailed
        information."""

    def logStarted(build, step, log):
        """A new Log has been started, probably because a step has just
        started running a shell command. 'log' is the IStatusLog object
        which can be queried for more information.

        This method may return an IStatusReceiver (such as 'self'), in which
        case the target's logChunk method will be invoked as text is added to
        the logfile. This receiver will automatically be unsubsribed when the
        log finishes."""

    def logChunk(build, step, log, channel, text):
        """Some text has been added to this log. 'channel' is one of
        LOG_CHANNEL_STDOUT, LOG_CHANNEL_STDERR, or LOG_CHANNEL_HEADER, as
        defined in IStatusLog.getChunks."""

    def logFinished(build, step, log):
        """A Log has been closed."""

    def stepFinished(build, step, results):
        """A step has just finished. 'results' is the result tuple described
        in IBuildStepStatus.getResults."""

    def buildFinished(builderName, build, results):
        """
        A build has just finished. 'results' is the result tuple described
        in L{IBuildStatus.getResults}.

        @type  builderName: string
        @type  build:       L{buildbot.status.builder.BuildStatus}
        @type  results:     tuple
        """

    def builderRemoved(builderName):
        """The Builder has been removed."""

class IControl(Interface):
    def addChange(change):
        """Add a change to all builders. Each Builder will decide for
        themselves whether the change is interesting or not, and may initiate
        a build as a result."""

    def submitBuildSet(buildset):
        """Submit a BuildSet object, which will eventually be run on all of
        the builders listed therein."""

    def getBuilder(name):
        """Retrieve the IBuilderControl object for the given Builder."""

class IBuilderControl(Interface):
    def requestBuild(request):
        """Queue a L{buildbot.process.base.BuildRequest} object for later
        building."""

    def requestBuildSoon(request):
        """Submit a BuildRequest like requestBuild, but raise a
        L{buildbot.interfaces.NoSlaveError} if no slaves are currently
        available, so it cannot be used to queue a BuildRequest in the hopes
        that a slave will eventually connect. This method is appropriate for
        use by things like the web-page 'Force Build' button."""

    def resubmitBuild(buildStatus, reason="<rebuild, no reason given>"):
        """Rebuild something we've already built before. This submits a
        BuildRequest to our Builder using the same SourceStamp as the earlier
        build. This has no effect (but may eventually raise an exception) if
        this Build has not yet finished."""

    def getPendingBuilds():
        """Return a list of L{IBuildRequestControl} objects for this Builder.
        Each one corresponds to a pending build that has not yet started (due
        to a scarcity of build slaves). These upcoming builds can be canceled
        through the control object."""

    def getBuild(number):
        """Attempt to return an IBuildControl object for the given build.
        Returns None if no such object is available. This will only work for
        the build that is currently in progress: once the build finishes,
        there is nothing to control anymore."""

    def ping(timeout=30):
        """Attempt to contact the slave and see if it is still alive. This
        returns a Deferred which fires with either True (the slave is still
        alive) or False (the slave did not respond). As a side effect, adds
        an event to this builder's column in the waterfall display
        containing the results of the ping."""
        # TODO: this ought to live in ISlaveControl, maybe with disconnect()
        # or something. However the event that is emitted is most useful in
        # the Builder column, so it kinda fits here too.

class IBuildRequestControl(Interface):
    def subscribe(observer):
        """Register a callable that will be invoked (with a single
        IBuildControl object) for each Build that is created to satisfy this
        request. There may be multiple Builds created in an attempt to handle
        the request: they may be interrupted by the user or abandoned due to
        a lost slave. The last Build (the one which actually gets to run to
        completion) is said to 'satisfy' the BuildRequest. The observer will
        be called once for each of these Builds, both old and new."""
    def unsubscribe(observer):
        """Unregister the callable that was registered with subscribe()."""
    def cancel():
        """Remove the build from the pending queue. Has no effect if the
        build has already been started."""

class IBuildControl(Interface):
    def getStatus():
        """Return an IBuildStatus object for the Build that I control."""
    def stopBuild(reason="<no reason given>"):
        """Halt the build. This has no effect if the build has already
        finished."""

class ILogFile(Interface):
    """This is the internal interface to a LogFile, used by the BuildStep to
    write data into the log.
    """
    def addStdout(data):
        pass
    def addStderr(data):
        pass
    def addHeader(data):
        pass
    def finish():
        """The process that is feeding the log file has finished, and no
        further data will be added. This closes the logfile."""

class ILogObserver(Interface):
    """Objects which provide this interface can be used in a BuildStep to
    watch the output of a LogFile and parse it incrementally.
    """

    # internal methods
    def setStep(step):
        pass
    def setLog(log):
        pass

    # methods called by the LogFile
    def logChunk(build, step, log, channel, text):
        pass

