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
import json
import os
import random
import re
import socket
import ssl
import time

from mercurial.i18n import _
from mercurial.node import hex, nullid
from mercurial import (
    commands,
    configitems,
    error,
    exchange,
    extensions,
    hg,
    match as matchmod,
    pycompat,
    registrar,
    scmutil,
    urllibcompat,
    util,
    vfs,
)

# Causes worker to purge caches on process exit and for task to retry.
EXIT_PURGE_CACHE = 72

testedwith = (
    b"4.5 4.6 4.7 4.8 4.9 5.0 5.1 5.2 5.3 5.4 5.5 5.6 5.7 5.8 5.9 6.0 6.1 6.2 6.3 6.4"
)
minimumhgversion = b"4.5"

cmdtable = {}
command = registrar.command(cmdtable)

configtable = {}
configitem = registrar.configitem(configtable)

configitem(b"robustcheckout", b"retryjittermin", default=configitems.dynamicdefault)
configitem(b"robustcheckout", b"retryjittermax", default=configitems.dynamicdefault)


def getsparse():
    from mercurial import sparse

    return sparse


def peerlookup(remote, v):
    with remote.commandexecutor() as e:
        return e.callcommand(b"lookup", {b"key": v}).result()


@command(
    b"robustcheckout",
    [
        (b"", b"upstream", b"", b"URL of upstream repo to clone from"),
        (b"r", b"revision", b"", b"Revision to check out"),
        (b"b", b"branch", b"", b"Branch to check out"),
        (b"", b"purge", False, b"Whether to purge the working directory"),
        (b"", b"sharebase", b"", b"Directory where shared repos should be placed"),
        (
            b"",
            b"networkattempts",
            3,
            b"Maximum number of attempts for network " b"operations",
        ),
        (b"", b"sparseprofile", b"", b"Sparse checkout profile to use (path in repo)"),
        (
            b"U",
            b"noupdate",
            False,
            b"the clone will include an empty working directory\n"
            b"(only a repository)",
        ),
    ],
    b"[OPTION]... URL DEST",
    norepo=True,
)
def robustcheckout(
    ui,
    url,
    dest,
    upstream=None,
    revision=None,
    branch=None,
    purge=False,
    sharebase=None,
    networkattempts=None,
    sparseprofile=None,
    noupdate=False,
):
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
        raise error.Abort(b"must specify one of --revision or --branch")

    if revision and branch:
        raise error.Abort(b"cannot specify both --revision and --branch")

    # Require revision to look like a SHA-1.
    if revision:
        if (
            len(revision) < 12
            or len(revision) > 40
            or not re.match(b"^[a-f0-9]+$", revision)
        ):
            raise error.Abort(
                b"--revision must be a SHA-1 fragment 12-40 " b"characters long"
            )

    sharebase = sharebase or ui.config(b"share", b"pool")
    if not sharebase:
        raise error.Abort(
            b"share base directory not defined; refusing to operate",
            hint=b"define share.pool config option or pass --sharebase",
        )

    # Sparse profile support was added in Mercurial 4.3, where it was highly
    # experimental. Because of the fragility of it, we only support sparse
    # profiles on 4.3. When 4.4 is released, we'll need to opt in to sparse
    # support. We /could/ silently fall back to non-sparse when not supported.
    # However, given that sparse has performance implications, we want to fail
    # fast if we can't satisfy the desired checkout request.
    if sparseprofile:
        try:
            extensions.find(b"sparse")
        except KeyError:
            raise error.Abort(
                b"sparse extension must be enabled to use " b"--sparseprofile"
            )

    ui.warn(b"(using Mercurial %s)\n" % util.version())

    # worker.backgroundclose only makes things faster if running anti-virus,
    # which our automation doesn't. Disable it.
    ui.setconfig(b"worker", b"backgroundclose", False)
    # Don't wait forever if the connection hangs
    ui.setconfig(b"http", b"timeout", 600)

    # By default the progress bar starts after 3s and updates every 0.1s. We
    # change this so it shows and updates every 1.0s.
    # We also tell progress to assume a TTY is present so updates are printed
    # even if there is no known TTY.
    # We make the config change here instead of in a config file because
    # otherwise we're at the whim of whatever configs are used in automation.
    ui.setconfig(b"progress", b"delay", 1.0)
    ui.setconfig(b"progress", b"refresh", 1.0)
    ui.setconfig(b"progress", b"assume-tty", True)

    sharebase = os.path.realpath(sharebase)

    optimes = []
    behaviors = set()
    start = time.time()

    try:
        return _docheckout(
            ui,
            url,
            dest,
            upstream,
            revision,
            branch,
            purge,
            sharebase,
            optimes,
            behaviors,
            networkattempts,
            sparse_profile=sparseprofile,
            noupdate=noupdate,
        )
    finally:
        overall = time.time() - start

        # We store the overall time multiple ways in order to help differentiate
        # the various "flavors" of operations.

        # ``overall`` is always the total operation time.
        optimes.append(("overall", overall))

        def record_op(name):
            # If special behaviors due to "corrupt" storage occur, we vary the
            # name to convey that.
            if "remove-store" in behaviors:
                name += "_rmstore"
            if "remove-wdir" in behaviors:
                name += "_rmwdir"

            optimes.append((name, overall))

        # We break out overall operations primarily by their network interaction
        # We have variants within for working directory operations.
        if "clone" in behaviors and "create-store" in behaviors:
            record_op("overall_clone")

            if "sparse-update" in behaviors:
                record_op("overall_clone_sparsecheckout")
            else:
                record_op("overall_clone_fullcheckout")

        elif "pull" in behaviors or "clone" in behaviors:
            record_op("overall_pull")

            if "sparse-update" in behaviors:
                record_op("overall_pull_sparsecheckout")
            else:
                record_op("overall_pull_fullcheckout")

            if "empty-wdir" in behaviors:
                record_op("overall_pull_emptywdir")
            else:
                record_op("overall_pull_populatedwdir")

        else:
            record_op("overall_nopull")

            if "sparse-update" in behaviors:
                record_op("overall_nopull_sparsecheckout")
            else:
                record_op("overall_nopull_fullcheckout")

            if "empty-wdir" in behaviors:
                record_op("overall_nopull_emptywdir")
            else:
                record_op("overall_nopull_populatedwdir")

        server_url = urllibcompat.urlreq.urlparse(url).netloc

        if "TASKCLUSTER_INSTANCE_TYPE" in os.environ:
            perfherder = {
                "framework": {
                    "name": "vcs",
                },
                "suites": [],
            }
            for op, duration in optimes:
                perfherder["suites"].append(
                    {
                        "name": op,
                        "value": duration,
                        "lowerIsBetter": True,
                        "shouldAlert": False,
                        "serverUrl": server_url.decode("utf-8"),
                        "hgVersion": util.version().decode("utf-8"),
                        "extraOptions": [os.environ["TASKCLUSTER_INSTANCE_TYPE"]],
                        "subtests": [],
                    }
                )
            ui.write(
                b"PERFHERDER_DATA: %s\n"
                % pycompat.bytestr(json.dumps(perfherder, sort_keys=True))
            )


def _docheckout(
    ui,
    url,
    dest,
    upstream,
    revision,
    branch,
    purge,
    sharebase,
    optimes,
    behaviors,
    networkattemptlimit,
    networkattempts=None,
    sparse_profile=None,
    noupdate=False,
):
    if not networkattempts:
        networkattempts = [1]

    def callself():
        return _docheckout(
            ui,
            url,
            dest,
            upstream,
            revision,
            branch,
            purge,
            sharebase,
            optimes,
            behaviors,
            networkattemptlimit,
            networkattempts=networkattempts,
            sparse_profile=sparse_profile,
            noupdate=noupdate,
        )

    @contextlib.contextmanager
    def timeit(op, behavior):
        behaviors.add(behavior)
        errored = False
        try:
            start = time.time()
            yield
        except Exception:
            errored = True
            raise
        finally:
            elapsed = time.time() - start

            if errored:
                op += "_errored"

            optimes.append((op, elapsed))

    ui.write(b"ensuring %s@%s is available at %s\n" % (url, revision or branch, dest))

    # We assume that we're the only process on the machine touching the
    # repository paths that we were told to use. This means our recovery
    # scenario when things aren't "right" is to just nuke things and start
    # from scratch. This is easier to implement than verifying the state
    # of the data and attempting recovery. And in some scenarios (such as
    # potential repo corruption), it is probably faster, since verifying
    # repos can take a while.

    destvfs = vfs.vfs(dest, audit=False, realpath=True)

    def deletesharedstore(path=None):
        storepath = path or destvfs.read(b".hg/sharedpath").strip()
        if storepath.endswith(b".hg"):
            storepath = os.path.dirname(storepath)

        storevfs = vfs.vfs(storepath, audit=False)
        storevfs.rmtree(forcibly=True)

    if destvfs.exists() and not destvfs.exists(b".hg"):
        raise error.Abort(b"destination exists but no .hg directory")

    # Refuse to enable sparse checkouts on existing checkouts. The reasoning
    # here is that another consumer of this repo may not be sparse aware. If we
    # enabled sparse, we would lock them out.
    if destvfs.exists() and sparse_profile and not destvfs.exists(b".hg/sparse"):
        raise error.Abort(
            b"cannot enable sparse profile on existing " b"non-sparse checkout",
            hint=b"use a separate working directory to use sparse",
        )

    # And the other direction for symmetry.
    if not sparse_profile and destvfs.exists(b".hg/sparse"):
        raise error.Abort(
            b"cannot use non-sparse checkout on existing sparse " b"checkout",
            hint=b"use a separate working directory to use sparse",
        )

    # Require checkouts to be tied to shared storage because efficiency.
    if destvfs.exists(b".hg") and not destvfs.exists(b".hg/sharedpath"):
        ui.warn(b"(destination is not shared; deleting)\n")
        with timeit("remove_unshared_dest", "remove-wdir"):
            destvfs.rmtree(forcibly=True)

    # Verify the shared path exists and is using modern pooled storage.
    if destvfs.exists(b".hg/sharedpath"):
        storepath = destvfs.read(b".hg/sharedpath").strip()

        ui.write(b"(existing repository shared store: %s)\n" % storepath)

        if not os.path.exists(storepath):
            ui.warn(b"(shared store does not exist; deleting destination)\n")
            with timeit("removed_missing_shared_store", "remove-wdir"):
                destvfs.rmtree(forcibly=True)
        elif not re.search(b"[a-f0-9]{40}/\.hg$", storepath.replace(b"\\", b"/")):
            ui.warn(
                b"(shared store does not belong to pooled storage; "
                b"deleting destination to improve efficiency)\n"
            )
            with timeit("remove_unpooled_store", "remove-wdir"):
                destvfs.rmtree(forcibly=True)

    if destvfs.isfileorlink(b".hg/wlock"):
        ui.warn(
            b"(dest has an active working directory lock; assuming it is "
            b"left over from a previous process and that the destination "
            b"is corrupt; deleting it just to be sure)\n"
        )
        with timeit("remove_locked_wdir", "remove-wdir"):
            destvfs.rmtree(forcibly=True)

    def handlerepoerror(e):
        if pycompat.bytestr(e) == _(b"abandoned transaction found"):
            ui.warn(b"(abandoned transaction found; trying to recover)\n")
            repo = hg.repository(ui, dest)
            if not repo.recover():
                ui.warn(b"(could not recover repo state; " b"deleting shared store)\n")
                with timeit("remove_unrecovered_shared_store", "remove-store"):
                    deletesharedstore()

            ui.warn(b"(attempting checkout from beginning)\n")
            return callself()

        raise

    # At this point we either have an existing working directory using
    # shared, pooled storage or we have nothing.

    def handlenetworkfailure():
        if networkattempts[0] >= networkattemptlimit:
            raise error.Abort(
                b"reached maximum number of network attempts; " b"giving up\n"
            )

        ui.warn(
            b"(retrying after network failure on attempt %d of %d)\n"
            % (networkattempts[0], networkattemptlimit)
        )

        # Do a backoff on retries to mitigate the thundering herd
        # problem. This is an exponential backoff with a multipler
        # plus random jitter thrown in for good measure.
        # With the default settings, backoffs will be:
        # 1) 2.5 - 6.5
        # 2) 5.5 - 9.5
        # 3) 11.5 - 15.5
        backoff = (2 ** networkattempts[0] - 1) * 1.5
        jittermin = ui.configint(b"robustcheckout", b"retryjittermin", 1000)
        jittermax = ui.configint(b"robustcheckout", b"retryjittermax", 5000)
        backoff += float(random.randint(jittermin, jittermax)) / 1000.0
        ui.warn(b"(waiting %.2fs before retry)\n" % backoff)
        time.sleep(backoff)

        networkattempts[0] += 1

    def handlepullerror(e):
        """Handle an exception raised during a pull.

        Returns True if caller should call ``callself()`` to retry.
        """
        if isinstance(e, error.Abort):
            if e.args[0] == _(b"repository is unrelated"):
                ui.warn(b"(repository is unrelated; deleting)\n")
                destvfs.rmtree(forcibly=True)
                return True
            elif e.args[0].startswith(_(b"stream ended unexpectedly")):
                ui.warn(b"%s\n" % e.args[0])
                # Will raise if failure limit reached.
                handlenetworkfailure()
                return True
        # TODO test this branch
        elif isinstance(e, error.ResponseError):
            if e.args[0].startswith(_(b"unexpected response from remote server:")):
                ui.warn(b"(unexpected response from remote server; retrying)\n")
                destvfs.rmtree(forcibly=True)
                # Will raise if failure limit reached.
                handlenetworkfailure()
                return True
        elif isinstance(e, ssl.SSLError):
            # Assume all SSL errors are due to the network, as Mercurial
            # should convert non-transport errors like cert validation failures
            # to error.Abort.
            ui.warn(b"ssl error: %s\n" % pycompat.bytestr(str(e)))
            handlenetworkfailure()
            return True
        elif isinstance(e, urllibcompat.urlerr.httperror) and e.code >= 500:
            ui.warn(b"http error: %s\n" % pycompat.bytestr(str(e.reason)))
            handlenetworkfailure()
            return True
        elif isinstance(e, urllibcompat.urlerr.urlerror):
            if isinstance(e.reason, socket.error):
                ui.warn(b"socket error: %s\n" % pycompat.bytestr(str(e.reason)))
                handlenetworkfailure()
                return True
            else:
                ui.warn(
                    b"unhandled URLError; reason type: %s; value: %s\n"
                    % (
                        pycompat.bytestr(e.reason.__class__.__name__),
                        pycompat.bytestr(str(e.reason)),
                    )
                )
        elif isinstance(e, socket.timeout):
            ui.warn(b"socket timeout\n")
            handlenetworkfailure()
            return True
        else:
            ui.warn(
                b"unhandled exception during network operation; type: %s; "
                b"value: %s\n"
                % (pycompat.bytestr(e.__class__.__name__), pycompat.bytestr(str(e)))
            )

        return False

    # Perform sanity checking of store. We may or may not know the path to the
    # local store. It depends if we have an existing destvfs pointing to a
    # share. To ensure we always find a local store, perform the same logic
    # that Mercurial's pooled storage does to resolve the local store path.
    cloneurl = upstream or url

    try:
        clonepeer = hg.peer(ui, {}, cloneurl)
        rootnode = peerlookup(clonepeer, b"0")
    except error.RepoLookupError:
        raise error.Abort(b"unable to resolve root revision from clone " b"source")
    except (
        error.Abort,
        ssl.SSLError,
        urllibcompat.urlerr.urlerror,
        socket.timeout,
    ) as e:
        if handlepullerror(e):
            return callself()
        raise

    if rootnode == nullid:
        raise error.Abort(b"source repo appears to be empty")

    storepath = os.path.join(sharebase, hex(rootnode))
    storevfs = vfs.vfs(storepath, audit=False)

    if storevfs.isfileorlink(b".hg/store/lock"):
        ui.warn(
            b"(shared store has an active lock; assuming it is left "
            b"over from a previous process and that the store is "
            b"corrupt; deleting store and destination just to be "
            b"sure)\n"
        )
        if destvfs.exists():
            with timeit("remove_dest_active_lock", "remove-wdir"):
                destvfs.rmtree(forcibly=True)

        with timeit("remove_shared_store_active_lock", "remove-store"):
            storevfs.rmtree(forcibly=True)

    if storevfs.exists() and not storevfs.exists(b".hg/requires"):
        ui.warn(
            b"(shared store missing requires file; this is a really "
            b"odd failure; deleting store and destination)\n"
        )
        if destvfs.exists():
            with timeit("remove_dest_no_requires", "remove-wdir"):
                destvfs.rmtree(forcibly=True)

        with timeit("remove_shared_store_no_requires", "remove-store"):
            storevfs.rmtree(forcibly=True)

    if storevfs.exists(b".hg/requires"):
        requires = set(storevfs.read(b".hg/requires").splitlines())
        # "share-safe" (enabled by default as of hg 6.1) moved most
        # requirements to a new file, so we need to look there as well to avoid
        # deleting and re-cloning each time
        if b"share-safe" in requires:
            requires |= set(storevfs.read(b".hg/store/requires").splitlines())
        # FUTURE when we require generaldelta, this is where we can check
        # for that.
        required = {b"dotencode", b"fncache"}

        missing = required - requires
        if missing:
            ui.warn(
                b"(shared store missing requirements: %s; deleting "
                b"store and destination to ensure optimal behavior)\n"
                % b", ".join(sorted(missing))
            )
            if destvfs.exists():
                with timeit("remove_dest_missing_requires", "remove-wdir"):
                    destvfs.rmtree(forcibly=True)

            with timeit("remove_shared_store_missing_requires", "remove-store"):
                storevfs.rmtree(forcibly=True)

    created = False

    if not destvfs.exists():
        # Ensure parent directories of destination exist.
        # Mercurial 3.8 removed ensuredirs and made makedirs race safe.
        if util.safehasattr(util, "ensuredirs"):
            makedirs = util.ensuredirs
        else:
            makedirs = util.makedirs

        makedirs(os.path.dirname(destvfs.base), notindexed=True)
        makedirs(sharebase, notindexed=True)

        if upstream:
            ui.write(b"(cloning from upstream repo %s)\n" % upstream)

        if not storevfs.exists():
            behaviors.add(b"create-store")

        try:
            with timeit("clone", "clone"):
                shareopts = {b"pool": sharebase, b"mode": b"identity"}
                res = hg.clone(
                    ui,
                    {},
                    clonepeer,
                    dest=dest,
                    update=False,
                    shareopts=shareopts,
                    stream=True,
                )
        except (
            error.Abort,
            ssl.SSLError,
            urllibcompat.urlerr.urlerror,
            socket.timeout,
        ) as e:
            if handlepullerror(e):
                return callself()
            raise
        except error.RepoError as e:
            return handlerepoerror(e)
        except error.RevlogError as e:
            ui.warn(b"(repo corruption: %s; deleting shared store)\n" % e)
            with timeit("remove_shared_store_revlogerror", "remote-store"):
                deletesharedstore()
            return callself()

        # TODO retry here.
        if res is None:
            raise error.Abort(b"clone failed")

        # Verify it is using shared pool storage.
        if not destvfs.exists(b".hg/sharedpath"):
            raise error.Abort(b"clone did not create a shared repo")

        created = True

    # The destination .hg directory should exist. Now make sure we have the
    # wanted revision.

    repo = hg.repository(ui, dest)

    # We only pull if we are using symbolic names or the requested revision
    # doesn't exist.
    havewantedrev = False

    if revision:
        try:
            ctx = scmutil.revsingle(repo, revision)
        except error.RepoLookupError:
            ctx = None

        if ctx:
            if not ctx.hex().startswith(revision):
                raise error.Abort(
                    b"--revision argument is ambiguous",
                    hint=b"must be the first 12+ characters of a " b"SHA-1 fragment",
                )

            checkoutrevision = ctx.hex()
            havewantedrev = True

    if not havewantedrev:
        ui.write(b"(pulling to obtain %s)\n" % (revision or branch,))

        remote = None
        try:
            remote = hg.peer(repo, {}, url)
            pullrevs = [peerlookup(remote, revision or branch)]
            checkoutrevision = hex(pullrevs[0])
            if branch:
                ui.warn(
                    b"(remote resolved %s to %s; "
                    b"result is not deterministic)\n" % (branch, checkoutrevision)
                )

            if checkoutrevision in repo:
                ui.warn(b"(revision already present locally; not pulling)\n")
            else:
                with timeit("pull", "pull"):
                    pullop = exchange.pull(repo, remote, heads=pullrevs)
                    if not pullop.rheads:
                        raise error.Abort(b"unable to pull requested revision")
        except (
            error.Abort,
            ssl.SSLError,
            urllibcompat.urlerr.urlerror,
            socket.timeout,
        ) as e:
            if handlepullerror(e):
                return callself()
            raise
        except error.RepoError as e:
            return handlerepoerror(e)
        except error.RevlogError as e:
            ui.warn(b"(repo corruption: %s; deleting shared store)\n" % e)
            deletesharedstore()
            return callself()
        finally:
            if remote:
                remote.close()

    # Now we should have the wanted revision in the store. Perform
    # working directory manipulation.

    # Avoid any working directory manipulations if `-U`/`--noupdate` was passed
    if noupdate:
        ui.write(b"(skipping update since `-U` was passed)\n")
        return None

    # Purge if requested. We purge before update because this way we're
    # guaranteed to not have conflicts on `hg update`.
    if purge and not created:
        ui.write(b"(purging working directory)\n")
        purge = getattr(commands, "purge", None)
        if not purge:
            purge = extensions.find(b"purge").purge

        # Mercurial 4.3 doesn't purge files outside the sparse checkout.
        # See https://bz.mercurial-scm.org/show_bug.cgi?id=5626. Force
        # purging by monkeypatching the sparse matcher.
        try:
            old_sparse_fn = getattr(repo.dirstate, "_sparsematchfn", None)
            if old_sparse_fn is not None:
                repo.dirstate._sparsematchfn = lambda: matchmod.always()

            with timeit("purge", "purge"):
                if purge(
                    ui,
                    repo,
                    all=True,
                    abort_on_err=True,
                    # The function expects all arguments to be
                    # defined.
                    **{"print": None, "print0": None, "dirs": None, "files": None}
                ):
                    raise error.Abort(b"error purging")
        finally:
            if old_sparse_fn is not None:
                repo.dirstate._sparsematchfn = old_sparse_fn

    # Update the working directory.

    if repo[b"."].node() == nullid:
        behaviors.add("empty-wdir")
    else:
        behaviors.add("populated-wdir")

    if sparse_profile:
        sparsemod = getsparse()

        # By default, Mercurial will ignore unknown sparse profiles. This could
        # lead to a full checkout. Be more strict.
        try:
            repo.filectx(sparse_profile, changeid=checkoutrevision).data()
        except error.ManifestLookupError:
            raise error.Abort(
                b"sparse profile %s does not exist at revision "
                b"%s" % (sparse_profile, checkoutrevision)
            )

        old_config = sparsemod.parseconfig(
            repo.ui, repo.vfs.tryread(b"sparse"), b"sparse"
        )

        old_includes, old_excludes, old_profiles = old_config

        if old_profiles == {sparse_profile} and not old_includes and not old_excludes:
            ui.write(
                b"(sparse profile %s already set; no need to update "
                b"sparse config)\n" % sparse_profile
            )
        else:
            if old_includes or old_excludes or old_profiles:
                ui.write(
                    b"(replacing existing sparse config with profile "
                    b"%s)\n" % sparse_profile
                )
            else:
                ui.write(b"(setting sparse config to profile %s)\n" % sparse_profile)

            # If doing an incremental update, this will perform two updates:
            # one to change the sparse profile and another to update to the new
            # revision. This is not desired. But there's not a good API in
            # Mercurial to do this as one operation.
            # TRACKING hg64 - Mercurial 6.4 and later require call to
            # dirstate.changing_parents(repo)
            def parentchange(repo):
                if util.safehasattr(repo.dirstate, "changing_parents"):
                    return repo.dirstate.changing_parents(repo)
                return repo.dirstate.parentchange()

            with repo.wlock(), parentchange(repo), timeit(
                "sparse_update_config", "sparse-update-config"
            ):
                # pylint --py3k: W1636
                fcounts = list(
                    map(
                        len,
                        sparsemod._updateconfigandrefreshwdir(
                            repo, [], [], [sparse_profile], force=True
                        ),
                    )
                )

                repo.ui.status(
                    b"%d files added, %d files dropped, "
                    b"%d files conflicting\n" % tuple(fcounts)
                )

            ui.write(b"(sparse refresh complete)\n")

    op = "update_sparse" if sparse_profile else "update"
    behavior = "update-sparse" if sparse_profile else "update"

    with timeit(op, behavior):
        if commands.update(ui, repo, rev=checkoutrevision, clean=True):
            raise error.Abort(b"error updating")

    ui.write(b"updated to %s\n" % checkoutrevision)

    return None


def extsetup(ui):
    # Ensure required extensions are loaded.
    for ext in (b"purge", b"share"):
        try:
            extensions.find(ext)
        except KeyError:
            extensions.load(ui, ext, None)
