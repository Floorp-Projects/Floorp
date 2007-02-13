
from buildbot import util, interfaces
from buildbot.twcompat import implements

class SourceStamp(util.ComparableMixin):
    """This is a tuple of (branch, revision, patchspec, changes).

    C{branch} is always valid, although it may be None to let the Source
    step use its default branch. There are four possibilities for the
    remaining elements:
     - (revision=REV, patchspec=None, changes=None): build REV
     - (revision=REV, patchspec=(LEVEL, DIFF), changes=None): checkout REV,
       then apply a patch to the source, with C{patch -pPATCHLEVEL <DIFF}.
     - (revision=None, patchspec=None, changes=[CHANGES]): let the Source
       step check out the latest revision indicated by the given Changes.
       CHANGES is a list of L{buildbot.changes.changes.Change} instances,
       and all must be on the same branch.
     - (revision=None, patchspec=None, changes=None): build the latest code
       from the given branch.
    """

    # all four of these are publically visible attributes
    branch = None
    revision = None
    patch = None
    changes = []

    compare_attrs = ('branch', 'revision', 'patch', 'changes')

    if implements:
        implements(interfaces.ISourceStamp)
    else:
        __implements__ = interfaces.ISourceStamp,

    def __init__(self, branch=None, revision=None, patch=None,
                 changes=None):
        self.branch = branch
        self.revision = revision
        self.patch = patch
        if changes:
            self.changes = changes
            self.branch = changes[0].branch

    def canBeMergedWith(self, other):
        if other.branch != self.branch:
            return False # the builds are completely unrelated

        if self.changes and other.changes:
            # TODO: consider not merging these. It's a tradeoff between
            # minimizing the number of builds and obtaining finer-grained
            # results.
            return True
        elif self.changes and not other.changes:
            return False # we're using changes, they aren't
        elif not self.changes and other.changes:
            return False # they're using changes, we aren't

        if self.patch or other.patch:
            return False # you can't merge patched builds with anything
        if self.revision == other.revision:
            # both builds are using the same specific revision, so they can
            # be merged. It might be the case that revision==None, so they're
            # both building HEAD.
            return True

        return False

    def mergeWith(self, others):
        """Generate a SourceStamp for the merger of me and all the other
        BuildRequests. This is called by a Build when it starts, to figure
        out what its sourceStamp should be."""

        # either we're all building the same thing (changes==None), or we're
        # all building changes (which can be merged)
        changes = []
        changes.extend(self.changes)
        for req in others:
            assert self.canBeMergedWith(req) # should have been checked already
            changes.extend(req.changes)
        newsource = SourceStamp(branch=self.branch,
                                revision=self.revision,
                                patch=self.patch,
                                changes=changes)
        return newsource

