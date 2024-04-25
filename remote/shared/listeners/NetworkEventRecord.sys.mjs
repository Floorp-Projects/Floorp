/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  NetworkRequest: "chrome://remote/content/shared/NetworkRequest.sys.mjs",
  NetworkResponse: "chrome://remote/content/shared/NetworkResponse.sys.mjs",
});

/**
 * The NetworkEventRecord implements the interface expected from network event
 * owners for consumers of the DevTools NetworkObserver.
 *
 * The NetworkEventRecord emits the before-request-sent event on behalf of the
 * NetworkListener instance which created it.
 */
export class NetworkEventRecord {
  #fromCache;
  #networkListener;
  #request;
  #response;

  /**
   *
   * @param {object} networkEvent
   *     The initial network event information (see createNetworkEvent() in
   *     NetworkUtils.sys.mjs).
   * @param {nsIChannel} channel
   *     The nsIChannel behind this network event.
   * @param {NetworkListener} networkListener
   *     The NetworkListener which created this NetworkEventRecord.
   * @param {NavigationManager} navigationManager
   *     The NavigationManager which belongs to the same session as this
   *     NetworkEventRecord.
   */
  constructor(networkEvent, channel, networkListener, navigationManager) {
    this.#request = new lazy.NetworkRequest(channel, {
      navigationManager,
      rawHeaders: networkEvent.rawHeaders,
    });
    this.#response = null;

    this.#fromCache = networkEvent.fromCache;

    this.#networkListener = networkListener;

    // NetworkObserver creates a network event when request headers have been
    // parsed.
    // According to the BiDi spec, we should emit beforeRequestSent when adding
    // request headers, see https://whatpr.org/fetch/1540.html#http-network-or-cache-fetch
    // step 8.17
    // Bug 1802181: switch the NetworkObserver to an event-based API.
    this.#emitBeforeRequestSent();

    // If the request is already blocked, we will not receive further updates,
    // emit a network.fetchError event immediately.
    if (networkEvent.blockedReason) {
      this.#emitFetchError();
    }
  }

  /**
   * Add network request POST data.
   *
   * Required API for a NetworkObserver event owner.
   *
   * @param {object} postData
   *     The request POST data.
   */
  addRequestPostData(postData) {
    this.#request.setPostData(postData);
  }

  /**
   * Add the initial network response information.
   *
   * Required API for a NetworkObserver event owner.
   *
   *
   * @param {object} options
   * @param {nsIChannel} options.channel
   *     The channel.
   * @param {boolean} options.fromCache
   * @param {string} options.rawHeaders
   */
  addResponseStart(options) {
    const { channel, fromCache, rawHeaders } = options;
    this.#response = new lazy.NetworkResponse(channel, {
      rawHeaders,
      fromCache: this.#fromCache || !!fromCache,
    });

    // This should be triggered when all headers have been received, matching
    // the WebDriverBiDi response started trigger in `4.6. HTTP-network fetch`
    // from the fetch specification, based on the PR visible at
    // https://github.com/whatwg/fetch/pull/1540
    this.#emitResponseStarted();
  }

  /**
   * Add connection security information.
   *
   * Required API for a NetworkObserver event owner.
   *
   * Not used for RemoteAgent.
   */
  addSecurityInfo() {}

  /**
   * Add network event timings.
   *
   * Required API for a NetworkObserver event owner.
   *
   * Not used for RemoteAgent.
   */
  addEventTimings() {}

  /**
   * Add response cache entry.
   *
   * Required API for a NetworkObserver event owner.
   *
   * Not used for RemoteAgent.
   */
  addResponseCache() {}

  /**
   * Add response content.
   *
   * Required API for a NetworkObserver event owner.
   *
   * @param {object} responseContent
   *     An object which represents the response content.
   * @param {object} responseInfo
   *     Additional meta data about the response.
   */
  addResponseContent(responseContent, responseInfo) {
    if (responseInfo.blockedReason) {
      this.#emitFetchError();
    } else {
      this.#response.addResponseContent(responseContent);
      this.#emitResponseCompleted();
    }
  }

  /**
   * Add server timings.
   *
   * Required API for a NetworkObserver event owner.
   *
   * Not used for RemoteAgent.
   */
  addServerTimings() {}

  /**
   * Add service worker timings.
   *
   * Required API for a NetworkObserver event owner.
   *
   * Not used for RemoteAgent.
   */
  addServiceWorkerTimings() {}

  onAuthPrompt(authDetails, authCallbacks) {
    this.#emitAuthRequired(authCallbacks);
  }

  #emitAuthRequired(authCallbacks) {
    this.#networkListener.emit("auth-required", {
      authCallbacks,
      request: this.#request,
      response: this.#response,
    });
  }

  #emitBeforeRequestSent() {
    this.#networkListener.emit("before-request-sent", {
      request: this.#request,
    });
  }

  #emitFetchError() {
    this.#networkListener.emit("fetch-error", {
      request: this.#request,
    });
  }

  #emitResponseCompleted() {
    this.#networkListener.emit("response-completed", {
      request: this.#request,
      response: this.#response,
    });
  }

  #emitResponseStarted() {
    this.#networkListener.emit("response-started", {
      request: this.#request,
      response: this.#response,
    });
  }
}
