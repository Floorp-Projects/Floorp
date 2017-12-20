# This software may be used and distributed according to the terms of the
# GNU General Public License version 2 or any later version.

"""Robustly perform a checkout.

This extension provides the ``hg robustcheckout`` command for
ensuring a working directory is updated to the specified revision
from a source repo using best practices to ensure optimal clone
times and storage efficiency.
"""

from __future__ import absolute_import

import contextlib
import errno
import functools
import os
import random
import re
import socket
import ssl
import time
import urllib2

from mercurial.i18n import _
from mercurial.node import hex, nullid
from mercurial import (
    commands,
    error,
    exchange,
    extensions,
    cmdutil,
    hg,
    match as matchmod,
    registrar,
    scmutil,
    util,
)

# TRACKING hg43
try:
    from mercurial import configitems
except ImportError:
    configitems = None

testedwith = '3.7 3.8 3.9 4.0 4.1 4.2 4.3 4.4'
minimumhgversion = '3.7'

cmdtable = {}

# TRACKING hg43 Mercurial 4.3 introduced registrar.command as a replacement for
# cmdutil.command.
if util.safehasattr(registrar, 'command'):
    command = registrar.command(cmdtable)
else:
    command = cmdutil.command(cmdtable)

# TRACKING hg43 Mercurial 4.3 introduced the config registrar. 4.4 requires
# config items to be registered to avoid a devel warning
if util.safehasattr(registrar, 'configitem'):
    configtable = {}
    configitem = registrar.configitem(configtable)

    configitem('robustcheckout', 'retryjittermin', default=configitems.dynamicdefault)
    configitem('robustcheckout', 'retryjittermax', default=configitems.dynamicdefault)


# Mercurial 4.2 introduced the vfs module and deprecated the symbol in
# scmutil.
def getvfs():
    try:
        from mercurial.vfs import vfs
        return vfs
    except ImportError:
        return scmutil.vfs


def getsparse():
    from mercurial import sparse
    return sparse


if os.name == 'nt':
    import ctypes

    # Get a reference to the DeleteFileW function
    # DeleteFileW accepts filenames encoded as a null terminated sequence of
    # wide chars (UTF-16). Python's ctypes.c_wchar_p correctly encodes unicode
    # strings to null terminated UTF-16 strings.
    # However, we receive (byte) strings from mercurial. When these are passed
    # to DeleteFileW via the c_wchar_p type, they are implicitly decoded via
    # the 'mbcs' encoding on windows.
    kernel32 = ctypes.windll.kernel32
    DeleteFile = kernel32.DeleteFileW
    DeleteFile.argtypes = [ctypes.c_wchar_p]
    DeleteFile.restype = ctypes.c_bool

    def unlinklong(fn):
        normalized_path = '\\\\?\\' + os.path.normpath(fn)
        if not DeleteFile(normalized_path):
            raise OSError(errno.EPERM, "couldn't remove long path", fn)

# Not needed on other platforms, but is handy for testing
else:
    def unlinklong(fn):
        os.unlink(fn)


def unlinkwrapper(unlinkorig, fn, ui):
    '''Calls unlink_long if original unlink function fails.'''
    try:
        ui.debug('calling unlink_orig %s\n' % fn)
        return unlinkorig(fn)
    except WindowsError as e:
        # Windows error 3 corresponds to ERROR_PATH_NOT_FOUND
        # only handle this case; re-raise the exception for other kinds of
        # failures.
        if e.winerror != 3:
            raise
        ui.debug('caught WindowsError ERROR_PATH_NOT_FOUND; '
                 'calling unlink_long %s\n' % fn)
        return unlinklong(fn)


@contextlib.contextmanager
def wrapunlink(ui):
    '''Context manager that temporarily monkeypatches unlink functions.'''
    purgemod = extensions.find('purge')
    to_wrap = [(purgemod.util, 'unlink')]

    # Pass along the ui object to the unlink_wrapper so we can get logging out
    # of it.
    wrapped = functools.partial(unlinkwrapper, ui=ui)

    # Wrap the original function(s) with our unlink wrapper.
    originals = {}
    for mod, func in to_wrap:
        ui.debug('wrapping %s %s\n' % (mod, func))
        originals[mod, func] = extensions.wrapfunction(mod, func, wrapped)

    try:
        yield
    finally:
        # Restore the originals.
        for mod, func in to_wrap:
            ui.debug('restoring %s %s\n' % (mod, func))
            setattr(mod, func, originals[mod, func])


def purgewrapper(orig, ui, *args, **kwargs):
    '''Runs original purge() command with unlink monkeypatched.'''
    with wrapunlink(ui):
        return orig(ui, *args, **kwargs)


@command('robustcheckout', [
    ('', 'upstream', '', 'URL of upstream repo to clone from'),
    ('r', 'revision', '', 'Revision to check out'),
    ('b', 'branch', '', 'Branch to check out'),
    ('', 'purge', False, 'Whether to purge the working directory'),
    ('', 'sharebase', '', 'Directory where shared repos should be placed'),
    ('', 'networkattempts', 3, 'Maximum number of attempts for network '
                               'operations'),
    ('', 'sparseprofile', '', 'Sparse checkout profile to use (path in repo)'),
    ],
    '[OPTION]... URL DEST',
    norepo=True)
def robustcheckout(ui, url, dest, upstream=None, revision=None, branch=None,
                   purge=False, sharebase=None, networkattempts=None,
                   sparseprofile=None):
    """Ensure a working copy has the specified revision checked out.

    Repository data is automatically pooled into the common directory
    specified by ``--sharebase``, which is a required argument. It is required
    because pooling storage prevents excessive cloning, which makes operations
    complete faster.

    One of ``--revision`` or ``--branch`` must be specified. ``--revision``
    is preferred, as it is deterministic and there is no ambiguity as to which
    revision will actually be checked out.

    If ``--upstream`` is used, the repo at that URL is used to perform the
    initial clone instead of cloning from the repo where the desired revision
    is located.

    ``--purge`` controls whether to removed untracked and ignored files from
    the working directory. If used, the end state of the working directory
    should only contain files explicitly under version control for the requested
    revision.

    ``--sparseprofile`` can be used to specify a sparse checkout profile to use.
    The sparse checkout profile corresponds to a file in the revision to be
    checked out. If a previous sparse profile or config is present, it will be
    replaced by this sparse profile. We choose not to "widen" the sparse config
    so operations are as deterministic as possible. If an existing checkout
    is present and it isn't using a sparse checkout, we error. This is to
    prevent accidentally enabling sparse on a repository that may have
    clients that aren't sparse aware. Sparse checkout support requires Mercurial
    4.3 or newer and the ``sparse`` extension must be enabled.
    """
    if not revision and not branch:
        raise error.Abort('must specify one of --revision or --branch')

    if revision and branch:
        raise error.Abort('cannot specify both --revision and --branch')

    # Require revision to look like a SHA-1.
    if revision:
        if len(revision) < 12 or len(revision) > 40 or not re.match('^[a-f0-9]+$', revision):
            raise error.Abort('--revision must be a SHA-1 fragment 12-40 '
                              'characters long')

    sharebase = sharebase or ui.config('share', 'pool')
    if not sharebase:
        raise error.Abort('share base directory not defined; refusing to operate',
                          hint='define share.pool config option or pass --sharebase')

    # Sparse profile support was added in Mercurial 4.3, where it was highly
    # experimental. Because of the fragility of it, we only support sparse
    # profiles on 4.3. When 4.4 is released, we'll need to opt in to sparse
    # support. We /could/ silently fall back to non-sparse when not supported.
    # However, given that sparse has performance implications, we want to fail
    # fast if we can't satisfy the desired checkout request.
    if sparseprofile:
        if util.versiontuple(n=2) not in ((4, 3), (4, 4)):
            raise error.Abort('sparse profile support only available for '
                              'Mercurial versions greater than 4.3 (using %s)' % util.version())

        try:
            extensions.find('sparse')
        except KeyError:
            raise error.Abort('sparse extension must be enabled to use '
                              '--sparseprofile')

    ui.warn('(using Mercurial %s)\n' % util.version())

    # worker.backgroundclose only makes things faster if running anti-virus,
    # which our automation doesn't. Disable it.
    ui.setconfig('worker', 'backgroundclose', False)

    # By default the progress bar starts after 3s and updates every 0.1s. We
    # change this so it shows and updates every 1.0s.
    # We also tell progress to assume a TTY is present so updates are printed
    # even if there is no known TTY.
    # We make the config change here instead of in a config file because
    # otherwise we're at the whim of whatever configs are used in automation.
    ui.setconfig('progress', 'delay', 1.0)
    ui.setconfig('progress', 'refresh', 1.0)
    ui.setconfig('progress', 'assume-tty', True)

    sharebase = os.path.realpath(sharebase)

    return _docheckout(ui, url, dest, upstream, revision, branch, purge,
                       sharebase, networkattempts,
                       sparse_profile=sparseprofile)


def _docheckout(ui, url, dest, upstream, revision, branch, purge, sharebase,
                networkattemptlimit, networkattempts=None, sparse_profile=None):
    if not networkattempts:
        networkattempts = [1]

    def callself():
        return _docheckout(ui, url, dest, upstream, revision, branch, purge,
                           sharebase, networkattemptlimit,
                           networkattempts=networkattempts,
                           sparse_profile=sparse_profile)

    ui.write('ensuring %s@%s is available at %s\n' % (url, revision or branch,
                                                      dest))

    # We assume that we're the only process on the machine touching the
    # repository paths that we were told to use. This means our recovery
    # scenario when things aren't "right" is to just nuke things and start
    # from scratch. This is easier to implement than verifying the state
    # of the data and attempting recovery. And in some scenarios (such as
    # potential repo corruption), it is probably faster, since verifying
    # repos can take a while.

    destvfs = getvfs()(dest, audit=False, realpath=True)

    def deletesharedstore(path=None):
        storepath = path or destvfs.read('.hg/sharedpath').strip()
        if storepath.endswith('.hg'):
            storepath = os.path.dirname(storepath)

        storevfs = getvfs()(storepath, audit=False)
        storevfs.rmtree(forcibly=True)

    if destvfs.exists() and not destvfs.exists('.hg'):
        raise error.Abort('destination exists but no .hg directory')

    # Refuse to enable sparse checkouts on existing checkouts. The reasoning
    # here is that another consumer of this repo may not be sparse aware. If we
    # enabled sparse, we would lock them out.
    if destvfs.exists() and sparse_profile and not destvfs.exists('.hg/sparse'):
        raise error.Abort('cannot enable sparse profile on existing '
                          'non-sparse checkout',
                          hint='use a separate working directory to use sparse')

    # And the other direction for symmetry.
    if not sparse_profile and destvfs.exists('.hg/sparse'):
        raise error.Abort('cannot use non-sparse checkout on existing sparse '
                          'checkout',
                          hint='use a separate working directory to use sparse')

    # Require checkouts to be tied to shared storage because efficiency.
    if destvfs.exists('.hg') and not destvfs.exists('.hg/sharedpath'):
        ui.warn('(destination is not shared; deleting)\n')
        destvfs.rmtree(forcibly=True)

    # Verify the shared path exists and is using modern pooled storage.
    if destvfs.exists('.hg/sharedpath'):
        storepath = destvfs.read('.hg/sharedpath').strip()

        ui.write('(existing repository shared store: %s)\n' % storepath)

        if not os.path.exists(storepath):
            ui.warn('(shared store does not exist; deleting destination)\n')
            destvfs.rmtree(forcibly=True)
        elif not re.search('[a-f0-9]{40}/\.hg$', storepath.replace('\\', '/')):
            ui.warn('(shared store does not belong to pooled storage; '
                    'deleting destination to improve efficiency)\n')
            destvfs.rmtree(forcibly=True)

    if destvfs.isfileorlink('.hg/wlock'):
        ui.warn('(dest has an active working directory lock; assuming it is '
                'left over from a previous process and that the destination '
                'is corrupt; deleting it just to be sure)\n')
        destvfs.rmtree(forcibly=True)

    def handlerepoerror(e):
        if e.message == _('abandoned transaction found'):
            ui.warn('(abandoned transaction found; trying to recover)\n')
            repo = hg.repository(ui, dest)
            if not repo.recover():
                ui.warn('(could not recover repo state; '
                        'deleting shared store)\n')
                deletesharedstore()

            ui.warn('(attempting checkout from beginning)\n')
            return callself()

        raise

    # At this point we either have an existing working directory using
    # shared, pooled storage or we have nothing.

    def handlenetworkfailure():
        if networkattempts[0] >= networkattemptlimit:
            raise error.Abort('reached maximum number of network attempts; '
                              'giving up\n')

        ui.warn('(retrying after network failure on attempt %d of %d)\n' %
                (networkattempts[0], networkattemptlimit))

        # Do a backoff on retries to mitigate the thundering herd
        # problem. This is an exponential backoff with a multipler
        # plus random jitter thrown in for good measure.
        # With the default settings, backoffs will be:
        # 1) 2.5 - 6.5
        # 2) 5.5 - 9.5
        # 3) 11.5 - 15.5
        backoff = (2 ** networkattempts[0] - 1) * 1.5
        jittermin = ui.configint('robustcheckout', 'retryjittermin', 1000)
        jittermax = ui.configint('robustcheckout', 'retryjittermax', 5000)
        backoff += float(random.randint(jittermin, jittermax)) / 1000.0
        ui.warn('(waiting %.2fs before retry)\n' % backoff)
        time.sleep(backoff)

        networkattempts[0] += 1

    def handlepullerror(e):
        """Handle an exception raised during a pull.

        Returns True if caller should call ``callself()`` to retry.
        """
        if isinstance(e, error.Abort):
            if e.args[0] == _('repository is unrelated'):
                ui.warn('(repository is unrelated; deleting)\n')
                destvfs.rmtree(forcibly=True)
                return True
            elif e.args[0].startswith(_('stream ended unexpectedly')):
                ui.warn('%s\n' % e.args[0])
                # Will raise if failure limit reached.
                handlenetworkfailure()
                return True
        elif isinstance(e, ssl.SSLError):
            # Assume all SSL errors are due to the network, as Mercurial
            # should convert non-transport errors like cert validation failures
            # to error.Abort.
            ui.warn('ssl error: %s\n' % e)
            handlenetworkfailure()
            return True
        elif isinstance(e, urllib2.URLError):
            if isinstance(e.reason, socket.error):
                ui.warn('socket error: %s\n' % e.reason)
                handlenetworkfailure()
                return True

        return False

    # Perform sanity checking of store. We may or may not know the path to the
    # local store. It depends if we have an existing destvfs pointing to a
    # share. To ensure we always find a local store, perform the same logic
    # that Mercurial's pooled storage does to resolve the local store path.
    cloneurl = upstream or url

    try:
        clonepeer = hg.peer(ui, {}, cloneurl)
        rootnode = clonepeer.lookup('0')
    except error.RepoLookupError:
        raise error.Abort('unable to resolve root revision from clone '
                          'source')
    except (error.Abort, ssl.SSLError, urllib2.URLError) as e:
        if handlepullerror(e):
            return callself()
        raise

    if rootnode == nullid:
        raise error.Abort('source repo appears to be empty')

    storepath = os.path.join(sharebase, hex(rootnode))
    storevfs = getvfs()(storepath, audit=False)

    if storevfs.isfileorlink('.hg/store/lock'):
        ui.warn('(shared store has an active lock; assuming it is left '
                'over from a previous process and that the store is '
                'corrupt; deleting store and destination just to be '
                'sure)\n')
        if destvfs.exists():
            destvfs.rmtree(forcibly=True)
        storevfs.rmtree(forcibly=True)

    if storevfs.exists() and not storevfs.exists('.hg/requires'):
        ui.warn('(shared store missing requires file; this is a really '
                'odd failure; deleting store and destination)\n')
        if destvfs.exists():
            destvfs.rmtree(forcibly=True)
        storevfs.rmtree(forcibly=True)

    if storevfs.exists('.hg/requires'):
        requires = set(storevfs.read('.hg/requires').splitlines())
        # FUTURE when we require generaldelta, this is where we can check
        # for that.
        required = {'dotencode', 'fncache'}

        missing = required - requires
        if missing:
            ui.warn('(shared store missing requirements: %s; deleting '
                    'store and destination to ensure optimal behavior)\n' %
                    ', '.join(sorted(missing)))
            if destvfs.exists():
                destvfs.rmtree(forcibly=True)
            storevfs.rmtree(forcibly=True)

    created = False

    if not destvfs.exists():
        # Ensure parent directories of destination exist.
        # Mercurial 3.8 removed ensuredirs and made makedirs race safe.
        if util.safehasattr(util, 'ensuredirs'):
            makedirs = util.ensuredirs
        else:
            makedirs = util.makedirs

        makedirs(os.path.dirname(destvfs.base), notindexed=True)
        makedirs(sharebase, notindexed=True)

        if upstream:
            ui.write('(cloning from upstream repo %s)\n' % upstream)

        try:
            res = hg.clone(ui, {}, clonepeer, dest=dest, update=False,
                           shareopts={'pool': sharebase, 'mode': 'identity'})
        except (error.Abort, ssl.SSLError, urllib2.URLError) as e:
            if handlepullerror(e):
                return callself()
            raise
        except error.RepoError as e:
            return handlerepoerror(e)
        except error.RevlogError as e:
            ui.warn('(repo corruption: %s; deleting shared store)\n' % e.message)
            deletesharedstore()
            return callself()

        # TODO retry here.
        if res is None:
            raise error.Abort('clone failed')

        # Verify it is using shared pool storage.
        if not destvfs.exists('.hg/sharedpath'):
            raise error.Abort('clone did not create a shared repo')

        created = True

    # The destination .hg directory should exist. Now make sure we have the
    # wanted revision.

    repo = hg.repository(ui, dest)

    # We only pull if we are using symbolic names or the requested revision
    # doesn't exist.
    havewantedrev = False
    if revision and revision in repo:
        ctx = repo[revision]

        if not ctx.hex().startswith(revision):
            raise error.Abort('--revision argument is ambiguous',
                              hint='must be the first 12+ characters of a '
                                   'SHA-1 fragment')

        checkoutrevision = ctx.hex()
        havewantedrev = True

    if not havewantedrev:
        ui.write('(pulling to obtain %s)\n' % (revision or branch,))

        remote = None
        try:
            remote = hg.peer(repo, {}, url)
            pullrevs = [remote.lookup(revision or branch)]
            checkoutrevision = hex(pullrevs[0])
            if branch:
                ui.warn('(remote resolved %s to %s; '
                        'result is not deterministic)\n' %
                        (branch, checkoutrevision))

            if checkoutrevision in repo:
                ui.warn('(revision already present locally; not pulling)\n')
            else:
                pullop = exchange.pull(repo, remote, heads=pullrevs)
                if not pullop.rheads:
                    raise error.Abort('unable to pull requested revision')
        except (error.Abort, ssl.SSLError, urllib2.URLError) as e:
            if handlepullerror(e):
                return callself()
            raise
        except error.RepoError as e:
            return handlerepoerror(e)
        except error.RevlogError as e:
            ui.warn('(repo corruption: %s; deleting shared store)\n' % e.message)
            deletesharedstore()
            return callself()
        finally:
            if remote:
                remote.close()

    # Now we should have the wanted revision in the store. Perform
    # working directory manipulation.

    # Purge if requested. We purge before update because this way we're
    # guaranteed to not have conflicts on `hg update`.
    if purge and not created:
        ui.write('(purging working directory)\n')
        purgeext = extensions.find('purge')

        # Mercurial 4.3 doesn't purge files outside the sparse checkout.
        # See https://bz.mercurial-scm.org/show_bug.cgi?id=5626. Force
        # purging by monkeypatching the sparse matcher.
        try:
            old_sparse_fn = getattr(repo.dirstate, '_sparsematchfn', None)
            if old_sparse_fn is not None:
                assert util.versiontuple(n=2) in ((4, 3), (4, 4))
                repo.dirstate._sparsematchfn = lambda: matchmod.always(repo.root, '')

            if purgeext.purge(ui, repo, all=True, abort_on_err=True,
                              # The function expects all arguments to be
                              # defined.
                              **{'print': None, 'print0': None, 'dirs': None,
                                 'files': None}):
                raise error.Abort('error purging')
        finally:
            if old_sparse_fn is not None:
                repo.dirstate._sparsematchfn = old_sparse_fn

    # Update the working directory.

    if sparse_profile:
        sparsemod = getsparse()

        # By default, Mercurial will ignore unknown sparse profiles. This could
        # lead to a full checkout. Be more strict.
        try:
            repo.filectx(sparse_profile, changeid=checkoutrevision).data()
        except error.ManifestLookupError:
            raise error.Abort('sparse profile %s does not exist at revision '
                              '%s' % (sparse_profile, checkoutrevision))

        old_config = sparsemod.parseconfig(repo.ui, repo.vfs.tryread('sparse'))
        old_includes, old_excludes, old_profiles = old_config

        if old_profiles == {sparse_profile} and not old_includes and not \
                old_excludes:
            ui.write('(sparse profile %s already set; no need to update '
                     'sparse config)\n' % sparse_profile)
        else:
            if old_includes or old_excludes or old_profiles:
                ui.write('(replacing existing sparse config with profile '
                         '%s)\n' % sparse_profile)
            else:
                ui.write('(setting sparse config to profile %s)\n' %
                         sparse_profile)

            # If doing an incremental update, this will perform two updates:
            # one to change the sparse profile and another to update to the new
            # revision. This is not desired. But there's not a good API in
            # Mercurial to do this as one operation.
            with repo.wlock():
                fcounts = map(len, sparsemod._updateconfigandrefreshwdir(
                    repo, [], [], [sparse_profile], force=True))

                repo.ui.status('%d files added, %d files dropped, '
                               '%d files conflicting\n' % tuple(fcounts))

            ui.write('(sparse refresh complete)\n')

    if commands.update(ui, repo, rev=checkoutrevision, clean=True):
        raise error.Abort('error updating')

    ui.write('updated to %s\n' % checkoutrevision)
    return None


def extsetup(ui):
    # Ensure required extensions are loaded.
    for ext in ('purge', 'share'):
        try:
            extensions.find(ext)
        except KeyError:
            extensions.load(ui, ext, None)

    purgemod = extensions.find('purge')
    extensions.wrapcommand(purgemod.cmdtable, 'purge', purgewrapper)
