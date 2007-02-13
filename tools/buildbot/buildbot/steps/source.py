# -*- test-case-name: buildbot.test.test_vc -*-

import warnings
from email.Utils import formatdate
from twisted.python import log
from buildbot.process.buildstep import LoggingBuildStep, LoggedRemoteCommand
from buildbot.interfaces import BuildSlaveTooOldError
from buildbot.status.builder import SKIPPED


class Source(LoggingBuildStep):
    """This is a base class to generate a source tree in the buildslave.
    Each version control system has a specialized subclass, and is expected
    to override __init__ and implement computeSourceRevision() and
    startVC(). The class as a whole builds up the self.args dictionary, then
    starts a LoggedRemoteCommand with those arguments.
    """

    # if the checkout fails, there's no point in doing anything else
    haltOnFailure = True
    notReally = False

    branch = None # the default branch, should be set in __init__

    def __init__(self, workdir, mode='update', alwaysUseLatest=False,
                 timeout=20*60, retry=None, **kwargs):
        """
        @type  workdir: string
        @param workdir: local directory (relative to the Builder's root)
                        where the tree should be placed

        @type  mode: string
        @param mode: the kind of VC operation that is desired:
           - 'update': specifies that the checkout/update should be
             performed directly into the workdir. Each build is performed
             in the same directory, allowing for incremental builds. This
             minimizes disk space, bandwidth, and CPU time. However, it
             may encounter problems if the build process does not handle
             dependencies properly (if you must sometimes do a 'clean
             build' to make sure everything gets compiled), or if source
             files are deleted but generated files can influence test
             behavior (e.g. python's .pyc files), or when source
             directories are deleted but generated files prevent CVS from
             removing them.

           - 'copy': specifies that the source-controlled workspace
             should be maintained in a separate directory (called the
             'copydir'), using checkout or update as necessary. For each
             build, a new workdir is created with a copy of the source
             tree (rm -rf workdir; cp -r copydir workdir). This doubles
             the disk space required, but keeps the bandwidth low
             (update instead of a full checkout). A full 'clean' build
             is performed each time.  This avoids any generated-file
             build problems, but is still occasionally vulnerable to
             problems such as a CVS repository being manually rearranged
             (causing CVS errors on update) which are not an issue with
             a full checkout.

           - 'clobber': specifies that the working directory should be
             deleted each time, necessitating a full checkout for each
             build. This insures a clean build off a complete checkout,
             avoiding any of the problems described above, but is
             bandwidth intensive, as the whole source tree must be
             pulled down for each build.

           - 'export': is like 'clobber', except that e.g. the 'cvs
             export' command is used to create the working directory.
             This command removes all VC metadata files (the
             CVS/.svn/{arch} directories) from the tree, which is
             sometimes useful for creating source tarballs (to avoid
             including the metadata in the tar file). Not all VC systems
             support export.

        @type  alwaysUseLatest: boolean
        @param alwaysUseLatest: whether to always update to the most
        recent available sources for this build.

        Normally the Source step asks its Build for a list of all
        Changes that are supposed to go into the build, then computes a
        'source stamp' (revision number or timestamp) that will cause
        exactly that set of changes to be present in the checked out
        tree. This is turned into, e.g., 'cvs update -D timestamp', or
        'svn update -r revnum'. If alwaysUseLatest=True, bypass this
        computation and always update to the latest available sources
        for each build.

        The source stamp helps avoid a race condition in which someone
        commits a change after the master has decided to start a build
        but before the slave finishes checking out the sources. At best
        this results in a build which contains more changes than the
        buildmaster thinks it has (possibly resulting in the wrong
        person taking the blame for any problems that result), at worst
        is can result in an incoherent set of sources (splitting a
        non-atomic commit) which may not build at all.

        @type  retry: tuple of ints (delay, repeats) (or None)
        @param retry: if provided, VC update failures are re-attempted up
                      to REPEATS times, with DELAY seconds between each
                      attempt. Some users have slaves with poor connectivity
                      to their VC repository, and they say that up to 80% of
                      their build failures are due to transient network
                      failures that could be handled by simply retrying a
                      couple times.

        """

        LoggingBuildStep.__init__(self, **kwargs)

        assert mode in ("update", "copy", "clobber", "export")
        if retry:
            delay, repeats = retry
            assert isinstance(repeats, int)
            assert repeats > 0
        self.args = {'mode': mode,
                     'workdir': workdir,
                     'timeout': timeout,
                     'retry': retry,
                     'patch': None, # set during .start
                     }
        self.alwaysUseLatest = alwaysUseLatest

        # Compute defaults for descriptions:
        description = ["updating"]
        descriptionDone = ["update"]
        if mode == "clobber":
            description = ["checkout"]
            # because checkingouting takes too much space
            descriptionDone = ["checkout"]
        elif mode == "export":
            description = ["exporting"]
            descriptionDone = ["export"]
        self.description = description
        self.descriptionDone = descriptionDone

    def describe(self, done=False):
        if done:
            return self.descriptionDone
        return self.description

    def computeSourceRevision(self, changes):
        """Each subclass must implement this method to do something more
        precise than -rHEAD every time. For version control systems that use
        repository-wide change numbers (SVN, P4), this can simply take the
        maximum such number from all the changes involved in this build. For
        systems that do not (CVS), it needs to create a timestamp based upon
        the latest Change, the Build's treeStableTimer, and an optional
        self.checkoutDelay value."""
        return None

    def start(self):
        if self.notReally:
            log.msg("faking %s checkout/update" % self.name)
            self.step_status.setColor("green")
            self.step_status.setText(["fake", self.name, "successful"])
            self.addCompleteLog("log",
                                "Faked %s checkout/update 'successful'\n" \
                                % self.name)
            return SKIPPED

        # what source stamp would this build like to use?
        s = self.build.getSourceStamp()
        # if branch is None, then use the Step's "default" branch
        branch = s.branch or self.branch
        # if revision is None, use the latest sources (-rHEAD)
        revision = s.revision
        if not revision and not self.alwaysUseLatest:
            revision = self.computeSourceRevision(s.changes)
        # if patch is None, then do not patch the tree after checkout

        # 'patch' is None or a tuple of (patchlevel, diff)
        patch = s.patch

        self.startVC(branch, revision, patch)

    def commandComplete(self, cmd):
        got_revision = None
        if cmd.updates.has_key("got_revision"):
            got_revision = cmd.updates["got_revision"][-1]
        self.setProperty("got_revision", got_revision)



class CVS(Source):
    """I do CVS checkout/update operations.

    Note: if you are doing anonymous/pserver CVS operations, you will need
    to manually do a 'cvs login' on each buildslave before the slave has any
    hope of success. XXX: fix then, take a cvs password as an argument and
    figure out how to do a 'cvs login' on each build
    """

    name = "cvs"

    #progressMetrics = ('output',)
    #
    # additional things to track: update gives one stderr line per directory
    # (starting with 'cvs server: Updating ') (and is fairly stable if files
    # is empty), export gives one line per directory (starting with 'cvs
    # export: Updating ') and another line per file (starting with U). Would
    # be nice to track these, requires grepping LogFile data for lines,
    # parsing each line. Might be handy to have a hook in LogFile that gets
    # called with each complete line.

    def __init__(self, cvsroot, cvsmodule, 
                 global_options=[], branch=None, checkoutDelay=None,
                 login=None,
                 clobber=0, export=0, copydir=None,
                 **kwargs):

        """
        @type  cvsroot: string
        @param cvsroot: CVS Repository from which the source tree should
                        be obtained. '/home/warner/Repository' for local
                        or NFS-reachable repositories,
                        ':pserver:anon@foo.com:/cvs' for anonymous CVS,
                        'user@host.com:/cvs' for non-anonymous CVS or
                        CVS over ssh. Lots of possibilities, check the
                        CVS documentation for more.

        @type  cvsmodule: string
        @param cvsmodule: subdirectory of CVS repository that should be
                          retrieved

        @type  login: string or None
        @param login: if not None, a string which will be provided as a
                      password to the 'cvs login' command, used when a
                      :pserver: method is used to access the repository.
                      This login is only needed once, but must be run
                      each time (just before the CVS operation) because
                      there is no way for the buildslave to tell whether
                      it was previously performed or not.

        @type  branch: string
        @param branch: the default branch name, will be used in a '-r'
                       argument to specify which branch of the source tree
                       should be used for this checkout. Defaults to None,
                       which means to use 'HEAD'.

        @type  checkoutDelay: int or None
        @param checkoutDelay: if not None, the number of seconds to put
                              between the last known Change and the
                              timestamp given to the -D argument. This
                              defaults to exactly half of the parent
                              Build's .treeStableTimer, but it could be
                              set to something else if your CVS change
                              notification has particularly weird
                              latency characteristics.

        @type  global_options: list of strings
        @param global_options: these arguments are inserted in the cvs
                               command line, before the
                               'checkout'/'update' command word. See
                               'cvs --help-options' for a list of what
                               may be accepted here.  ['-r'] will make
                               the checked out files read only. ['-r',
                               '-R'] will also assume the repository is
                               read-only (I assume this means it won't
                               use locks to insure atomic access to the
                               ,v files)."""
                               
        self.checkoutDelay = checkoutDelay
        self.branch = branch

        if not kwargs.has_key('mode') and (clobber or export or copydir):
            # deal with old configs
            warnings.warn("Please use mode=, not clobber/export/copydir",
                          DeprecationWarning)
            if export:
                kwargs['mode'] = "export"
            elif clobber:
                kwargs['mode'] = "clobber"
            elif copydir:
                kwargs['mode'] = "copy"
            else:
                kwargs['mode'] = "update"

        Source.__init__(self, **kwargs)

        self.args.update({'cvsroot': cvsroot,
                          'cvsmodule': cvsmodule,
                          'global_options': global_options,
                          'login': login,
                          })

    def computeSourceRevision(self, changes):
        if not changes:
            return None
        lastChange = max([c.when for c in changes])
        if self.checkoutDelay is not None:
            when = lastChange + self.checkoutDelay
        else:
            lastSubmit = max([r.submittedAt for r in self.build.requests])
            when = (lastChange + lastSubmit) / 2
        return formatdate(when)

    def startVC(self, branch, revision, patch):
        if self.slaveVersionIsOlderThan("cvs", "1.39"):
            # the slave doesn't know to avoid re-using the same sourcedir
            # when the branch changes. We have no way of knowing which branch
            # the last build used, so if we're using a non-default branch and
            # either 'update' or 'copy' modes, it is safer to refuse to
            # build, and tell the user they need to upgrade the buildslave.
            if (branch != self.branch
                and self.args['mode'] in ("update", "copy")):
                m = ("This buildslave (%s) does not know about multiple "
                     "branches, and using mode=%s would probably build the "
                     "wrong tree. "
                     "Refusing to build. Please upgrade the buildslave to "
                     "buildbot-0.7.0 or newer." % (self.build.slavename,
                                                   self.args['mode']))
                log.msg(m)
                raise BuildSlaveTooOldError(m)

        if branch is None:
            branch = "HEAD"
        self.args['branch'] = branch
        self.args['revision'] = revision
        self.args['patch'] = patch

        if self.args['branch'] == "HEAD" and self.args['revision']:
            # special case. 'cvs update -r HEAD -D today' gives no files
            # TODO: figure out why, see if it applies to -r BRANCH
            self.args['branch'] = None

        # deal with old slaves
        warnings = []
        slavever = self.slaveVersion("cvs", "old")

        if slavever == "old":
            # 0.5.0
            if self.args['mode'] == "export":
                self.args['export'] = 1
            elif self.args['mode'] == "clobber":
                self.args['clobber'] = 1
            elif self.args['mode'] == "copy":
                self.args['copydir'] = "source"
            self.args['tag'] = self.args['branch']
            assert not self.args['patch'] # 0.5.0 slave can't do patch

        cmd = LoggedRemoteCommand("cvs", self.args)
        self.startCommand(cmd, warnings)


class SVN(Source):
    """I perform Subversion checkout/update operations."""

    name = 'svn'

    def __init__(self, svnurl=None, baseURL=None, defaultBranch=None,
                 directory=None, **kwargs):
        """
        @type  svnurl: string
        @param svnurl: the URL which points to the Subversion server,
                       combining the access method (HTTP, ssh, local file),
                       the repository host/port, the repository path, the
                       sub-tree within the repository, and the branch to
                       check out. Using C{svnurl} does not enable builds of
                       alternate branches: use C{baseURL} to enable this.
                       Use exactly one of C{svnurl} and C{baseURL}.

        @param baseURL: if branches are enabled, this is the base URL to
                        which a branch name will be appended. It should
                        probably end in a slash. Use exactly one of
                        C{svnurl} and C{baseURL}.
                         
        @param defaultBranch: if branches are enabled, this is the branch
                              to use if the Build does not specify one
                              explicitly. It will simply be appended
                              to C{baseURL} and the result handed to
                              the SVN command.
        """

        if not kwargs.has_key('workdir') and directory is not None:
            # deal with old configs
            warnings.warn("Please use workdir=, not directory=",
                          DeprecationWarning)
            kwargs['workdir'] = directory

        self.svnurl = svnurl
        self.baseURL = baseURL
        self.branch = defaultBranch

        Source.__init__(self, **kwargs)

        if not svnurl and not baseURL:
            raise ValueError("you must use exactly one of svnurl and baseURL")


    def computeSourceRevision(self, changes):
        if not changes:
            return None
        lastChange = max([int(c.revision) for c in changes])
        return lastChange

    def startVC(self, branch, revision, patch):

        # handle old slaves
        warnings = []
        slavever = self.slaveVersion("svn", "old")
        if not slavever:
            m = "slave does not have the 'svn' command"
            raise BuildSlaveTooOldError(m)

        if self.slaveVersionIsOlderThan("svn", "1.39"):
            # the slave doesn't know to avoid re-using the same sourcedir
            # when the branch changes. We have no way of knowing which branch
            # the last build used, so if we're using a non-default branch and
            # either 'update' or 'copy' modes, it is safer to refuse to
            # build, and tell the user they need to upgrade the buildslave.
            if (branch != self.branch
                and self.args['mode'] in ("update", "copy")):
                m = ("This buildslave (%s) does not know about multiple "
                     "branches, and using mode=%s would probably build the "
                     "wrong tree. "
                     "Refusing to build. Please upgrade the buildslave to "
                     "buildbot-0.7.0 or newer." % (self.build.slavename,
                                                   self.args['mode']))
                raise BuildSlaveTooOldError(m)

        if slavever == "old":
            # 0.5.0 compatibility
            if self.args['mode'] in ("clobber", "copy"):
                # TODO: use some shell commands to make up for the
                # deficiency, by blowing away the old directory first (thus
                # forcing a full checkout)
                warnings.append("WARNING: this slave can only do SVN updates"
                                ", not mode=%s\n" % self.args['mode'])
                log.msg("WARNING: this slave only does mode=update")
            if self.args['mode'] == "export":
                raise BuildSlaveTooOldError("old slave does not have "
                                            "mode=export")
            self.args['directory'] = self.args['workdir']
            if revision is not None:
                # 0.5.0 can only do HEAD. We have no way of knowing whether
                # the requested revision is HEAD or not, and for
                # slowly-changing trees this will probably do the right
                # thing, so let it pass with a warning
                m = ("WARNING: old slave can only update to HEAD, not "
                     "revision=%s" % revision)
                log.msg(m)
                warnings.append(m + "\n")
            revision = "HEAD" # interprets this key differently
            if patch:
                raise BuildSlaveTooOldError("old slave can't do patch")

        if self.svnurl:
            assert not branch # we need baseURL= to use branches
            self.args['svnurl'] = self.svnurl
        else:
            self.args['svnurl'] = self.baseURL + branch
        self.args['revision'] = revision
        self.args['patch'] = patch

        revstuff = []
        if branch is not None and branch != self.branch:
            revstuff.append("[branch]")
        if revision is not None:
            revstuff.append("r%s" % revision)
        self.description.extend(revstuff)
        self.descriptionDone.extend(revstuff)

        cmd = LoggedRemoteCommand("svn", self.args)
        self.startCommand(cmd, warnings)


class Darcs(Source):
    """Check out a source tree from a Darcs repository at 'repourl'.

    To the best of my knowledge, Darcs has no concept of file modes. This
    means the eXecute-bit will be cleared on all source files. As a result,
    you may need to invoke configuration scripts with something like:

    C{s(step.Configure, command=['/bin/sh', './configure'])}
    """

    name = "darcs"

    def __init__(self, repourl=None, baseURL=None, defaultBranch=None,
                 **kwargs):
        """
        @type  repourl: string
        @param repourl: the URL which points at the Darcs repository. This
                        is used as the default branch. Using C{repourl} does
                        not enable builds of alternate branches: use
                        C{baseURL} to enable this. Use either C{repourl} or
                        C{baseURL}, not both.

        @param baseURL: if branches are enabled, this is the base URL to
                        which a branch name will be appended. It should
                        probably end in a slash. Use exactly one of
                        C{repourl} and C{baseURL}.
                         
        @param defaultBranch: if branches are enabled, this is the branch
                              to use if the Build does not specify one
                              explicitly. It will simply be appended to
                              C{baseURL} and the result handed to the
                              'darcs pull' command.
        """
        self.repourl = repourl
        self.baseURL = baseURL
        self.branch = defaultBranch
        Source.__init__(self, **kwargs)
        assert kwargs['mode'] != "export", \
               "Darcs does not have an 'export' mode"
        if (not repourl and not baseURL) or (repourl and baseURL):
            raise ValueError("you must provide exactly one of repourl and"
                             " baseURL")

    def startVC(self, branch, revision, patch):
        slavever = self.slaveVersion("darcs")
        if not slavever:
            m = "slave is too old, does not know about darcs"
            raise BuildSlaveTooOldError(m)

        if self.slaveVersionIsOlderThan("darcs", "1.39"):
            if revision:
                # TODO: revisit this once we implement computeSourceRevision
                m = "0.6.6 slaves can't handle args['revision']"
                raise BuildSlaveTooOldError(m)

            # the slave doesn't know to avoid re-using the same sourcedir
            # when the branch changes. We have no way of knowing which branch
            # the last build used, so if we're using a non-default branch and
            # either 'update' or 'copy' modes, it is safer to refuse to
            # build, and tell the user they need to upgrade the buildslave.
            if (branch != self.branch
                and self.args['mode'] in ("update", "copy")):
                m = ("This buildslave (%s) does not know about multiple "
                     "branches, and using mode=%s would probably build the "
                     "wrong tree. "
                     "Refusing to build. Please upgrade the buildslave to "
                     "buildbot-0.7.0 or newer." % (self.build.slavename,
                                                   self.args['mode']))
                raise BuildSlaveTooOldError(m)

        if self.repourl:
            assert not branch # we need baseURL= to use branches
            self.args['repourl'] = self.repourl
        else:
            self.args['repourl'] = self.baseURL + branch
        self.args['revision'] = revision
        self.args['patch'] = patch

        revstuff = []
        if branch is not None and branch != self.branch:
            revstuff.append("[branch]")
        self.description.extend(revstuff)
        self.descriptionDone.extend(revstuff)

        cmd = LoggedRemoteCommand("darcs", self.args)
        self.startCommand(cmd)


class Git(Source):
    """Check out a source tree from a git repository 'repourl'."""

    name = "git"

    def __init__(self, repourl, **kwargs):
        """
        @type  repourl: string
        @param repourl: the URL which points at the git repository
        """
        self.branch = None # TODO
        Source.__init__(self, **kwargs)
        self.args['repourl'] = repourl

    def startVC(self, branch, revision, patch):
        self.args['branch'] = branch
        self.args['revision'] = revision
        self.args['patch'] = patch
        slavever = self.slaveVersion("git")
        if not slavever:
            raise BuildSlaveTooOldError("slave is too old, does not know "
                                        "about git")
        cmd = LoggedRemoteCommand("git", self.args)
        self.startCommand(cmd)


class Arch(Source):
    """Check out a source tree from an Arch repository named 'archive'
    available at 'url'. 'version' specifies which version number (development
    line) will be used for the checkout: this is mostly equivalent to a
    branch name. This version uses the 'tla' tool to do the checkout, to use
    'baz' see L{Bazaar} instead.
    """

    name = "arch"
    # TODO: slaves >0.6.6 will accept args['build-config'], so use it

    def __init__(self, url, version, archive=None, **kwargs):
        """
        @type  url: string
        @param url: the Arch coordinates of the repository. This is
                    typically an http:// URL, but could also be the absolute
                    pathname of a local directory instead.

        @type  version: string
        @param version: the category--branch--version to check out. This is
                        the default branch. If a build specifies a different
                        branch, it will be used instead of this.

        @type  archive: string
        @param archive: The archive name. If provided, it must match the one
                        that comes from the repository. If not, the
                        repository's default will be used.
        """
        self.branch = version
        Source.__init__(self, **kwargs)
        self.args.update({'url': url,
                          'archive': archive,
                          })

    def computeSourceRevision(self, changes):
        # in Arch, fully-qualified revision numbers look like:
        #  arch@buildbot.sourceforge.net--2004/buildbot--dev--0--patch-104
        # For any given builder, all of this is fixed except the patch-104.
        # The Change might have any part of the fully-qualified string, so we
        # just look for the last part. We return the "patch-NN" string.
        if not changes:
            return None
        lastChange = None
        for c in changes:
            if not c.revision:
                continue
            if c.revision.endswith("--base-0"):
                rev = 0
            else:
                i = c.revision.rindex("patch")
                rev = int(c.revision[i+len("patch-"):])
            lastChange = max(lastChange, rev)
        if lastChange is None:
            return None
        if lastChange == 0:
            return "base-0"
        return "patch-%d" % lastChange

    def checkSlaveVersion(self, cmd, branch):
        warnings = []
        slavever = self.slaveVersion(cmd)
        if not slavever:
            m = "slave is too old, does not know about %s" % cmd
            raise BuildSlaveTooOldError(m)

        # slave 1.28 and later understand 'revision'
        if self.slaveVersionIsOlderThan(cmd, "1.28"):
            if not self.alwaysUseLatest:
                # we don't know whether our requested revision is the latest
                # or not. If the tree does not change very quickly, this will
                # probably build the right thing, so emit a warning rather
                # than refuse to build at all
                m = "WARNING, buildslave is too old to use a revision"
                log.msg(m)
                warnings.append(m + "\n")

        if self.slaveVersionIsOlderThan(cmd, "1.39"):
            # the slave doesn't know to avoid re-using the same sourcedir
            # when the branch changes. We have no way of knowing which branch
            # the last build used, so if we're using a non-default branch and
            # either 'update' or 'copy' modes, it is safer to refuse to
            # build, and tell the user they need to upgrade the buildslave.
            if (branch != self.branch
                and self.args['mode'] in ("update", "copy")):
                m = ("This buildslave (%s) does not know about multiple "
                     "branches, and using mode=%s would probably build the "
                     "wrong tree. "
                     "Refusing to build. Please upgrade the buildslave to "
                     "buildbot-0.7.0 or newer." % (self.build.slavename,
                                                   self.args['mode']))
                log.msg(m)
                raise BuildSlaveTooOldError(m)

        return warnings

    def startVC(self, branch, revision, patch):
        self.args['version'] = branch
        self.args['revision'] = revision
        self.args['patch'] = patch
        warnings = self.checkSlaveVersion("arch", branch)

        revstuff = []
        if branch is not None and branch != self.branch:
            revstuff.append("[branch]")
        if revision is not None:
            revstuff.append("patch%s" % revision)
        self.description.extend(revstuff)
        self.descriptionDone.extend(revstuff)

        cmd = LoggedRemoteCommand("arch", self.args)
        self.startCommand(cmd, warnings)


class Bazaar(Arch):
    """Bazaar is an alternative client for Arch repositories. baz is mostly
    compatible with tla, but archive registration is slightly different."""

    # TODO: slaves >0.6.6 will accept args['build-config'], so use it

    def __init__(self, url, version, archive, **kwargs):
        """
        @type  url: string
        @param url: the Arch coordinates of the repository. This is
                    typically an http:// URL, but could also be the absolute
                    pathname of a local directory instead.

        @type  version: string
        @param version: the category--branch--version to check out

        @type  archive: string
        @param archive: The archive name (required). This must always match
                        the one that comes from the repository, otherwise the
                        buildslave will attempt to get sources from the wrong
                        archive.
        """
        self.branch = version
        Source.__init__(self, **kwargs)
        self.args.update({'url': url,
                          'archive': archive,
                          })

    def startVC(self, branch, revision, patch):
        self.args['version'] = branch
        self.args['revision'] = revision
        self.args['patch'] = patch
        warnings = self.checkSlaveVersion("bazaar", branch)

        revstuff = []
        if branch is not None and branch != self.branch:
            revstuff.append("[branch]")
        if revision is not None:
            revstuff.append("patch%s" % revision)
        self.description.extend(revstuff)
        self.descriptionDone.extend(revstuff)

        cmd = LoggedRemoteCommand("bazaar", self.args)
        self.startCommand(cmd, warnings)

class Mercurial(Source):
    """Check out a source tree from a mercurial repository 'repourl'."""

    name = "hg"

    def __init__(self, repourl=None, baseURL=None, defaultBranch=None,
                 **kwargs):
        """
        @type  repourl: string
        @param repourl: the URL which points at the Mercurial repository.
                        This is used as the default branch. Using C{repourl}
                        does not enable builds of alternate branches: use
                        C{baseURL} to enable this. Use either C{repourl} or
                        C{baseURL}, not both.

        @param baseURL: if branches are enabled, this is the base URL to
                        which a branch name will be appended. It should
                        probably end in a slash. Use exactly one of
                        C{repourl} and C{baseURL}.

        @param defaultBranch: if branches are enabled, this is the branch
                              to use if the Build does not specify one
                              explicitly. It will simply be appended to
                              C{baseURL} and the result handed to the
                              'hg clone' command.
        """
        self.repourl = repourl
        self.baseURL = baseURL
        self.branch = defaultBranch
        Source.__init__(self, **kwargs)
        if (not repourl and not baseURL) or (repourl and baseURL):
            raise ValueError("you must provide exactly one of repourl and"
                             " baseURL")

    def startVC(self, branch, revision, patch):
        slavever = self.slaveVersion("hg")
        if not slavever:
            raise BuildSlaveTooOldError("slave is too old, does not know "
                                        "about hg")

        if self.repourl:
            assert not branch # we need baseURL= to use branches
            self.args['repourl'] = self.repourl
        else:
            self.args['repourl'] = self.baseURL + branch
        self.args['revision'] = revision
        self.args['patch'] = patch

        revstuff = []
        if branch is not None and branch != self.branch:
            revstuff.append("[branch]")
        self.description.extend(revstuff)
        self.descriptionDone.extend(revstuff)

        cmd = LoggedRemoteCommand("hg", self.args)
        self.startCommand(cmd)


class P4(Source):
    """ P4 is a class for accessing perforce revision control"""
    name = "p4"

    def __init__(self, p4base, defaultBranch=None, p4port=None, p4user=None,
                 p4passwd=None, p4extra_views=[],
                 p4client='buildbot_%(slave)s_%(builder)s', **kwargs):
        """
        @type  p4base: string
        @param p4base: A view into a perforce depot, typically
                       "//depot/proj/"

        @type  defaultBranch: string
        @param defaultBranch: Identify a branch to build by default. Perforce
                              is a view based branching system. So, the branch
                              is normally the name after the base. For example,
                              branch=1.0 is view=//depot/proj/1.0/...
                              branch=1.1 is view=//depot/proj/1.1/...

        @type  p4port: string
        @param p4port: Specify the perforce server to connection in the format
                       <host>:<port>. Example "perforce.example.com:1666"

        @type  p4user: string
        @param p4user: The perforce user to run the command as.

        @type  p4passwd: string
        @param p4passwd: The password for the perforce user.

        @type  p4extra_views: list of tuples
        @param p4extra_views: Extra views to be added to
                              the client that is being used.

        @type  p4client: string
        @param p4client: The perforce client to use for this buildslave.
        """

        self.branch = defaultBranch
        Source.__init__(self, **kwargs)
        self.args['p4port'] = p4port
        self.args['p4user'] = p4user
        self.args['p4passwd'] = p4passwd
        self.args['p4base'] = p4base
        self.args['p4extra_views'] = p4extra_views
        self.args['p4client'] = p4client % {
            'slave': self.build.slavename,
            'builder': self.build.builder.name,
        }

    def computeSourceRevision(self, changes):
        if not changes:
            return None
        lastChange = max([int(c.revision) for c in changes])
        return lastChange

    def startVC(self, branch, revision, patch):
        slavever = self.slaveVersion("p4")
        assert slavever, "slave is too old, does not know about p4"
        args = dict(self.args)
        args['branch'] = branch or self.branch
        args['revision'] = revision
        args['patch'] = patch
        cmd = LoggedRemoteCommand("p4", args)
        self.startCommand(cmd)

class P4Sync(Source):
    """This is a partial solution for using a P4 source repository. You are
    required to manually set up each build slave with a useful P4
    environment, which means setting various per-slave environment variables,
    and creating a P4 client specification which maps the right files into
    the slave's working directory. Once you have done that, this step merely
    performs a 'p4 sync' to update that workspace with the newest files.

    Each slave needs the following environment:

     - PATH: the 'p4' binary must be on the slave's PATH
     - P4USER: each slave needs a distinct user account
     - P4CLIENT: each slave needs a distinct client specification

    You should use 'p4 client' (?) to set up a client view spec which maps
    the desired files into $SLAVEBASE/$BUILDERBASE/source .
    """

    name = "p4sync"

    def __init__(self, p4port, p4user, p4passwd, p4client, **kwargs):
        assert kwargs['mode'] == "copy", "P4Sync can only be used in mode=copy"
        self.branch = None
        Source.__init__(self, **kwargs)
        self.args['p4port'] = p4port
        self.args['p4user'] = p4user
        self.args['p4passwd'] = p4passwd
        self.args['p4client'] = p4client

    def computeSourceRevision(self, changes):
        if not changes:
            return None
        lastChange = max([int(c.revision) for c in changes])
        return lastChange

    def startVC(self, branch, revision, patch):
        slavever = self.slaveVersion("p4sync")
        assert slavever, "slave is too old, does not know about p4"
        cmd = LoggedRemoteCommand("p4sync", self.args)
        self.startCommand(cmd)

class Monotone(Source):
    """Check out a revision from a monotone server at 'server_addr',
    branch 'branch'.  'revision' specifies which revision id to check
    out.

    This step will first create a local database, if necessary, and then pull
    the contents of the server into the database.  Then it will do the
    checkout/update from this database."""

    name = "monotone"

    def __init__(self, server_addr, branch, db_path="monotone.db",
                 monotone="monotone",
                 **kwargs):
        Source.__init__(self, **kwargs)
        self.args.update({"server_addr": server_addr,
                          "branch": branch,
                          "db_path": db_path,
                          "monotone": monotone})

    def computeSourceRevision(self, changes):
        if not changes:
            return None
        return changes[-1].revision

    def startVC(self):
        slavever = self.slaveVersion("monotone")
        assert slavever, "slave is too old, does not know about monotone"
        cmd = LoggedRemoteCommand("monotone", self.args)
        self.startCommand(cmd)

