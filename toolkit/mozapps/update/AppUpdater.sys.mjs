/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";
import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  UpdateLog: "resource://gre/modules/UpdateLog.sys.mjs",
  UpdateUtils: "resource://gre/modules/UpdateUtils.sys.mjs",
});

const PREF_APP_UPDATE_CANCELATIONS_OSX = "app.update.cancelations.osx";
const PREF_APP_UPDATE_ELEVATE_NEVER = "app.update.elevate.never";

class AbortError extends Error {
  constructor(...params) {
    super(...params);
    this.name = this.constructor.name;
  }
}

/**
 * `AbortablePromise`s automatically add themselves to this set on construction
 * and remove themselves when they settle.
 */
var gPendingAbortablePromises = new Set();

/**
 * Creates a Promise that can be resolved immediately with an abort method.
 *
 * Note that the underlying Promise will probably still run to completion since
 * there isn't any general way to abort Promises. So if it is possible to abort
 * the operation instead or in addition to using this class, that is preferable.
 */
class AbortablePromise {
  #abortFn;
  #promise;
  #hasCompleted = false;

  constructor(promise) {
    let abortPromise = new Promise((resolve, reject) => {
      this.#abortFn = () => reject(new AbortError());
    });
    this.#promise = Promise.race([promise, abortPromise]);
    this.#promise = this.#promise.finally(() => {
      this.#hasCompleted = true;
      gPendingAbortablePromises.delete(this);
    });
    gPendingAbortablePromises.add(this);
  }

  abort() {
    if (this.#hasCompleted) {
      return;
    }
    this.#abortFn();
  }

  /**
   * This can be `await`ed on to get the result of the `AbortablePromise`. It
   * will resolve with the value that the Promise provided to the constructor
   * resolves with.
   */
  get promise() {
    return this.#promise;
  }

  /**
   * Will be `true` if the Promise provided to the constructor has resolved or
   * `abort()` has been called. Otherwise `false`.
   */
  get hasCompleted() {
    return this.#hasCompleted;
  }
}

function makeAbortable(promise) {
  let abortable = new AbortablePromise(promise);
  return abortable.promise;
}

function abortAllPromises() {
  for (const promise of gPendingAbortablePromises) {
    promise.abort();
  }
}

/**
 * This class checks for app updates in the foreground.  It has several public
 * methods for checking for updates, downloading updates, stopping the current
 * update, and getting the current update status.  It can also register
 * listeners that will be called back as different stages of updates occur.
 */
export class AppUpdater {
  #listeners = new Set();
  #status = AppUpdater.STATUS.NEVER_CHECKED;
  // This will basically be set to `true` when `AppUpdater.check` is called and
  // back to `false` right before it returns.
  // It is also set to `true` during an update swap and back to `false` when the
  // swap completes.
  #updateBusy = false;
  // When settings require that the user be asked for permission to download
  // updates and we have an update to download, we will assign a function.
  // Calling this function allows the download to proceed.
  #permissionToDownloadGivenFn = null;
  #_update = null;
  // Keeps track of if we have an `update-swap` listener connected. We only
  // connect it when the status is `READY_TO_RESTART`, but we can't use that to
  // tell if its connected because we might be in the middle of an update swap
  // in which case the status will have temporarily changed.
  #swapListenerConnected = false;

  constructor() {
    try {
      this.QueryInterface = ChromeUtils.generateQI([
        "nsIObserver",
        "nsISupportsWeakReference",
      ]);
    } catch (e) {
      this.#onException(e);
    }
  }

  #onException(exception) {
    try {
      this.#update = null;

      if (this.#swapListenerConnected) {
        LOG("AppUpdater:#onException - Removing update-swap listener");
        Services.obs.removeObserver(this, "update-swap");
        this.#swapListenerConnected = false;
      }

      if (exception instanceof AbortError) {
        // This should be where we end up if `AppUpdater.stop()` is called while
        // `AppUpdater.check` is running or during an update swap.
        LOG(
          "AppUpdater:#onException - Caught AbortError. Setting status " +
            "NEVER_CHECKED"
        );
        this.#setStatus(AppUpdater.STATUS.NEVER_CHECKED);
      } else {
        LOG(
          "AppUpdater:#onException - Exception caught. Setting status " +
            "INTERNAL_ERROR"
        );
        console.error(exception);
        this.#setStatus(AppUpdater.STATUS.INTERNAL_ERROR);
      }
    } catch (e) {
      LOG(
        "AppUpdater:#onException - Caught additional exception while " +
          "handling previous exception"
      );
      console.error(e);
    }
  }

  /**
   * This can be accessed by consumers to inspect the update that is being
   * prepared for installation. It will always be null if `AppUpdater.check`
   * hasn't been called yet. `AppUpdater.check` will set it to an instance of
   * nsIUpdate once there is one available. This may be immediate, if an update
   * is already downloading or has been downloaded. It may be delayed if an
   * update check needs to be performed first. It also may remain null if the
   * browser is up to date or if the update check fails.
   *
   * Regarding the difference between `AppUpdater.update`, `AppUpdater.#update`,
   * and `AppUpdater.#_update`:
   *  - `AppUpdater.update` and `AppUpdater.#update` are effectively identical
   *    except that `AppUpdater.update` is readonly since it should not be
   *    changed from outside this class.
   *  - `AppUpdater.#_update` should only ever be modified by the setter for
   *    `AppUpdater.#update` in order to ensure that the "foregroundDownload"
   *    property is set on assignment.
   * The quick and easy rule for using these is to always use `#update`
   * internally and (of course) always use `update` externally.
   */
  get update() {
    return this.#update;
  }

  get #update() {
    return this.#_update;
  }

  set #update(update) {
    this.#_update = update;
    if (this.#_update) {
      this.#_update.QueryInterface(Ci.nsIWritablePropertyBag);
      this.#_update.setProperty("foregroundDownload", "true");
    }
  }

  /**
   * The main entry point for checking for updates.  As different stages of the
   * check and possible subsequent update occur, the updater's status is set and
   * listeners are called.
   *
   * Note that this is the correct entry point, regardless of the current state
   * of the updater. Although the function name suggests that this function will
   * start an update check, it will only do that if we aren't already in the
   * update process. Otherwise, it will simply monitor the update process,
   * update its own status, and call listeners.
   *
   * This function is async and will resolve when the update is ready to
   * install, or a failure state is reached.
   * However, most callers probably don't actually want to track its progress by
   * awaiting on this function. More likely, it is desired to kick this function
   * off without awaiting and add a listener via addListener. This allows the
   * caller to see when the updater is checking for an update, downloading it,
   * etc rather than just knowing "now it's running" and "now it's done".
   *
   * Note that calling this function while this instance is already performing
   * or monitoring an update check/download will have no effect. In other words,
   * it is only really necessary/useful to call this function when the status is
   * `NEVER_CHECKED` or `NO_UPDATES_FOUND`.
   */
  async check() {
    try {
      // We don't want to end up with multiple instances of the same `async`
      // functions waiting on the same events, so if we are already busy going
      // through the update state, don't enter this function. This must not
      // be in the try/catch that sets #updateBusy to false in its finally
      // block.
      if (this.#updateBusy) {
        return;
      }
    } catch (e) {
      this.#onException(e);
    }

    try {
      this.#updateBusy = true;
      this.#update = null;

      if (this.#swapListenerConnected) {
        LOG("AppUpdater:check - Removing update-swap listener");
        Services.obs.removeObserver(this, "update-swap");
        this.#swapListenerConnected = false;
      }

      if (!AppConstants.MOZ_UPDATER || this.#updateDisabledByPackage) {
        LOG(
          "AppUpdater:check -" +
            "AppConstants.MOZ_UPDATER=" +
            AppConstants.MOZ_UPDATER +
            "this.#updateDisabledByPackage: " +
            this.#updateDisabledByPackage
        );
        this.#setStatus(AppUpdater.STATUS.NO_UPDATER);
        return;
      }

      if (this.aus.disabled) {
        LOG("AppUpdater:check - AUS disabled");
        this.#setStatus(AppUpdater.STATUS.UPDATE_DISABLED_BY_POLICY);
        return;
      }

      let updateState = this.aus.currentState;
      let stateName = this.aus.getStateName(updateState);
      LOG(`AppUpdater:check - currentState=${stateName}`);

      if (updateState == Ci.nsIApplicationUpdateService.STATE_PENDING) {
        LOG("AppUpdater:check - ready for restart");
        this.#onReadyToRestart();
        return;
      }

      if (this.aus.isOtherInstanceHandlingUpdates) {
        LOG("AppUpdater:check - this.aus.isOtherInstanceHandlingUpdates");
        this.#setStatus(AppUpdater.STATUS.OTHER_INSTANCE_HANDLING_UPDATES);
        return;
      }

      if (updateState == Ci.nsIApplicationUpdateService.STATE_DOWNLOADING) {
        LOG("AppUpdater:check - downloading");
        this.#update = this.um.downloadingUpdate;
        await this.#downloadUpdate();
        return;
      }

      if (updateState == Ci.nsIApplicationUpdateService.STATE_STAGING) {
        LOG("AppUpdater:check - staging");
        this.#update = this.um.readyUpdate;
        await this.#awaitStagingComplete();
        return;
      }

      // Clear prefs that could prevent a user from discovering available updates.
      if (Services.prefs.prefHasUserValue(PREF_APP_UPDATE_CANCELATIONS_OSX)) {
        Services.prefs.clearUserPref(PREF_APP_UPDATE_CANCELATIONS_OSX);
      }
      if (Services.prefs.prefHasUserValue(PREF_APP_UPDATE_ELEVATE_NEVER)) {
        Services.prefs.clearUserPref(PREF_APP_UPDATE_ELEVATE_NEVER);
      }
      this.#setStatus(AppUpdater.STATUS.CHECKING);
      LOG("AppUpdater:check - starting update check");
      let check = this.checker.checkForUpdates(this.checker.FOREGROUND_CHECK);
      let result;
      try {
        result = await makeAbortable(check.result);
      } catch (e) {
        // If we are aborting, stop the update check on our way out.
        if (e instanceof AbortError) {
          this.checker.stopCheck(check.id);
        }
        throw e;
      }

      if (!result.checksAllowed) {
        // This shouldn't happen. The cases where this can happen should be
        // handled specifically, above.
        LOG("AppUpdater:check - !checksAllowed; INTERNAL_ERROR");
        this.#setStatus(AppUpdater.STATUS.INTERNAL_ERROR);
        return;
      }

      if (!result.succeeded) {
        LOG("AppUpdater:check - Update check failed; CHECKING_FAILED");
        this.#setStatus(AppUpdater.STATUS.CHECKING_FAILED);
        return;
      }

      LOG("AppUpdater:check - Update check succeeded");
      this.#update = this.aus.selectUpdate(result.updates);
      if (!this.#update) {
        LOG("AppUpdater:check - result: NO_UPDATES_FOUND");
        this.#setStatus(AppUpdater.STATUS.NO_UPDATES_FOUND);
        return;
      }

      if (this.#update.unsupported) {
        LOG("AppUpdater:check - result: UNSUPPORTED SYSTEM");
        this.#setStatus(AppUpdater.STATUS.UNSUPPORTED_SYSTEM);
        return;
      }

      if (!this.aus.canApplyUpdates) {
        LOG("AppUpdater:check - result: MANUAL_UPDATE");
        this.#setStatus(AppUpdater.STATUS.MANUAL_UPDATE);
        return;
      }

      let updateAuto = await makeAbortable(
        lazy.UpdateUtils.getAppUpdateAutoEnabled()
      );
      if (!updateAuto || this.aus.manualUpdateOnly) {
        LOG(
          "AppUpdater:check - Need to wait for user approval to start the " +
            "download."
        );

        let downloadPermissionPromise = new Promise(resolve => {
          this.#permissionToDownloadGivenFn = resolve;
        });
        // There are other interfaces through which the user can start the
        // download, so we want to listen both for permission, and for the
        // download to independently start.
        let downloadStartPromise = Promise.race([
          downloadPermissionPromise,
          this.aus.stateTransition,
        ]);

        this.#setStatus(AppUpdater.STATUS.DOWNLOAD_AND_INSTALL);

        await makeAbortable(downloadStartPromise);
        LOG("AppUpdater:check - Got user approval. Proceeding with download");
        // If we resolved because of `aus.stateTransition`, we may actually be
        // downloading a different update now.
        if (this.um.downloadingUpdate) {
          this.#update = this.um.downloadingUpdate;
        }
      } else {
        LOG(
          "AppUpdater:check - updateAuto is active and " +
            "manualUpdateOnlydateOnly is inactive. Start the download."
        );
      }
      await this.#downloadUpdate();
    } catch (e) {
      this.#onException(e);
    } finally {
      this.#updateBusy = false;
    }
  }

  /**
   * This only has an effect if the status is `DOWNLOAD_AND_INSTALL`.This
   * indicates that the user has configured Firefox not to download updates
   * without permission, and we are waiting the user's permission.
   * This function should be called if and only if the user's permission was
   * given as it will allow the update download to proceed.
   */
  allowUpdateDownload() {
    if (this.#permissionToDownloadGivenFn) {
      this.#permissionToDownloadGivenFn();
    }
  }

  // true if updating is disabled because we're running in an app package.
  // This is distinct from aus.disabled because we need to avoid
  // messages being shown to the user about an "administrator" handling
  // updates; packaged apps may be getting updated by an administrator or they
  // may not be, and we don't have a good way to tell the difference from here,
  // so we err to the side of less confusion for unmanaged users.
  get #updateDisabledByPackage() {
    return Services.sysinfo.getProperty("isPackagedApp");
  }

  // true when updating in background is enabled.
  get #updateStagingEnabled() {
    LOG(
      "AppUpdater:#updateStagingEnabled" +
        "canStageUpdates: " +
        this.aus.canStageUpdates
    );
    return (
      !this.aus.disabled &&
      !this.#updateDisabledByPackage &&
      this.aus.canStageUpdates
    );
  }

  /**
   * Downloads an update mar or connects to an in-progress download.
   * Doesn't resolve until the update is ready to install, or a failure state
   * is reached.
   */
  async #downloadUpdate() {
    this.#setStatus(AppUpdater.STATUS.DOWNLOADING);

    let success = await this.aus.downloadUpdate(this.#update, false);
    if (!success) {
      LOG("AppUpdater:#downloadUpdate - downloadUpdate failed.");
      this.#setStatus(AppUpdater.STATUS.DOWNLOAD_FAILED);
      return;
    }

    await this.#awaitDownloadComplete();
  }

  /**
   * Listens for a download to complete.
   * Doesn't resolve until the update is ready to install, or a failure state
   * is reached.
   */
  async #awaitDownloadComplete() {
    let updateState = this.aus.currentState;
    if (
      updateState != Ci.nsIApplicationUpdateService.STATE_DOWNLOADING &&
      updateState != Ci.nsIApplicationUpdateService.STATE_SWAP
    ) {
      throw new Error(
        "AppUpdater:#awaitDownloadComplete invoked in unexpected state: " +
          this.aus.getStateName(updateState)
      );
    }

    // We may already be in the `DOWNLOADING` state, depending on how we entered
    // this function. But we actually want to alert the listeners again even if
    // we are because `this.update.selectedPatch` is null early in the
    // downloading state, but it should be set by now and listeners may want to
    // update UI based on that.
    this.#setStatus(AppUpdater.STATUS.DOWNLOADING);

    const updateDownloadProgress = (progress, progressMax) => {
      this.#setStatus(AppUpdater.STATUS.DOWNLOADING, progress, progressMax);
    };

    const progressObserver = {
      onStartRequest(aRequest) {
        LOG(
          `AppUpdater:#awaitDownloadComplete.observer.onStartRequest - ` +
            `aRequest: ${aRequest}`
        );
      },

      onStatus(aRequest, aStatus, aStatusArg) {
        LOG(
          `AppUpdater:#awaitDownloadComplete.observer.onStatus ` +
            `- aRequest: ${aRequest}, aStatus: ${aStatus}, ` +
            `aStatusArg: ${aStatusArg}`
        );
      },

      onProgress(aRequest, aProgress, aProgressMax) {
        LOG(
          `AppUpdater:#awaitDownloadComplete.observer.onProgress ` +
            `- aRequest: ${aRequest}, aProgress: ${aProgress}, ` +
            `aProgressMax: ${aProgressMax}`
        );
        updateDownloadProgress(aProgress, aProgressMax);
      },

      onStopRequest(aRequest, aStatusCode) {
        LOG(
          `AppUpdater:#awaitDownloadComplete.observer.onStopRequest ` +
            `- aRequest: ${aRequest}, aStatusCode: ${aStatusCode}`
        );
      },

      QueryInterface: ChromeUtils.generateQI([
        "nsIProgressEventSink",
        "nsIRequestObserver",
      ]),
    };

    let listenForProgress =
      updateState == Ci.nsIApplicationUpdateService.STATE_DOWNLOADING;

    if (listenForProgress) {
      this.aus.addDownloadListener(progressObserver);
      LOG("AppUpdater:#awaitDownloadComplete - Registered download listener");
    }

    LOG("AppUpdater:#awaitDownloadComplete - Waiting for state transition.");
    try {
      await makeAbortable(this.aus.stateTransition);
    } finally {
      if (listenForProgress) {
        this.aus.removeDownloadListener(progressObserver);
        LOG("AppUpdater:#awaitDownloadComplete - Download listener removed");
      }
    }

    updateState = this.aus.currentState;
    LOG(
      "AppUpdater:#awaitDownloadComplete - State transition seen. New state: " +
        this.aus.getStateName(updateState)
    );

    switch (updateState) {
      case Ci.nsIApplicationUpdateService.STATE_IDLE:
        LOG(
          "AppUpdater:#awaitDownloadComplete - Setting status DOWNLOAD_FAILED."
        );
        this.#setStatus(AppUpdater.STATUS.DOWNLOAD_FAILED);
        break;
      case Ci.nsIApplicationUpdateService.STATE_STAGING:
        LOG("AppUpdater:#awaitDownloadComplete - awaiting staging completion.");
        await this.#awaitStagingComplete();
        break;
      case Ci.nsIApplicationUpdateService.STATE_PENDING:
        LOG("AppUpdater:#awaitDownloadComplete - ready to restart.");
        this.#onReadyToRestart();
        break;
      default:
        LOG(
          "AppUpdater:#awaitDownloadComplete - Setting status INTERNAL_ERROR."
        );
        this.#setStatus(AppUpdater.STATUS.INTERNAL_ERROR);
        break;
    }
  }

  /**
   * This function registers an observer that watches for the staging process
   * to complete. Once it does, it sets the status to either request that the
   * user restarts to install the update on success, request that the user
   * manually download and install the newer version, or automatically download
   * a complete update if applicable.
   * Doesn't resolve until the update is ready to install, or a failure state
   * is reached.
   */
  async #awaitStagingComplete() {
    let updateState = this.aus.currentState;
    if (updateState != Ci.nsIApplicationUpdateService.STATE_STAGING) {
      throw new Error(
        "AppUpdater:#awaitStagingComplete invoked in unexpected state: " +
          this.aus.getStateName(updateState)
      );
    }

    LOG("AppUpdater:#awaitStagingComplete - Setting status STAGING.");
    this.#setStatus(AppUpdater.STATUS.STAGING);

    LOG("AppUpdater:#awaitStagingComplete - Waiting for state transition.");
    await makeAbortable(this.aus.stateTransition);

    updateState = this.aus.currentState;
    LOG(
      "AppUpdater:#awaitStagingComplete - State transition seen. New state: " +
        this.aus.getStateName(updateState)
    );

    switch (updateState) {
      case Ci.nsIApplicationUpdateService.STATE_PENDING:
        LOG("AppUpdater:#awaitStagingComplete - ready for restart");
        this.#onReadyToRestart();
        break;
      case Ci.nsIApplicationUpdateService.STATE_IDLE:
        LOG("AppUpdater:#awaitStagingComplete - DOWNLOAD_FAILED");
        this.#setStatus(AppUpdater.STATUS.DOWNLOAD_FAILED);
        break;
      case Ci.nsIApplicationUpdateService.STATE_DOWNLOADING:
        // We've fallen back to downloading the complete update because the
        // partial update failed to be staged. Return to the downloading stage.
        LOG(
          "AppUpdater:#awaitStagingComplete - Partial update must have " +
            "failed to stage. Downloading complete update."
        );
        await this.#awaitDownloadComplete();
        break;
      default:
        LOG(
          "AppUpdater:#awaitStagingComplete - Setting status INTERNAL_ERROR."
        );
        this.#setStatus(AppUpdater.STATUS.INTERNAL_ERROR);
        break;
    }
  }

  #onReadyToRestart() {
    let updateState = this.aus.currentState;
    if (updateState != Ci.nsIApplicationUpdateService.STATE_PENDING) {
      throw new Error(
        "AppUpdater:#onReadyToRestart invoked in unexpected state: " +
          this.aus.getStateName(updateState)
      );
    }

    LOG("AppUpdater:#onReadyToRestart - Setting status READY_FOR_RESTART.");
    if (this.#swapListenerConnected) {
      LOG(
        "AppUpdater:#onReadyToRestart - update-swap listener already attached"
      );
    } else {
      this.#swapListenerConnected = true;
      LOG("AppUpdater:#onReadyToRestart - Attaching update-swap listener");
      Services.obs.addObserver(this, "update-swap", /* ownsWeak */ true);
    }
    this.#setStatus(AppUpdater.STATUS.READY_FOR_RESTART);
  }

  /**
   * Stops the current check for updates and any ongoing download.
   *
   * If this is called before `AppUpdater.check()` is called or after it
   * resolves, this should have no effect. If this is called while `check()` is
   * still running, `AppUpdater` will return to the NEVER_CHECKED state. We
   * don't really want to leave it in any of the intermediary states after we
   * have disconnected all the listeners that would allow those states to ever
   * change.
   */
  stop() {
    LOG("AppUpdater:stop called");
    if (this.#swapListenerConnected) {
      LOG("AppUpdater:stop - Removing update-swap listener");
      Services.obs.removeObserver(this, "update-swap");
      this.#swapListenerConnected = false;
    }
    abortAllPromises();
  }

  /**
   * {AppUpdater.STATUS} The status of the current check or update.
   *
   * Note that until AppUpdater.check has been called, this will always be set
   * to NEVER_CHECKED.
   */
  get status() {
    return this.#status;
  }

  /**
   * Adds a listener function that will be called back on status changes as
   * different stages of updates occur.  The function will be called without
   * arguments for most status changes; see the comments around the STATUS value
   * definitions below.  This is safe to call multiple times with the same
   * function.  It will be added only once.
   *
   * @param {function} listener
   *   The listener function to add.
   */
  addListener(listener) {
    this.#listeners.add(listener);
  }

  /**
   * Removes a listener.  This is safe to call multiple times with the same
   * function, or with a function that was never added.
   *
   * @param {function} listener
   *   The listener function to remove.
   */
  removeListener(listener) {
    this.#listeners.delete(listener);
  }

  /**
   * Sets the updater's current status and calls listeners.
   *
   * @param {AppUpdater.STATUS} status
   *   The new updater status.
   * @param {*} listenerArgs
   *   Arguments to pass to listeners.
   */
  #setStatus(status, ...listenerArgs) {
    this.#status = status;
    for (let listener of this.#listeners) {
      listener(status, ...listenerArgs);
    }
    return status;
  }

  observe(subject, topic, status) {
    LOG(
      "AppUpdater:observe " +
        "- subject: " +
        subject +
        ", topic: " +
        topic +
        ", status: " +
        status
    );
    switch (topic) {
      case "update-swap":
        // This is asynchronous, but we don't really want to wait for it in this
        // observer.
        this.#handleUpdateSwap();
        break;
    }
  }

  async #handleUpdateSwap() {
    try {
      // This must not be in the try/catch that sets #updateBusy to `false` in
      // its finally block.
      // There really shouldn't be any way to enter this function when
      // `#updateBusy` is `true`. But let's just be safe because we don't want
      // to ever end up with two things running at once.
      if (this.#updateBusy) {
        return;
      }
    } catch (e) {
      this.#onException(e);
    }

    try {
      this.#updateBusy = true;

      // During an update swap, the new update will initially be stored in
      // `downloadingUpdate`. Part way through, it will be moved into
      // `readyUpdate` and `downloadingUpdate` will be set to `null`.
      this.#update = this.um.downloadingUpdate;
      if (!this.#update) {
        this.#update = this.um.readyUpdate;
      }

      await this.#awaitDownloadComplete();
    } catch (e) {
      this.#onException(e);
    } finally {
      this.#updateBusy = false;
    }
  }
}

XPCOMUtils.defineLazyServiceGetter(
  AppUpdater.prototype,
  "aus",
  "@mozilla.org/updates/update-service;1",
  "nsIApplicationUpdateService"
);
XPCOMUtils.defineLazyServiceGetter(
  AppUpdater.prototype,
  "checker",
  "@mozilla.org/updates/update-checker;1",
  "nsIUpdateChecker"
);
XPCOMUtils.defineLazyServiceGetter(
  AppUpdater.prototype,
  "um",
  "@mozilla.org/updates/update-manager;1",
  "nsIUpdateManager"
);

AppUpdater.STATUS = {
  // Updates are allowed and there's no downloaded or staged update, but the
  // AppUpdater hasn't checked for updates yet, so it doesn't know more than
  // that.
  NEVER_CHECKED: 0,

  // The updater isn't available (AppConstants.MOZ_UPDATER is falsey).
  NO_UPDATER: 1,

  // "appUpdate" is not allowed by policy.
  UPDATE_DISABLED_BY_POLICY: 2,

  // Another app instance is handling updates.
  OTHER_INSTANCE_HANDLING_UPDATES: 3,

  // There's an update, but it's not supported on this system.
  UNSUPPORTED_SYSTEM: 4,

  // The user must apply updates manually.
  MANUAL_UPDATE: 5,

  // The AppUpdater is checking for updates.
  CHECKING: 6,

  // The AppUpdater checked for updates and none were found.
  NO_UPDATES_FOUND: 7,

  // The AppUpdater is downloading an update.  Listeners are notified of this
  // status as a download starts.  They are also notified on download progress,
  // and in that case they are passed two arguments: the current download
  // progress and the total download size.
  DOWNLOADING: 8,

  // The AppUpdater tried to download an update but it failed.
  DOWNLOAD_FAILED: 9,

  // There's an update available, but the user wants us to ask them to download
  // and install it.
  DOWNLOAD_AND_INSTALL: 10,

  // An update is staging.
  STAGING: 11,

  // An update is downloaded and staged and will be applied on restart.
  READY_FOR_RESTART: 12,

  // Essential components of the updater are failing and preventing us from
  // updating.
  INTERNAL_ERROR: 13,

  // Failed to check for updates, network timeout, dns errors could cause this
  CHECKING_FAILED: 14,

  /**
   * Is the given `status` a terminal state in the update state machine?
   *
   * A terminal state means that the `check()` method has completed.
   *
   * N.b.: `DOWNLOAD_AND_INSTALL` is not considered terminal because the normal
   * flow is that Firefox will show UI prompting the user to install, and when
   * the user interacts, the `check()` method will continue through the update
   * state machine.
   *
   * @returns {boolean} `true` if `status` is terminal.
   */
  isTerminalStatus(status) {
    return ![
      AppUpdater.STATUS.CHECKING,
      AppUpdater.STATUS.DOWNLOAD_AND_INSTALL,
      AppUpdater.STATUS.DOWNLOADING,
      AppUpdater.STATUS.NEVER_CHECKED,
      AppUpdater.STATUS.STAGING,
    ].includes(status);
  },

  /**
   * Turn the given `status` into a string for debugging.
   *
   * @returns {?string} representation of given numerical `status`.
   */
  debugStringFor(status) {
    for (let [k, v] of Object.entries(AppUpdater.STATUS)) {
      if (v == status) {
        return k;
      }
    }
    return null;
  },
};

/**
 * Logs a string to the error console. If enabled, also logs to the update
 * messages file.
 * @param   string
 *          The string to write to the error console.
 */
function LOG(string) {
  lazy.UpdateLog.logPrefixedString("AUS:AUM", string);
}
