// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// This module provides a facility for disconnecting Sync and FxA, optionally
// sanitizing profile data as part of the process.

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  Log: "resource://gre/modules/Log.jsm",
  Sanitizer: "resource:///modules/Sanitizer.jsm",
  AsyncShutdown: "resource://gre/modules/AsyncShutdown.jsm",
  fxAccounts: "resource://gre/modules/FxAccounts.jsm",
  setTimeout: "resource://gre/modules/Timer.jsm",
});

this.EXPORTED_SYMBOLS = ["SyncDisconnect"];

this.SyncDisconnectInternal = {
  lockRetryInterval: 1000, // wait 1 seconds before trying for the lock again.
  lockRetryCount: 120, // Try 120 times (==2 mins) before giving up in disgust.
  promiseDisconnectFinished: null, // If we are sanitizing, a promise for completion.

  // mocked by tests.
  getWeave() {
    return ChromeUtils.import("resource://services-sync/main.js", {}).Weave;
  },

  // Returns a promise that resolves when we are not syncing, waiting until
  // a current Sync completes if necessary. Resolves with true if we
  // successfully waited, in which case the sync lock will have been taken to
  // ensure future syncs don't state, or resolves with false if we gave up
  // waiting for the sync to complete (in which case we didn't take a lock -
  // but note that Sync probably remains locked in this case regardless.)
  async promiseNotSyncing(abortController) {
    let weave = this.getWeave();
    let log = Log.repository.getLogger("Sync.Service");
    // We might be syncing - poll for up to 2 minutes waiting for the lock.
    // (2 minutes seems extreme, but should be very rare.)
    return new Promise(resolve => {
      abortController.signal.onabort = () => {
        resolve(false);
      };

      let attempts = 0;
      let checkLock = () => {
        if (abortController.signal.aborted) {
          // We've already resolved, so don't want a new timer to ever start.
          return;
        }
        if (weave.Service.lock()) {
          resolve(true);
          return;
        }
        attempts += 1;
        if (attempts >= this.lockRetryCount) {
          log.error("Gave up waiting for the sync lock - going ahead with sanitize anyway");
          resolve(false);
          return;
        }
        log.debug("Waiting a couple of seconds to get the sync lock");
        setTimeout(checkLock, this.lockRetryInterval);
      };
      checkLock();
    });
  },

  // Sanitize Sync-related data.
  async doSanitizeSyncData() {
    let weave = this.getWeave();
    // Get the sync logger - if stuff goes wrong it can be useful to have that
    // recorded in the sync logs.
    let log = Log.repository.getLogger("Sync.Service");
    log.info("Starting santitize of Sync data");
    try {
      // We clobber data for all Sync engines that are enabled.
      await weave.Service.promiseInitialized;
      weave.Service.enabled = false;

      log.info("starting actual sanitization");
      for (let engine of weave.Service.engineManager.getAll()) {
        if (engine.enabled) {
          try {
            log.info("Wiping engine", engine.name);
            await engine.wipeClient();
          } catch (ex) {
            log.error("Failed to wipe engine", ex);
          }
        }
      }
      log.info("Finished wiping sync data");
    } catch (ex) {
      log.error("Failed to sanitize Sync data", ex);
      console.error("Failed to sanitize Sync data", ex);
    }
    try {
      // ensure any logs we wrote are flushed to disk.
      await weave.Service.errorHandler.resetFileLog();
    } catch (ex) {
      console.log("Failed to flush the Sync log", ex);
    }
  },

  // Sanitize all Browser data.
  async doSanitizeBrowserData() {
    try {
      // sanitize everything other than "open windows" (and we don't do that
      // because it may confuse the user - they probably want to see
      // about:prefs with the disconnection reflected.
      let itemsToClear = Object.keys(Sanitizer.items).filter(k => k != "openWindows");
      await Sanitizer.sanitize(itemsToClear);
    } catch (ex) {
      console.error("Failed to sanitize other data", ex);
    }
  },

  async doSyncAndAccountDisconnect(shouldUnlock) {
    // We do a startOver of Sync first - if we do the account first we end
    // up with Sync configured but FxA not configured, which causes the browser
    // UI to briefly enter a "needs reauth" state.
    let Weave = this.getWeave();
    await Weave.Service.startOver();
    await fxAccounts.signOut();
    // Sync may have been disabled if we santized, so re-enable it now or
    // else the user will be unable to resync should they sign in before a
    // restart.
    Weave.Service.enabled = true;

    // and finally, if we managed to get the lock before, we should unlock it
    // now.
    if (shouldUnlock) {
      Weave.Service.unlock();
    }
  },

  // Start the sanitization process. Returns a promise that resolves when
  // the sanitize is complete, and an AbortController which can be used to
  // abort the process of waiting for a sync to complete.
  async _startDisconnect(abortController,
                         {sanitizeSyncData = false, sanitizeBrowserData = false} = {}) {
    // This is a bit convoluted - we want to wait for a sync to finish before
    // sanitizing, but want to abort that wait if the browser shuts down while
    // we are waiting (in which case we'll charge ahead anyway).
    // So we do this by using an AbortController and passing that to the
    // function that waits for the sync lock - it will immediately resolve
    // if the abort controller is aborted.
    let log = Log.repository.getLogger("Sync.Service");
    log.info("waiting for any existing syncs to complete");
    let locked = await this.promiseNotSyncing(abortController);

    if (sanitizeSyncData) {
      await this.doSanitizeSyncData();
    }

    // We disconnect before sanitizing the browser data - in a worst-case
    // scenario where the sanitize takes so long that even the shutdown
    // blocker doesn't allow it to finish, we should still at least be in
    // a disconnected state on the next startup.
    log.info("disconnecting account");
    await this.doSyncAndAccountDisconnect(locked);

    if (sanitizeBrowserData) {
      await this.doSanitizeBrowserData();
    }

  },

  async disconnect(options) {
    if (this.promiseDisconnectFinished) {
        throw new Error("A disconnect is already in progress");
    }
    let abortController = new AbortController();
    let promiseDisconnectFinished = this._startDisconnect(abortController, options);
    this.promiseDisconnectFinished = promiseDisconnectFinished;
    let shutdownBlocker = () => {
      // oh dear - we are sanitizing (probably stuck waiting for a sync to
      // complete) and the browser is shutting down. Let's avoid the wait
      // for sync to complete and continue the process anyway.
      abortController.abort();
      return promiseDisconnectFinished;
    };
    AsyncShutdown.quitApplicationGranted.addBlocker(
      "SyncDisconnect: removing requested data",
      shutdownBlocker);

    // wait for it to finish - hopefully without the blocker being called.
    await promiseDisconnectFinished;
    this.promiseDisconnectFinished = null;

    // sanitize worked so remove our blocker - it's a noop if the blocker
    // did call us.
    AsyncShutdown.quitApplicationGranted.removeBlocker(shutdownBlocker);
  },
};

this.SyncDisconnect = {
    get promiseDisconnectFinished() {
        return SyncDisconnectInternal.promiseDisconnectFinished;
    },

    disconnect(options) {
      return SyncDisconnectInternal.disconnect(options);
    }
};
