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
import re

from mercurial.i18n import _
from mercurial.node import hex
from mercurial import (
    commands,
    error,
    exchange,
    extensions,
    cmdutil,
    hg,
    scmutil,
    util,
)

testedwith = '3.5 3.6 3.7 3.8'
minimumhgversion = '3.7'

cmdtable = {}
command = cmdutil.command(cmdtable)


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
    ],
    '[OPTION]... URL DEST',
    norepo=True)
def robustcheckout(ui, url, dest, upstream=None, revision=None, branch=None,
                   purge=False, sharebase=None):
    """Ensure a working copy has the specified revision checked out."""
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

    # worker.backgroundclose only makes things faster if running anti-virus,
    # which our automation doesn't. Disable it.
    ui.setconfig('worker', 'backgroundclose', False)

    sharebase = os.path.realpath(sharebase)

    return _docheckout(ui, url, dest, upstream, revision, branch, purge,
                       sharebase)

def _docheckout(ui, url, dest, upstream, revision, branch, purge, sharebase):
    def callself():
        return _docheckout(ui, url, dest, upstream, revision, branch, purge,
                           sharebase)

    ui.write('ensuring %s@%s is available at %s\n' % (url, revision or branch,
                                                      dest))

    destvfs = scmutil.vfs(dest, audit=False, realpath=True)

    if destvfs.exists() and not destvfs.exists('.hg'):
        raise error.Abort('destination exists but no .hg directory')

    # Require checkouts to be tied to shared storage because efficiency.
    if destvfs.exists('.hg') and not destvfs.exists('.hg/sharedpath'):
        ui.warn('(destination is not shared; deleting)\n')
        destvfs.rmtree(forcibly=True)

    # Verify the shared path exists and is using modern pooled storage.
    if destvfs.exists('.hg/sharedpath'):
        storepath = destvfs.read('.hg/sharedpath').strip()

        ui.write('(existing repository shared store: %s)\n' % storepath)

        if not os.path.exists(storepath):
            ui.warn('(shared store does not exist; deleting)\n')
            destvfs.rmtree(forcibly=True)
        elif not re.search('[a-f0-9]{40}/\.hg$', storepath.replace('\\', '/')):
            ui.warn('(shared store does not belong to pooled storage; '
                    'deleting to improve efficiency)\n')
            destvfs.rmtree(forcibly=True)

        # FUTURE when we require generaldelta, this is where we can check
        # for that.

    def deletesharedstore():
        storepath = destvfs.read('.hg/sharedpath').strip()
        if storepath.endswith('.hg'):
            storepath = os.path.dirname(storepath)

        storevfs = scmutil.vfs(storepath, audit=False)
        storevfs.rmtree(forcibly=True)

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
        cloneurl = upstream or url

        try:
            res = hg.clone(ui, {}, cloneurl, dest=dest, update=False,
                           shareopts={'pool': sharebase, 'mode': 'identity'})
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
        except error.Abort as e:
            if e.message == _('repository is unrelated'):
                ui.warn('(repository is unrelated; deleting)\n')
                destvfs.rmtree(forcibly=True)
                return callself()

            raise
        except error.RepoError as e:
            return handlerepoerror(e)
        except error.RevlogError as e:
            ui.warn('(repo corruption: %s; deleting shared store)\n' % e.message)
            deletesharedstore()
            return callself()
        finally:
            remote.close()

    # Now we should have the wanted revision in the store. Perform
    # working directory manipulation.

    # Purge if requested. We purge before update because this way we're
    # guaranteed to not have conflicts on `hg update`.
    if purge and not created:
        ui.write('(purging working directory)\n')
        purgeext = extensions.find('purge')

        if purgeext.purge(ui, repo, all=True, abort_on_err=True,
                          # The function expects all arguments to be
                          # defined.
                          **{'print': None, 'print0': None, 'dirs': None,
                             'files': None}):
            raise error.Abort('error purging')

    # Update the working directory.
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
