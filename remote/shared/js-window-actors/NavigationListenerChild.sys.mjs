/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  Log: "chrome://remote/content/shared/Log.sys.mjs",
  truncate: "chrome://remote/content/shared/Format.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "logger", () => lazy.Log.get());

export class NavigationListenerChild extends JSWindowActorChild {
  #listener;
  #webProgress;

  constructor() {
    super();

    this.#listener = {
      onLocationChange: this.#onLocationChange,
      onStateChange: this.#onStateChange,
      QueryInterface: ChromeUtils.generateQI([
        "nsIWebProgressListener",
        "nsISupportsWeakReference",
      ]),
    };
    this.#webProgress = null;
  }

  actorCreated() {
    this.#webProgress = this.manager.browsingContext.docShell
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIWebProgress);

    this.#webProgress.addProgressListener(
      this.#listener,
      Ci.nsIWebProgress.NOTIFY_LOCATION |
        Ci.nsIWebProgress.NOTIFY_STATE_DOCUMENT
    );
  }

  didDestroy() {
    try {
      this.#webProgress.removeProgressListener(this.#listener);
    } catch (e) {
      // Ignore potential errors if the window global was already destroyed.
    }
  }

  // Note: we rely on events and messages to trigger the actor creation, but
  // all the logic is in the actorCreated callback. The handleEvent and
  // receiveMessage methods are only there as placeholders to avoid errors.

  /**
   * See note above
   */
  handleEvent(event) {}

  /**
   * See note above
   */
  receiveMessage(message) {}

  /**
   * A browsing context might be replaced before reaching the parent process,
   * instead we serialize enough information to retrieve the navigable in the
   * parent process.
   *
   * If the browsing context is top level, then the browserId can be used to
   * find the browser element and the new browsing context.
   * Otherwise (frames) the browsing context should not be replaced and the
   * browsing context id should be enough to find the browsing context.
   *
   * @param {BrowsingContext} browsingContext
   *     The browsing context for which we want to get details.
   * @returns {object}
   *     An object that returns the following properties:
   *       - browserId: browser id for this browsing context
   *       - browsingContextId: browsing context id
   *       - isTopBrowsingContext: flag that indicates if the browsing context is
   *         top level
   *
   */
  #getBrowsingContextDetails(browsingContext) {
    return {
      browserId: browsingContext.browserId,
      browsingContextId: browsingContext.id,
      isTopBrowsingContext: browsingContext.parent === null,
    };
  }

  #getTargetURI(request) {
    try {
      return request.QueryInterface(Ci.nsIChannel).originalURI;
    } catch (e) {}

    return null;
  }

  #onLocationChange = (progress, request, location, stateFlags) => {
    if (stateFlags & Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT) {
      const context = progress.browsingContext;

      lazy.logger.trace(
        `[${context.id}] NavigationListener onLocationChange,` +
          lazy.truncate` location: ${location.spec}`
      );

      this.sendAsyncMessage("NavigationListenerChild:locationChanged", {
        contextDetails: this.#getBrowsingContextDetails(context),
        url: location.spec,
      });
    }
  };

  #onStateChange = (progress, request, stateFlags, status) => {
    const context = progress.browsingContext;
    const targetURI = this.#getTargetURI(request);

    const isBindingAborted = status == Cr.NS_BINDING_ABORTED;
    const isStart = !!(stateFlags & Ci.nsIWebProgressListener.STATE_START);
    const isStop = !!(stateFlags & Ci.nsIWebProgressListener.STATE_STOP);

    if (lazy.Log.isTraceLevelOrMore) {
      const isNetwork = !!(
        stateFlags & Ci.nsIWebProgressListener.STATE_IS_NETWORK
      );
      lazy.logger.trace(
        `[${context.id}] NavigationListener onStateChange,` +
          ` stateFlags: ${stateFlags}, status: ${status}, isStart: ${isStart},` +
          ` isStop: ${isStop}, isNetwork: ${isNetwork},` +
          ` isBindingAborted: ${isBindingAborted},` +
          lazy.truncate` targetURI: ${targetURI?.spec}`
      );
    }

    try {
      if (isStart) {
        this.sendAsyncMessage("NavigationListenerChild:navigationStarted", {
          contextDetails: this.#getBrowsingContextDetails(context),
          url: targetURI?.spec,
        });

        return;
      }

      if (isStop && !isBindingAborted) {
        // Skip NS_BINDING_ABORTED state changes as this can happen during a
        // browsing context + process change and we should get the real stop state
        // change from the correct process later.
        this.sendAsyncMessage("NavigationListenerChild:navigationStopped", {
          contextDetails: this.#getBrowsingContextDetails(context),
          url: targetURI?.spec,
        });
      }
    } catch (e) {
      if (e.name === "InvalidStateError") {
        // We'll arrive here if we no longer have our manager, so we can
        // just swallow this error.
        return;
      }
      throw e;
    }
  };
}
