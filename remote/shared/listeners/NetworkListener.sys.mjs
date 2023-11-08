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

  constructor() {
    lazy.EventEmitter.decorate(this);

    this.#listening = false;
  }

  destroy() {
    this.stopListening();
  }

  startListening() {
    if (this.#listening) {
      return;
    }

    this.#devtoolsNetworkObserver = new lazy.NetworkObserver({
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
    return new lazy.NetworkEventRecord(networkEvent, channel, this);
  };
}
