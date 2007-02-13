# -*- test-case-name: buildbot.test.test_status -*-

from twisted.internet import reactor
from twisted.spread import pb
from twisted.python import log
from buildbot import util

class StepProgress:
    """I keep track of how much progress a single BuildStep has made.

    Progress is measured along various axes. Time consumed is one that is
    available for all steps. Amount of command output is another, and may be
    better quantified by scanning the output for markers to derive number of
    files compiled, directories walked, tests run, etc.

    I am created when the build begins, and given to a BuildProgress object
    so it can track the overall progress of the whole build.

    """

    startTime = None
    stopTime = None
    expectedTime = None
    buildProgress = None
    debug = False

    def __init__(self, name, metricNames):
        self.name = name
        self.progress = {}
        self.expectations = {}
        for m in metricNames:
            self.progress[m] = None
            self.expectations[m] = None

    def setBuildProgress(self, bp):
        self.buildProgress = bp

    def setExpectations(self, metrics):
        """The step can call this to explicitly set a target value for one
        of its metrics. E.g., ShellCommands knows how many commands it will
        execute, so it could set the 'commands' expectation."""
        for metric, value in metrics.items():
            self.expectations[metric] = value
        self.buildProgress.newExpectations()

    def setExpectedTime(self, seconds):
        self.expectedTime = seconds
        self.buildProgress.newExpectations()

    def start(self):
        if self.debug: print "StepProgress.start[%s]" % self.name
        self.startTime = util.now()

    def setProgress(self, metric, value):
        """The step calls this as progress is made along various axes."""
        if self.debug:
            print "setProgress[%s][%s] = %s" % (self.name, metric, value)
        self.progress[metric] = value
        if self.debug:
            r = self.remaining()
            print " step remaining:", r
        self.buildProgress.newProgress()

    def finish(self):
        """This stops the 'time' metric and marks the step as finished
        overall. It should be called after the last .setProgress has been
        done for each axis."""
        if self.debug: print "StepProgress.finish[%s]" % self.name
        self.stopTime = util.now()
        self.buildProgress.stepFinished(self.name)

    def totalTime(self):
        if self.startTime != None and self.stopTime != None:
            return self.stopTime - self.startTime

    def remaining(self):
        if self.startTime == None:
            return self.expectedTime
        if self.stopTime != None:
            return 0 # already finished
        # TODO: replace this with cleverness that graphs each metric vs.
        # time, then finds the inverse function. Will probably need to save
        # a timestamp with each setProgress update, when finished, go back
        # and find the 2% transition points, then save those 50 values in a
        # list. On the next build, do linear interpolation between the two
        # closest samples to come up with a percentage represented by that
        # metric.

        # TODO: If no other metrics are available, just go with elapsed
        # time. Given the non-time-uniformity of text output from most
        # steps, this would probably be better than the text-percentage
        # scheme currently implemented.

        percentages = []
        for metric, value in self.progress.items():
            expectation = self.expectations[metric]
            if value != None and expectation != None:
                p = 1.0 * value / expectation
                percentages.append(p)
        if percentages:
            avg = reduce(lambda x,y: x+y, percentages) / len(percentages)
            if avg > 1.0:
                # overdue
                avg = 1.0
            if avg < 0.0:
                avg = 0.0
        if percentages and self.expectedTime != None:
            return self.expectedTime - (avg * self.expectedTime)
        if self.expectedTime is not None:
            # fall back to pure time
            return self.expectedTime - (util.now() - self.startTime)
        return None # no idea


class WatcherState:
    def __init__(self, interval):
        self.interval = interval
        self.timer = None
        self.needUpdate = 0

class BuildProgress(pb.Referenceable):
    """I keep track of overall build progress. I hold a list of StepProgress
    objects.
    """

    def __init__(self, stepProgresses):
        self.steps = {}
        for s in stepProgresses:
            self.steps[s.name] = s
            s.setBuildProgress(self)
        self.finishedSteps = []
        self.watchers = {}
        self.debug = 0

    def setExpectationsFrom(self, exp):
        """Set our expectations from the builder's Expectations object."""
        for name, metrics in exp.steps.items():
            s = self.steps[name]
            s.setExpectedTime(exp.times[name])
            s.setExpectations(exp.steps[name])

    def newExpectations(self):
        """Call this when one of the steps has changed its expectations.
        This should trigger us to update our ETA value and notify any
        subscribers."""
        pass # subscribers are not implemented: they just poll

    def stepFinished(self, stepname):
        assert(stepname not in self.finishedSteps)
        self.finishedSteps.append(stepname)
        if len(self.finishedSteps) == len(self.steps.keys()):
            self.sendLastUpdates()
            
    def newProgress(self):
        r = self.remaining()
        if self.debug:
            print " remaining:", r
        if r != None:
            self.sendAllUpdates()
        
    def remaining(self):
        # sum eta of all steps
        sum = 0
        for name, step in self.steps.items():
            rem = step.remaining()
            if rem == None:
                return None # not sure
            sum += rem
        return sum
    def eta(self):
        left = self.remaining()
        if left == None:
            return None # not sure
        done = util.now() + left
        return done


    def remote_subscribe(self, remote, interval=5):
        # [interval, timer, needUpdate]
        # don't send an update more than once per interval
        self.watchers[remote] = WatcherState(interval)
        remote.notifyOnDisconnect(self.removeWatcher)
        self.updateWatcher(remote)
        self.startTimer(remote)
        log.msg("BuildProgress.remote_subscribe(%s)" % remote)
    def remote_unsubscribe(self, remote):
        # TODO: this doesn't work. I think 'remote' will always be different
        # than the object that appeared in _subscribe.
        log.msg("BuildProgress.remote_unsubscribe(%s)" % remote)
        self.removeWatcher(remote)
        #remote.dontNotifyOnDisconnect(self.removeWatcher)
    def removeWatcher(self, remote):
        #log.msg("removeWatcher(%s)" % remote)
        try:
            timer = self.watchers[remote].timer
            if timer:
                timer.cancel()
            del self.watchers[remote]
        except KeyError:
            log.msg("Weird, removeWatcher on non-existent subscriber:",
                    remote)
    def sendAllUpdates(self):
        for r in self.watchers.keys():
            self.updateWatcher(r)
    def updateWatcher(self, remote):
        # an update wants to go to this watcher. Send it if we can, otherwise
        # queue it for later
        w = self.watchers[remote]
        if not w.timer:
            # no timer, so send update now and start the timer
            self.sendUpdate(remote)
            self.startTimer(remote)
        else:
            # timer is running, just mark as needing an update
            w.needUpdate = 1
    def startTimer(self, remote):
        w = self.watchers[remote]
        timer = reactor.callLater(w.interval, self.watcherTimeout, remote)
        w.timer = timer
    def sendUpdate(self, remote, last=0):
        self.watchers[remote].needUpdate = 0
        #text = self.asText() # TODO: not text, duh
        try:
            remote.callRemote("progress", self.remaining())
            if last:
                remote.callRemote("finished", self)
        except:
            log.deferr()
            self.removeWatcher(remote)

    def watcherTimeout(self, remote):
        w = self.watchers.get(remote, None)
        if not w:
            return # went away
        w.timer = None
        if w.needUpdate:
            self.sendUpdate(remote)
            self.startTimer(remote)
    def sendLastUpdates(self):
        for remote in self.watchers.keys():
            self.sendUpdate(remote, 1)
            self.removeWatcher(remote)

        
class Expectations:
    debug = False
    # decay=1.0 ignores all but the last build
    # 0.9 is short time constant. 0.1 is very long time constant
    # TODO: let decay be specified per-metric
    decay = 0.5

    def __init__(self, buildprogress):
        """Create us from a successful build. We will expect each step to
        take as long as it did in that build."""

        # .steps maps stepname to dict2
        # dict2 maps metricname to final end-of-step value
        self.steps = {}

        # .times maps stepname to per-step elapsed time
        self.times = {}

        for name, step in buildprogress.steps.items():
            self.steps[name] = {}
            for metric, value in step.progress.items():
                self.steps[name][metric] = value
            self.times[name] = None
            if step.startTime is not None and step.stopTime is not None:
                self.times[name] = step.stopTime - step.startTime

    def wavg(self, old, current):
        if old is None:
            return current
        if current is None:
            return old
        else:
            return (current * self.decay) + (old * (1 - self.decay))

    def update(self, buildprogress):
        for name, stepprogress in buildprogress.steps.items():
            old = self.times[name]
            current = stepprogress.totalTime()
            if current == None:
                log.msg("Expectations.update: current[%s] was None!" % name)
                continue
            new = self.wavg(old, current)
            self.times[name] = new
            if self.debug:
                print "new expected time[%s] = %s, old %s, cur %s" % \
                      (name, new, old, current)
            
            for metric, current in stepprogress.progress.items():
                old = self.steps[name][metric]
                new = self.wavg(old, current)
                if self.debug:
                    print "new expectation[%s][%s] = %s, old %s, cur %s" % \
                          (name, metric, new, old, current)
                self.steps[name][metric] = new

    def expectedBuildTime(self):
        if None in self.times.values():
            return None
        #return sum(self.times.values())
        # python-2.2 doesn't have 'sum'. TODO: drop python-2.2 support
        s = 0
        for v in self.times.values():
            s += v
        return s
