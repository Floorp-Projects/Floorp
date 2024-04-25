/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  NetworkUtils:
    "resource://devtools/shared/network-observer/NetworkUtils.sys.mjs",
});

/**
 * The NetworkResponse class is a wrapper around the internal channel which
 * provides getters and methods closer to fetch's response concept
 * (https://fetch.spec.whatwg.org/#concept-response).
 */
export class NetworkResponse {
  #channel;
  #decodedBodySize;
  #encodedBodySize;
  #fromCache;
  #headersTransmittedSize;
  #status;
  #statusMessage;
  #totalTransmittedSize;
  #wrappedChannel;

  /**
   *
   * @param {nsIChannel} channel
   *     The channel for the response.
   * @param {object} params
   * @param {boolean} params.fromCache
   *     Whether the response was read from the cache or not.
   * @param {string=} params.rawHeaders
   *     The response's raw (ie potentially compressed) headers
   */
  constructor(channel, params) {
    this.#channel = channel;
    const { fromCache, rawHeaders = "" } = params;
    this.#fromCache = fromCache;
    this.#wrappedChannel = ChannelWrapper.get(channel);

    this.#decodedBodySize = 0;
    this.#encodedBodySize = 0;
    this.#headersTransmittedSize = rawHeaders.length;
    this.#totalTransmittedSize = rawHeaders.length;

    // TODO: responseStatus and responseStatusText are sometimes inconsistent.
    // For instance, they might be (304, Not Modified) when retrieved during the
    // responseStarted event, and then (200, OK) during the responseCompleted
    // event.
    // For now consider them as immutable and store them on startup.
    this.#status = this.#channel.responseStatus;
    this.#statusMessage = this.#channel.responseStatusText;
  }

  get decodedBodySize() {
    return this.#decodedBodySize;
  }

  get encodedBodySize() {
    return this.#encodedBodySize;
  }

  get headersTransmittedSize() {
    return this.#headersTransmittedSize;
  }

  get fromCache() {
    return this.#fromCache;
  }

  get protocol() {
    return lazy.NetworkUtils.getProtocol(this.#channel);
  }

  get serializedURL() {
    return this.#channel.URI.spec;
  }

  get status() {
    return this.#status;
  }

  get statusMessage() {
    return this.#statusMessage;
  }

  get totalTransmittedSize() {
    return this.#totalTransmittedSize;
  }

  addResponseContent(responseContent) {
    this.#decodedBodySize = responseContent.decodedBodySize;
    this.#encodedBodySize = responseContent.bodySize;
    this.#totalTransmittedSize = responseContent.transferredSize;
  }

  getComputedMimeType() {
    // TODO: DevTools NetworkObserver is computing a similar value in
    // addResponseContent, but uses an inconsistent implementation in
    // addResponseStart. This approach can only be used as early as in
    // addResponseHeaders. We should move this logic to the NetworkObserver and
    // expose mimeType in addResponseStart. Bug 1809670.
    let mimeType = "";

    try {
      mimeType = this.#wrappedChannel.contentType;
      const contentCharset = this.#channel.contentCharset;
      if (contentCharset) {
        mimeType += `;charset=${contentCharset}`;
      }
    } catch (e) {
      // Ignore exceptions when reading contentType/contentCharset
    }

    return mimeType;
  }

  getHeadersList() {
    const headers = [];

    this.#channel.visitOriginalResponseHeaders({
      visitHeader(name, value) {
        headers.push([name, value]);
      },
    });

    return headers;
  }
}
