#! /usr/bin/python

from twisted.application import service
from buildbot.twcompat import implements

from buildbot.interfaces import IStatusReceiver
from buildbot import util, pbutil

class StatusReceiver:
    if implements:
        implements(IStatusReceiver)
    else:
        __implements__ = IStatusReceiver,

    def buildsetSubmitted(self, buildset):
        pass

    def builderAdded(self, builderName, builder):
        pass

    def builderChangedState(self, builderName, state):
        pass

    def buildStarted(self, builderName, build):
        pass

    def buildETAUpdate(self, build, ETA):
        pass

    def stepStarted(self, build, step):
        pass

    def stepETAUpdate(self, build, step, ETA, expectations):
        pass

    def logStarted(self, build, step, log):
        pass

    def logChunk(self, build, step, log, channel, text):
        pass

    def logFinished(self, build, step, log):
        pass

    def stepFinished(self, build, step, results):
        pass

    def buildFinished(self, builderName, build, results):
        pass

    def builderRemoved(self, builderName):
        pass

class StatusReceiverMultiService(StatusReceiver, service.MultiService,
                                 util.ComparableMixin):
    if implements:
        implements(IStatusReceiver)
    else:
        __implements__ = IStatusReceiver, service.MultiService.__implements__

    def __init__(self):
        service.MultiService.__init__(self)


class StatusReceiverPerspective(StatusReceiver, pbutil.NewCredPerspective):
    if implements:
        implements(IStatusReceiver)
    else:
        __implements__ = (IStatusReceiver,
                          pbutil.NewCredPerspective.__implements__)
