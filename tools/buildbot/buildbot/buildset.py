
from buildbot.process import base
from buildbot.status import builder


class BuildSet:
    """I represent a set of potential Builds, all of the same source tree,
    across a specified list of Builders. I can represent a build of a
    specific version of the source tree (named by source.branch and
    source.revision), or a build of a certain set of Changes
    (source.changes=list)."""

    def __init__(self, builderNames, source, reason=None, bsid=None):
        """
        @param source: a L{buildbot.sourcestamp.SourceStamp}
        """
        self.builderNames = builderNames
        self.source = source
        self.reason = reason
        self.stillHopeful = True
        self.status = bss = builder.BuildSetStatus(source, reason,
                                                   builderNames, bsid)

    def waitUntilSuccess(self):
        return self.status.waitUntilSuccess()
    def waitUntilFinished(self):
        return self.status.waitUntilFinished()

    def start(self, builders):
        """This is called by the BuildMaster to actually create and submit
        the BuildRequests."""
        self.requests = []
        reqs = []

        # create the requests
        for b in builders:
            req = base.BuildRequest(self.reason, self.source, b.name)
            reqs.append((b, req))
            self.requests.append(req)
            d = req.waitUntilFinished()
            d.addCallback(self.requestFinished, req)

        # tell our status about them
        req_statuses = [req.status for req in self.requests]
        self.status.setBuildRequestStatuses(req_statuses)

        # now submit them
        for b,req in reqs:
            b.submitBuildRequest(req)

    def requestFinished(self, buildstatus, req):
        # TODO: this is where individual build status results are aggregated
        # into a BuildSet-wide status. Consider making a rule that says one
        # WARNINGS results in the overall status being WARNINGS too. The
        # current rule is that any FAILURE means FAILURE, otherwise you get
        # SUCCESS.
        self.requests.remove(req)
        results = buildstatus.getResults()
        if results == builder.FAILURE:
            self.status.setResults(results)
            if self.stillHopeful:
                # oh, cruel reality cuts deep. no joy for you. This is the
                # first failure. This flunks the overall BuildSet, so we can
                # notify success watchers that they aren't going to be happy.
                self.stillHopeful = False
                self.status.giveUpHope()
                self.status.notifySuccessWatchers()
        if not self.requests:
            # that was the last build, so we can notify finished watchers. If
            # we haven't failed by now, we can claim success.
            if self.stillHopeful:
                self.status.setResults(builder.SUCCESS)
                self.status.notifySuccessWatchers()
            self.status.notifyFinishedWatchers()

