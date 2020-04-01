/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { EventEmitter } = ChromeUtils.import(
  "resource://gre/modules/EventEmitter.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { NetUtil } = ChromeUtils.import("resource://gre/modules/NetUtil.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { CommonUtils } = ChromeUtils.import(
  "resource://services-common/utils.js"
);
const { ChannelEventSinkFactory } = ChromeUtils.import(
  "chrome://remote/content/domains/parent/network/ChannelEventSink.jsm"
);

const CC = Components.Constructor;

const BinaryInputStream = CC(
  "@mozilla.org/binaryinputstream;1",
  "nsIBinaryInputStream",
  "setInputStream"
);
const BinaryOutputStream = CC(
  "@mozilla.org/binaryoutputstream;1",
  "nsIBinaryOutputStream",
  "setOutputStream"
);
const StorageStream = CC(
  "@mozilla.org/storagestream;1",
  "nsIStorageStream",
  "init"
);
XPCOMUtils.defineLazyServiceGetter(
  this,
  "gActivityDistributor",
  "@mozilla.org/network/http-activity-distributor;1",
  "nsIHttpActivityDistributor"
);

// Cap response storage with 100Mb per tracked tab.
const MAX_RESPONSE_STORAGE_SIZE = 100 * 1024 * 1024;

class NetworkObserver {
  constructor() {
    EventEmitter.decorate(this);
    this._browserSessionCount = new Map();
    gActivityDistributor.addObserver(this);
    ChannelEventSinkFactory.getService().registerCollector(this);

    this._redirectMap = new Map();

    // Request interception state.
    this._browserSuspendedChannels = new Map();
    this._extraHTTPHeaders = new Map();
    this._browserResponseStorages = new Map();

    this._onRequest = this._onRequest.bind(this);
    this._onExamineResponse = this._onResponse.bind(
      this,
      false /* fromCache */
    );
    this._onCachedResponse = this._onResponse.bind(this, true /* fromCache */);
    Services.obs.addObserver(this._onRequest, "http-on-modify-request");
    Services.obs.addObserver(
      this._onExamineResponse,
      "http-on-examine-response"
    );
    Services.obs.addObserver(
      this._onCachedResponse,
      "http-on-examine-cached-response"
    );
    Services.obs.addObserver(
      this._onCachedResponse,
      "http-on-examine-merged-response"
    );
  }

  dispose() {
    gActivityDistributor.removeObserver(this);
    ChannelEventSinkFactory.getService().unregisterCollector(this);

    Services.obs.removeObserver(this._onRequest, "http-on-modify-request");
    Services.obs.removeObserver(
      this._onExamineResponse,
      "http-on-examine-response"
    );
    Services.obs.removeObserver(
      this._onCachedResponse,
      "http-on-examine-cached-response"
    );
    Services.obs.removeObserver(
      this._onCachedResponse,
      "http-on-examine-merged-response"
    );
  }

  setExtraHTTPHeaders(browser, headers) {
    if (!headers) {
      this._extraHTTPHeaders.delete(browser);
    } else {
      this._extraHTTPHeaders.set(browser, headers);
    }
  }

  enableRequestInterception(browser) {
    if (!this._browserSuspendedChannels.has(browser)) {
      this._browserSuspendedChannels.set(browser, new Map());
    }
  }

  disableRequestInterception(browser) {
    const suspendedChannels = this._browserSuspendedChannels.get(browser);
    if (!suspendedChannels) {
      return;
    }
    this._browserSuspendedChannels.delete(browser);
    for (const channel of suspendedChannels.values()) {
      channel.resume();
    }
  }

  resumeSuspendedRequest(browser, requestId, headers) {
    const suspendedChannels = this._browserSuspendedChannels.get(browser);
    if (!suspendedChannels) {
      throw new Error(`Request interception is not enabled`);
    }
    const httpChannel = suspendedChannels.get(requestId);
    if (!httpChannel) {
      throw new Error(`Cannot find request "${requestId}"`);
    }
    if (headers) {
      // 1. Clear all previous headers.
      for (const header of requestHeaders(httpChannel)) {
        httpChannel.setRequestHeader(header.name, "", false /* merge */);
      }
      // 2. Set new headers.
      for (const header of headers) {
        httpChannel.setRequestHeader(
          header.name,
          header.value,
          false /* merge */
        );
      }
    }
    suspendedChannels.delete(requestId);
    httpChannel.resume();
  }

  getResponseBody(browser, requestId) {
    const responseStorage = this._browserResponseStorages.get(browser);
    if (!responseStorage) {
      throw new Error("Responses are not tracked for the given browser");
    }
    return responseStorage.getBase64EncodedResponse(requestId);
  }

  abortSuspendedRequest(browser, aRequestId) {
    const suspendedChannels = this._browserSuspendedChannels.get(browser);
    if (!suspendedChannels) {
      throw new Error(`Request interception is not enabled`);
    }
    const httpChannel = suspendedChannels.get(aRequestId);
    if (!httpChannel) {
      throw new Error(`Cannot find request "${aRequestId}"`);
    }
    suspendedChannels.delete(aRequestId);
    httpChannel.cancel(Cr.NS_ERROR_FAILURE);
    httpChannel.resume();
    this.emit("requestfailed", httpChannel, {
      requestId: requestId(httpChannel),
      errorCode: getNetworkErrorStatusText(httpChannel.status),
    });
  }

  _onChannelRedirect(oldChannel, newChannel) {
    // We can be called with any nsIChannel, but are interested only in HTTP channels
    try {
      oldChannel.QueryInterface(Ci.nsIHttpChannel);
      newChannel.QueryInterface(Ci.nsIHttpChannel);
    } catch (ex) {
      return;
    }

    const httpChannel = oldChannel.QueryInterface(Ci.nsIHttpChannel);
    const loadContext = getLoadContext(httpChannel);
    if (
      !loadContext ||
      !this._browserSessionCount.has(loadContext.topFrameElement)
    ) {
      return;
    }
    this._redirectMap.set(newChannel, oldChannel);
  }

  observeActivity(
    channel,
    activityType,
    activitySubtype,
    timestamp,
    extraSizeData,
    extraStringData
  ) {
    if (
      activityType !== Ci.nsIHttpActivityObserver.ACTIVITY_TYPE_HTTP_TRANSACTION
    ) {
      return;
    }
    try {
      channel.QueryInterface(Ci.nsIHttpChannel);
    } catch (ex) {
      return;
    }

    const httpChannel = channel.QueryInterface(Ci.nsIHttpChannel);
    const loadContext = getLoadContext(httpChannel);
    if (
      !loadContext ||
      !this._browserSessionCount.has(loadContext.topFrameElement)
    ) {
      return;
    }
    if (
      activitySubtype !==
      Ci.nsIHttpActivityObserver.ACTIVITY_SUBTYPE_TRANSACTION_CLOSE
    ) {
      return;
    }
    this.emit("requestfinished", httpChannel, {
      requestId: requestId(httpChannel),
    });
  }

  _onRequest(channel, topic) {
    try {
      channel.QueryInterface(Ci.nsIHttpChannel);
    } catch (ex) {
      return;
    }
    const httpChannel = channel.QueryInterface(Ci.nsIHttpChannel);
    const loadContext = getLoadContext(httpChannel);
    if (
      !loadContext ||
      !this._browserSessionCount.has(loadContext.topFrameElement) ||
      !loadContext.topFrameElement.currentURI
    ) {
      return;
    }
    const extraHeaders = this._extraHTTPHeaders.get(
      loadContext.topFrameElement
    );
    if (extraHeaders) {
      for (const header of extraHeaders) {
        httpChannel.setRequestHeader(
          header.name,
          header.value,
          false /* merge */
        );
      }
    }
    const causeType = httpChannel.loadInfo
      ? httpChannel.loadInfo.externalContentPolicyType
      : Ci.nsIContentPolicy.TYPE_OTHER;
    const suspendedChannels = this._browserSuspendedChannels.get(
      loadContext.topFrameElement
    );
    if (suspendedChannels) {
      httpChannel.suspend();
      suspendedChannels.set(requestId(httpChannel), httpChannel);
    }
    const oldChannel = this._redirectMap.get(httpChannel);
    this._redirectMap.delete(httpChannel);

    // Install response body hooks.
    new ResponseBodyListener(this, loadContext.topFrameElement, httpChannel);

    this.emit("request", httpChannel, {
      url: httpChannel.URI.spec,
      suspended: suspendedChannels ? true : undefined,
      requestId: requestId(httpChannel),
      redirectedFrom: oldChannel ? requestId(oldChannel) : undefined,
      postData: readRequestPostData(httpChannel),
      headers: requestHeaders(httpChannel),
      method: httpChannel.requestMethod,
      isNavigationRequest: httpChannel.isMainDocumentChannel,
      cause: causeTypeToString(causeType),
    });
  }

  _onResponse(fromCache, httpChannel, topic) {
    const loadContext = getLoadContext(httpChannel);
    if (
      !loadContext ||
      !this._browserSessionCount.has(loadContext.topFrameElement)
    ) {
      return;
    }
    httpChannel.QueryInterface(Ci.nsIHttpChannelInternal);
    const headers = [];
    httpChannel.visitResponseHeaders({
      visitHeader: (name, value) => headers.push({ name, value }),
    });

    let remoteIPAddress = undefined;
    let remotePort = undefined;
    try {
      remoteIPAddress = httpChannel.remoteAddress;
      remotePort = httpChannel.remotePort;
    } catch (e) {
      // remoteAddress is not defined for cached requests.
    }
    this.emit("response", httpChannel, {
      requestId: requestId(httpChannel),
      securityDetails: getSecurityDetails(httpChannel),
      fromCache,
      headers,
      remoteIPAddress,
      remotePort,
      status: httpChannel.responseStatus,
      statusText: httpChannel.responseStatusText,
    });
  }

  _onResponseFinished(browser, httpChannel, body) {
    const responseStorage = this._browserResponseStorages.get(browser);
    if (!responseStorage) {
      return;
    }
    responseStorage.addResponseBody(httpChannel, body);
    this.emit("requestfinished", httpChannel, {
      requestId: requestId(httpChannel),
    });
  }

  startTrackingBrowserNetwork(browser) {
    const value = this._browserSessionCount.get(browser) || 0;
    this._browserSessionCount.set(browser, value + 1);
    if (value === 0) {
      this._browserResponseStorages.set(
        browser,
        new ResponseStorage(
          MAX_RESPONSE_STORAGE_SIZE,
          MAX_RESPONSE_STORAGE_SIZE / 10
        )
      );
    }
    return () => this.stopTrackingBrowserNetwork(browser);
  }

  stopTrackingBrowserNetwork(browser) {
    const value = this._browserSessionCount.get(browser);
    if (value) {
      this._browserSessionCount.set(browser, value - 1);
    } else {
      this._browserSessionCount.delete(browser);
      this._browserResponseStorages.delete(browser);
    }
  }
}

const protocolVersionNames = {
  [Ci.nsITransportSecurityInfo.TLS_VERSION_1]: "TLS 1",
  [Ci.nsITransportSecurityInfo.TLS_VERSION_1_1]: "TLS 1.1",
  [Ci.nsITransportSecurityInfo.TLS_VERSION_1_2]: "TLS 1.2",
  [Ci.nsITransportSecurityInfo.TLS_VERSION_1_3]: "TLS 1.3",
};

function getSecurityDetails(httpChannel) {
  const securityInfo = httpChannel.securityInfo;
  if (!securityInfo) {
    return null;
  }
  securityInfo.QueryInterface(Ci.nsITransportSecurityInfo);
  if (!securityInfo.serverCert) {
    return null;
  }
  return {
    protocol: protocolVersionNames[securityInfo.protocolVersion] || "<unknown>",
    subjectName: securityInfo.serverCert.commonName,
    issuer: securityInfo.serverCert.issuerCommonName,
    // Convert to seconds.
    validFrom: securityInfo.serverCert.validity.notBefore / 1000 / 1000,
    validTo: securityInfo.serverCert.validity.notAfter / 1000 / 1000,
  };
}

function readRequestPostData(httpChannel) {
  if (!(httpChannel instanceof Ci.nsIUploadChannel)) {
    return undefined;
  }
  const iStream = httpChannel.uploadStream;
  if (!iStream) {
    return undefined;
  }
  const isSeekableStream = iStream instanceof Ci.nsISeekableStream;

  let prevOffset;
  if (isSeekableStream) {
    prevOffset = iStream.tell();
    iStream.seek(Ci.nsISeekableStream.NS_SEEK_SET, 0);
  }

  // Read data from the stream.
  let text = undefined;
  try {
    text = NetUtil.readInputStreamToString(iStream, iStream.available());
    const converter = Cc[
      "@mozilla.org/intl/scriptableunicodeconverter"
    ].createInstance(Ci.nsIScriptableUnicodeConverter);
    converter.charset = "UTF-8";
    text = converter.ConvertToUnicode(text);
  } catch (err) {
    text = undefined;
  }

  // Seek locks the file, so seek to the beginning only if necko hasn"t
  // read it yet, since necko doesn"t seek to 0 before reading (at lest
  // not till 459384 is fixed).
  if (isSeekableStream && prevOffset == 0) {
    iStream.seek(Ci.nsISeekableStream.NS_SEEK_SET, 0);
  }
  return text;
}

function getLoadContext(httpChannel) {
  let loadContext = null;
  try {
    if (httpChannel.notificationCallbacks) {
      loadContext = httpChannel.notificationCallbacks.getInterface(
        Ci.nsILoadContext
      );
    }
  } catch (e) {}
  try {
    if (!loadContext && httpChannel.loadGroup) {
      loadContext = httpChannel.loadGroup.notificationCallbacks.getInterface(
        Ci.nsILoadContext
      );
    }
  } catch (e) {}
  return loadContext;
}

function requestId(httpChannel) {
  return httpChannel.channelId + "";
}

function requestHeaders(httpChannel) {
  const headers = [];
  httpChannel.visitRequestHeaders({
    visitHeader: (name, value) => headers.push({ name, value }),
  });
  return headers;
}

function causeTypeToString(causeType) {
  for (let key in Ci.nsIContentPolicy) {
    if (Ci.nsIContentPolicy[key] === causeType) {
      return key;
    }
  }
  return "TYPE_OTHER";
}

class ResponseStorage {
  constructor(maxTotalSize, maxResponseSize) {
    this._totalSize = 0;
    this._maxResponseSize = maxResponseSize;
    this._maxTotalSize = maxTotalSize;
    this._responses = new Map();
  }

  addResponseBody(httpChannel, body) {
    if (body.length > this._maxResponseSize) {
      this._responses.set(requestId, {
        evicted: true,
        body: "",
      });
      return;
    }
    let encodings = [];
    if (
      httpChannel instanceof Ci.nsIEncodedChannel &&
      httpChannel.contentEncodings &&
      !httpChannel.applyConversion
    ) {
      const encodingHeader = httpChannel.getResponseHeader("Content-Encoding");
      encodings = encodingHeader.split(/\s*\t*,\s*\t*/);
    }
    this._responses.set(requestId(httpChannel), { body, encodings });
    this._totalSize += body.length;
    if (this._totalSize > this._maxTotalSize) {
      for (let [, response] of this._responses) {
        this._totalSize -= response.body.length;
        response.body = "";
        response.evicted = true;
        if (this._totalSize < this._maxTotalSize) {
          break;
        }
      }
    }
  }

  getBase64EncodedResponse(requestId) {
    const response = this._responses.get(requestId);
    if (!response) {
      throw new Error(`Request "${requestId}" is not found`);
    }
    if (response.evicted) {
      return { base64body: "", evicted: true };
    }
    let result = response.body;
    if (response.encodings && response.encodings.length) {
      for (const encoding of response.encodings) {
        result = CommonUtils.convertString(result, encoding, "uncompressed");
      }
    }
    return { base64body: btoa(result) };
  }
}

class ResponseBodyListener {
  constructor(networkObserver, browser, httpChannel) {
    this._networkObserver = networkObserver;
    this._browser = browser;
    this._httpChannel = httpChannel;
    this._chunks = [];
    this.QueryInterface = ChromeUtils.generateQI([Ci.nsIStreamListener]);
    httpChannel.QueryInterface(Ci.nsITraceableChannel);
    this.originalListener = httpChannel.setNewListener(this);
  }

  onDataAvailable(aRequest, aInputStream, aOffset, aCount) {
    const iStream = new BinaryInputStream(aInputStream);
    const sStream = new StorageStream(8192, aCount, null);
    const oStream = new BinaryOutputStream(sStream.getOutputStream(0));

    // Copy received data as they come.
    const data = iStream.readBytes(aCount);
    this._chunks.push(data);

    oStream.writeBytes(data, aCount);
    this.originalListener.onDataAvailable(
      aRequest,
      sStream.newInputStream(0),
      aOffset,
      aCount
    );
  }

  onStartRequest(aRequest) {
    this.originalListener.onStartRequest(aRequest);
  }

  onStopRequest(aRequest, aStatusCode) {
    this.originalListener.onStopRequest(aRequest, aStatusCode);
    const body = this._chunks.join("");
    delete this._chunks;
    this._networkObserver._onResponseFinished(
      this._browser,
      this._httpChannel,
      body
    );
  }
}

function getNetworkErrorStatusText(status) {
  if (!status) {
    return null;
  }
  for (const key of Object.keys(Cr)) {
    if (Cr[key] === status) {
      return key;
    }
  }
  // Security module. The following is taken from
  // https://developer.mozilla.org/en-US/docs/Web/API/XMLHttpRequest/How_to_check_the_secruity_state_of_an_XMLHTTPRequest_over_SSL
  if ((status & 0xff0000) === 0x5a0000) {
    // NSS_SEC errors (happen below the base value because of negative vals)
    if (
      (status & 0xffff) <
      Math.abs(Ci.nsINSSErrorsService.NSS_SEC_ERROR_BASE)
    ) {
      // The bases are actually negative, so in our positive numeric space, we
      // need to subtract the base off our value.
      const nssErr =
        Math.abs(Ci.nsINSSErrorsService.NSS_SEC_ERROR_BASE) - (status & 0xffff);
      switch (nssErr) {
        case 11:
          return "SEC_ERROR_EXPIRED_CERTIFICATE";
        case 12:
          return "SEC_ERROR_REVOKED_CERTIFICATE";
        case 13:
          return "SEC_ERROR_UNKNOWN_ISSUER";
        case 20:
          return "SEC_ERROR_UNTRUSTED_ISSUER";
        case 21:
          return "SEC_ERROR_UNTRUSTED_CERT";
        case 36:
          return "SEC_ERROR_CA_CERT_INVALID";
        case 90:
          return "SEC_ERROR_INADEQUATE_KEY_USAGE";
        case 176:
          return "SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED";
        default:
          return "SEC_ERROR_UNKNOWN";
      }
    }
    const sslErr =
      Math.abs(Ci.nsINSSErrorsService.NSS_SSL_ERROR_BASE) - (status & 0xffff);
    switch (sslErr) {
      case 3:
        return "SSL_ERROR_NO_CERTIFICATE";
      case 4:
        return "SSL_ERROR_BAD_CERTIFICATE";
      case 8:
        return "SSL_ERROR_UNSUPPORTED_CERTIFICATE_TYPE";
      case 9:
        return "SSL_ERROR_UNSUPPORTED_VERSION";
      case 12:
        return "SSL_ERROR_BAD_CERT_DOMAIN";
      default:
        return "SSL_ERROR_UNKNOWN";
    }
  }
  return "<unknown error>";
}

var EXPORTED_SYMBOLS = ["NetworkObserver"];
this.NetworkObserver = NetworkObserver;
