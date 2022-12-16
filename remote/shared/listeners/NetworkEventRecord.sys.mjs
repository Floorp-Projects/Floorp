/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  TabManager: "chrome://remote/content/shared/TabManager.sys.mjs",
});

/**
 * The NetworkEventRecord implements the interface expected from network event
 * owners for consumers of the DevTools NetworkObserver.
 *
 * The NetworkEventRecord emits the before-request-sent event on behalf of the
 * NetworkListener instance which created it.
 */
export class NetworkEventRecord {
  #contextId;
  #channel;
  #networkListener;
  #requestData;
  #requestId;
  #responseData;

  /**
   *
   * @param {Object} networkEvent
   *     The initial network event information (see createNetworkEvent() in
   *     NetworkUtils.sys.mjs).
   * @param {nsIChannel} channel
   *     The nsIChannel behind this network event.
   * @param {NetworkListener} networkListener
   *     The NetworkListener which created this NetworkEventRecord.
   */
  constructor(networkEvent, channel, networkListener) {
    const browsingContext = BrowsingContext.get(networkEvent.browsingContextID);
    this.#contextId = lazy.TabManager.getIdForBrowsingContext(browsingContext);
    this.#channel = channel;
    this.#networkListener = networkListener;

    // The wrappedChannel id remains identical across redirects, whereas
    // nsIChannel.channelId is different for each and every request.
    const wrappedChannel = ChannelWrapper.get(channel);
    this.#requestId = wrappedChannel.id.toString();

    this.#requestData = {
      bodySize: 0,
      cookies: [],
      headers: [],
      headersSize: networkEvent.headersSize || null,
      method: networkEvent.method || null,
      request: this.#requestId,
      timings: {},
      url: networkEvent.url || null,
    };
  }

  /**
   * This method has to be defined to match the event owner API of the
   * NetworkObserver. It will only be called once per network event record, so
   * despite the name we will simply store the headers and rawHeaders.
   *
   * Set network request headers.
   *
   * @param {Array} headers
   *     The request headers array.
   * @param {string=} rawHeaders
   *     The raw headers source.
   */
  addRequestHeaders(headers, rawHeaders) {
    this.#requestData.headers = headers;
    this.#requestData.rawHeaders = rawHeaders;
  }

  /**
   * Set network request cookies.
   *
   * This method has to be defined to match the event owner API of the
   * NetworkObserver. It will only be called once per network event record, so
   * despite the name we will simply store the cookies.
   *
   * @param {Array} cookies
   *     The request cookies array.
   */
  addRequestCookies(cookies) {
    this.#requestData.cookies = cookies;

    // By design, the NetworkObserver will synchronously create a "network event"
    // then call addRequestHeaders and finally addRequestCookies.
    // According to the BiDi spec, we should emit beforeRequestSent when adding
    // request headers, see https://whatpr.org/fetch/1540.html#http-network-or-cache-fetch
    // step 8.17
    // Bug 1802181: switch the NetworkObserver to an event-based API.
    this.#emitBeforeRequestSent();
  }

  // Expected network event owner API from the NetworkObserver.
  addEventTimings() {}
  addRequestPostData() {}
  addResponseCache() {}
  addResponseContent() {}
  addResponseCookies() {}
  addResponseHeaders() {}
  addResponseStart() {}
  addSecurityInfo() {}
  addServerTimings() {}

  #emitBeforeRequestSent() {
    const timedChannel = this.#channel.QueryInterface(Ci.nsITimedChannel);
    this.#requestData.timings = this.#getTimingsFromTimedChannel(timedChannel);

    this.#networkListener.emit("before-request-sent", {
      contextId: this.#contextId,
      redirectCount: timedChannel.redirectCount,
      requestData: this.#requestData,
      requestId: this.#requestId,
      timestamp: Date.now(),
    });
  }

  /**
   * Convert the provided request timing to a timing relative to the beginning
   * of the request. All timings are numbers representing high definition
   * timestamps.
   *
   * @param {number} timing
   *     High definition timestamp for a request timing relative from the time
   *     origin.
   * @param {number} requestTime
   *     High definition timestamp for the request start time relative from the
   *     time origin.
   * @returns {number} High definition timestamp for the request timing relative
   *     to the start time of the request, or 0 if the provided timing was 0.
   */
  #convertTimestamp(timing, requestTime) {
    if (timing == 0) {
      return 0;
    }

    return timing - requestTime;
  }

  #getTimingsFromTimedChannel(timedChannel) {
    const {
      channelCreationTime,
      redirectStartTime,
      redirectEndTime,
      dispatchFetchEventStartTime,
      cacheReadStartTime,
      domainLookupStartTime,
      domainLookupEndTime,
      connectStartTime,
      connectEndTime,
      secureConnectionStartTime,
      requestStartTime,
      responseStartTime,
      responseEndTime,
    } = timedChannel;

    // fetchStart should be the post-redirect start time, which should be the
    // first non-zero timing from: dispatchFetchEventStart, cacheReadStart and
    // domainLookupStart. See https://www.w3.org/TR/navigation-timing-2/#processing-model
    const fetchStartTime =
      dispatchFetchEventStartTime ||
      cacheReadStartTime ||
      domainLookupStartTime;

    // Bug 1805478: Per spec, the origin time should match Performance API's
    // originTime for the global which initiated the request. This is not
    // available in the parent process, so for now we will use 0.
    const originTime = 0;

    return {
      originTime,
      requestTime: this.#convertTimestamp(channelCreationTime, originTime),
      redirectStart: this.#convertTimestamp(redirectStartTime, originTime),
      redirectEnd: this.#convertTimestamp(redirectEndTime, originTime),
      fetchStart: this.#convertTimestamp(fetchStartTime, originTime),
      dnsStart: this.#convertTimestamp(domainLookupStartTime, originTime),
      dnsEnd: this.#convertTimestamp(domainLookupEndTime, originTime),
      connectStart: this.#convertTimestamp(connectStartTime, originTime),
      connectEnd: this.#convertTimestamp(connectEndTime, originTime),
      tlsStart: this.#convertTimestamp(secureConnectionStartTime, originTime),
      tlsEnd: this.#convertTimestamp(connectEndTime, originTime),
      requestStart: this.#convertTimestamp(requestStartTime, originTime),
      responseStart: this.#convertTimestamp(responseStartTime, originTime),
      responseEnd: this.#convertTimestamp(responseEndTime, originTime),
    };
  }
}
