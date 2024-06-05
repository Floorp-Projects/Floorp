/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  EventEmitter: "resource://gre/modules/EventEmitter.sys.mjs",
  NetworkObserver:
    "resource://devtools/shared/network-observer/NetworkObserver.sys.mjs",

  NetworkEventRecord:
    "chrome://remote/content/shared/listeners/NetworkEventRecord.sys.mjs",
});

/**
 * The NetworkListener listens to all network activity from the parent
 * process.
 *
 * Example:
 * ```
 * const listener = new NetworkListener();
 * listener.on("before-request-sent", onBeforeRequestSent);
 * listener.startListening();
 *
 * const onBeforeRequestSent = (eventName, data = {}) => {
 *   const { cntextId, redirectCount, requestData, requestId, timestamp } = data;
 *   ...
 * };
 * ```
 *
 * @fires before-request-sent
 *    The NetworkListener emits "before-request-sent" events, with the
 *    following object as payload:
 *      - {number} browsingContextId - The browsing context id of the browsing
 *        context where this request was performed.
 *      - {number} redirectCount - The request's redirect count.
 *      - {RequestData} requestData - The request's data as expected by
 *        WebDriver BiDi.
 *      - {string} requestId - The id of the request, consistent across
 *        redirects.
 *      - {number} timestamp - Timestamp when the event was generated.
 */
export class NetworkListener {
  #devtoolsNetworkObserver;
  #listening;
  #navigationManager;
  #networkEventsMap;

  constructor(navigationManager) {
    lazy.EventEmitter.decorate(this);

    this.#listening = false;
    this.#navigationManager = navigationManager;

    // This map is going to be used in NetworkEventRecord,
    // but because we need to have one instance of the map per session,
    // we have to create and store it here (since each session has a dedicated NetworkListener).
    this.#networkEventsMap = new Map();
  }

  destroy() {
    this.stopListening();
  }

  startListening() {
    if (this.#listening) {
      return;
    }

    this.#devtoolsNetworkObserver = new lazy.NetworkObserver({
      earlyEvents: true,
      ignoreChannelFunction: this.#ignoreChannelFunction,
      onNetworkEvent: this.#onNetworkEvent,
    });

    // Enable the auth prompt listening to support the auth-required event and
    // phase.
    this.#devtoolsNetworkObserver.setAuthPromptListenerEnabled(true);

    this.#listening = true;
  }

  stopListening() {
    if (!this.#listening) {
      return;
    }

    this.#devtoolsNetworkObserver.destroy();
    this.#devtoolsNetworkObserver = null;

    this.#listening = false;
  }

  #ignoreChannelFunction = channel => {
    // Bug 1826210: Ignore file channels which don't support the same APIs as
    // regular HTTP channels.
    if (channel instanceof Ci.nsIFileChannel) {
      return true;
    }

    // Ignore chrome-privileged or DevTools-initiated requests
    if (
      channel.loadInfo?.loadingDocument === null &&
      (channel.loadInfo.loadingPrincipal ===
        Services.scriptSecurityManager.getSystemPrincipal() ||
        channel.loadInfo.isInDevToolsContext)
    ) {
      return true;
    }

    return false;
  };

  #onNetworkEvent = (networkEvent, channel) => {
    return new lazy.NetworkEventRecord(
      networkEvent,
      channel,
      this,
      this.#navigationManager,
      this.#networkEventsMap
    );
  };
}
