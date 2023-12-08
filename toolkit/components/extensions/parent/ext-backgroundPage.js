/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { ExtensionParent } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionParent.sys.mjs"
);
var {
  HiddenExtensionPage,
  promiseBackgroundViewLoaded,
  watchExtensionWorkerContextLoaded,
} = ExtensionParent;

ChromeUtils.defineESModuleGetters(this, {
  ExtensionTelemetry: "resource://gre/modules/ExtensionTelemetry.sys.mjs",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
});

ChromeUtils.defineLazyGetter(this, "serviceWorkerManager", () => {
  return Cc["@mozilla.org/serviceworkers/manager;1"].getService(
    Ci.nsIServiceWorkerManager
  );
});

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "backgroundIdleTimeout",
  "extensions.background.idle.timeout",
  30000,
  null,
  // Minimum 100ms, max 5min
  delay => Math.min(Math.max(delay, 100), 5 * 60 * 1000)
);

// Pref used in tests to assert background page state set to
// stopped on an extension process crash.
XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "disableRestartPersistentAfterCrash",
  "extensions.background.disableRestartPersistentAfterCrash",
  false
);

// eslint-disable-next-line mozilla/reject-importGlobalProperties
Cu.importGlobalProperties(["DOMException"]);

function notifyBackgroundScriptStatus(addonId, isRunning) {
  // Notify devtools when the background scripts is started or stopped
  // (used to show the current status in about:debugging).
  const subject = { addonId, isRunning };
  Services.obs.notifyObservers(subject, "extension:background-script-status");
}

// Same as nsITelemetry msSinceProcessStartExcludingSuspend but returns
// undefined instead of throwing an extension.
function msSinceProcessStartExcludingSuspend() {
  let now;
  try {
    now = Services.telemetry.msSinceProcessStartExcludingSuspend();
  } catch (err) {
    Cu.reportError(err);
  }
  return now;
}

/**
 * Background Page state transitions:
 *
 *    ------> STOPPED <-------
 *    |         |            |
 *    |         v            |
 *    |      STARTING >------|
 *    |         |            |
 *    |         v            ^
 *    |----< RUNNING ----> SUSPENDING
 *              ^            v
 *              |------------|
 *
 * STARTING:   The background is being built.
 * RUNNING:    The background is running.
 * SUSPENDING: The background is suspending, runtime.onSuspend will be called.
 * STOPPED:    The background is not running.
 *
 * For persistent backgrounds, SUSPENDING is not used.
 *
 * See BackgroundContextOwner for the exact relation.
 */
const BACKGROUND_STATE = {
  STARTING: "starting",
  RUNNING: "running",
  SUSPENDING: "suspending",
  STOPPED: "stopped",
};

// Responsible for the background_page section of the manifest.
class BackgroundPage extends HiddenExtensionPage {
  constructor(extension, options) {
    super(extension, "background");

    this.page = options.page || null;
    this.isGenerated = !!options.scripts;

    // Last background/event page created time (retrieved using
    // Services.telemetry.msSinceProcessStartExcludingSuspend when the
    // parent process proxy context has been created).
    this.msSinceCreated = null;

    if (this.page) {
      this.url = this.extension.baseURI.resolve(this.page);
    } else if (this.isGenerated) {
      this.url = this.extension.baseURI.resolve(
        "_generated_background_page.html"
      );
    }
  }

  async build() {
    const { extension } = this;
    ExtensionTelemetry.backgroundPageLoad.stopwatchStart(extension, this);

    let context;
    try {
      await this.createBrowserElement();
      if (!this.browser) {
        throw new Error(
          "Extension shut down before the background page was created"
        );
      }
      extension._backgroundPageFrameLoader = this.browser.frameLoader;

      extensions.emit("extension-browser-inserted", this.browser);

      let contextPromise = promiseBackgroundViewLoaded(this.browser);
      this.browser.fixupAndLoadURIString(this.url, {
        triggeringPrincipal: extension.principal,
      });

      context = await contextPromise;
      // NOTE: context can be null if the load failed.

      this.msSinceCreated = msSinceProcessStartExcludingSuspend();

      ExtensionTelemetry.backgroundPageLoad.stopwatchFinish(extension, this);
    } catch (e) {
      // Extension was down before the background page has loaded.
      ExtensionTelemetry.backgroundPageLoad.stopwatchCancel(extension, this);
      throw e;
    }

    return context;
  }

  shutdown() {
    this.extension._backgroundPageFrameLoader = null;
    super.shutdown();
  }
}

// Responsible for the background.service_worker section of the manifest.
class BackgroundWorker {
  constructor(extension, options) {
    this.extension = extension;
    this.workerScript = options.service_worker;

    if (!this.workerScript) {
      throw new Error("Missing mandatory background.service_worker property");
    }
  }

  get registrationInfo() {
    const { principal } = this.extension;
    return serviceWorkerManager.getRegistrationForAddonPrincipal(principal);
  }

  getWorkerInfo(descriptorId) {
    return this.registrationInfo?.getWorkerByID(descriptorId);
  }

  validateWorkerInfoForContext(context) {
    const { extension } = this;
    if (!this.getWorkerInfo(context.workerDescriptorId)) {
      throw new Error(
        `ServiceWorkerInfo not found for ${extension.policy.debugName} contextId ${context.contextId}`
      );
    }
  }

  async build() {
    const { extension } = this;
    let context;
    const contextPromise = new Promise(resolve => {
      // TODO bug 1844486: resolve and/or unwatch when startup is interrupted.
      let unwatch = watchExtensionWorkerContextLoaded(
        { extension, viewType: "background_worker" },
        context => {
          unwatch();
          this.validateWorkerInfoForContext(context);
          resolve(context);
        }
      );
    });

    // TODO(Bug 17228327): follow up to spawn the active worker for a previously installed
    // background service worker.
    await serviceWorkerManager.registerForAddonPrincipal(
      this.extension.principal
    );

    // TODO bug 1844486: Confirm that a shutdown() call during the above or
    // below `await` calls can interrupt build() without leaving a stray worker
    // registration behind.

    context = await contextPromise;

    await this.waitForActiveWorker();
    return context;
  }

  shutdown(isAppShutdown) {
    // All service worker registrations related to the extensions will be unregistered
    // - when the extension is shutting down if the application is not also shutting down
    //   shutdown (in which case a previously registered service worker is expected to stay
    //   active across browser restarts).
    // - when the extension has been uninstalled
    if (!isAppShutdown) {
      this.registrationInfo?.forceShutdown();
    }
  }

  waitForActiveWorker() {
    const { extension, registrationInfo } = this;
    return new Promise((resolve, reject) => {
      const resolveOnActive = () => {
        if (
          registrationInfo.activeWorker?.state ===
          Ci.nsIServiceWorkerInfo.STATE_ACTIVATED
        ) {
          resolve();
          return true;
        }
        return false;
      };

      const rejectOnUnregistered = () => {
        if (registrationInfo.unregistered) {
          reject(
            new Error(
              `Background service worker unregistered for "${extension.policy.debugName}"`
            )
          );
          return true;
        }
        return false;
      };

      if (resolveOnActive() || rejectOnUnregistered()) {
        return;
      }

      const listener = {
        onChange() {
          if (resolveOnActive() || rejectOnUnregistered()) {
            registrationInfo.removeListener(listener);
          }
        },
      };
      registrationInfo.addListener(listener);
    });
  }
}

/**
 * The BackgroundContextOwner is instantiated at most once per extension and
 * tracks the state of the background context. State changes can be triggered
 * by explicit calls to methods with the "setBgState" prefix, but also by the
 * background context itself, e.g. via an extension process crash.
 *
 * This class identifies the following stages of interest:
 *
 * 1. Initially no active background, waiting for a signal to get started.
 *    - method: none (at constructor and after setBgStateStopped)
 *    - state: STOPPED
 *    - context: null
 * 2. Parent-triggered background startup
 *    - method: setBgStateStarting
 *    - state: STARTING (was STOPPED)
 *    - context: null
 * 3. Background context creation observed in parent
 *    - method: none (observed by ExtensionParent's recvCreateProxyContext)
 *      TODO: add method to observe and keep track of it sooner than stage 4.
 *    - state: STARTING
 *    - context: ProxyContextParent subclass (was null)
 * 4. Parent-observed background startup completion
 *    - method: setBgStateRunning
 *    - state: RUNNING (was STARTING)
 *    - context: ProxyContextParent (was null)
 * 5. Background context unloaded for any reason
 *    - method: setBgStateStopped
 *      TODO bug 1844217: This is only implemented for process crashes and
 *          intentionally triggered terminations, not navigations/reloads.
 *          When unloads happen due to navigations/reloads, context will be
 *          null but the state will still be RUNNING.
 *    - state: STOPPED (was STOPPED, STARTING, RUNNING or SUSPENDING)
 *    - context: null (was ProxyContextParent if stage 4 ran).
 *    - Continue at stage 1 if the extension has not shut down yet.
 */
class BackgroundContextOwner {
  /**
   * @property {BackgroundBuilder} backgroundBuilder
   *
   * The source of parent-triggered background state changes.
   */
  backgroundBuilder;

  /**
   * @property {Extension} [extension]
   *
   * The Extension associated with the background. This is always set and
   * cleared at extension shutdown.
   */
  extension;

  /**
   * @property {BackgroundPage|BackgroundWorker} [bgInstance]
   *
   * The BackgroundClass instance responsible for creating the background
   * context. This is set as soon as there is a desire to start a background,
   * and cleared as soon as the background context is not wanted any more.
   *
   * This field is set iff extension.backgroundState is not STOPPED.
   */
  bgInstance = null;

  /**
   * @property {ExtensionPageContextParent|BackgroundWorkerContextParent} [context]
   *
   * The parent-side counterpart to a background context in a child. The value
   * is a subclass of ProxyContextParent, which manages its own lifetime. The
   * class is ultimately instantiated through bgInstance. It can be destroyed by
   * bgInstance or externally (e.g. by the context itself or a process crash).
   * The reference to the context is cleared as soon as the context is unloaded.
   *
   * This is currently set when the background has fully loaded. To access the
   * background context before that, use |extension.backgroundContext|.
   *
   * This field is set when extension.backgroundState is RUNNING or SUSPENDING.
   */
  context = null;

  /**
   * @property {boolean} [canBePrimed]
   *
   * This property reflects whether persistent listeners can be primed. This
   * means that `backgroundState` is `STOPPED` and the listeners haven't been
   * primed yet. It is initially `true`, and set to `false` as soon as
   * listeners are primed. It can become `true` again if `primeBackground` was
   * skipped due to `shouldPrimeBackground` being `false`.
   * NOTE: this flag is set for both event pages and persistent background pages.
   */
  canBePrimed = true;

  /**
   * @property {boolean} [shouldPrimeBackground]
   *
   * This property controls whether we should prime listeners. Under normal
   * conditions, this should always be `true` but when too many crashes have
   * occurred, we might have to disable process spawning, which would lead to
   * this property being set to `false`.
   */
  shouldPrimeBackground = true;

  get #hasEnteredShutdown() {
    // This getter is just a small helper to make sure we always check for
    // the extension shutdown being already initiated.
    // Ordinarily the extension object is expected to be nullified from the
    // onShutdown method, but extension.hasShutdown is set earlier and because
    // the shutdown goes through some async steps there is a chance for other
    // internals to be hit while the hasShutdown flag is set bug onShutdown
    // not hit yet.
    return this.extension.hasShutdown || Services.startup.shuttingDown;
  }

  /**
   * @param {BackgroundBuilder} backgroundBuilder
   * @param {Extension} extension
   */
  constructor(backgroundBuilder, extension) {
    this.backgroundBuilder = backgroundBuilder;
    this.extension = extension;
    this.onExtensionProcessCrashed = this.onExtensionProcessCrashed.bind(this);
    this.onApplicationInForeground = this.onApplicationInForeground.bind(this);
    this.onExtensionEnableProcessSpawning =
      this.onExtensionEnableProcessSpawning.bind(this);

    extension.backgroundState = BACKGROUND_STATE.STOPPED;

    extensions.on("extension-process-crash", this.onExtensionProcessCrashed);
    extensions.on(
      "extension-enable-process-spawning",
      this.onExtensionEnableProcessSpawning
    );
    // We only defer handling extension process crashes for persistent
    // background context.
    if (extension.persistentBackground) {
      extensions.on("application-foreground", this.onApplicationInForeground);
    }
  }

  /**
   * setBgStateStarting - right before the background context is initialized.
   *
   * @param {BackgroundWorker|BackgroundPage} bgInstance
   */
  setBgStateStarting(bgInstance) {
    if (!this.extension) {
      throw new Error(`Cannot start background after extension shutdown.`);
    }
    if (this.bgInstance) {
      throw new Error(`Cannot start multiple background instances`);
    }
    this.extension.backgroundState = BACKGROUND_STATE.STARTING;
    this.bgInstance = bgInstance;
    // Often already false, except if we're waking due to a listener that was
    // registered with isInStartup=true.
    this.canBePrimed = false;
  }

  /**
   * setBgStateRunning - when the background context has fully loaded.
   *
   * This method may throw if the background should no longer be active; if that
   * is the case, the caller should make sure that the background is cleaned up
   * by calling setBgStateStopped.
   *
   * @param {ExtensionPageContextParent|BackgroundWorkerContextParent} context
   */
  setBgStateRunning(context) {
    if (!this.extension) {
      // Caller should have checked this.
      throw new Error(`Extension has shut down before startup completion.`);
    }
    if (this.context) {
      // This can currently not happen - we set the context only once.
      // TODO bug 1844217: Handle navigation (bug 1286083). For now, reject.
      throw new Error(`Context already set before at startup completion.`);
    }
    if (!context) {
      throw new Error(`Context not found at startup completion.`);
    }
    if (context.unloaded) {
      throw new Error(`Context has unloaded before startup completion.`);
    }
    this.extension.backgroundState = BACKGROUND_STATE.RUNNING;
    this.context = context;
    context.callOnClose(this);

    // When the background startup completes successfully, update the set of
    // events that should be persisted.
    EventManager.clearPrimedListeners(this.extension, true);

    // This notification will be balanced in setBgStateStopped / close.
    notifyBackgroundScriptStatus(this.extension.id, true);

    this.extension.emit("background-script-started");
  }

  /**
   * setBgStateStopped - when the background context has unloaded or should be
   * unloaded. Regardless of the actual state at the entry of this method, upon
   * returning the background is considered stopped.
   *
   * If the context was active at the time of the invocation, the actual unload
   * of |this.context| is asynchronous as it may involve a round-trip to the
   * child process.
   *
   * @param {boolean} [isAppShutdown]
   */
  setBgStateStopped(isAppShutdown) {
    const backgroundState = this.extension.backgroundState;
    if (this.context) {
      this.context.forgetOnClose(this);
      this.context = null;
      // This is the counterpart to the notification in setBgStateRunning.
      notifyBackgroundScriptStatus(this.extension.id, false);
    }

    // We only need to call clearPrimedListeners for states STOPPED and STARTING
    // because setBgStateRunning clears all primed listeners when it switches
    // from STARTING to RUNNING. Further, the only way to get primed listeners
    // is by a primeListeners call, which only happens in the STOPPED state.
    if (
      backgroundState === BACKGROUND_STATE.STOPPED ||
      backgroundState === BACKGROUND_STATE.STARTING
    ) {
      EventManager.clearPrimedListeners(this.extension, false);
    }

    // Ensure there is no backgroundTimer running
    this.backgroundBuilder.clearIdleTimer();

    const bgInstance = this.bgInstance;
    if (bgInstance) {
      this.bgInstance = null;
      isAppShutdown ||= Services.startup.shuttingDown;
      // bgInstance.shutdown() unloads the associated context, if any.
      bgInstance.shutdown(isAppShutdown);
      this.backgroundBuilder.onBgInstanceShutdown(bgInstance);
    }

    this.extension.backgroundState = BACKGROUND_STATE.STOPPED;
    if (backgroundState === BACKGROUND_STATE.STARTING) {
      this.extension.emit("background-script-aborted");
    }

    if (this.extension.hasShutdown) {
      this.extension = null;
    } else if (this.shouldPrimeBackground) {
      // Prime again, so that a stopped background can always be revived when
      // needed.
      this.backgroundBuilder.primeBackground(false);
    } else {
      this.canBePrimed = true;
    }
  }

  // Called by registration via context.callOnClose (if this.context is set).
  close() {
    // close() is called when:
    // - background context unloads (without replacement context).
    // - extension process crashes (without replacement context).
    // - background context reloads (context likely replaced by new context).
    // - background context navigates (context likely replaced by new context).
    //
    // When the background is gone without replacement, switch to STOPPED.
    // TODO bug 1286083: Drop support for navigations.

    // To fully maintain the state, we should call this.setBgStateStopped();
    // But we cannot do that yet because that would close background pages upon
    // reload and navigation, which would be a backwards-incompatible change.
    // For now, we only do the bare minimum here.
    //
    // Note that once a navigation or reload starts, that the context is
    // untracked. This is a pre-existing issue that we should fix later.
    // TODO bug 1844217: Detect context replacement and update this.context.
    if (this.context) {
      this.context.forgetOnClose(this);
      this.context = null;
      // This is the counterpart to the notification in setBgStateRunning.
      notifyBackgroundScriptStatus(this.extension.id, false);
    }
  }

  restartPersistentBackgroundAfterCrash() {
    const { extension } = this;
    if (
      this.#hasEnteredShutdown ||
      // Ignore if the background state isn't the one expected to be set
      // after a crash.
      extension.backgroundState !== BACKGROUND_STATE.STOPPED ||
      // Auto-restart persistent background scripts after crash disabled by prefs.
      disableRestartPersistentAfterCrash
    ) {
      return;
    }

    // Persistent background pages are re-primed from setBgStateStopped when we
    // are hitting a crash (if the threshold was not exceeded, otherwise they
    // are going to be re-primed from onExtensionEnableProcessSpawning).
    extension.emit("start-background-script");
  }

  onExtensionEnableProcessSpawning() {
    if (this.#hasEnteredShutdown) {
      return;
    }

    if (!this.canBePrimed) {
      return;
    }

    // Allow priming again.
    this.shouldPrimeBackground = true;
    this.backgroundBuilder.primeBackground(false);

    if (this.extension.persistentBackground) {
      this.restartPersistentBackgroundAfterCrash();
    }
  }

  onApplicationInForeground(eventName, data) {
    if (
      this.#hasEnteredShutdown ||
      // Past the silent crash handling threashold.
      data.processSpawningDisabled
    ) {
      return;
    }

    this.restartPersistentBackgroundAfterCrash();
  }

  onExtensionProcessCrashed(eventName, data) {
    if (this.#hasEnteredShutdown) {
      return;
    }

    // data.childID holds the process ID of the crashed extension process.
    // For now, assume that there is only one, so clean up unconditionally.

    this.shouldPrimeBackground = !data.processSpawningDisabled;

    // We only need to clean up if a bgInstance has been created. Without it,
    // there is only state in the parent process, not the child, and a crashed
    // extension process doesn't affect us.
    if (this.bgInstance) {
      this.setBgStateStopped();
    }

    if (this.extension.persistentBackground) {
      // Defer to when back in foreground and/or process spawning is explicitly re-enabled.
      if (!data.appInForeground || data.processSpawningDisabled) {
        return;
      }

      this.restartPersistentBackgroundAfterCrash();
    }
  }

  // Called by ExtensionAPI.onShutdown (once).
  onShutdown(isAppShutdown) {
    // If a background context was active during extension shutdown, then
    // close() was called before onShutdown, which clears |this.extension|.
    // If the background has not fully started yet, then we have to clear here.
    if (this.extension) {
      this.setBgStateStopped(isAppShutdown);
    }
    extensions.off("extension-process-crash", this.onExtensionProcessCrashed);
    extensions.off(
      "extension-enable-process-spawning",
      this.onExtensionEnableProcessSpawning
    );
    extensions.off("application-foreground", this.onApplicationInForeground);
  }
}

/**
 * BackgroundBuilder manages the creation and parent-triggered termination of
 * the background context. Non-parent-triggered terminations are usually due to
 * an external cause (e.g. crashes) and detected by BackgroundContextOwner.
 *
 * Because these external terminations can happen at any time, and the creation
 * and suspension of the background context is async, the methods of this
 * BackgroundBuilder class necessarily need to check the state of the background
 * before proceeding with the operation (and abort + clean up as needed).
 *
 * The following interruptions are explicitly accounted for:
 * - Extension shuts down.
 * - Background unloads for any reason.
 * - Another background instance starts in the meantime.
 */
class BackgroundBuilder {
  constructor(extension) {
    this.extension = extension;
    this.backgroundContextOwner = new BackgroundContextOwner(this, extension);
  }

  async build() {
    if (this.backgroundContextOwner.bgInstance) {
      return;
    }

    let { extension } = this;
    let { manifest } = extension;
    extension.backgroundState = BACKGROUND_STATE.STARTING;

    this.isWorker =
      !!manifest.background.service_worker &&
      WebExtensionPolicy.backgroundServiceWorkerEnabled;

    let BackgroundClass = this.isWorker ? BackgroundWorker : BackgroundPage;

    const bgInstance = new BackgroundClass(extension, manifest.background);
    this.backgroundContextOwner.setBgStateStarting(bgInstance);
    let context;
    try {
      context = await bgInstance.build();
    } catch (e) {
      Cu.reportError(e);
      // If background startup gets interrupted (e.g. extension shutdown),
      // bgInstance.shutdown() is called and backgroundContextOwner.bgInstance
      // is cleared.
      if (this.backgroundContextOwner.bgInstance === bgInstance) {
        this.backgroundContextOwner.setBgStateStopped();
      }
      return;
    }

    if (context) {
      // Wait until all event listeners registered by the script so far
      // to be handled. We then set listenerPromises to null, which indicates
      // to addListener that the background script has finished loading.
      await Promise.all(context.listenerPromises);
      context.listenerPromises = null;
    }

    if (this.backgroundContextOwner.bgInstance !== bgInstance) {
      // Background closed/restarted in the meantime.
      return;
    }

    try {
      this.backgroundContextOwner.setBgStateRunning(context);
    } catch (e) {
      Cu.reportError(e);
      this.backgroundContextOwner.setBgStateStopped();
    }
  }

  observe(subject, topic, data) {
    if (topic == "timer-callback") {
      let { extension } = this;
      this.clearIdleTimer();
      extension?.terminateBackground();
    }
  }

  clearIdleTimer() {
    this.backgroundTimer?.cancel();
    this.backgroundTimer = null;
  }

  resetIdleTimer() {
    this.clearIdleTimer();
    let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    timer.init(this, backgroundIdleTimeout, Ci.nsITimer.TYPE_ONE_SHOT);
    this.backgroundTimer = timer;
  }

  primeBackground(isInStartup = true) {
    let { extension } = this;

    if (this.backgroundContextOwner.bgInstance) {
      // This should never happen. The need to prime listeners is mutually
      // exclusive with the existence of a background instance.
      throw new Error(`bgInstance exists before priming ${extension.id}`);
    }

    // Used by runtime messaging to wait for background page listeners.
    let bgStartupPromise = new Promise(resolve => {
      let done = () => {
        extension.off("background-script-started", done);
        extension.off("background-script-aborted", done);
        extension.off("shutdown", done);
        resolve();
      };
      extension.on("background-script-started", done);
      extension.on("background-script-aborted", done);
      extension.on("shutdown", done);
    });

    extension.promiseBackgroundStarted = () => {
      return bgStartupPromise;
    };

    extension.wakeupBackground = () => {
      if (extension.hasShutdown) {
        return Promise.reject(
          new Error(
            "wakeupBackground called while the extension was already shutting down"
          )
        );
      }
      extension.emit("background-script-event");
      // `extension.wakeupBackground` is set back to the original arrow function
      // when the background page is terminated and `primeBackground` is called again.
      extension.wakeupBackground = () => bgStartupPromise;
      return bgStartupPromise;
    };

    let resetBackgroundIdle = (eventName, resetIdleDetails) => {
      this.clearIdleTimer();
      if (!this.extension || extension.persistentBackground) {
        // Extension was already shut down or is persistent and
        // does not idle timout.
        return;
      }
      // TODO remove at an appropriate point in the future prior
      // to general availability.  There may be some racy conditions
      // with idle timeout between an event starting and the event firing
      // but we still want testing with an idle timeout.
      if (
        !Services.prefs.getBoolPref("extensions.background.idle.enabled", true)
      ) {
        return;
      }

      if (
        extension.backgroundState == BACKGROUND_STATE.SUSPENDING &&
        // After we begin suspending the background, parent API calls from
        // runtime.onSuspend listeners shouldn't cancel the suspension.
        resetIdleDetails?.reason !== "parentApiCall"
      ) {
        extension.backgroundState = BACKGROUND_STATE.RUNNING;
        // call runtime.onSuspendCanceled
        extension.emit("background-script-suspend-canceled");
      }

      this.resetIdleTimer();

      if (
        eventName === "background-script-reset-idle" &&
        // TODO(Bug 1790087): record similar telemetry for background service worker.
        !this.isWorker
      ) {
        // Record the reason for resetting the event page idle timeout
        // in a idle result histogram, with the category set based
        // on the reason for resetting (defaults to 'reset_other'
        // if resetIdleDetails.reason is missing or not mapped into the
        // telemetry histogram categories).
        //
        // Keep this in sync with the categories listed in Histograms.json
        // for "WEBEXT_EVENTPAGE_IDLE_RESULT_COUNT".
        let category = "reset_other";
        switch (resetIdleDetails?.reason) {
          case "event":
            category = "reset_event";
            return; // not break; because too frequent, see bug 1868960.
          case "hasActiveNativeAppPorts":
            category = "reset_nativeapp";
            break;
          case "hasActiveStreamFilter":
            category = "reset_streamfilter";
            break;
          case "pendingListeners":
            category = "reset_listeners";
            break;
          case "parentApiCall":
            category = "reset_parentapicall";
            return; // not break; because too frequent, see bug 1868960.
        }

        ExtensionTelemetry.eventPageIdleResult.histogramAdd({
          extension,
          category,
        });
      }
    };

    // Listen for events from the EventManager
    extension.on("background-script-reset-idle", resetBackgroundIdle);
    // After the background is started, initiate the first timer
    extension.once("background-script-started", resetBackgroundIdle);

    // TODO bug 1844488: terminateBackground should account for externally
    // triggered background restarts. It does currently performs various
    // backgroundState checks, but it is possible for the background to have
    // been crashes or restarted in the meantime.
    extension.terminateBackground = async ({
      ignoreDevToolsAttached = false,
      disableResetIdleForTest = false, // Disable all reset idle checks for testing purpose.
    } = {}) => {
      await bgStartupPromise;
      if (!this.extension || this.extension.hasShutdown) {
        // Extension was already shut down.
        return;
      }
      if (extension.backgroundState != BACKGROUND_STATE.RUNNING) {
        return;
      }

      if (
        !ignoreDevToolsAttached &&
        ExtensionParent.DebugUtils.hasDevToolsAttached(extension.id)
      ) {
        extension.emit("background-script-suspend-ignored");
        return;
      }

      // Similar to what happens in recent Chrome version for MV3 extensions, extensions non-persistent
      // background scripts with a nativeMessaging port still open or a sendNativeMessage request still
      // pending an answer are exempt from being terminated when the idle timeout expires.
      // The motivation, as for the similar change that Chrome applies to MV3 extensions, is that using
      // the native messaging API have already an higher barrier due to having to specify a native messaging
      // host app in their manifest and the user also have to install the native app separately as a native
      // application).
      if (
        !disableResetIdleForTest &&
        extension.backgroundContext?.hasActiveNativeAppPorts
      ) {
        extension.emit("background-script-reset-idle", {
          reason: "hasActiveNativeAppPorts",
        });
        return;
      }

      if (
        !disableResetIdleForTest &&
        extension.backgroundContext?.pendingRunListenerPromisesCount
      ) {
        extension.emit("background-script-reset-idle", {
          reason: "pendingListeners",
          pendingListeners:
            extension.backgroundContext.pendingRunListenerPromisesCount,
        });
        // Clear the pending promises being tracked when we have reset the idle
        // once because some where still pending, so that the pending listeners
        // calls can reset the idle timer only once.
        extension.backgroundContext.clearPendingRunListenerPromises();
        return;
      }

      const childId = extension.backgroundContext?.childId;
      if (
        childId !== undefined &&
        extension.hasPermission("webRequestBlocking") &&
        (extension.manifestVersion <= 3 ||
          extension.hasPermission("webRequestFilterResponse"))
      ) {
        // Ask to the background page context in the child process to check if there are
        // StreamFilter instances active (e.g. ones with status "transferringdata" or "suspended",
        // see StreamFilterStatus enum defined in StreamFilter.webidl).
        // TODO(Bug 1748533): consider additional changes to prevent a StreamFilter that never gets to an
        // inactive state from preventing an even page from being ever suspended.
        const hasActiveStreamFilter =
          await ExtensionParent.ParentAPIManager.queryStreamFilterSuspendCancel(
            extension.backgroundContext.childId
          ).catch(err => {
            // an AbortError raised from the JSWindowActor is expected if the background page was already been
            // terminated in the meantime, and so we only log the errors that don't match these particular conditions.
            if (
              extension.backgroundState == BACKGROUND_STATE.STOPPED &&
              DOMException.isInstance(err) &&
              err.name === "AbortError"
            ) {
              return false;
            }
            Cu.reportError(err);
            return false;
          });
        if (!disableResetIdleForTest && hasActiveStreamFilter) {
          extension.emit("background-script-reset-idle", {
            reason: "hasActiveStreamFilter",
          });
          return;
        }

        // Return earlier if extension have started or completed its shutdown in the meantime.
        if (
          extension.backgroundState !== BACKGROUND_STATE.RUNNING ||
          extension.hasShutdown
        ) {
          return;
        }
      }

      extension.backgroundState = BACKGROUND_STATE.SUSPENDING;
      this.clearIdleTimer();
      // call runtime.onSuspend
      await extension.emit("background-script-suspend");
      // If in the meantime another event fired, state will be RUNNING,
      // and if it was shutdown it will be STOPPED.
      if (extension.backgroundState != BACKGROUND_STATE.SUSPENDING) {
        return;
      }
      extension.off("background-script-reset-idle", resetBackgroundIdle);

      // TODO(Bug 1790087): record similar telemetry for background service worker.
      if (!this.isWorker) {
        ExtensionTelemetry.eventPageIdleResult.histogramAdd({
          extension,
          category: "suspend",
        });
      }

      this.backgroundContextOwner.setBgStateStopped(false);
    };

    EventManager.primeListeners(extension, isInStartup);
    // Avoid setting the flag to false when called during extension startup.
    if (!isInStartup) {
      this.backgroundContextOwner.canBePrimed = false;
    }

    // TODO: start-background-script and background-script-event should be
    // unregistered when build() starts or when the extension shuts down.
    extension.once("start-background-script", async () => {
      if (!this.extension || this.extension.hasShutdown) {
        // Extension was shut down. Don't build the background page.
        // Primed listeners have been cleared in onShutdown.
        return;
      }
      await this.build();
    });

    // There are two ways to start the background page:
    // 1. If a primed event fires, then start the background page as
    //    soon as we have painted a browser window.
    // 2. After all windows have been restored on startup (see onManifestEntry).
    extension.once("background-script-event", async () => {
      await ExtensionParent.browserPaintedPromise;
      extension.emit("start-background-script");
    });
  }

  onBgInstanceShutdown(bgInstance) {
    const { msSinceCreated } = bgInstance;
    const { extension } = this;

    // Emit an event for tests.
    extension.emit("shutdown-background-script");

    if (msSinceCreated) {
      const now = msSinceProcessStartExcludingSuspend();
      if (
        now &&
        // TODO(Bug 1790087): record similar telemetry for background service worker.
        !(this.isWorker || extension.persistentBackground)
      ) {
        ExtensionTelemetry.eventPageRunningTime.histogramAdd({
          extension,
          value: now - msSinceCreated,
        });
      }
    }
  }
}

this.backgroundPage = class extends ExtensionAPI {
  async onManifestEntry(entryName) {
    let { extension } = this;

    // When in PPB background pages all run in a private context.  This check
    // simply avoids an extraneous error in the console since the BaseContext
    // will throw.
    if (
      PrivateBrowsingUtils.permanentPrivateBrowsing &&
      !extension.privateBrowsingAllowed
    ) {
      return;
    }

    this.backgroundBuilder = new BackgroundBuilder(extension);

    // runtime.onStartup event support.  We listen for the first
    // background startup then emit a first-run event.
    extension.once("background-script-started", () => {
      extension.emit("background-first-run");
    });

    this.backgroundBuilder.primeBackground();

    // Persistent backgrounds are started immediately except during APP_STARTUP.
    // Non-persistent backgrounds must be started immediately for new install or enable
    // to initialize the addon and create the persisted listeners.
    // updateReason is set when an extension is updated during APP_STARTUP.
    if (
      extension.testNoDelayedStartup ||
      extension.startupReason !== "APP_STARTUP" ||
      extension.updateReason
    ) {
      // TODO bug 1543354: Avoid AsyncShutdown timeouts by removing the await
      // here, at least for non-test situations.
      await this.backgroundBuilder.build();

      // The task in ExtensionParent.browserPaintedPromise below would be fully
      // skipped because of the above build() that sets bgInstance. Return early
      // so that it is obvious that the logic is skipped.
      return;
    }

    ExtensionParent.browserStartupPromise.then(() => {
      // Return early if the background has started in the meantime. This can
      // happen if a primed listener (isInStartup) has been triggered.
      if (
        !this.backgroundBuilder ||
        this.backgroundBuilder.backgroundContextOwner.bgInstance ||
        !this.backgroundBuilder.backgroundContextOwner.canBePrimed
      ) {
        return;
      }

      // We either start the background page immediately, or fully prime for
      // real.
      this.backgroundBuilder.backgroundContextOwner.canBePrimed = false;

      // If there are no listeners for the extension that were persisted, we need to
      // start the event page so they can be registered.
      if (
        extension.persistentBackground ||
        !extension.persistentListeners?.size ||
        // If runtime.onStartup has a listener and this is app_startup,
        // start the extension so it will fire the event.
        (extension.startupReason == "APP_STARTUP" &&
          extension.persistentListeners?.get("runtime").has("onStartup"))
      ) {
        extension.emit("start-background-script");
      } else {
        // During startup we only prime startup blocking listeners.  At
        // this stage we need to prime all listeners for event pages.
        EventManager.clearPrimedListeners(extension, false);
        // Allow re-priming by deleting existing listeners.
        extension.persistentListeners = null;
        EventManager.primeListeners(extension, false);
      }
    });
  }

  onShutdown(isAppShutdown) {
    if (this.backgroundBuilder) {
      this.backgroundBuilder.backgroundContextOwner.onShutdown(isAppShutdown);
      this.backgroundBuilder = null;
    }
  }
};
