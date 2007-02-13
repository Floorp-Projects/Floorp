# -*- test-case-name: buildbot.test.test_status -*-

from __future__ import generators

from twisted.python import log
from twisted.persisted import styles
from twisted.internet import reactor, defer
from twisted.protocols import basic

import os, shutil, sys, re, urllib
try:
    import cPickle
    pickle = cPickle
except ImportError:
    import pickle
try:
    from cStringIO import StringIO
except ImportError:
    from StringIO import StringIO

# sibling imports
from buildbot import interfaces, util, sourcestamp
from buildbot.twcompat import implements, providedBy

SUCCESS, WARNINGS, FAILURE, SKIPPED, EXCEPTION = range(5)
Results = ["success", "warnings", "failure", "skipped", "exception"]


# build processes call the following methods:
#
#  setDefaults
#
#  currentlyBuilding
#  currentlyIdle
#  currentlyInterlocked
#  currentlyOffline
#  currentlyWaiting
#
#  setCurrentActivity
#  updateCurrentActivity
#  addFileToCurrentActivity
#  finishCurrentActivity
#
#  startBuild
#  finishBuild

STDOUT = interfaces.LOG_CHANNEL_STDOUT
STDERR = interfaces.LOG_CHANNEL_STDERR
HEADER = interfaces.LOG_CHANNEL_HEADER
ChunkTypes = ["stdout", "stderr", "header"]

class LogFileScanner(basic.NetstringReceiver):
    def __init__(self, chunk_cb, channels=[]):
        self.chunk_cb = chunk_cb
        self.channels = channels

    def stringReceived(self, line):
        channel = int(line[0])
        if not self.channels or (channel in self.channels):
            self.chunk_cb((channel, line[1:]))

class LogFileProducer:
    """What's the plan?

    the LogFile has just one FD, used for both reading and writing.
    Each time you add an entry, fd.seek to the end and then write.

    Each reader (i.e. Producer) keeps track of their own offset. The reader
    starts by seeking to the start of the logfile, and reading forwards.
    Between each hunk of file they yield chunks, so they must remember their
    offset before yielding and re-seek back to that offset before reading
    more data. When their read() returns EOF, they're finished with the first
    phase of the reading (everything that's already been written to disk).

    After EOF, the remaining data is entirely in the current entries list.
    These entries are all of the same channel, so we can do one "".join and
    obtain a single chunk to be sent to the listener. But since that involves
    a yield, and more data might arrive after we give up control, we have to
    subscribe them before yielding. We can't subscribe them any earlier,
    otherwise they'd get data out of order.

    We're using a generator in the first place so that the listener can
    throttle us, which means they're pulling. But the subscription means
    we're pushing. Really we're a Producer. In the first phase we can be
    either a PullProducer or a PushProducer. In the second phase we're only a
    PushProducer.

    So the client gives a LogFileConsumer to File.subscribeConsumer . This
    Consumer must have registerProducer(), unregisterProducer(), and
    writeChunk(), and is just like a regular twisted.interfaces.IConsumer,
    except that writeChunk() takes chunks (tuples of (channel,text)) instead
    of the normal write() which takes just text. The LogFileConsumer is
    allowed to call stopProducing, pauseProducing, and resumeProducing on the
    producer instance it is given. """

    paused = False
    subscribed = False
    BUFFERSIZE = 2048

    def __init__(self, logfile, consumer):
        self.logfile = logfile
        self.consumer = consumer
        self.chunkGenerator = self.getChunks()
        consumer.registerProducer(self, True)

    def getChunks(self):
        f = self.logfile.getFile()
        offset = 0
        chunks = []
        p = LogFileScanner(chunks.append)
        f.seek(offset)
        data = f.read(self.BUFFERSIZE)
        offset = f.tell()
        while data:
            p.dataReceived(data)
            while chunks:
                c = chunks.pop(0)
                yield c
            f.seek(offset)
            data = f.read(self.BUFFERSIZE)
            offset = f.tell()
        del f

        # now subscribe them to receive new entries
        self.subscribed = True
        self.logfile.watchers.append(self)
        d = self.logfile.waitUntilFinished()

        # then give them the not-yet-merged data
        if self.logfile.runEntries:
            channel = self.logfile.runEntries[0][0]
            text = "".join([c[1] for c in self.logfile.runEntries])
            yield (channel, text)

        # now we've caught up to the present. Anything further will come from
        # the logfile subscription. We add the callback *after* yielding the
        # data from runEntries, because the logfile might have finished
        # during the yield.
        d.addCallback(self.logfileFinished)

    def stopProducing(self):
        # TODO: should we still call consumer.finish? probably not.
        self.paused = True
        self.consumer = None
        self.done()

    def done(self):
        if self.chunkGenerator:
            self.chunkGenerator = None # stop making chunks
        if self.subscribed:
            self.logfile.watchers.remove(self)
            self.subscribed = False

    def pauseProducing(self):
        self.paused = True

    def resumeProducing(self):
        # Twisted-1.3.0 has a bug which causes hangs when resumeProducing
        # calls transport.write (there is a recursive loop, fixed in 2.0 in
        # t.i.abstract.FileDescriptor.doWrite by setting the producerPaused
        # flag *before* calling resumeProducing). To work around this, we
        # just put off the real resumeProducing for a moment. This probably
        # has a performance hit, but I'm going to assume that the log files
        # are not retrieved frequently enough for it to be an issue.

        reactor.callLater(0, self._resumeProducing)

    def _resumeProducing(self):
        self.paused = False
        if not self.chunkGenerator:
            return
        try:
            while not self.paused:
                chunk = self.chunkGenerator.next()
                self.consumer.writeChunk(chunk)
                # we exit this when the consumer says to stop, or we run out
                # of chunks
        except StopIteration:
            # if the generator finished, it will have done releaseFile
            self.chunkGenerator = None
        # now everything goes through the subscription, and they don't get to
        # pause anymore

    def logChunk(self, build, step, logfile, channel, chunk):
        if self.consumer:
            self.consumer.writeChunk((channel, chunk))

    def logfileFinished(self, logfile):
        self.done()
        if self.consumer:
            self.consumer.unregisterProducer()
            self.consumer.finish()
            self.consumer = None

class LogFile:
    """A LogFile keeps all of its contents on disk, in a non-pickle format to
    which new entries can easily be appended. The file on disk has a name
    like 12-log-compile-output, under the Builder's directory. The actual
    filename is generated (before the LogFile is created) by
    L{BuildStatus.generateLogfileName}.

    Old LogFile pickles (which kept their contents in .entries) must be
    upgraded. The L{BuilderStatus} is responsible for doing this, when it
    loads the L{BuildStatus} into memory. The Build pickle is not modified,
    so users who go from 0.6.5 back to 0.6.4 don't have to lose their
    logs."""

    if implements:
        implements(interfaces.IStatusLog, interfaces.ILogFile)
    else:
        __implements__ = (interfaces.IStatusLog, interfaces.ILogFile)

    finished = False
    length = 0
    chunkSize = 10*1000
    runLength = 0
    runEntries = [] # provided so old pickled builds will getChunks() ok
    entries = None
    BUFFERSIZE = 2048
    filename = None # relative to the Builder's basedir
    openfile = None

    def __init__(self, parent, name, logfilename):
        """
        @type  parent: L{BuildStepStatus}
        @param parent: the Step that this log is a part of
        @type  name: string
        @param name: the name of this log, typically 'output'
        @type  logfilename: string
        @param logfilename: the Builder-relative pathname for the saved entries
        """
        self.step = parent
        self.name = name
        self.filename = logfilename
        fn = self.getFilename()
        if os.path.exists(fn):
            # the buildmaster was probably stopped abruptly, before the
            # BuilderStatus could be saved, so BuilderStatus.nextBuildNumber
            # is out of date, and we're overlapping with earlier builds now.
            # Warn about it, but then overwrite the old pickle file
            log.msg("Warning: Overwriting old serialized Build at %s" % fn)
        self.openfile = open(fn, "w+")
        self.runEntries = []
        self.watchers = []
        self.finishedWatchers = []

    def getFilename(self):
        return os.path.join(self.step.build.builder.basedir, self.filename)

    def hasContents(self):
        return os.path.exists(self.getFilename())

    def getName(self):
        return self.name

    def getStep(self):
        return self.step

    def isFinished(self):
        return self.finished
    def waitUntilFinished(self):
        if self.finished:
            d = defer.succeed(self)
        else:
            d = defer.Deferred()
            self.finishedWatchers.append(d)
        return d

    def getFile(self):
        if self.openfile:
            # this is the filehandle we're using to write to the log, so
            # don't close it!
            return self.openfile
        # otherwise they get their own read-only handle
        return open(self.getFilename(), "r")

    def getText(self):
        # this produces one ginormous string
        return "".join(self.getChunks([STDOUT, STDERR], onlyText=True))

    def getTextWithHeaders(self):
        return "".join(self.getChunks(onlyText=True))

    def getChunks(self, channels=[], onlyText=False):
        # generate chunks for everything that was logged at the time we were
        # first called, so remember how long the file was when we started.
        # Don't read beyond that point. The current contents of
        # self.runEntries will follow.

        # this returns an iterator, which means arbitrary things could happen
        # while we're yielding. This will faithfully deliver the log as it
        # existed when it was started, and not return anything after that
        # point. To use this in subscribe(catchup=True) without missing any
        # data, you must insure that nothing will be added to the log during
        # yield() calls.

        f = self.getFile()
        offset = 0
        f.seek(0, 2)
        remaining = f.tell()

        leftover = None
        if self.runEntries and (not channels or
                                (self.runEntries[0][0] in channels)):
            leftover = (self.runEntries[0][0],
                        "".join([c[1] for c in self.runEntries]))

        # freeze the state of the LogFile by passing a lot of parameters into
        # a generator
        return self._generateChunks(f, offset, remaining, leftover,
                                    channels, onlyText)

    def _generateChunks(self, f, offset, remaining, leftover,
                        channels, onlyText):
        chunks = []
        p = LogFileScanner(chunks.append, channels)
        f.seek(offset)
        data = f.read(min(remaining, self.BUFFERSIZE))
        remaining -= len(data)
        offset = f.tell()
        while data:
            p.dataReceived(data)
            while chunks:
                channel, text = chunks.pop(0)
                if onlyText:
                    yield text
                else:
                    yield (channel, text)
            f.seek(offset)
            data = f.read(min(remaining, self.BUFFERSIZE))
            remaining -= len(data)
            offset = f.tell()
        del f

        if leftover:
            if onlyText:
                yield leftover[1]
            else:
                yield leftover

    def readlines(self, channel=STDOUT):
        """Return an iterator that produces newline-terminated lines,
        excluding header chunks."""
        # TODO: make this memory-efficient, by turning it into a generator
        # that retrieves chunks as necessary, like a pull-driven version of
        # twisted.protocols.basic.LineReceiver
        alltext = "".join(self.getChunks([channel], onlyText=True))
        io = StringIO(alltext)
        return io.readlines()

    def subscribe(self, receiver, catchup):
        if self.finished:
            return
        self.watchers.append(receiver)
        if catchup:
            for channel, text in self.getChunks():
                # TODO: add logChunks(), to send over everything at once?
                receiver.logChunk(self.step.build, self.step, self,
                                  channel, text)

    def unsubscribe(self, receiver):
        if receiver in self.watchers:
            self.watchers.remove(receiver)

    def subscribeConsumer(self, consumer):
        p = LogFileProducer(self, consumer)
        p.resumeProducing()

    # interface used by the build steps to add things to the log

    def merge(self):
        # merge all .runEntries (which are all of the same type) into a
        # single chunk for .entries
        if not self.runEntries:
            return
        channel = self.runEntries[0][0]
        text = "".join([c[1] for c in self.runEntries])
        assert channel < 10
        f = self.openfile
        f.seek(0, 2)
        offset = 0
        while offset < len(text):
            size = min(len(text)-offset, self.chunkSize)
            f.write("%d:%d" % (1 + size, channel))
            f.write(text[offset:offset+size])
            f.write(",")
            offset += size
        self.runEntries = []
        self.runLength = 0

    def addEntry(self, channel, text):
        assert not self.finished
        # we only add to .runEntries here. merge() is responsible for adding
        # merged chunks to .entries
        if self.runEntries and channel != self.runEntries[0][0]:
            self.merge()
        self.runEntries.append((channel, text))
        self.runLength += len(text)
        if self.runLength >= self.chunkSize:
            self.merge()

        for w in self.watchers:
            w.logChunk(self.step.build, self.step, self, channel, text)
        self.length += len(text)

    def addStdout(self, text):
        self.addEntry(STDOUT, text)
    def addStderr(self, text):
        self.addEntry(STDERR, text)
    def addHeader(self, text):
        self.addEntry(HEADER, text)

    def finish(self):
        self.merge()
        if self.openfile:
            # we don't do an explicit close, because there might be readers
            # shareing the filehandle. As soon as they stop reading, the
            # filehandle will be released and automatically closed. We will
            # do a sync, however, to make sure the log gets saved in case of
            # a crash.
            os.fsync(self.openfile.fileno())
            del self.openfile
        self.finished = True
        watchers = self.finishedWatchers
        self.finishedWatchers = []
        for w in watchers:
            w.callback(self)
        self.watchers = []

    # persistence stuff
    def __getstate__(self):
        d = self.__dict__.copy()
        del d['step'] # filled in upon unpickling
        del d['watchers']
        del d['finishedWatchers']
        d['entries'] = [] # let 0.6.4 tolerate the saved log. TODO: really?
        if d.has_key('finished'):
            del d['finished']
        if d.has_key('openfile'):
            del d['openfile']
        return d

    def __setstate__(self, d):
        self.__dict__ = d
        self.watchers = [] # probably not necessary
        self.finishedWatchers = [] # same
        # self.step must be filled in by our parent
        self.finished = True

    def upgrade(self, logfilename):
        """Save our .entries to a new-style offline log file (if necessary),
        and modify our in-memory representation to use it. The original
        pickled LogFile (inside the pickled Build) won't be modified."""
        self.filename = logfilename
        if not os.path.exists(self.getFilename()):
            self.openfile = open(self.getFilename(), "w")
            self.finished = False
            for channel,text in self.entries:
                self.addEntry(channel, text)
            self.finish() # releases self.openfile, which will be closed
        del self.entries


class HTMLLogFile:
    if implements:
        implements(interfaces.IStatusLog)
    else:
        __implements__ = interfaces.IStatusLog,

    filename = None

    def __init__(self, parent, name, logfilename, html):
        self.step = parent
        self.name = name
        self.filename = logfilename
        self.html = html

    def getName(self):
        return self.name # set in BuildStepStatus.addLog
    def getStep(self):
        return self.step

    def isFinished(self):
        return True
    def waitUntilFinished(self):
        return defer.succeed(self)

    def hasContents(self):
        return True
    def getText(self):
        return self.html # looks kinda like text
    def getTextWithHeaders(self):
        return self.html
    def getChunks(self):
        return [(STDERR, self.html)]

    def subscribe(self, receiver, catchup):
        pass
    def unsubscribe(self, receiver):
        pass

    def finish(self):
        pass

    def __getstate__(self):
        d = self.__dict__.copy()
        del d['step']
        return d

    def upgrade(self, logfilename):
        pass


class Event:
    if implements:
        implements(interfaces.IStatusEvent)
    else:
        __implements__ = interfaces.IStatusEvent,

    started = None
    finished = None
    text = []
    color = None

    # IStatusEvent methods
    def getTimes(self):
        return (self.started, self.finished)
    def getText(self):
        return self.text
    def getColor(self):
        return self.color
    def getLogs(self):
        return []

    def finish(self):
        self.finished = util.now()

class TestResult:
    if implements:
        implements(interfaces.ITestResult)
    else:
        __implements__ = interfaces.ITestResult,

    def __init__(self, name, results, text, logs):
        assert isinstance(name, tuple)
        self.name = name
        self.results = results
        self.text = text
        self.logs = logs

    def getName(self):
        return self.name

    def getResults(self):
        return self.results

    def getText(self):
        return self.text

    def getLogs(self):
        return self.logs


class BuildSetStatus:
    if implements:
        implements(interfaces.IBuildSetStatus)
    else:
        __implements__ = interfaces.IBuildSetStatus,

    def __init__(self, source, reason, builderNames, bsid=None):
        self.source = source
        self.reason = reason
        self.builderNames = builderNames
        self.id = bsid
        self.successWatchers = []
        self.finishedWatchers = []
        self.stillHopeful = True
        self.finished = False

    def setBuildRequestStatuses(self, buildRequestStatuses):
        self.buildRequests = buildRequestStatuses
    def setResults(self, results):
        # the build set succeeds only if all its component builds succeed
        self.results = results
    def giveUpHope(self):
        self.stillHopeful = False


    def notifySuccessWatchers(self):
        for d in self.successWatchers:
            d.callback(self)
        self.successWatchers = []

    def notifyFinishedWatchers(self):
        self.finished = True
        for d in self.finishedWatchers:
            d.callback(self)
        self.finishedWatchers = []

    # methods for our clients

    def getSourceStamp(self):
        return self.source
    def getReason(self):
        return self.reason
    def getResults(self):
        return self.results
    def getID(self):
        return self.id

    def getBuilderNames(self):
        return self.builderNames
    def getBuildRequests(self):
        return self.buildRequests
    def isFinished(self):
        return self.finished
    
    def waitUntilSuccess(self):
        if self.finished or not self.stillHopeful:
            # the deferreds have already fired
            return defer.succeed(self)
        d = defer.Deferred()
        self.successWatchers.append(d)
        return d

    def waitUntilFinished(self):
        if self.finished:
            return defer.succeed(self)
        d = defer.Deferred()
        self.finishedWatchers.append(d)
        return d

class BuildRequestStatus:
    if implements:
        implements(interfaces.IBuildRequestStatus)
    else:
        __implements__ = interfaces.IBuildRequestStatus,

    def __init__(self, source, builderName):
        self.source = source
        self.builderName = builderName
        self.builds = [] # list of BuildStatus objects
        self.observers = []

    def buildStarted(self, build):
        self.builds.append(build)
        for o in self.observers[:]:
            o(build)

    # methods called by our clients
    def getSourceStamp(self):
        return self.source
    def getBuilderName(self):
        return self.builderName
    def getBuilds(self):
        return self.builds

    def subscribe(self, observer):
        self.observers.append(observer)
        for b in self.builds:
            observer(b)
    def unsubscribe(self, observer):
        self.observers.remove(observer)


class BuildStepStatus(styles.Versioned):
    """
    I represent a collection of output status for a
    L{buildbot.process.step.BuildStep}.

    @type color: string
    @cvar color: color that this step feels best represents its
                 current mood. yellow,green,red,orange are the
                 most likely choices, although purple indicates
                 an exception
    @type progress: L{buildbot.status.progress.StepProgress}
    @cvar progress: tracks ETA for the step
    @type text: list of strings
    @cvar text: list of short texts that describe the command and its status
    @type text2: list of strings
    @cvar text2: list of short texts added to the overall build description
    @type logs: dict of string -> L{buildbot.status.builder.LogFile}
    @ivar logs: logs of steps
    """
    # note that these are created when the Build is set up, before each
    # corresponding BuildStep has started.
    if implements:
        implements(interfaces.IBuildStepStatus, interfaces.IStatusEvent)
    else:
        __implements__ = interfaces.IBuildStepStatus, interfaces.IStatusEvent
    persistenceVersion = 1

    started = None
    finished = None
    progress = None
    text = []
    color = None
    results = (None, [])
    text2 = []
    watchers = []
    updates = {}
    finishedWatchers = []

    def __init__(self, parent):
        assert interfaces.IBuildStatus(parent)
        self.build = parent
        self.logs = []
        self.urls = {}
        self.watchers = []
        self.updates = {}
        self.finishedWatchers = []

    def getName(self):
        """Returns a short string with the name of this step. This string
        may have spaces in it."""
        return self.name

    def getBuild(self):
        return self.build

    def getTimes(self):
        return (self.started, self.finished)

    def getExpectations(self):
        """Returns a list of tuples (name, current, target)."""
        if not self.progress:
            return []
        ret = []
        metrics = self.progress.progress.keys()
        metrics.sort()
        for m in metrics:
            t = (m, self.progress.progress[m], self.progress.expectations[m])
            ret.append(t)
        return ret

    def getLogs(self):
        return self.logs

    def getURLs(self):
        return self.urls.copy()

    def isFinished(self):
        return (self.finished is not None)

    def waitUntilFinished(self):
        if self.finished:
            d = defer.succeed(self)
        else:
            d = defer.Deferred()
            self.finishedWatchers.append(d)
        return d

    # while the step is running, the following methods make sense.
    # Afterwards they return None

    def getETA(self):
        if self.started is None:
            return None # not started yet
        if self.finished is not None:
            return None # already finished
        if not self.progress:
            return None # no way to predict
        return self.progress.remaining()

    # Once you know the step has finished, the following methods are legal.
    # Before this step has finished, they all return None.

    def getText(self):
        """Returns a list of strings which describe the step. These are
        intended to be displayed in a narrow column. If more space is
        available, the caller should join them together with spaces before
        presenting them to the user."""
        return self.text

    def getColor(self):
        """Returns a single string with the color that should be used to
        display this step. 'green', 'orange', 'red', 'yellow' and 'purple'
        are the most likely ones."""
        return self.color

    def getResults(self):
        """Return a tuple describing the results of the step.
        'result' is one of the constants in L{buildbot.status.builder}:
        SUCCESS, WARNINGS, FAILURE, or SKIPPED.
        'strings' is an optional list of strings that the step wants to
        append to the overall build's results. These strings are usually
        more terse than the ones returned by getText(): in particular,
        successful Steps do not usually contribute any text to the
        overall build.

        @rtype:   tuple of int, list of strings
        @returns: (result, strings)
        """
        return (self.results, self.text2)

    # subscription interface

    def subscribe(self, receiver, updateInterval=10):
        # will get logStarted, logFinished, stepETAUpdate
        assert receiver not in self.watchers
        self.watchers.append(receiver)
        self.sendETAUpdate(receiver, updateInterval)

    def sendETAUpdate(self, receiver, updateInterval):
        self.updates[receiver] = None
        # they might unsubscribe during stepETAUpdate
        receiver.stepETAUpdate(self.build, self,
                           self.getETA(), self.getExpectations())
        if receiver in self.watchers:
            self.updates[receiver] = reactor.callLater(updateInterval,
                                                       self.sendETAUpdate,
                                                       receiver,
                                                       updateInterval)

    def unsubscribe(self, receiver):
        if receiver in self.watchers:
            self.watchers.remove(receiver)
        if receiver in self.updates:
            if self.updates[receiver] is not None:
                self.updates[receiver].cancel()
            del self.updates[receiver]


    # methods to be invoked by the BuildStep

    def setName(self, stepname):
        self.name = stepname

    def setProgress(self, stepprogress):
        self.progress = stepprogress

    def stepStarted(self):
        self.started = util.now()
        if self.build:
            self.build.stepStarted(self)

    def addLog(self, name):
        assert self.started # addLog before stepStarted won't notify watchers
        logfilename = self.build.generateLogfileName(self.name, name)
        log = LogFile(self, name, logfilename)
        self.logs.append(log)
        for w in self.watchers:
            receiver = w.logStarted(self.build, self, log)
            if receiver:
                log.subscribe(receiver, True)
                d = log.waitUntilFinished()
                d.addCallback(lambda log: log.unsubscribe(receiver))
        d = log.waitUntilFinished()
        d.addCallback(self.logFinished)
        return log

    def addHTMLLog(self, name, html):
        assert self.started # addLog before stepStarted won't notify watchers
        logfilename = self.build.generateLogfileName(self.name, name)
        log = HTMLLogFile(self, name, logfilename, html)
        self.logs.append(log)
        for w in self.watchers:
            receiver = w.logStarted(self.build, self, log)
            # TODO: think about this: there isn't much point in letting
            # them subscribe
            #if receiver:
            #    log.subscribe(receiver, True)
            w.logFinished(self.build, self, log)

    def logFinished(self, log):
        for w in self.watchers:
            w.logFinished(self.build, self, log)

    def addURL(self, name, url):
        self.urls[name] = url

    def setColor(self, color):
        self.color = color
    def setText(self, text):
        self.text = text
    def setText2(self, text):
        self.text2 = text

    def stepFinished(self, results):
        self.finished = util.now()
        self.results = results
        for loog in self.logs:
            if not loog.isFinished():
                loog.finish()

        for r in self.updates.keys():
            if self.updates[r] is not None:
                self.updates[r].cancel()
                del self.updates[r]

        watchers = self.finishedWatchers
        self.finishedWatchers = []
        for w in watchers:
            w.callback(self)

    # persistence

    def __getstate__(self):
        d = styles.Versioned.__getstate__(self)
        del d['build'] # filled in when loading
        if d.has_key('progress'):
            del d['progress']
        del d['watchers']
        del d['finishedWatchers']
        del d['updates']
        return d

    def __setstate__(self, d):
        styles.Versioned.__setstate__(self, d)
        # self.build must be filled in by our parent
        for loog in self.logs:
            loog.step = self

    def upgradeToVersion1(self):
        if not hasattr(self, "urls"):
            self.urls = {}


class BuildStatus(styles.Versioned):
    if implements:
        implements(interfaces.IBuildStatus, interfaces.IStatusEvent)
    else:
        __implements__ = interfaces.IBuildStatus, interfaces.IStatusEvent
    persistenceVersion = 2

    source = None
    reason = None
    changes = []
    blamelist = []
    progress = None
    started = None
    finished = None
    currentStep = None
    text = []
    color = None
    results = None
    slavename = "???"

    # these lists/dicts are defined here so that unserialized instances have
    # (empty) values. They are set in __init__ to new objects to make sure
    # each instance gets its own copy.
    watchers = []
    updates = {}
    finishedWatchers = []
    testResults = {}

    def __init__(self, parent, number):
        """
        @type  parent: L{BuilderStatus}
        @type  number: int
        """
        assert interfaces.IBuilderStatus(parent)
        self.builder = parent
        self.number = number
        self.watchers = []
        self.updates = {}
        self.finishedWatchers = []
        self.steps = []
        self.testResults = {}
        self.properties = {}

    # IBuildStatus

    def getBuilder(self):
        """
        @rtype: L{BuilderStatus}
        """
        return self.builder

    def getProperty(self, propname):
        return self.properties[propname]

    def getNumber(self):
        return self.number

    def getPreviousBuild(self):
        if self.number == 0:
            return None
        return self.builder.getBuild(self.number-1)

    def getSourceStamp(self):
        return (self.source.branch, self.source.revision, self.source.patch)

    def getReason(self):
        return self.reason

    def getChanges(self):
        return self.changes

    def getResponsibleUsers(self):
        return self.blamelist

    def getInterestedUsers(self):
        # TODO: the Builder should add others: sheriffs, domain-owners
        return self.blamelist

    def getSteps(self):
        """Return a list of IBuildStepStatus objects. For invariant builds
        (those which always use the same set of Steps), this should be the
        complete list, however some of the steps may not have started yet
        (step.getTimes()[0] will be None). For variant builds, this may not
        be complete (asking again later may give you more of them)."""
        return self.steps

    def getTimes(self):
        return (self.started, self.finished)

    def isFinished(self):
        return (self.finished is not None)

    def waitUntilFinished(self):
        if self.finished:
            d = defer.succeed(self)
        else:
            d = defer.Deferred()
            self.finishedWatchers.append(d)
        return d

    # while the build is running, the following methods make sense.
    # Afterwards they return None

    def getETA(self):
        if self.finished is not None:
            return None
        if not self.progress:
            return None
        eta = self.progress.eta()
        if eta is None:
            return None
        return eta - util.now()

    def getCurrentStep(self):
        return self.currentStep

    # Once you know the build has finished, the following methods are legal.
    # Before ths build has finished, they all return None.

    def getText(self):
        text = []
        text.extend(self.text)
        for s in self.steps:
            text.extend(s.text2)
        return text

    def getColor(self):
        return self.color

    def getResults(self):
        return self.results

    def getSlavename(self):
        return self.slavename

    def getTestResults(self):
        return self.testResults

    def getLogs(self):
        # TODO: steps should contribute significant logs instead of this
        # hack, which returns every log from every step. The logs should get
        # names like "compile" and "test" instead of "compile.output"
        logs = []
        for s in self.steps:
            for log in s.getLogs():
                logs.append(log)
        return logs

    # subscription interface

    def subscribe(self, receiver, updateInterval=None):
        # will receive stepStarted and stepFinished messages
        # and maybe buildETAUpdate
        self.watchers.append(receiver)
        if updateInterval is not None:
            self.sendETAUpdate(receiver, updateInterval)

    def sendETAUpdate(self, receiver, updateInterval):
        self.updates[receiver] = None
        ETA = self.getETA()
        if ETA is not None:
            receiver.buildETAUpdate(self, self.getETA())
        # they might have unsubscribed during buildETAUpdate
        if receiver in self.watchers:
            self.updates[receiver] = reactor.callLater(updateInterval,
                                                       self.sendETAUpdate,
                                                       receiver,
                                                       updateInterval)

    def unsubscribe(self, receiver):
        if receiver in self.watchers:
            self.watchers.remove(receiver)
        if receiver in self.updates:
            if self.updates[receiver] is not None:
                self.updates[receiver].cancel()
            del self.updates[receiver]

    # methods for the base.Build to invoke

    def addStepWithName(self, name):
        """The Build is setting up, and has added a new BuildStep to its
        list. Create a BuildStepStatus object to which it can send status
        updates."""

        s = BuildStepStatus(self)
        s.setName(name)
        self.steps.append(s)
        return s

    def setProperty(self, propname, value):
        self.properties[propname] = value

    def addTestResult(self, result):
        self.testResults[result.getName()] = result

    def setSourceStamp(self, sourceStamp):
        self.source = sourceStamp
        self.changes = self.source.changes

    def setReason(self, reason):
        self.reason = reason
    def setBlamelist(self, blamelist):
        self.blamelist = blamelist
    def setProgress(self, progress):
        self.progress = progress

    def buildStarted(self, build):
        """The Build has been set up and is about to be started. It can now
        be safely queried, so it is time to announce the new build."""

        self.started = util.now()
        # now that we're ready to report status, let the BuilderStatus tell
        # the world about us
        self.builder.buildStarted(self)

    def setSlavename(self, slavename):
        self.slavename = slavename

    def setText(self, text):
        assert isinstance(text, (list, tuple))
        self.text = text
    def setColor(self, color):
        self.color = color
    def setResults(self, results):
        self.results = results

    def buildFinished(self):
        self.currentStep = None
        self.finished = util.now()

        for r in self.updates.keys():
            if self.updates[r] is not None:
                self.updates[r].cancel()
                del self.updates[r]

        watchers = self.finishedWatchers
        self.finishedWatchers = []
        for w in watchers:
            w.callback(self)

    # methods called by our BuildStepStatus children

    def stepStarted(self, step):
        self.currentStep = step
        name = self.getBuilder().getName()
        for w in self.watchers:
            receiver = w.stepStarted(self, step)
            if receiver:
                if type(receiver) == type(()):
                    step.subscribe(receiver[0], receiver[1])
                else:
                    step.subscribe(receiver)
                d = step.waitUntilFinished()
                d.addCallback(lambda step: step.unsubscribe(receiver))

        step.waitUntilFinished().addCallback(self._stepFinished)

    def _stepFinished(self, step):
        results = step.getResults()
        for w in self.watchers:
            w.stepFinished(self, step, results)

    # methods called by our BuilderStatus parent

    def pruneLogs(self):
        # this build is somewhat old: remove the build logs to save space
        # TODO: delete logs visible through IBuildStatus.getLogs
        for s in self.steps:
            s.pruneLogs()

    def pruneSteps(self):
        # this build is very old: remove the build steps too
        self.steps = []

    # persistence stuff

    def generateLogfileName(self, stepname, logname):
        """Return a filename (relative to the Builder's base directory) where
        the logfile's contents can be stored uniquely.

        The base filename is made by combining our build number, the Step's
        name, and the log's name, then removing unsuitable characters. The
        filename is then made unique by appending _0, _1, etc, until it does
        not collide with any other logfile.

        These files are kept in the Builder's basedir (rather than a
        per-Build subdirectory) because that makes cleanup easier: cron and
        find will help get rid of the old logs, but the empty directories are
        more of a hassle to remove."""

        starting_filename = "%d-log-%s-%s" % (self.number, stepname, logname)
        starting_filename = re.sub(r'[^\w\.\-]', '_', starting_filename)
        # now make it unique
        unique_counter = 0
        filename = starting_filename
        while filename in [l.filename
                           for step in self.steps
                           for l in step.getLogs()
                           if l.filename]:
            filename = "%s_%d" % (starting_filename, unique_counter)
            unique_counter += 1
        return filename

    def __getstate__(self):
        d = styles.Versioned.__getstate__(self)
        # for now, a serialized Build is always "finished". We will never
        # save unfinished builds.
        if not self.finished:
            d['finished'] = True
            # TODO: push an "interrupted" step so it is clear that the build
            # was interrupted. The builder will have a 'shutdown' event, but
            # someone looking at just this build will be confused as to why
            # the last log is truncated.
        del d['builder'] # filled in by our parent when loading
        del d['watchers']
        del d['updates']
        del d['finishedWatchers']
        return d

    def __setstate__(self, d):
        styles.Versioned.__setstate__(self, d)
        # self.builder must be filled in by our parent when loading
        for step in self.steps:
            step.build = self
        self.watchers = []
        self.updates = {}
        self.finishedWatchers = []

    def upgradeToVersion1(self):
        if hasattr(self, "sourceStamp"):
            # the old .sourceStamp attribute wasn't actually very useful
            maxChangeNumber, patch = self.sourceStamp
            changes = getattr(self, 'changes', [])
            source = sourcestamp.SourceStamp(branch=None,
                                             revision=None,
                                             patch=patch,
                                             changes=changes)
            self.source = source
            self.changes = source.changes
            del self.sourceStamp

    def upgradeToVersion2(self):
        self.properties = {}

    def upgradeLogfiles(self):
        # upgrade any LogFiles that need it. This must occur after we've been
        # attached to our Builder, and after we know about all LogFiles of
        # all Steps (to get the filenames right).
        assert self.builder
        for s in self.steps:
            for l in s.getLogs():
                if l.filename:
                    pass # new-style, log contents are on disk
                else:
                    logfilename = self.generateLogfileName(s.name, l.name)
                    # let the logfile update its .filename pointer,
                    # transferring its contents onto disk if necessary
                    l.upgrade(logfilename)

    def saveYourself(self):
        filename = os.path.join(self.builder.basedir, "%d" % self.number)
        if os.path.isdir(filename):
            # leftover from 0.5.0, which stored builds in directories
            shutil.rmtree(filename, ignore_errors=True)
        tmpfilename = filename + ".tmp"
        try:
            pickle.dump(self, open(tmpfilename, "wb"), -1)
            if sys.platform == 'win32':
                # windows cannot rename a file on top of an existing one, so
                # fall back to delete-first. There are ways this can fail and
                # lose the builder's history, so we avoid using it in the
                # general (non-windows) case
                if os.path.exists(filename):
                    os.unlink(filename)
            os.rename(tmpfilename, filename)
        except:
            log.msg("unable to save build %s-#%d" % (self.builder.name,
                                                     self.number))
            log.err()



class BuilderStatus(styles.Versioned):
    """I handle status information for a single process.base.Builder object.
    That object sends status changes to me (frequently as Events), and I
    provide them on demand to the various status recipients, like the HTML
    waterfall display and the live status clients. It also sends build
    summaries to me, which I log and provide to status clients who aren't
    interested in seeing details of the individual build steps.

    I am responsible for maintaining the list of historic Events and Builds,
    pruning old ones, and loading them from / saving them to disk.

    I live in the buildbot.process.base.Builder object, in the .statusbag
    attribute.

    @type  category: string
    @ivar  category: user-defined category this builder belongs to; can be
                     used to filter on in status clients
    """

    if implements:
        implements(interfaces.IBuilderStatus)
    else:
        __implements__ = interfaces.IBuilderStatus,
    persistenceVersion = 1

    # these limit the amount of memory we consume, as well as the size of the
    # main Builder pickle. The Build and LogFile pickles on disk must be
    # handled separately.
    buildCacheSize = 30
    buildHorizon = 100 # forget builds beyond this
    stepHorizon = 50 # forget steps in builds beyond this

    category = None
    currentBigState = "offline" # or idle/waiting/interlocked/building
    basedir = None # filled in by our parent

    def __init__(self, buildername, category=None):
        self.name = buildername
        self.category = category

        self.slavenames = []
        self.events = []
        # these three hold Events, and are used to retrieve the current
        # state of the boxes.
        self.lastBuildStatus = None
        #self.currentBig = None
        #self.currentSmall = None
        self.currentBuilds = []
        self.pendingBuilds = []
        self.nextBuild = None
        self.watchers = []
        self.buildCache = [] # TODO: age builds out of the cache

    # persistence

    def __getstate__(self):
        # when saving, don't record transient stuff like what builds are
        # currently running, because they won't be there when we start back
        # up. Nor do we save self.watchers, nor anything that gets set by our
        # parent like .basedir and .status
        d = styles.Versioned.__getstate__(self)
        d['watchers'] = []
        del d['buildCache']
        for b in self.currentBuilds:
            b.saveYourself()
            # TODO: push a 'hey, build was interrupted' event
        del d['currentBuilds']
        del d['pendingBuilds']
        del d['currentBigState']
        del d['basedir']
        del d['status']
        del d['nextBuildNumber']
        return d

    def __setstate__(self, d):
        # when loading, re-initialize the transient stuff. Remember that
        # upgradeToVersion1 and such will be called after this finishes.
        styles.Versioned.__setstate__(self, d)
        self.buildCache = []
        self.currentBuilds = []
        self.pendingBuilds = []
        self.watchers = []
        self.slavenames = []
        # self.basedir must be filled in by our parent
        # self.status must be filled in by our parent

    def upgradeToVersion1(self):
        if hasattr(self, 'slavename'):
            self.slavenames = [self.slavename]
            del self.slavename
        if hasattr(self, 'nextBuildNumber'):
            del self.nextBuildNumber # determineNextBuildNumber chooses this

    def determineNextBuildNumber(self):
        """Scan our directory of saved BuildStatus instances to determine
        what our self.nextBuildNumber should be. Set it one larger than the
        highest-numbered build we discover. This is called by the top-level
        Status object shortly after we are created or loaded from disk.
        """
        existing_builds = [int(f)
                           for f in os.listdir(self.basedir)
                           if re.match("^\d+$", f)]
        if existing_builds:
            self.nextBuildNumber = max(existing_builds) + 1
        else:
            self.nextBuildNumber = 0

    def saveYourself(self):
        for b in self.buildCache:
            if not b.isFinished:
                # interrupted build, need to save it anyway.
                # BuildStatus.saveYourself will mark it as interrupted.
                b.saveYourself()
        filename = os.path.join(self.basedir, "builder")
        tmpfilename = filename + ".tmp"
        try:
            pickle.dump(self, open(tmpfilename, "wb"), -1)
            if sys.platform == 'win32':
                # windows cannot rename a file on top of an existing one
                if os.path.exists(filename):
                    os.unlink(filename)
            os.rename(tmpfilename, filename)
        except:
            log.msg("unable to save builder %s" % self.name)
            log.err()
        

    # build cache management

    def addBuildToCache(self, build):
        if build in self.buildCache:
            return
        self.buildCache.append(build)
        while len(self.buildCache) > self.buildCacheSize:
            self.buildCache.pop(0)

    def getBuildByNumber(self, number):
        for b in self.currentBuilds:
            if b.number == number:
                return b
        for build in self.buildCache:
            if build.number == number:
                return build
        filename = os.path.join(self.basedir, "%d" % number)
        try:
            build = pickle.load(open(filename, "rb"))
            styles.doUpgrade()
            build.builder = self
            # handle LogFiles from after 0.5.0 and before 0.6.5
            build.upgradeLogfiles()
            self.addBuildToCache(build)
            return build
        except IOError:
            raise IndexError("no such build %d" % number)
        except EOFError:
            raise IndexError("corrupted build pickle %d" % number)

    def prune(self):
        return # TODO: change this to walk through the filesystem
        # first, blow away all builds beyond our build horizon
        self.builds = self.builds[-self.buildHorizon:]
        # then prune steps in builds past the step horizon
        for b in self.builds[0:-self.stepHorizon]:
            b.pruneSteps()

    # IBuilderStatus methods
    def getName(self):
        return self.name

    def getState(self):
        return (self.currentBigState, self.currentBuilds)

    def getSlaves(self):
        return [self.status.getSlave(name) for name in self.slavenames]

    def getPendingBuilds(self):
        return self.pendingBuilds

    def getCurrentBuilds(self):
        return self.currentBuilds

    def getLastFinishedBuild(self):
        b = self.getBuild(-1)
        if not (b and b.isFinished()):
            b = self.getBuild(-2)
        return b

    def getBuild(self, number):
        if number < 0:
            number = self.nextBuildNumber + number
        if number < 0 or number >= self.nextBuildNumber:
            return None

        try:
            return self.getBuildByNumber(number)
        except IndexError:
            return None

    def getEvent(self, number):
        try:
            return self.events[number]
        except IndexError:
            return None

    def eventGenerator(self):
        """This function creates a generator which will provide all of this
        Builder's status events, starting with the most recent and
        progressing backwards in time. """

        # remember the oldest-to-earliest flow here. "next" means earlier.

        # TODO: interleave build steps and self.events by timestamp.
        # TODO: um, I think we're already doing that.

        eventIndex = -1
        e = self.getEvent(eventIndex)
        for Nb in range(1, self.nextBuildNumber+1):
            b = self.getBuild(-Nb)
            if not b:
                break
            steps = b.getSteps()
            for Ns in range(1, len(steps)+1):
                if steps[-Ns].started:
                    step_start = steps[-Ns].getTimes()[0]
                    while e is not None and e.getTimes()[0] > step_start:
                        yield e
                        eventIndex -= 1
                        e = self.getEvent(eventIndex)
                    yield steps[-Ns]
            yield b
        while e is not None:
            yield e
            eventIndex -= 1
            e = self.getEvent(eventIndex)

    def subscribe(self, receiver):
        # will get builderChangedState, buildStarted, and buildFinished
        self.watchers.append(receiver)
        self.publishState(receiver)

    def unsubscribe(self, receiver):
        self.watchers.remove(receiver)

    ## Builder interface (methods called by the Builder which feeds us)

    def setSlavenames(self, names):
        self.slavenames = names

    def addEvent(self, text=[], color=None):
        # this adds a duration event. When it is done, the user should call
        # e.finish(). They can also mangle it by modifying .text and .color
        e = Event()
        e.started = util.now()
        e.text = text
        e.color = color
        self.events.append(e)
        return e # they are free to mangle it further

    def addPointEvent(self, text=[], color=None):
        # this adds a point event, one which occurs as a single atomic
        # instant of time.
        e = Event()
        e.started = util.now()
        e.finished = 0
        e.text = text
        e.color = color
        self.events.append(e)
        return e # for consistency, but they really shouldn't touch it

    def setBigState(self, state):
        needToUpdate = state != self.currentBigState
        self.currentBigState = state
        if needToUpdate:
            self.publishState()

    def publishState(self, target=None):
        state = self.currentBigState

        if target is not None:
            # unicast
            target.builderChangedState(self.name, state)
            return
        for w in self.watchers:
            w.builderChangedState(self.name, state)

    def newBuild(self):
        """The Builder has decided to start a build, but the Build object is
        not yet ready to report status (it has not finished creating the
        Steps). Create a BuildStatus object that it can use."""
        number = self.nextBuildNumber
        self.nextBuildNumber += 1
        # TODO: self.saveYourself(), to make sure we don't forget about the
        # build number we've just allocated. This is not quite as important
        # as it was before we switch to determineNextBuildNumber, but I think
        # it may still be useful to have the new build save itself.
        s = BuildStatus(self, number)
        s.waitUntilFinished().addCallback(self._buildFinished)
        return s

    def addBuildRequest(self, brstatus):
        self.pendingBuilds.append(brstatus)
    def removeBuildRequest(self, brstatus):
        self.pendingBuilds.remove(brstatus)

    # buildStarted is called by our child BuildStatus instances
    def buildStarted(self, s):
        """Now the BuildStatus object is ready to go (it knows all of its
        Steps, its ETA, etc), so it is safe to notify our watchers."""

        assert s.builder is self # paranoia
        assert s.number == self.nextBuildNumber - 1
        assert s not in self.currentBuilds
        self.currentBuilds.append(s)
        self.addBuildToCache(s)

        # now that the BuildStatus is prepared to answer queries, we can
        # announce the new build to all our watchers

        for w in self.watchers: # TODO: maybe do this later? callLater(0)?
            receiver = w.buildStarted(self.getName(), s)
            if receiver:
                if type(receiver) == type(()):
                    s.subscribe(receiver[0], receiver[1])
                else:
                    s.subscribe(receiver)
                d = s.waitUntilFinished()
                d.addCallback(lambda s: s.unsubscribe(receiver))


    def _buildFinished(self, s):
        assert s in self.currentBuilds
        s.saveYourself()
        self.currentBuilds.remove(s)

        name = self.getName()
        results = s.getResults()
        for w in self.watchers:
            w.buildFinished(name, s, results)

        self.prune() # conserve disk


    # waterfall display (history)

    # I want some kind of build event that holds everything about the build:
    # why, what changes went into it, the results of the build, itemized
    # test results, etc. But, I do kind of need something to be inserted in
    # the event log first, because intermixing step events and the larger
    # build event is fraught with peril. Maybe an Event-like-thing that
    # doesn't have a file in it but does have links. Hmm, that's exactly
    # what it does now. The only difference would be that this event isn't
    # pushed to the clients.

    # publish to clients
    def sendLastBuildStatus(self, client):
        #client.newLastBuildStatus(self.lastBuildStatus)
        pass
    def sendCurrentActivityBigToEveryone(self):
        for s in self.subscribers:
            self.sendCurrentActivityBig(s)
    def sendCurrentActivityBig(self, client):
        state = self.currentBigState
        if state == "offline":
            client.currentlyOffline()
        elif state == "idle":
            client.currentlyIdle()
        elif state == "building":
            client.currentlyBuilding()
        else:
            log.msg("Hey, self.currentBigState is weird:", state)
            
    
    ## HTML display interface

    def getEventNumbered(self, num):
        # deal with dropped events, pruned events
        first = self.events[0].number
        if first + len(self.events)-1 != self.events[-1].number:
            log.msg(self,
                    "lost an event somewhere: [0] is %d, [%d] is %d" % \
                    (self.events[0].number,
                     len(self.events) - 1,
                     self.events[-1].number))
            for e in self.events:
                log.msg("e[%d]: " % e.number, e)
            return None
        offset = num - first
        log.msg(self, "offset", offset)
        try:
            return self.events[offset]
        except IndexError:
            return None

    ## Persistence of Status
    def loadYourOldEvents(self):
        if hasattr(self, "allEvents"):
            # first time, nothing to get from file. Note that this is only if
            # the Application gets .run() . If it gets .save()'ed, then the
            # .allEvents attribute goes away in the initial __getstate__ and
            # we try to load a non-existent file.
            return
        self.allEvents = self.loadFile("events", [])
        if self.allEvents:
            self.nextEventNumber = self.allEvents[-1].number + 1
        else:
            self.nextEventNumber = 0
    def saveYourOldEvents(self):
        self.saveFile("events", self.allEvents)

    ## clients

    def addClient(self, client):
        if client not in self.subscribers:
            self.subscribers.append(client)
            self.sendLastBuildStatus(client)
            self.sendCurrentActivityBig(client)
            client.newEvent(self.currentSmall)
    def removeClient(self, client):
        if client in self.subscribers:
            self.subscribers.remove(client)

class SlaveStatus:
    if implements:
        implements(interfaces.ISlaveStatus)
    else:
        __implements__ = interfaces.ISlaveStatus,

    admin = None
    host = None
    connected = False

    def __init__(self, name):
        self.name = name

    def getName(self):
        return self.name
    def getAdmin(self):
        return self.admin
    def getHost(self):
        return self.host
    def isConnected(self):
        return self.connected

    def setAdmin(self, admin):
        self.admin = admin
    def setHost(self, host):
        self.host = host
    def setConnected(self, isConnected):
        self.connected = isConnected

class Status:
    """
    I represent the status of the buildmaster.
    """
    if implements:
        implements(interfaces.IStatus)
    else:
        __implements__ = interfaces.IStatus,

    def __init__(self, botmaster, basedir):
        """
        @type  botmaster: L{buildbot.master.BotMaster}
        @param botmaster: the Status object uses C{.botmaster} to get at
                          both the L{buildbot.master.BuildMaster} (for
                          various buildbot-wide parameters) and the
                          actual Builders (to get at their L{BuilderStatus}
                          objects). It is not allowed to change or influence
                          anything through this reference.
        @type  basedir: string
        @param basedir: this provides a base directory in which saved status
                        information (changes.pck, saved Build status
                        pickles) can be stored
        """
        self.botmaster = botmaster
        self.basedir = basedir
        self.watchers = []
        self.activeBuildSets = []
        assert os.path.isdir(basedir)


    # methods called by our clients

    def getProjectName(self):
        return self.botmaster.parent.projectName
    def getProjectURL(self):
        return self.botmaster.parent.projectURL
    def getBuildbotURL(self):
        return self.botmaster.parent.buildbotURL

    def getURLForThing(self, thing):
        prefix = self.getBuildbotURL()
        if not prefix:
            return None
        if providedBy(thing, interfaces.IStatus):
            return prefix
        if providedBy(thing, interfaces.ISchedulerStatus):
            pass
        if providedBy(thing, interfaces.IBuilderStatus):
            builder = thing
            return prefix + urllib.quote(builder.getName(), safe='')
        if providedBy(thing, interfaces.IBuildStatus):
            build = thing
            builder = build.getBuilder()
            return "%s%s/builds/%d" % (
                prefix,
                urllib.quote(builder.getName(), safe=''),
                build.getNumber())
        if providedBy(thing, interfaces.IBuildStepStatus):
            step = thing
            build = step.getBuild()
            builder = build.getBuilder()
            return "%s%s/builds/%d/%s" % (
                prefix,
                urllib.quote(builder.getName(), safe=''),
                build.getNumber(),
                "step-" + urllib.quote(step.getName(), safe=''))
        # IBuildSetStatus
        # IBuildRequestStatus
        # ISlaveStatus

        # IStatusEvent
        if providedBy(thing, interfaces.IStatusEvent):
            from buildbot.changes import changes
            # TODO: this is goofy, create IChange or something
            if isinstance(thing, changes.Change):
                change = thing
                return "%schanges/%d" % (prefix, change.number)

        if providedBy(thing, interfaces.IStatusLog):
            log = thing
            step = log.getStep()
            build = step.getBuild()
            builder = build.getBuilder()

            logs = step.getLogs()
            for i in range(len(logs)):
                if log is logs[i]:
                    lognum = i
                    break
            else:
                return None
            return "%s%s/builds/%d/%s/%d" % (
                prefix,
                urllib.quote(builder.getName(), safe=''),
                build.getNumber(),
                "step-" + urllib.quote(step.getName(), safe=''),
                lognum)


    def getSchedulers(self):
        return self.botmaster.parent.allSchedulers()

    def getBuilderNames(self, categories=None):
        if categories == None:
            return self.botmaster.builderNames[:] # don't let them break it
        
        l = []
        # respect addition order
        for name in self.botmaster.builderNames:
            builder = self.botmaster.builders[name]
            if builder.builder_status.category in categories:
                l.append(name)
        return l

    def getBuilder(self, name):
        """
        @rtype: L{BuilderStatus}
        """
        return self.botmaster.builders[name].builder_status

    def getSlave(self, slavename):
        return self.botmaster.slaves[slavename].slave_status

    def getBuildSets(self):
        return self.activeBuildSets[:]

    def subscribe(self, target):
        self.watchers.append(target)
        for name in self.botmaster.builderNames:
            self.announceNewBuilder(target, name, self.getBuilder(name))
    def unsubscribe(self, target):
        self.watchers.remove(target)


    # methods called by upstream objects

    def announceNewBuilder(self, target, name, builder_status):
        t = target.builderAdded(name, builder_status)
        if t:
            builder_status.subscribe(t)

    def builderAdded(self, name, basedir, category=None):
        """
        @rtype: L{BuilderStatus}
        """
        filename = os.path.join(self.basedir, basedir, "builder")
        log.msg("trying to load status pickle from %s" % filename)
        builder_status = None
        try:
            builder_status = pickle.load(open(filename, "rb"))
            styles.doUpgrade()
        except IOError:
            log.msg("no saved status pickle, creating a new one")
        except:
            log.msg("error while loading status pickle, creating a new one")
            log.msg("error follows:")
            log.err()
        if not builder_status:
            builder_status = BuilderStatus(name, category)
            builder_status.addPointEvent(["builder", "created"])
        log.msg("added builder %s in category %s" % (name, category))
        # an unpickled object might not have category set from before,
        # so set it here to make sure
        builder_status.category = category
        builder_status.basedir = os.path.join(self.basedir, basedir)
        builder_status.name = name # it might have been updated
        builder_status.status = self

        if not os.path.isdir(builder_status.basedir):
            os.mkdir(builder_status.basedir)
        builder_status.determineNextBuildNumber()

        builder_status.setBigState("offline")

        for t in self.watchers:
            self.announceNewBuilder(t, name, builder_status)

        return builder_status

    def builderRemoved(self, name):
        for t in self.watchers:
            t.builderRemoved(name)

    def prune(self):
        for b in self.botmaster.builders.values():
            b.builder_status.prune()

    def buildsetSubmitted(self, bss):
        self.activeBuildSets.append(bss)
        bss.waitUntilFinished().addCallback(self.activeBuildSets.remove)
        for t in self.watchers:
            t.buildsetSubmitted(bss)
