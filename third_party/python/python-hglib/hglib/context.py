import hglib.client  # Circular dependency.
from hglib import util, templates
from hglib.error import CommandError
from hglib.util import b, strtobytes, integertypes

_nullcset = [b('-1'), b('0000000000000000000000000000000000000000'), b(''),
             b(''), b(''), b(''), b('')]

class changectx(object):
    """A changecontext object makes access to data related to a particular
    changeset convenient."""
    def __init__(self, repo, changeid=b('')):
        """changeid is a revision number, node, or tag"""
        if changeid == b(''):
            changeid = b('.')
        self._repo = repo
        if isinstance(changeid, hglib.client.revision):
            cset = changeid
        elif changeid == -1:
            cset = _nullcset
        else:
            if isinstance(changeid, integertypes):
                changeid = b('rev(') + strtobytes(changeid) + b(')')

            notfound = False
            try:
                cset = self._repo.log(changeid)
                # hg bbf4f3dfd700 gave a null result for tip+1
                if (cset and cset[0][1] == _nullcset[1]
                    and cset[0][0] != _nullcset[0]):
                    notfound = True
            except CommandError:
                notfound = True

            if notfound or not len(cset):
                raise ValueError('changeid %r not found in repo' % changeid)
            if len(cset) > 1:
                raise ValueError('changeid must yield a single changeset')
            cset = cset[0]

        self._rev, self._node, self._tags = cset[:3]
        self._branch, self._author, self._description, self._date = cset[3:]

        self._rev = int(self._rev)

        self._tags = self._tags.split()
        try:
            self._tags.remove(b('tip'))
        except ValueError:
            pass

        self._ignored = None
        self._clean = None

    def __str__(self):
        return self._node[:12].decode('latin-1')

    def __int__(self):
        return self._rev

    def __repr__(self):
        return "<changectx %s>" % str(self)

    def __hash__(self):
        try:
            return hash(self._rev)
        except AttributeError:
            return id(self)

    def __eq__(self, other):
        try:
            return self._rev == other._rev
        except AttributeError:
            return False

    def __ne__(self, other):
        return not (self == other)

    def __nonzero__(self):
        return self._rev != -1

    def __bool__(self):
        return self.__nonzero__()

    def __contains__(self, key):
        return key in self._manifest

    def __iter__(self):
        for f in sorted(self._manifest):
            yield f

    @util.propertycache
    def _status(self):
        return self._parsestatus(self._repo.status(change=strtobytes(self)))[:4]

    def _parsestatus(self, stat):
        d = dict((c, [])
                 for c in (b('M'), b('A'), b('R'), b('!'), b('?'), b('I'),
                    b('C'), b(' ')))
        for k, path in stat:
            d[k].append(path)
        return (d[b('M')], d[b('A')], d[b('R')], d[b('!')], d[b('?')],
                d[b('I')], d[b('C')])

    def status(self, ignored=False, clean=False):
        """Explicit status query
        Unless this method is used to query the working copy status, the
        _status property will implicitly read the status using its default
        arguments."""
        stat = self._parsestatus(self._repo.status(change=strtobytes(self),
                                                   ignored=ignored,
                                                   clean=clean))
        self._unknown = self._ignored = self._clean = None
        if ignored:
            self._ignored = stat[5]
        if clean:
            self._clean = stat[6]
        self._status = stat[:4]
        return stat

    def rev(self):
        return self._rev

    def node(self):
        return self._node

    def tags(self):
        return self._tags

    def branch(self):
        return self._branch

    def author(self):
        return self._author

    def user(self):
        return self._author

    def date(self):
        return self._date

    def description(self):
        return self._description

    def files(self):
        return sorted(self._status[0] + self._status[1] + self._status[2])

    def modified(self):
        return self._status[0]

    def added(self):
        return self._status[1]

    def removed(self):
        return self._status[2]

    def ignored(self):
        if self._ignored is None:
            self.status(ignored=True)
        return self._ignored

    def clean(self):
        if self._clean is None:
            self.status(clean=True)
        return self._clean

    @util.propertycache
    def _manifest(self):
        d = {}
        for node, p, e, s, path in self._repo.manifest(rev=strtobytes(self)):
            d[path] = node
        return d

    def manifest(self):
        return self._manifest

    def hex(self):
        return hex(self._node)

    @util.propertycache
    def _parents(self):
        """return contexts for each parent changeset"""
        par = self._repo.parents(rev=strtobytes(self))
        if not par:
            return [changectx(self._repo, -1)]
        return [changectx(self._repo, int(cset.rev)) for cset in par]

    def parents(self):
        return self._parents

    def p1(self):
        return self._parents[0]

    def p2(self):
        if len(self._parents) == 2:
            return self._parents[1]
        return changectx(self._repo, -1)

    @util.propertycache
    def _bookmarks(self):
        books = [bm for bm in self._repo.bookmarks()[0] if bm[1] == self._rev]

        bms = []
        for name, r, n in books:
            bms.append(name)
        return bms

    def bookmarks(self):
        return self._bookmarks

    def hidden(self):
        """return True if the changeset is hidden, else False"""
        return bool(self._repo.log(revrange=self._node + b(' and hidden()'),
                                   hidden=True))

    def phase(self):
        """return the phase of the changeset (public, draft or secret)"""
        return self._repo.phase(strtobytes(self._rev))[0][1]

    def children(self):
        """return contexts for each child changeset"""
        for c in self._repo.log(b('children(') + self._node + b(')')):
            yield changectx(self._repo, c)

    def ancestors(self):
        for a in self._repo.log(b('ancestors(') + self._node + b(')')):
            yield changectx(self._repo, a)

    def descendants(self):
        for d in self._repo.log(b('descendants(') + self._node + b(')')):
            yield changectx(self._repo, d)

    def ancestor(self, c2):
        """
        return the ancestor context of self and c2
        """
        return changectx(self._repo,
                         b('ancestor(') + self + b(', ') + c2 + b(')'))
