/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
/* globals NetworkHelper, Services, DevToolsUtils, NetUtil,
   gActivityDistributor */

"use strict";

const {Cc, Ci, Cu, Cr} = require("chrome");

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

loader.lazyRequireGetter(this, "NetworkHelper",
                         "devtools/toolkit/webconsole/network-helper");
loader.lazyImporter(this, "Services", "resource://gre/modules/Services.jsm");
loader.lazyRequireGetter(this, "DevToolsUtils",
                         "devtools/toolkit/DevToolsUtils");
loader.lazyImporter(this, "NetUtil", "resource://gre/modules/NetUtil.jsm");
loader.lazyServiceGetter(this, "gActivityDistributor",
                         "@mozilla.org/network/http-activity-distributor;1",
                         "nsIHttpActivityDistributor");

///////////////////////////////////////////////////////////////////////////////
// Network logging
///////////////////////////////////////////////////////////////////////////////

// The maximum uint32 value.
const PR_UINT32_MAX = 4294967295;

// HTTP status codes.
const HTTP_MOVED_PERMANENTLY = 301;
const HTTP_FOUND = 302;
const HTTP_SEE_OTHER = 303;
const HTTP_TEMPORARY_REDIRECT = 307;

// The maximum number of bytes a NetworkResponseListener can hold.
const RESPONSE_BODY_LIMIT = 1048576; // 1 MB

/**
 * The network response listener implements the nsIStreamListener and
 * nsIRequestObserver interfaces. This is used within the NetworkMonitor feature
 * to get the response body of the request.
 *
 * The code is mostly based on code listings from:
 *
 *   http://www.softwareishard.com/blog/firebug/
 *      nsitraceablechannel-intercept-http-traffic/
 *
 * @constructor
 * @param object aOwner
 *        The response listener owner. This object needs to hold the
 *        |openResponses| object.
 * @param object aHttpActivity
 *        HttpActivity object associated with this request. See NetworkMonitor
 *        for more information.
 */
function NetworkResponseListener(aOwner, aHttpActivity)
{
  this.owner = aOwner;
  this.receivedData = "";
  this.httpActivity = aHttpActivity;
  this.bodySize = 0;
  let channel = this.httpActivity.channel;
  this._wrappedNotificationCallbacks = channel.notificationCallbacks;
  channel.notificationCallbacks = this;
}
exports.NetworkResponseListener = NetworkResponseListener;

NetworkResponseListener.prototype = {
  QueryInterface:
    XPCOMUtils.generateQI([Ci.nsIStreamListener, Ci.nsIInputStreamCallback,
                           Ci.nsIRequestObserver, Ci.nsIInterfaceRequestor,
                           Ci.nsISupports]),

  // nsIInterfaceRequestor implementation

  /**
   * This object implements nsIProgressEventSink, but also needs to forward
   * interface requests to the notification callbacks of other objects.
   */
  getInterface(iid) {
    if (iid.equals(Ci.nsIProgressEventSink)) {
      return this;
    }
    if (this._wrappedNotificationCallbacks) {
      return this._wrappedNotificationCallbacks.getInterface(iid);
    }
    throw Cr.NS_ERROR_NO_INTERFACE;
  },

  /**
   * Forward notifications for interfaces this object implements, in case other
   * objects also implemented them.
   */
  _forwardNotification(iid, method, args) {
    if (!this._wrappedNotificationCallbacks) {
      return;
    }
    try {
      let impl = this._wrappedNotificationCallbacks.getInterface(iid);
      impl[method].apply(impl, args);
    } catch (e) {
      if (e.result != Cr.NS_ERROR_NO_INTERFACE) {
        throw e;
      }
    }
  },

  /**
   * This NetworkResponseListener tracks the NetworkMonitor.openResponses object
   * to find the associated uncached headers.
   * @private
   */
  _foundOpenResponse: false,

  /**
   * If the channel already had notificationCallbacks, hold them here internally
   * so that we can forward getInterface requests to that object.
   */
  _wrappedNotificationCallbacks: null,

  /**
   * The response listener owner.
   */
  owner: null,

  /**
   * The response will be written into the outputStream of this nsIPipe.
   * Both ends of the pipe must be blocking.
   */
  sink: null,

  /**
   * The HttpActivity object associated with this response.
   */
  httpActivity: null,

  /**
   * Stores the received data as a string.
   */
  receivedData: null,

  /**
   * The uncompressed, decoded response body size.
   */
  bodySize: null,

  /**
   * Response body size on the wire, potentially compressed / encoded.
   */
  transferredSize: null,

  /**
   * The nsIRequest we are started for.
   */
  request: null,

  /**
   * Set the async listener for the given nsIAsyncInputStream. This allows us to
   * wait asynchronously for any data coming from the stream.
   *
   * @param nsIAsyncInputStream aStream
   *        The input stream from where we are waiting for data to come in.
   * @param nsIInputStreamCallback aListener
   *        The input stream callback you want. This is an object that must have
   *        the onInputStreamReady() method. If the argument is null, then the
   *        current callback is removed.
   * @return void
   */
  setAsyncListener: function NRL_setAsyncListener(aStream, aListener)
  {
    // Asynchronously wait for the stream to be readable or closed.
    aStream.asyncWait(aListener, 0, 0, Services.tm.mainThread);
  },

  /**
   * Stores the received data, if request/response body logging is enabled. It
   * also does limit the number of stored bytes, based on the
   * RESPONSE_BODY_LIMIT constant.
   *
   * Learn more about nsIStreamListener at:
   * https://developer.mozilla.org/en/XPCOM_Interface_Reference/nsIStreamListener
   *
   * @param nsIRequest aRequest
   * @param nsISupports aContext
   * @param nsIInputStream aInputStream
   * @param unsigned long aOffset
   * @param unsigned long aCount
   */
  onDataAvailable:
  function NRL_onDataAvailable(aRequest, aContext, aInputStream, aOffset, aCount)
  {
    this._findOpenResponse();
    let data = NetUtil.readInputStreamToString(aInputStream, aCount);

    this.bodySize += aCount;

    if (!this.httpActivity.discardResponseBody &&
        this.receivedData.length < RESPONSE_BODY_LIMIT) {
      this.receivedData += NetworkHelper.
                           convertToUnicode(data, aRequest.contentCharset);
    }
  },

  /**
   * See documentation at
   * https://developer.mozilla.org/En/NsIRequestObserver
   *
   * @param nsIRequest aRequest
   * @param nsISupports aContext
   */
  onStartRequest: function NRL_onStartRequest(aRequest)
  {
    // Converter will call this again, we should just ignore that.
    if (this.request)
      return;

    this.request = aRequest;
    this._getSecurityInfo();
    this._findOpenResponse();
    // We need to track the offset for the onDataAvailable calls where we pass the data
    // from our pipe to the coverter.
    this.offset = 0;

    // In the multi-process mode, the conversion happens on the child side while we can
    // only monitor the channel on the parent side. If the content is gzipped, we have
    // to unzip it ourself. For that we use the stream converter services.
    let channel = this.request;
    if (channel instanceof Ci.nsIEncodedChannel &&
        channel.contentEncodings &&
        !channel.applyConversion) {
      let encodingHeader = channel.getResponseHeader("Content-Encoding");
      let scs = Cc["@mozilla.org/streamConverters;1"].
        getService(Ci.nsIStreamConverterService);
      let encodings = encodingHeader.split(/\s*\t*,\s*\t*/);
      let nextListener = this;
      let acceptedEncodings = ["gzip", "deflate", "x-gzip", "x-deflate"];
      for (let i in encodings) {
        // There can be multiple conversions applied
        let enc = encodings[i].toLowerCase();
        if (acceptedEncodings.indexOf(enc) > -1) {
          this.converter = scs.asyncConvertData(enc, "uncompressed", nextListener, null);
          nextListener = this.converter;
        }
      }
      if (this.converter) {
        this.converter.onStartRequest(this.request, null);
      }
    }
    // Asynchronously wait for the data coming from the request.
    this.setAsyncListener(this.sink.inputStream, this);
  },

  /**
   * Parse security state of this request and report it to the client.
   */
  _getSecurityInfo: DevToolsUtils.makeInfallible(function NRL_getSecurityInfo() {
    // Take the security information from the original nsIHTTPChannel instead of
    // the nsIRequest received in onStartRequest. If response to this request
    // was a redirect from http to https, the request object seems to contain
    // security info for the https request after redirect.
    let secinfo = this.httpActivity.channel.securityInfo;
    let info = NetworkHelper.parseSecurityInfo(secinfo, this.httpActivity);

    this.httpActivity.owner.addSecurityInfo(info);
  }),

  /**
   * Handle the onStopRequest by closing the sink output stream.
   *
   * For more documentation about nsIRequestObserver go to:
   * https://developer.mozilla.org/En/NsIRequestObserver
   */
  onStopRequest: function NRL_onStopRequest()
  {
    this._findOpenResponse();
    this.sink.outputStream.close();
  },

  // nsIProgressEventSink implementation

  /**
   * Handle progress event as data is transferred.  This is used to record the
   * size on the wire, which may be compressed / encoded.
   */
  onProgress: function(request, context, progress, progressMax) {
    this.transferredSize = progress;
    // Need to forward as well to keep things like Download Manager's progress
    // bar working properly.
    this._forwardNotification(Ci.nsIProgressEventSink, "onProgress", arguments);
  },

  onStatus: function () {
    this._forwardNotification(Ci.nsIProgressEventSink, "onStatus", arguments);
  },

  /**
   * Find the open response object associated to the current request. The
   * NetworkMonitor._httpResponseExaminer() method saves the response headers in
   * NetworkMonitor.openResponses. This method takes the data from the open
   * response object and puts it into the HTTP activity object, then sends it to
   * the remote Web Console instance.
   *
   * @private
   */
  _findOpenResponse: function NRL__findOpenResponse()
  {
    if (!this.owner || this._foundOpenResponse) {
      return;
    }

    let openResponse = null;

    for (let id in this.owner.openResponses) {
      let item = this.owner.openResponses[id];
      if (item.channel === this.httpActivity.channel) {
        openResponse = item;
        break;
      }
    }

    if (!openResponse) {
      return;
    }
    this._foundOpenResponse = true;

    delete this.owner.openResponses[openResponse.id];

    this.httpActivity.owner.addResponseHeaders(openResponse.headers);
    this.httpActivity.owner.addResponseCookies(openResponse.cookies);
  },

  /**
   * Clean up the response listener once the response input stream is closed.
   * This is called from onStopRequest() or from onInputStreamReady() when the
   * stream is closed.
   * @return void
   */
  onStreamClose: function NRL_onStreamClose()
  {
    if (!this.httpActivity) {
      return;
    }
    // Remove our listener from the request input stream.
    this.setAsyncListener(this.sink.inputStream, null);

    this._findOpenResponse();

    if (!this.httpActivity.discardResponseBody && this.receivedData.length) {
      this._onComplete(this.receivedData);
    }
    else if (!this.httpActivity.discardResponseBody &&
             this.httpActivity.responseStatus == 304) {
      // Response is cached, so we load it from cache.
      let charset = this.request.contentCharset || this.httpActivity.charset;
      NetworkHelper.loadFromCache(this.httpActivity.url, charset,
                                  this._onComplete.bind(this));
    }
    else {
      this._onComplete();
    }
  },

  /**
   * Handler for when the response completes. This function cleans up the
   * response listener.
   *
   * @param string [aData]
   *        Optional, the received data coming from the response listener or
   *        from the cache.
   */
  _onComplete: function NRL__onComplete(aData)
  {
    let response = {
      mimeType: "",
      text: aData || "",
    };

    response.size = response.text.length;
    response.transferredSize = this.transferredSize;

    try {
      response.mimeType = this.request.contentType;
    }
    catch (ex) { }

    if (!response.mimeType || !NetworkHelper.isTextMimeType(response.mimeType)) {
      response.encoding = "base64";
      response.text = btoa(response.text);
    }

    if (response.mimeType && this.request.contentCharset) {
      response.mimeType += "; charset=" + this.request.contentCharset;
    }

    this.receivedData = "";

    this.httpActivity.owner.addResponseContent(
      response,
      this.httpActivity.discardResponseBody
    );

    this._wrappedNotificationCallbacks = null;
    this.httpActivity.channel = null;
    this.httpActivity.owner = null;
    this.httpActivity = null;
    this.sink = null;
    this.inputStream = null;
    this.converter = null;
    this.request = null;
    this.owner = null;
  },

  /**
   * The nsIInputStreamCallback for when the request input stream is ready -
   * either it has more data or it is closed.
   *
   * @param nsIAsyncInputStream aStream
   *        The sink input stream from which data is coming.
   * @returns void
   */
  onInputStreamReady: function NRL_onInputStreamReady(aStream)
  {
    if (!(aStream instanceof Ci.nsIAsyncInputStream) || !this.httpActivity) {
      return;
    }

    let available = -1;
    try {
      // This may throw if the stream is closed normally or due to an error.
      available = aStream.available();
    }
    catch (ex) { }

    if (available != -1) {
      if (available != 0) {
        if (this.converter) {
          this.converter.onDataAvailable(this.request, null, aStream, this.offset, available);
        } else {
          this.onDataAvailable(this.request, null, aStream, this.offset, available);
        }
      }
      this.offset += available;
      this.setAsyncListener(aStream, this);
    }
    else {
      this.onStreamClose();
      this.offset = 0;
    }
  },
}; // NetworkResponseListener.prototype


/**
 * The network monitor uses the nsIHttpActivityDistributor to monitor network
 * requests. The nsIObserverService is also used for monitoring
 * http-on-examine-response notifications. All network request information is
 * routed to the remote Web Console.
 *
 * @constructor
 * @param object aFilters
 *        Object with the filters to use for network requests:
 *        - window (nsIDOMWindow): filter network requests by the associated
 *        window object.
 *        - appId (number): filter requests by the appId.
 *        - topFrame (nsIDOMElement): filter requests by their topFrameElement.
 *        Filters are optional. If any of these filters match the request is
 *        logged (OR is applied). If no filter is provided then all requests are
 *        logged.
 * @param object aOwner
 *        The network monitor owner. This object needs to hold:
 *        - onNetworkEvent(aRequestInfo, aChannel, aNetworkMonitor).
 *        This method is invoked once for every new network request and it is
 *        given the following arguments: the initial network request
 *        information, and the channel. The third argument is the NetworkMonitor
 *        instance.
 *        onNetworkEvent() must return an object which holds several add*()
 *        methods which are used to add further network request/response
 *        information.
 */
function NetworkMonitor(aFilters, aOwner)
{
  if (aFilters) {
    this.window = aFilters.window;
    this.appId = aFilters.appId;
    this.topFrame = aFilters.topFrame;
  }
  if (!this.window && !this.appId && !this.topFrame) {
    this._logEverything = true;
  }
  this.owner = aOwner;
  this.openRequests = {};
  this.openResponses = {};
  this._httpResponseExaminer =
    DevToolsUtils.makeInfallible(this._httpResponseExaminer).bind(this);
}
exports.NetworkMonitor = NetworkMonitor;

NetworkMonitor.prototype = {
  _logEverything: false,
  window: null,
  appId: null,
  topFrame: null,

  httpTransactionCodes: {
    0x5001: "REQUEST_HEADER",
    0x5002: "REQUEST_BODY_SENT",
    0x5003: "RESPONSE_START",
    0x5004: "RESPONSE_HEADER",
    0x5005: "RESPONSE_COMPLETE",
    0x5006: "TRANSACTION_CLOSE",

    0x804b0003: "STATUS_RESOLVING",
    0x804b000b: "STATUS_RESOLVED",
    0x804b0007: "STATUS_CONNECTING_TO",
    0x804b0004: "STATUS_CONNECTED_TO",
    0x804b0005: "STATUS_SENDING_TO",
    0x804b000a: "STATUS_WAITING_FOR",
    0x804b0006: "STATUS_RECEIVING_FROM"
  },

  // Network response bodies are piped through a buffer of the given size (in
  // bytes).
  responsePipeSegmentSize: null,

  owner: null,

  /**
   * Whether to save the bodies of network requests and responses. Disabled by
   * default to save memory.
   * @type boolean
   */
  saveRequestAndResponseBodies: false,

  /**
   * Object that holds the HTTP activity objects for ongoing requests.
   */
  openRequests: null,

  /**
   * Object that holds response headers coming from this._httpResponseExaminer.
   */
  openResponses: null,

  /**
   * The network monitor initializer.
   */
  init: function NM_init()
  {
    this.responsePipeSegmentSize = Services.prefs
                                   .getIntPref("network.buffer.cache.size");

    gActivityDistributor.addObserver(this);

    if (Services.appinfo.processType != Ci.nsIXULRuntime.PROCESS_TYPE_CONTENT) {
      Services.obs.addObserver(this._httpResponseExaminer,
                               "http-on-examine-response", false);
      Services.obs.addObserver(this._httpResponseExaminer,
                               "http-on-examine-cached-response", false);
    }
  },

  /**
   * Observe notifications for the http-on-examine-response topic, coming from
   * the nsIObserverService.
   *
   * @private
   * @param nsIHttpChannel aSubject
   * @param string aTopic
   * @returns void
   */
  _httpResponseExaminer: function NM__httpResponseExaminer(aSubject, aTopic)
  {
    // The httpResponseExaminer is used to retrieve the uncached response
    // headers. The data retrieved is stored in openResponses. The
    // NetworkResponseListener is responsible with updating the httpActivity
    // object with the data from the new object in openResponses.

    if (!this.owner ||
        (aTopic != "http-on-examine-response" &&
         aTopic != "http-on-examine-cached-response") ||
        !(aSubject instanceof Ci.nsIHttpChannel)) {
      return;
    }

    let channel = aSubject.QueryInterface(Ci.nsIHttpChannel);

    if (!this._matchRequest(channel)) {
      return;
    }

    let response = {
      id: gSequenceId(),
      channel: channel,
      headers: [],
      cookies: [],
    };

    let setCookieHeader = null;

    channel.visitResponseHeaders({
      visitHeader: function NM__visitHeader(aName, aValue) {
        let lowerName = aName.toLowerCase();
        if (lowerName == "set-cookie") {
          setCookieHeader = aValue;
        }
        response.headers.push({ name: aName, value: aValue });
      }
    });

    if (!response.headers.length) {
      return; // No need to continue.
    }

    if (setCookieHeader) {
      response.cookies = NetworkHelper.parseSetCookieHeader(setCookieHeader);
    }

    // Determine the HTTP version.
    let httpVersionMaj = {};
    let httpVersionMin = {};

    channel.QueryInterface(Ci.nsIHttpChannelInternal);
    channel.getResponseVersion(httpVersionMaj, httpVersionMin);

    response.status = channel.responseStatus;
    response.statusText = channel.responseStatusText;
    response.httpVersion = "HTTP/" + httpVersionMaj.value + "." +
                                     httpVersionMin.value;

    this.openResponses[response.id] = response;

    if(aTopic === "http-on-examine-cached-response") {
      // If this is a cached response, there never was a request event
      // so we need to construct one here so the frontend gets all the
      // expected events.
      let httpActivity = this._createNetworkEvent(channel, { fromCache: true });
      httpActivity.owner.addResponseStart({
        httpVersion: response.httpVersion,
        remoteAddress: "",
        remotePort: "",
        status: response.status,
        statusText: response.statusText,
        headersSize: 0,
      }, "", true);

      // There also is never any timing events, so we can fire this
      // event with zeroed out values.
      let timings = this._setupHarTimings(httpActivity, true);
      httpActivity.owner.addEventTimings(timings.total, timings.timings);
    }
  },

  /**
   * Begin observing HTTP traffic that originates inside the current tab.
   *
   * @see https://developer.mozilla.org/en/XPCOM_Interface_Reference/nsIHttpActivityObserver
   *
   * @param nsIHttpChannel aChannel
   * @param number aActivityType
   * @param number aActivitySubtype
   * @param number aTimestamp
   * @param number aExtraSizeData
   * @param string aExtraStringData
   */
  observeActivity: DevToolsUtils.makeInfallible(function NM_observeActivity(aChannel, aActivityType, aActivitySubtype, aTimestamp, aExtraSizeData, aExtraStringData)
  {
    if (!this.owner ||
        aActivityType != gActivityDistributor.ACTIVITY_TYPE_HTTP_TRANSACTION &&
        aActivityType != gActivityDistributor.ACTIVITY_TYPE_SOCKET_TRANSPORT) {
      return;
    }

    if (!(aChannel instanceof Ci.nsIHttpChannel)) {
      return;
    }

    aChannel = aChannel.QueryInterface(Ci.nsIHttpChannel);

    if (aActivitySubtype ==
        gActivityDistributor.ACTIVITY_SUBTYPE_REQUEST_HEADER) {
      this._onRequestHeader(aChannel, aTimestamp, aExtraStringData);
      return;
    }

    // Iterate over all currently ongoing requests. If aChannel can't
    // be found within them, then exit this function.
    let httpActivity = null;
    for (let id in this.openRequests) {
      let item = this.openRequests[id];
      if (item.channel === aChannel) {
        httpActivity = item;
        break;
      }
    }

    if (!httpActivity) {
      return;
    }

    let transCodes = this.httpTransactionCodes;

    // Store the time information for this activity subtype.
    if (aActivitySubtype in transCodes) {
      let stage = transCodes[aActivitySubtype];
      if (stage in httpActivity.timings) {
        httpActivity.timings[stage].last = aTimestamp;
      }
      else {
        httpActivity.timings[stage] = {
          first: aTimestamp,
          last: aTimestamp,
        };
      }
    }

    switch (aActivitySubtype) {
      case gActivityDistributor.ACTIVITY_SUBTYPE_REQUEST_BODY_SENT:
        this._onRequestBodySent(httpActivity);
        break;
      case gActivityDistributor.ACTIVITY_SUBTYPE_RESPONSE_HEADER:
        this._onResponseHeader(httpActivity, aExtraStringData);
        break;
      case gActivityDistributor.ACTIVITY_SUBTYPE_TRANSACTION_CLOSE:
        this._onTransactionClose(httpActivity);
        break;
      default:
        break;
    }
  }),

  /**
   * Check if a given network request should be logged by this network monitor
   * instance based on the current filters.
   *
   * @private
   * @param nsIHttpChannel aChannel
   *        Request to check.
   * @return boolean
   *         True if the network request should be logged, false otherwise.
   */
  _matchRequest: function NM__matchRequest(aChannel)
  {
    if (this._logEverything) {
      return true;
    }

    // Ignore requests from chrome or add-on code when we are monitoring
    // content.
    // TODO: one particular test (browser_styleeditor_fetch-from-cache.js) needs
    // the DevToolsUtils.testing check. We will move to a better way to serve
    // its needs in bug 1167188, where this check should be removed.
    if (!DevToolsUtils.testing && aChannel.loadInfo &&
        aChannel.loadInfo.loadingDocument === null &&
        aChannel.loadInfo.loadingPrincipal === Services.scriptSecurityManager.getSystemPrincipal()) {
      return false;
    }

    if (this.window) {
      // Since frames support, this.window may not be the top level content
      // frame, so that we can't only compare with win.top.
      let win = NetworkHelper.getWindowForRequest(aChannel);
      while(win) {
        if (win == this.window) {
          return true;
        }
        if (win.parent == win) {
          break;
        }
        win = win.parent;
      }
    }

    if (this.topFrame) {
      let topFrame = NetworkHelper.getTopFrameForRequest(aChannel);
      if (topFrame && topFrame === this.topFrame) {
        return true;
      }
    }

    if (this.appId) {
      let appId = NetworkHelper.getAppIdForRequest(aChannel);
      if (appId && appId == this.appId) {
        return true;
      }
    }

    // The following check is necessary because beacon channels don't come
    // associated with a load group. Bug 1160837 will hopefully introduce a
    // platform fix that will render the following code entirely useless.
    if (aChannel.loadInfo &&
        aChannel.loadInfo.contentPolicyType == Ci.nsIContentPolicy.TYPE_BEACON) {
      let nonE10sMatch = this.window &&
                         aChannel.loadInfo.loadingDocument === this.window.document;
      let e10sMatch = this.topFrame &&
                      this.topFrame.contentPrincipal &&
                      this.topFrame.contentPrincipal.equals(aChannel.loadInfo.loadingPrincipal) &&
                      this.topFrame.contentPrincipal.URI.spec == aChannel.referrer.spec;
      let b2gMatch = this.appId &&
                     aChannel.loadInfo.loadingPrincipal.appId === this.appId;
      if (nonE10sMatch || e10sMatch || b2gMatch) {
        return true;
      }
    }

    return false;
  },

  /**
   *
   */
  _createNetworkEvent: function(aChannel, { timestamp, extraStringData, fromCache }) {
    let win = NetworkHelper.getWindowForRequest(aChannel);
    let httpActivity = this.createActivityObject(aChannel);

    // see NM__onRequestBodySent()
    httpActivity.charset = win ? win.document.characterSet : null;

    aChannel.QueryInterface(Ci.nsIPrivateBrowsingChannel);
    httpActivity.private = aChannel.isChannelPrivate;

    if(timestamp) {
      httpActivity.timings.REQUEST_HEADER = {
        first: timestamp,
        last: timestamp
      };
    }

    let event = {};
    event.method = aChannel.requestMethod;
    event.url = aChannel.URI.spec;
    event.private = httpActivity.private;
    event.headersSize = 0;
    event.startedDateTime = (timestamp ? new Date(Math.round(timestamp / 1000)) : new Date()).toISOString();
    event.fromCache = fromCache;

    if(extraStringData) {
      event.headersSize = extraStringData.length;
    }

    // Determine if this is an XHR request.
    httpActivity.isXHR = event.isXHR =
        (aChannel.loadInfo.contentPolicyType === Ci.nsIContentPolicy.TYPE_XMLHTTPREQUEST);

    // Determine the HTTP version.
    let httpVersionMaj = {};
    let httpVersionMin = {};
    aChannel.QueryInterface(Ci.nsIHttpChannelInternal);
    aChannel.getRequestVersion(httpVersionMaj, httpVersionMin);

    event.httpVersion = "HTTP/" + httpVersionMaj.value + "." +
                                  httpVersionMin.value;

    event.discardRequestBody = !this.saveRequestAndResponseBodies;
    event.discardResponseBody = !this.saveRequestAndResponseBodies;

    let headers = [];
    let cookies = [];
    let cookieHeader = null;

    // Copy the request header data.
    aChannel.visitRequestHeaders({
      visitHeader: function NM__visitHeader(aName, aValue)
      {
        if (aName == "Cookie") {
          cookieHeader = aValue;
        }
        headers.push({ name: aName, value: aValue });
      }
    });

    if (cookieHeader) {
      cookies = NetworkHelper.parseCookieHeader(cookieHeader);
    }

    httpActivity.owner = this.owner.onNetworkEvent(event, aChannel);

    this._setupResponseListener(httpActivity);

    httpActivity.owner.addRequestHeaders(headers, extraStringData);
    httpActivity.owner.addRequestCookies(cookies);

    this.openRequests[httpActivity.id] = httpActivity;
    return httpActivity;
  },

  /**
   * Handler for ACTIVITY_SUBTYPE_REQUEST_HEADER. When a request starts the
   * headers are sent to the server. This method creates the |httpActivity|
   * object where we store the request and response information that is
   * collected through its lifetime.
   *
   * @private
   * @param nsIHttpChannel aChannel
   * @param number aTimestamp
   * @param string aExtraStringData
   * @return void
   */
  _onRequestHeader:
  function NM__onRequestHeader(aChannel, aTimestamp, aExtraStringData)
  {
    if (!this._matchRequest(aChannel)) {
      return;
    }

    this._createNetworkEvent(aChannel, { timestamp: aTimestamp,
                                         extraStringData: aExtraStringData });
  },

  /**
   * Create the empty HTTP activity object. This object is used for storing all
   * the request and response information.
   *
   * This is a HAR-like object. Conformance to the spec is not guaranteed at
   * this point.
   *
   * TODO: Bug 708717 - Add support for network log export to HAR
   *
   * @see http://www.softwareishard.com/blog/har-12-spec
   * @param nsIHttpChannel aChannel
   *        The HTTP channel for which the HTTP activity object is created.
   * @return object
   *         The new HTTP activity object.
   */
  createActivityObject: function NM_createActivityObject(aChannel)
  {
    return {
      id: gSequenceId(),
      channel: aChannel,
      charset: null, // see NM__onRequestHeader()
      url: aChannel.URI.spec,
      hostname: aChannel.URI.host, // needed for host specific security info
      discardRequestBody: !this.saveRequestAndResponseBodies,
      discardResponseBody: !this.saveRequestAndResponseBodies,
      timings: {}, // internal timing information, see NM_observeActivity()
      responseStatus: null, // see NM__onResponseHeader()
      owner: null, // the activity owner which is notified when changes happen
    };
  },

  /**
   * Setup the network response listener for the given HTTP activity. The
   * NetworkResponseListener is responsible for storing the response body.
   *
   * @private
   * @param object aHttpActivity
   *        The HTTP activity object we are tracking.
   */
  _setupResponseListener: function NM__setupResponseListener(aHttpActivity)
  {
    let channel = aHttpActivity.channel;
    channel.QueryInterface(Ci.nsITraceableChannel);

    // The response will be written into the outputStream of this pipe.
    // This allows us to buffer the data we are receiving and read it
    // asynchronously.
    // Both ends of the pipe must be blocking.
    let sink = Cc["@mozilla.org/pipe;1"].createInstance(Ci.nsIPipe);

    // The streams need to be blocking because this is required by the
    // stream tee.
    sink.init(false, false, this.responsePipeSegmentSize, PR_UINT32_MAX, null);

    // Add listener for the response body.
    let newListener = new NetworkResponseListener(this, aHttpActivity);

    // Remember the input stream, so it isn't released by GC.
    newListener.inputStream = sink.inputStream;
    newListener.sink = sink;

    let tee = Cc["@mozilla.org/network/stream-listener-tee;1"].
              createInstance(Ci.nsIStreamListenerTee);

    let originalListener = channel.setNewListener(tee);

    tee.init(originalListener, sink.outputStream, newListener);
  },

  /**
   * Handler for ACTIVITY_SUBTYPE_REQUEST_BODY_SENT. The request body is logged
   * here.
   *
   * @private
   * @param object aHttpActivity
   *        The HTTP activity object we are working with.
   */
  _onRequestBodySent: function NM__onRequestBodySent(aHttpActivity)
  {
    if (aHttpActivity.discardRequestBody) {
      return;
    }

    let sentBody = NetworkHelper.
                   readPostTextFromRequest(aHttpActivity.channel,
                                           aHttpActivity.charset);

    if (!sentBody && this.window &&
        aHttpActivity.url == this.window.location.href) {
      // If the request URL is the same as the current page URL, then
      // we can try to get the posted text from the page directly.
      // This check is necessary as otherwise the
      //   NetworkHelper.readPostTextFromPageViaWebNav()
      // function is called for image requests as well but these
      // are not web pages and as such don't store the posted text
      // in the cache of the webpage.
      let webNav = this.window.QueryInterface(Ci.nsIInterfaceRequestor).
                   getInterface(Ci.nsIWebNavigation);
      sentBody = NetworkHelper.
                 readPostTextFromPageViaWebNav(webNav, aHttpActivity.charset);
    }

    if (sentBody) {
      aHttpActivity.owner.addRequestPostData({ text: sentBody });
    }
  },

  /**
   * Handler for ACTIVITY_SUBTYPE_RESPONSE_HEADER. This method stores
   * information about the response headers.
   *
   * @private
   * @param object aHttpActivity
   *        The HTTP activity object we are working with.
   * @param string aExtraStringData
   *        The uncached response headers.
   */
  _onResponseHeader:
  function NM__onResponseHeader(aHttpActivity, aExtraStringData)
  {
    // aExtraStringData contains the uncached response headers. The first line
    // contains the response status (e.g. HTTP/1.1 200 OK).
    //
    // Note: The response header is not saved here. Calling the
    // channel.visitResponseHeaders() method at this point sometimes causes an
    // NS_ERROR_NOT_AVAILABLE exception.
    //
    // We could parse aExtraStringData to get the headers and their values, but
    // that is not trivial to do in an accurate manner. Hence, we save the
    // response headers in this._httpResponseExaminer().

    let headers = aExtraStringData.split(/\r\n|\n|\r/);
    let statusLine = headers.shift();
    let statusLineArray = statusLine.split(" ");

    let response = {};
    response.httpVersion = statusLineArray.shift();
    response.remoteAddress = aHttpActivity.channel.remoteAddress;
    response.remotePort = aHttpActivity.channel.remotePort;
    response.status = statusLineArray.shift();
    response.statusText = statusLineArray.join(" ");
    response.headersSize = aExtraStringData.length;

    aHttpActivity.responseStatus = response.status;

    // Discard the response body for known response statuses.
    switch (parseInt(response.status)) {
      case HTTP_MOVED_PERMANENTLY:
      case HTTP_FOUND:
      case HTTP_SEE_OTHER:
      case HTTP_TEMPORARY_REDIRECT:
        aHttpActivity.discardResponseBody = true;
        break;
    }

    response.discardResponseBody = aHttpActivity.discardResponseBody;

    aHttpActivity.owner.addResponseStart(response, aExtraStringData);
  },

  /**
   * Handler for ACTIVITY_SUBTYPE_TRANSACTION_CLOSE. This method updates the HAR
   * timing information on the HTTP activity object and clears the request
   * from the list of known open requests.
   *
   * @private
   * @param object aHttpActivity
   *        The HTTP activity object we work with.
   */
  _onTransactionClose: function NM__onTransactionClose(aHttpActivity)
  {
    let result = this._setupHarTimings(aHttpActivity);
    aHttpActivity.owner.addEventTimings(result.total, result.timings);
    delete this.openRequests[aHttpActivity.id];
  },

  /**
   * Update the HTTP activity object to include timing information as in the HAR
   * spec. The HTTP activity object holds the raw timing information in
   * |timings| - these are timings stored for each activity notification. The
   * HAR timing information is constructed based on these lower level
   * data.
   *
   * @param object aHttpActivity
   *        The HTTP activity object we are working with.
   * @param boolean fromCache
   *        Indicates that the result was returned from the browser cache
   * @return object
   *         This object holds two properties:
   *         - total - the total time for all of the request and response.
   *         - timings - the HAR timings object.
   */
  _setupHarTimings: function NM__setupHarTimings(aHttpActivity, fromCache)
  {
    if(fromCache) {
      // If it came from the browser cache, we have no timing
      // information and these should all be 0
      return {
        total: 0,
        timings: {
          blocked: 0,
          dns: 0,
          connect: 0,
          send: 0,
          wait: 0,
          receive: 0
        }
      };
    }

    let timings = aHttpActivity.timings;
    let harTimings = {};

    // Not clear how we can determine "blocked" time.
    harTimings.blocked = -1;

    // DNS timing information is available only in when the DNS record is not
    // cached.
    harTimings.dns = timings.STATUS_RESOLVING && timings.STATUS_RESOLVED ?
                     timings.STATUS_RESOLVED.last -
                     timings.STATUS_RESOLVING.first : -1;

    if (timings.STATUS_CONNECTING_TO && timings.STATUS_CONNECTED_TO) {
      harTimings.connect = timings.STATUS_CONNECTED_TO.last -
                           timings.STATUS_CONNECTING_TO.first;
    }
    else if (timings.STATUS_SENDING_TO) {
      harTimings.connect = timings.STATUS_SENDING_TO.first -
                           timings.REQUEST_HEADER.first;
    }
    else {
      harTimings.connect = -1;
    }

    if ((timings.STATUS_WAITING_FOR || timings.STATUS_RECEIVING_FROM) &&
        (timings.STATUS_CONNECTED_TO || timings.STATUS_SENDING_TO)) {
      harTimings.send = (timings.STATUS_WAITING_FOR ||
                         timings.STATUS_RECEIVING_FROM).first -
                        (timings.STATUS_CONNECTED_TO ||
                         timings.STATUS_SENDING_TO).last;
    }
    else {
      harTimings.send = -1;
    }

    if (timings.RESPONSE_START) {
      harTimings.wait = timings.RESPONSE_START.first -
                        (timings.REQUEST_BODY_SENT ||
                         timings.STATUS_SENDING_TO).last;
    }
    else {
      harTimings.wait = -1;
    }

    if (timings.RESPONSE_START && timings.RESPONSE_COMPLETE) {
      harTimings.receive = timings.RESPONSE_COMPLETE.last -
                           timings.RESPONSE_START.first;
    }
    else {
      harTimings.receive = -1;
    }

    let totalTime = 0;
    for (let timing in harTimings) {
      let time = Math.max(Math.round(harTimings[timing] / 1000), -1);
      harTimings[timing] = time;
      if (time > -1) {
        totalTime += time;
      }
    }

    return {
      total: totalTime,
      timings: harTimings,
    };
  },

  /**
   * Suspend Web Console activity. This is called when all Web Consoles are
   * closed.
   */
  destroy: function NM_destroy()
  {
    if (Services.appinfo.processType != Ci.nsIXULRuntime.PROCESS_TYPE_CONTENT) {
      Services.obs.removeObserver(this._httpResponseExaminer,
                                  "http-on-examine-response");
    }

    gActivityDistributor.removeObserver(this);

    this.openRequests = {};
    this.openResponses = {};
    this.owner = null;
    this.window = null;
    this.topFrame = null;
  },
}; // NetworkMonitor.prototype


/**
 * The NetworkMonitorChild is used to proxy all of the network activity of the
 * child app process from the main process. The child WebConsoleActor creates an
 * instance of this object.
 *
 * Network requests for apps happen in the main process. As such,
 * a NetworkMonitor instance is used by the WebappsActor in the main process to
 * log the network requests for this child process.
 *
 * The main process creates NetworkEventActorProxy instances per request. These
 * send the data to this object using the nsIMessageManager. Here we proxy the
 * data to the WebConsoleActor or to a NetworkEventActor.
 *
 * @constructor
 * @param number appId
 *        The web appId of the child process.
 * @param nsIMessageManager messageManager
 *        The nsIMessageManager to use to communicate with the parent process.
 * @param string connID
 *        The connection ID to use for send messages to the parent process.
 * @param object owner
 *        The WebConsoleActor that is listening for the network requests.
 */
function NetworkMonitorChild(appId, messageManager, connID, owner) {
  this.appId = appId;
  this.connID = connID;
  this.owner = owner;
  this._messageManager = messageManager;
  this._onNewEvent = this._onNewEvent.bind(this);
  this._onUpdateEvent = this._onUpdateEvent.bind(this);
  this._netEvents = new Map();
}
exports.NetworkMonitorChild = NetworkMonitorChild;

NetworkMonitorChild.prototype = {
  appId: null,
  owner: null,
  _netEvents: null,
  _saveRequestAndResponseBodies: false,

  get saveRequestAndResponseBodies() {
    return this._saveRequestAndResponseBodies;
  },

  set saveRequestAndResponseBodies(val) {
    this._saveRequestAndResponseBodies = val;

    this._messageManager.sendAsyncMessage("debug:netmonitor:" + this.connID, {
      appId: this.appId,
      action: "setPreferences",
      preferences: {
        saveRequestAndResponseBodies: this._saveRequestAndResponseBodies,
      },
    });
  },

  init: function() {
    let mm = this._messageManager;
    mm.addMessageListener("debug:netmonitor:" + this.connID + ":newEvent",
                          this._onNewEvent);
    mm.addMessageListener("debug:netmonitor:" + this.connID + ":updateEvent",
                          this._onUpdateEvent);
    mm.sendAsyncMessage("debug:netmonitor:" + this.connID, {
      appId: this.appId,
      action: "start",
    });
  },

  _onNewEvent: DevToolsUtils.makeInfallible(function _onNewEvent(msg) {
    let {id, event} = msg.data;
    let actor = this.owner.onNetworkEvent(event);
    this._netEvents.set(id, Cu.getWeakReference(actor));
  }),

  _onUpdateEvent: DevToolsUtils.makeInfallible(function _onUpdateEvent(msg) {
    let {id, method, args} = msg.data;
    let weakActor = this._netEvents.get(id);
    let actor = weakActor ? weakActor.get() : null;
    if (!actor) {
      Cu.reportError("Received debug:netmonitor:updateEvent for unknown event ID: " + id);
      return;
    }
    if (!(method in actor)) {
      Cu.reportError("Received debug:netmonitor:updateEvent unsupported method: " + method);
      return;
    }
    actor[method].apply(actor, args);
  }),

  destroy: function() {
    let mm = this._messageManager;
    try {
      mm.removeMessageListener("debug:netmonitor:" + this.connID + ":newEvent",
                               this._onNewEvent);
      mm.removeMessageListener("debug:netmonitor:" + this.connID + ":updateEvent",
                               this._onUpdateEvent);
    } catch(e) {
      // On b2g, when registered to a new root docshell,
      // all message manager functions throw when trying to call them during
      // message-manager-disconnect event.
      // As there is no attribute/method on message manager to know
      // if they are still usable or not, we can only catch the exception...
    }
    this._netEvents.clear();
    this._messageManager = null;
    this.owner = null;
  },
}; // NetworkMonitorChild.prototype

/**
 * The NetworkEventActorProxy is used to send network request information from
 * the main process to the child app process. One proxy is used per request.
 * Similarly, one NetworkEventActor in the child app process is used per
 * request. The client receives all network logs from the child actors.
 *
 * The child process has a NetworkMonitorChild instance that is listening for
 * all network logging from the main process. The net monitor shim is used to
 * proxy the data to the WebConsoleActor instance of the child process.
 *
 * @constructor
 * @param nsIMessageManager messageManager
 *        The message manager for the child app process. This is used for
 *        communication with the NetworkMonitorChild instance of the process.
 * @param string connID
 *        The connection ID to use to send messages to the child process.
 */
function NetworkEventActorProxy(messageManager, connID) {
  this.id = gSequenceId();
  this.connID = connID;
  this.messageManager = messageManager;
}
exports.NetworkEventActorProxy = NetworkEventActorProxy;

NetworkEventActorProxy.methodFactory = function(method) {
  return DevToolsUtils.makeInfallible(function() {
    let args = Array.slice(arguments);
    let mm = this.messageManager;
    mm.sendAsyncMessage("debug:netmonitor:" + this.connID + ":updateEvent", {
      id: this.id,
      method: method,
      args: args,
    });
  }, "NetworkEventActorProxy." + method);
};

NetworkEventActorProxy.prototype = {
  /**
   * Initialize the network event. This method sends the network request event
   * to the content process.
   *
   * @param object event
   *        Object describing the network request.
   * @return object
   *         This object.
   */
  init: DevToolsUtils.makeInfallible(function(event)
  {
    let mm = this.messageManager;
    mm.sendAsyncMessage("debug:netmonitor:" + this.connID + ":newEvent", {
      id: this.id,
      event: event,
    });
    return this;
  }),
};

(function() {
  // Listeners for new network event data coming from the NetworkMonitor.
  let methods = ["addRequestHeaders", "addRequestCookies", "addRequestPostData",
                 "addResponseStart", "addSecurityInfo", "addResponseHeaders",
                 "addResponseCookies", "addResponseContent", "addEventTimings"];
  let factory = NetworkEventActorProxy.methodFactory;
  for (let method of methods) {
    NetworkEventActorProxy.prototype[method] = factory(method);
  }
})();


/**
 * The NetworkMonitor manager used by the Webapps actor in the main process.
 * This object uses the message manager to listen for requests from the child
 * process to start/stop the network monitor.
 *
 * @constructor
 * @param nsIDOMElement frame
 *        The browser frame to work with (mozbrowser).
 * @param string id
 *        Instance identifier to use for messages.
 */
function NetworkMonitorManager(frame, id)
{
  this.id = id;
  let mm = frame.QueryInterface(Ci.nsIFrameLoaderOwner).frameLoader.messageManager;
  this.messageManager = mm;
  this.frame = frame;
  this.onNetMonitorMessage = this.onNetMonitorMessage.bind(this);
  this.onNetworkEvent = this.onNetworkEvent.bind(this);

  mm.addMessageListener("debug:netmonitor:" + id, this.onNetMonitorMessage);
}
exports.NetworkMonitorManager = NetworkMonitorManager;

NetworkMonitorManager.prototype = {
  netMonitor: null,
  frame: null,
  messageManager: null,

  /**
   * Handler for "debug:monitor" messages received through the message manager
   * from the content process.
   *
   * @param object msg
   *        Message from the content.
   */
  onNetMonitorMessage: DevToolsUtils.makeInfallible(function _onNetMonitorMessage(msg) {
    let { action, appId } = msg.json;
    // Pipe network monitor data from parent to child via the message manager.
    switch (action) {
      case "start":
        if (!this.netMonitor) {
          this.netMonitor = new NetworkMonitor({
            topFrame: this.frame,
            appId: appId,
          }, this);
          this.netMonitor.init();
        }
        break;

      case "setPreferences": {
        let {preferences} = msg.json;
        for (let key of Object.keys(preferences)) {
          if (key == "saveRequestAndResponseBodies" && this.netMonitor) {
            this.netMonitor.saveRequestAndResponseBodies = preferences[key];
          }
        }
        break;
      }

      case "stop":
        if (this.netMonitor) {
          this.netMonitor.destroy();
          this.netMonitor = null;
        }
        break;

      case "disconnect":
        this.destroy();
        break;
    }
  }),

  /**
   * Handler for new network requests. This method is invoked by the current
   * NetworkMonitor instance.
   *
   * @param object event
   *        Object describing the network request.
   * @return object
   *         A NetworkEventActorProxy instance which is notified when further
   *         data about the request is available.
   */
  onNetworkEvent: DevToolsUtils.makeInfallible(function _onNetworkEvent(event) {
    return new NetworkEventActorProxy(this.messageManager, this.id).init(event);
  }),

  destroy: function()
  {
    if (this.messageManager) {
      this.messageManager.removeMessageListener("debug:netmonitor:" + this.id,
                                                this.onNetMonitorMessage);
    }
    this.messageManager = null;
    this.filters = null;

    if (this.netMonitor) {
      this.netMonitor.destroy();
      this.netMonitor = null;
    }
  },
}; // NetworkMonitorManager.prototype


/**
 * A WebProgressListener that listens for location changes.
 *
 * This progress listener is used to track file loads and other kinds of
 * location changes.
 *
 * @constructor
 * @param object aWindow
 *        The window for which we need to track location changes.
 * @param object aOwner
 *        The listener owner which needs to implement two methods:
 *        - onFileActivity(aFileURI)
 *        - onLocationChange(aState, aTabURI, aPageTitle)
 */
function ConsoleProgressListener(aWindow, aOwner)
{
  this.window = aWindow;
  this.owner = aOwner;
}
exports.ConsoleProgressListener = ConsoleProgressListener;

ConsoleProgressListener.prototype = {
  /**
   * Constant used for startMonitor()/stopMonitor() that tells you want to
   * monitor file loads.
   */
  MONITOR_FILE_ACTIVITY: 1,

  /**
   * Constant used for startMonitor()/stopMonitor() that tells you want to
   * monitor page location changes.
   */
  MONITOR_LOCATION_CHANGE: 2,

  /**
   * Tells if you want to monitor file activity.
   * @private
   * @type boolean
   */
  _fileActivity: false,

  /**
   * Tells if you want to monitor location changes.
   * @private
   * @type boolean
   */
  _locationChange: false,

  /**
   * Tells if the console progress listener is initialized or not.
   * @private
   * @type boolean
   */
  _initialized: false,

  _webProgress: null,

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener,
                                         Ci.nsISupportsWeakReference]),

  /**
   * Initialize the ConsoleProgressListener.
   * @private
   */
  _init: function CPL__init()
  {
    if (this._initialized) {
      return;
    }

    this._webProgress = this.window.QueryInterface(Ci.nsIInterfaceRequestor)
                        .getInterface(Ci.nsIWebNavigation)
                        .QueryInterface(Ci.nsIWebProgress);
    this._webProgress.addProgressListener(this,
                                          Ci.nsIWebProgress.NOTIFY_STATE_ALL);

    this._initialized = true;
  },

  /**
   * Start a monitor/tracker related to the current nsIWebProgressListener
   * instance.
   *
   * @param number aMonitor
   *        Tells what you want to track. Available constants:
   *        - this.MONITOR_FILE_ACTIVITY
   *          Track file loads.
   *        - this.MONITOR_LOCATION_CHANGE
   *          Track location changes for the top window.
   */
  startMonitor: function CPL_startMonitor(aMonitor)
  {
    switch (aMonitor) {
      case this.MONITOR_FILE_ACTIVITY:
        this._fileActivity = true;
        break;
      case this.MONITOR_LOCATION_CHANGE:
        this._locationChange = true;
        break;
      default:
        throw new Error("ConsoleProgressListener: unknown monitor type " +
                        aMonitor + "!");
    }
    this._init();
  },

  /**
   * Stop a monitor.
   *
   * @param number aMonitor
   *        Tells what you want to stop tracking. See this.startMonitor() for
   *        the list of constants.
   */
  stopMonitor: function CPL_stopMonitor(aMonitor)
  {
    switch (aMonitor) {
      case this.MONITOR_FILE_ACTIVITY:
        this._fileActivity = false;
        break;
      case this.MONITOR_LOCATION_CHANGE:
        this._locationChange = false;
        break;
      default:
        throw new Error("ConsoleProgressListener: unknown monitor type " +
                        aMonitor + "!");
    }

    if (!this._fileActivity && !this._locationChange) {
      this.destroy();
    }
  },

  onStateChange:
  function CPL_onStateChange(aProgress, aRequest, aState, aStatus)
  {
    if (!this.owner) {
      return;
    }

    if (this._fileActivity) {
      this._checkFileActivity(aProgress, aRequest, aState, aStatus);
    }

    if (this._locationChange) {
      this._checkLocationChange(aProgress, aRequest, aState, aStatus);
    }
  },

  /**
   * Check if there is any file load, given the arguments of
   * nsIWebProgressListener.onStateChange. If the state change tells that a file
   * URI has been loaded, then the remote Web Console instance is notified.
   * @private
   */
  _checkFileActivity:
  function CPL__checkFileActivity(aProgress, aRequest, aState, aStatus)
  {
    if (!(aState & Ci.nsIWebProgressListener.STATE_START)) {
      return;
    }

    let uri = null;
    if (aRequest instanceof Ci.imgIRequest) {
      let imgIRequest = aRequest.QueryInterface(Ci.imgIRequest);
      uri = imgIRequest.URI;
    }
    else if (aRequest instanceof Ci.nsIChannel) {
      let nsIChannel = aRequest.QueryInterface(Ci.nsIChannel);
      uri = nsIChannel.URI;
    }

    if (!uri || !uri.schemeIs("file") && !uri.schemeIs("ftp")) {
      return;
    }

    this.owner.onFileActivity(uri.spec);
  },

  /**
   * Check if the current window.top location is changing, given the arguments
   * of nsIWebProgressListener.onStateChange. If that is the case, the remote
   * Web Console instance is notified.
   * @private
   */
  _checkLocationChange:
  function CPL__checkLocationChange(aProgress, aRequest, aState, aStatus)
  {
    let isStart = aState & Ci.nsIWebProgressListener.STATE_START;
    let isStop = aState & Ci.nsIWebProgressListener.STATE_STOP;
    let isNetwork = aState & Ci.nsIWebProgressListener.STATE_IS_NETWORK;
    let isWindow = aState & Ci.nsIWebProgressListener.STATE_IS_WINDOW;

    // Skip non-interesting states.
    if (!isNetwork || !isWindow || aProgress.DOMWindow != this.window) {
      return;
    }

    if (isStart && aRequest instanceof Ci.nsIChannel) {
      this.owner.onLocationChange("start", aRequest.URI.spec, "");
    }
    else if (isStop) {
      this.owner.onLocationChange("stop", this.window.location.href,
                                  this.window.document.title);
    }
  },

  onLocationChange: function() {},
  onStatusChange: function() {},
  onProgressChange: function() {},
  onSecurityChange: function() {},

  /**
   * Destroy the ConsoleProgressListener.
   */
  destroy: function CPL_destroy()
  {
    if (!this._initialized) {
      return;
    }

    this._initialized = false;
    this._fileActivity = false;
    this._locationChange = false;

    try {
      this._webProgress.removeProgressListener(this);
    }
    catch (ex) {
      // This can throw during browser shutdown.
    }

    this._webProgress = null;
    this.window = null;
    this.owner = null;
  },
}; // ConsoleProgressListener.prototype

function gSequenceId() { return gSequenceId.n++; }
gSequenceId.n = 1;
