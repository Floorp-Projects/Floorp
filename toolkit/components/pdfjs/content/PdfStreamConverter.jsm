/* Copyright 2012 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

"use strict";

var EXPORTED_SYMBOLS = ["PdfStreamConverter"];

const PDFJS_EVENT_ID = "pdf.js.message";
const PREF_PREFIX = "pdfjs";
const PDF_VIEWER_ORIGIN = "resource://pdf.js";
const PDF_VIEWER_WEB_PAGE = "resource://pdf.js/web/viewer.html";
const MAX_NUMBER_OF_PREFS = 50;
const MAX_STRING_PREF_LENGTH = 128;
const PDF_CONTENT_TYPE = "application/pdf";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "AsyncPrefs",
  "resource://gre/modules/AsyncPrefs.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "NetUtil",
  "resource://gre/modules/NetUtil.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "NetworkManager",
  "resource://pdf.js/PdfJsNetwork.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "PdfJsTelemetry",
  "resource://pdf.js/PdfJsTelemetry.jsm"
);

ChromeUtils.defineModuleGetter(this, "PdfJs", "resource://pdf.js/PdfJs.jsm");

ChromeUtils.defineModuleGetter(
  this,
  "PdfSandbox",
  "resource://pdf.js/PdfSandbox.jsm"
);

XPCOMUtils.defineLazyGlobalGetters(this, ["XMLHttpRequest"]);

var Svc = {};
XPCOMUtils.defineLazyServiceGetter(
  Svc,
  "mime",
  "@mozilla.org/mime;1",
  "nsIMIMEService"
);
XPCOMUtils.defineLazyServiceGetter(
  Svc,
  "handlers",
  "@mozilla.org/uriloader/handler-service;1",
  "nsIHandlerService"
);

XPCOMUtils.defineLazyGetter(this, "gOurBinary", () => {
  let file = Services.dirsvc.get("XREExeF", Ci.nsIFile);
  // Make sure to get the .app on macOS
  if (AppConstants.platform == "macosx") {
    while (file) {
      if (/\.app\/?$/i.test(file.leafName)) {
        break;
      }
      file = file.parent;
    }
  }
  return file;
});

function getBoolPref(pref, def) {
  try {
    return Services.prefs.getBoolPref(pref);
  } catch (ex) {
    return def;
  }
}

function getIntPref(pref, def) {
  try {
    return Services.prefs.getIntPref(pref);
  } catch (ex) {
    return def;
  }
}

function getStringPref(pref, def) {
  try {
    return Services.prefs.getStringPref(pref);
  } catch (ex) {
    return def;
  }
}

function log(aMsg) {
  if (!getBoolPref(PREF_PREFIX + ".pdfBugEnabled", false)) {
    return;
  }
  var msg = "PdfStreamConverter.js: " + (aMsg.join ? aMsg.join("") : aMsg);
  Services.console.logStringMessage(msg);
  dump(msg + "\n");
}

function getDOMWindow(aChannel, aPrincipal) {
  var requestor = aChannel.notificationCallbacks
    ? aChannel.notificationCallbacks
    : aChannel.loadGroup.notificationCallbacks;
  var win = requestor.getInterface(Ci.nsIDOMWindow);
  // Ensure the window wasn't navigated to something that is not PDF.js.
  if (!win.document.nodePrincipal.equals(aPrincipal)) {
    return null;
  }
  return win;
}

function getActor(window) {
  try {
    return window.windowGlobalChild.getActor("Pdfjs");
  } catch (ex) {
    return null;
  }
}

function getLocalizedStrings(path) {
  var stringBundle = Services.strings.createBundle(
    "chrome://pdf.js/locale/" + path
  );

  var map = {};
  for (let string of stringBundle.getSimpleEnumeration()) {
    var key = string.key,
      property = "textContent";
    var i = key.lastIndexOf(".");
    if (i >= 0) {
      property = key.substring(i + 1);
      key = key.substring(0, i);
    }
    if (!(key in map)) {
      map[key] = {};
    }
    map[key][property] = string.value;
  }
  return map;
}

function isValidMatchesCount(data) {
  if (typeof data !== "object" || data === null) {
    return false;
  }
  const { current, total } = data;
  if (
    typeof total !== "number" ||
    total < 0 ||
    typeof current !== "number" ||
    current < 0 ||
    current > total
  ) {
    return false;
  }
  return true;
}

// PDF data storage
function PdfDataListener(length) {
  this.length = length; // less than 0, if length is unknown
  this.buffers = [];
  this.loaded = 0;
}

PdfDataListener.prototype = {
  append: function PdfDataListener_append(chunk) {
    // In most of the cases we will pass data as we receive it, but at the
    // beginning of the loading we may accumulate some data.
    this.buffers.push(chunk);
    this.loaded += chunk.length;
    if (this.length >= 0 && this.length < this.loaded) {
      this.length = -1; // reset the length, server is giving incorrect one
    }
    this.onprogress(this.loaded, this.length >= 0 ? this.length : void 0);
  },
  readData: function PdfDataListener_readData() {
    if (this.buffers.length === 0) {
      return null;
    }
    if (this.buffers.length === 1) {
      return this.buffers.pop();
    }
    // There are multiple buffers that need to be combined into a single
    // buffer.
    let combinedLength = 0;
    for (let buffer of this.buffers) {
      combinedLength += buffer.length;
    }
    let combinedArray = new Uint8Array(combinedLength);
    let writeOffset = 0;
    while (this.buffers.length) {
      let buffer = this.buffers.shift();
      combinedArray.set(buffer, writeOffset);
      writeOffset += buffer.length;
    }
    return combinedArray;
  },
  get isDone() {
    return !!this.isDataReady;
  },
  finish: function PdfDataListener_finish() {
    this.isDataReady = true;
    if (this.oncompleteCallback) {
      this.oncompleteCallback(this.readData());
    }
  },
  error: function PdfDataListener_error(errorCode) {
    this.errorCode = errorCode;
    if (this.oncompleteCallback) {
      this.oncompleteCallback(null, errorCode);
    }
  },
  onprogress() {},
  get oncomplete() {
    return this.oncompleteCallback;
  },
  set oncomplete(value) {
    this.oncompleteCallback = value;
    if (this.isDataReady) {
      value(this.readData());
    }
    if (this.errorCode) {
      value(null, this.errorCode);
    }
  },
};

/**
 * All the privileged actions.
 */
class ChromeActions {
  constructor(domWindow, contentDispositionFilename) {
    this.domWindow = domWindow;
    this.contentDispositionFilename = contentDispositionFilename;
    this.telemetryState = {
      documentInfo: false,
      firstPageInfo: false,
      streamTypesUsed: {},
      fontTypesUsed: {},
      fallbackErrorsReported: {},
    };
    this.sandbox = null;
    this.unloadListener = null;
  }

  createSandbox(data, sendResponse) {
    function sendResp(res) {
      if (sendResponse) {
        sendResponse(res);
      }
      return res;
    }

    if (!getBoolPref(PREF_PREFIX + ".enableScripting", false)) {
      return sendResp(false);
    }

    if (this.sandbox !== null) {
      return sendResp(true);
    }

    try {
      this.sandbox = new PdfSandbox(this.domWindow, data);
    } catch (err) {
      // If there's an error here, it means that something is really wrong
      // on pdf.js side during sandbox initialization phase.
      Cu.reportError(err);
      return sendResp(false);
    }

    this.unloadListener = () => {
      this.destroySandbox();
    };
    this.domWindow.addEventListener("unload", this.unloadListener);

    return sendResp(true);
  }

  dispatchEventInSandbox(event) {
    if (this.sandbox) {
      this.sandbox.dispatchEvent(event);
    }
  }

  destroySandbox() {
    if (this.sandbox) {
      this.domWindow.removeEventListener("unload", this.unloadListener);
      this.sandbox.destroy();
      this.sandbox = null;
    }
  }

  isInPrivateBrowsing() {
    return PrivateBrowsingUtils.isContentWindowPrivate(this.domWindow);
  }

  getWindowOriginAttributes() {
    try {
      return this.domWindow.document.nodePrincipal.originAttributes;
    } catch (err) {
      return {};
    }
  }

  download(data, sendResponse) {
    var self = this;
    var originalUrl = data.originalUrl;
    var blobUrl = data.blobUrl || originalUrl;
    // The data may not be downloaded so we need just retry getting the pdf with
    // the original url.
    var originalUri = NetUtil.newURI(originalUrl);
    var filename = data.filename;
    if (
      typeof filename !== "string" ||
      (!/\.pdf$/i.test(filename) && !data.isAttachment)
    ) {
      filename = "document.pdf";
    }
    var blobUri = NetUtil.newURI(blobUrl);

    // If the download was triggered from the ctrl/cmd+s or "Save Page As"
    // launch the "Save As" dialog.
    if (data.sourceEventType == "save") {
      let actor = getActor(this.domWindow);
      actor.sendAsyncMessage("PDFJS:Parent:saveURL", {
        blobUrl,
        filename,
      });
      return;
    }

    // The download is from the fallback bar or the download button, so trigger
    // the open dialog to make it easier for users to save in the downloads
    // folder or launch a different PDF viewer.
    var extHelperAppSvc = Cc[
      "@mozilla.org/uriloader/external-helper-app-service;1"
    ].getService(Ci.nsIExternalHelperAppService);

    var docIsPrivate = this.isInPrivateBrowsing();
    var netChannel = NetUtil.newChannel({
      uri: blobUri,
      loadUsingSystemPrincipal: true,
    });
    if (
      "nsIPrivateBrowsingChannel" in Ci &&
      netChannel instanceof Ci.nsIPrivateBrowsingChannel
    ) {
      netChannel.setPrivate(docIsPrivate);
    }
    NetUtil.asyncFetch(netChannel, function(aInputStream, aResult) {
      if (!Components.isSuccessCode(aResult)) {
        if (sendResponse) {
          sendResponse(true);
        }
        return;
      }
      // Create a nsIInputStreamChannel so we can set the url on the channel
      // so the filename will be correct.
      var channel = Cc[
        "@mozilla.org/network/input-stream-channel;1"
      ].createInstance(Ci.nsIInputStreamChannel);
      channel.QueryInterface(Ci.nsIChannel);
      try {
        // contentDisposition/contentDispositionFilename is readonly before FF18
        channel.contentDisposition = Ci.nsIChannel.DISPOSITION_ATTACHMENT;
        if (self.contentDispositionFilename && !data.isAttachment) {
          channel.contentDispositionFilename = self.contentDispositionFilename;
        } else {
          channel.contentDispositionFilename = filename;
        }
      } catch (e) {}
      channel.setURI(originalUri);
      channel.loadInfo = netChannel.loadInfo;
      channel.contentStream = aInputStream;
      if (
        "nsIPrivateBrowsingChannel" in Ci &&
        channel instanceof Ci.nsIPrivateBrowsingChannel
      ) {
        channel.setPrivate(docIsPrivate);
      }

      var listener = {
        extListener: null,
        onStartRequest(aRequest) {
          var loadContext = self.domWindow.docShell.QueryInterface(
            Ci.nsILoadContext
          );
          this.extListener = extHelperAppSvc.doContent(
            data.isAttachment ? "application/octet-stream" : PDF_CONTENT_TYPE,
            aRequest,
            loadContext,
            false
          );
          this.extListener.onStartRequest(aRequest);
        },
        onStopRequest(aRequest, aStatusCode) {
          if (this.extListener) {
            this.extListener.onStopRequest(aRequest, aStatusCode);
          }
          // Notify the content code we're done downloading.
          if (sendResponse) {
            sendResponse(false);
          }
        },
        onDataAvailable(aRequest, aDataInputStream, aOffset, aCount) {
          this.extListener.onDataAvailable(
            aRequest,
            aDataInputStream,
            aOffset,
            aCount
          );
        },
      };

      channel.asyncOpen(listener);
    });
  }

  getLocale() {
    return Services.locale.requestedLocale || "en-US";
  }

  getStrings(data) {
    try {
      // Lazy initialization of localizedStrings
      if (!("localizedStrings" in this)) {
        this.localizedStrings = getLocalizedStrings("viewer.properties");
      }
      var result = this.localizedStrings[data];
      return JSON.stringify(result || null);
    } catch (e) {
      log("Unable to retrieve localized strings: " + e);
      return "null";
    }
  }

  supportsIntegratedFind() {
    // Integrated find is only supported when we're not in a frame
    return this.domWindow.windowGlobalChild.browsingContext.parent === null;
  }

  supportsDocumentFonts() {
    var prefBrowser = getIntPref("browser.display.use_document_fonts", 1);
    var prefGfx = getBoolPref("gfx.downloadable_fonts.enabled", true);
    return !!prefBrowser && prefGfx;
  }

  supportedMouseWheelZoomModifierKeys() {
    return {
      ctrlKey: getIntPref("mousewheel.with_control.action", 3) === 3,
      metaKey: getIntPref("mousewheel.with_meta.action", 1) === 3,
    };
  }

  isInAutomation() {
    return Cu.isInAutomation;
  }

  reportTelemetry(data) {
    var probeInfo = JSON.parse(data);
    switch (probeInfo.type) {
      case "documentInfo":
        if (!this.telemetryState.documentInfo) {
          PdfJsTelemetry.onDocumentVersion(probeInfo.version);
          PdfJsTelemetry.onDocumentGenerator(probeInfo.generator);
          if (probeInfo.formType) {
            PdfJsTelemetry.onForm(probeInfo.formType);
          }
          this.telemetryState.documentInfo = true;
        }
        break;
      case "pageInfo":
        if (!this.telemetryState.firstPageInfo) {
          PdfJsTelemetry.onTimeToView(probeInfo.timestamp);
          this.telemetryState.firstPageInfo = true;
        }
        break;
      case "documentStats":
        // documentStats can be called several times for one documents.
        // if stream/font types are reported, trying not to submit the same
        // enumeration value multiple times.
        var documentStats = probeInfo.stats;
        if (!documentStats || typeof documentStats !== "object") {
          break;
        }
        var i,
          streamTypes = documentStats.streamTypes,
          key;
        var STREAM_TYPE_ID_LIMIT = 20;
        i = 0;
        for (key in streamTypes) {
          if (++i > STREAM_TYPE_ID_LIMIT) {
            break;
          }
          if (!this.telemetryState.streamTypesUsed[key]) {
            PdfJsTelemetry.onStreamType(key);
            this.telemetryState.streamTypesUsed[key] = true;
          }
        }
        var fontTypes = documentStats.fontTypes;
        var FONT_TYPE_ID_LIMIT = 20;
        i = 0;
        for (key in fontTypes) {
          if (++i > FONT_TYPE_ID_LIMIT) {
            break;
          }
          if (!this.telemetryState.fontTypesUsed[key]) {
            PdfJsTelemetry.onFontType(key);
            this.telemetryState.fontTypesUsed[key] = true;
          }
        }
        break;
      case "print":
        PdfJsTelemetry.onPrint();
        break;
      case "unsupportedFeature":
        if (!this.telemetryState.fallbackErrorsReported[probeInfo.featureId]) {
          PdfJsTelemetry.onFallbackError(probeInfo.featureId);
          this.telemetryState.fallbackErrorsReported[
            probeInfo.featureId
          ] = true;
        }
        break;
      case "tagged":
        PdfJsTelemetry.onTagged(probeInfo.tagged);
        break;
    }
  }

  /**
   * @param {Object} args - Object with `featureId` and `url` properties.
   * @param {function} sendResponse - Callback function.
   */
  fallback(args, sendResponse) {
    sendResponse(false);
  }

  updateFindControlState(data) {
    if (!this.supportsIntegratedFind()) {
      return;
    }
    // Verify what we're sending to the findbar.
    var result = data.result;
    var findPrevious = data.findPrevious;
    var findPreviousType = typeof findPrevious;
    if (
      typeof result !== "number" ||
      result < 0 ||
      result > 3 ||
      (findPreviousType !== "undefined" && findPreviousType !== "boolean")
    ) {
      return;
    }
    // Allow the `matchesCount` property to be optional, and ensure that
    // it's valid before including it in the data sent to the findbar.
    let matchesCount = null;
    if (isValidMatchesCount(data.matchesCount)) {
      matchesCount = data.matchesCount;
    }
    // Same for the `rawQuery` property.
    let rawQuery = null;
    if (typeof data.rawQuery === "string") {
      rawQuery = data.rawQuery;
    }

    let actor = getActor(this.domWindow);
    actor?.sendAsyncMessage("PDFJS:Parent:updateControlState", {
      result,
      findPrevious,
      matchesCount,
      rawQuery,
    });
  }

  updateFindMatchesCount(data) {
    if (!this.supportsIntegratedFind()) {
      return;
    }
    // Verify what we're sending to the findbar.
    if (!isValidMatchesCount(data)) {
      return;
    }

    let actor = getActor(this.domWindow);
    actor?.sendAsyncMessage("PDFJS:Parent:updateMatchesCount", data);
  }

  setPreferences(prefs, sendResponse) {
    var defaultBranch = Services.prefs.getDefaultBranch(PREF_PREFIX + ".");
    var numberOfPrefs = 0;
    var prefValue, prefName;
    for (var key in prefs) {
      if (++numberOfPrefs > MAX_NUMBER_OF_PREFS) {
        log(
          "setPreferences - Exceeded the maximum number of preferences " +
            "that is allowed to be set at once."
        );
        break;
      } else if (!defaultBranch.getPrefType(key)) {
        continue;
      }
      prefValue = prefs[key];
      prefName = PREF_PREFIX + "." + key;
      switch (typeof prefValue) {
        case "boolean":
          AsyncPrefs.set(prefName, prefValue);
          break;
        case "number":
          AsyncPrefs.set(prefName, prefValue);
          break;
        case "string":
          if (prefValue.length > MAX_STRING_PREF_LENGTH) {
            log(
              "setPreferences - Exceeded the maximum allowed length " +
                "for a string preference."
            );
          } else {
            AsyncPrefs.set(prefName, prefValue);
          }
          break;
      }
    }
    if (sendResponse) {
      sendResponse(true);
    }
  }

  getPreferences(prefs, sendResponse) {
    var defaultBranch = Services.prefs.getDefaultBranch(PREF_PREFIX + ".");
    var currentPrefs = {},
      numberOfPrefs = 0;
    var prefValue, prefName;
    for (var key in prefs) {
      if (++numberOfPrefs > MAX_NUMBER_OF_PREFS) {
        log(
          "getPreferences - Exceeded the maximum number of preferences " +
            "that is allowed to be fetched at once."
        );
        break;
      } else if (!defaultBranch.getPrefType(key)) {
        continue;
      }
      prefValue = prefs[key];
      prefName = PREF_PREFIX + "." + key;
      switch (typeof prefValue) {
        case "boolean":
          currentPrefs[key] = getBoolPref(prefName, prefValue);
          break;
        case "number":
          currentPrefs[key] = getIntPref(prefName, prefValue);
          break;
        case "string":
          currentPrefs[key] = getStringPref(prefName, prefValue);
          break;
      }
    }
    let result = JSON.stringify(currentPrefs);
    if (sendResponse) {
      sendResponse(result);
    }
    return result;
  }
}

/**
 * This is for range requests.
 */
class RangedChromeActions extends ChromeActions {
  constructor(
    domWindow,
    contentDispositionFilename,
    originalRequest,
    rangeEnabled,
    streamingEnabled,
    dataListener
  ) {
    super(domWindow, contentDispositionFilename);
    this.dataListener = dataListener;
    this.originalRequest = originalRequest;
    this.rangeEnabled = rangeEnabled;
    this.streamingEnabled = streamingEnabled;

    this.pdfUrl = originalRequest.URI.spec;
    this.contentLength = originalRequest.contentLength;

    // Pass all the headers from the original request through
    var httpHeaderVisitor = {
      headers: {},
      visitHeader(aHeader, aValue) {
        if (aHeader === "Range") {
          // When loading the PDF from cache, firefox seems to set the Range
          // request header to fetch only the unfetched portions of the file
          // (e.g. 'Range: bytes=1024-'). However, we want to set this header
          // manually to fetch the PDF in chunks.
          return;
        }
        this.headers[aHeader] = aValue;
      },
    };
    if (originalRequest.visitRequestHeaders) {
      originalRequest.visitRequestHeaders(httpHeaderVisitor);
    }

    var self = this;
    var xhr_onreadystatechange = function xhr_onreadystatechange() {
      if (this.readyState === 1) {
        // LOADING
        var netChannel = this.channel;
        // override this XMLHttpRequest's OriginAttributes with our cached parent window's
        // OriginAttributes, as we are currently running under the SystemPrincipal
        this.setOriginAttributes(self.getWindowOriginAttributes());
        if (
          "nsIPrivateBrowsingChannel" in Ci &&
          netChannel instanceof Ci.nsIPrivateBrowsingChannel
        ) {
          var docIsPrivate = self.isInPrivateBrowsing();
          netChannel.setPrivate(docIsPrivate);
        }
      }
    };
    var getXhr = function getXhr() {
      var xhr = new XMLHttpRequest();
      xhr.addEventListener("readystatechange", xhr_onreadystatechange);
      return xhr;
    };

    this.networkManager = new NetworkManager(this.pdfUrl, {
      httpHeaders: httpHeaderVisitor.headers,
      getXhr,
    });

    // If we are in range request mode, this means we manually issued xhr
    // requests, which we need to abort when we leave the page
    domWindow.addEventListener("unload", function unload(e) {
      domWindow.removeEventListener(e.type, unload);
      self.abortLoading();
    });
  }

  initPassiveLoading() {
    let data, done;
    if (!this.streamingEnabled) {
      this.originalRequest.cancel(Cr.NS_BINDING_ABORTED);
      this.originalRequest = null;
      data = this.dataListener.readData();
      done = this.dataListener.isDone;
      this.dataListener = null;
    } else {
      data = this.dataListener.readData();
      done = this.dataListener.isDone;

      this.dataListener.onprogress = (loaded, total) => {
        this.domWindow.postMessage(
          {
            pdfjsLoadAction: "progressiveRead",
            loaded,
            total,
            chunk: this.dataListener.readData(),
          },
          PDF_VIEWER_ORIGIN
        );
      };
      this.dataListener.oncomplete = () => {
        if (!done && this.dataListener.isDone) {
          this.domWindow.postMessage(
            {
              pdfjsLoadAction: "progressiveDone",
            },
            PDF_VIEWER_ORIGIN
          );
        }
        this.dataListener = null;
      };
    }

    this.domWindow.postMessage(
      {
        pdfjsLoadAction: "supportsRangedLoading",
        rangeEnabled: this.rangeEnabled,
        streamingEnabled: this.streamingEnabled,
        pdfUrl: this.pdfUrl,
        length: this.contentLength,
        data,
        done,
        filename: this.contentDispositionFilename,
      },
      PDF_VIEWER_ORIGIN
    );

    return true;
  }

  requestDataRange(args) {
    if (!this.rangeEnabled) {
      return;
    }

    var begin = args.begin;
    var end = args.end;
    var domWindow = this.domWindow;
    // TODO(mack): Support error handler. We're not currently not handling
    // errors from chrome code for non-range requests, so this doesn't
    // seem high-pri
    this.networkManager.requestRange(begin, end, {
      onDone: function RangedChromeActions_onDone(aArgs) {
        domWindow.postMessage(
          {
            pdfjsLoadAction: "range",
            begin: aArgs.begin,
            chunk: aArgs.chunk,
          },
          PDF_VIEWER_ORIGIN
        );
      },
      onProgress: function RangedChromeActions_onProgress(evt) {
        domWindow.postMessage(
          {
            pdfjsLoadAction: "rangeProgress",
            loaded: evt.loaded,
          },
          PDF_VIEWER_ORIGIN
        );
      },
    });
  }

  abortLoading() {
    this.networkManager.abortAllRequests();
    if (this.originalRequest) {
      this.originalRequest.cancel(Cr.NS_BINDING_ABORTED);
      this.originalRequest = null;
    }
    this.dataListener = null;
  }
}

/**
 * This is for a single network stream.
 */
class StandardChromeActions extends ChromeActions {
  constructor(
    domWindow,
    contentDispositionFilename,
    originalRequest,
    dataListener
  ) {
    super(domWindow, contentDispositionFilename);
    this.originalRequest = originalRequest;
    this.dataListener = dataListener;
  }

  initPassiveLoading() {
    if (!this.dataListener) {
      return false;
    }

    this.dataListener.onprogress = (loaded, total) => {
      this.domWindow.postMessage(
        {
          pdfjsLoadAction: "progress",
          loaded,
          total,
        },
        PDF_VIEWER_ORIGIN
      );
    };

    this.dataListener.oncomplete = (data, errorCode) => {
      this.domWindow.postMessage(
        {
          pdfjsLoadAction: "complete",
          data,
          errorCode,
          filename: this.contentDispositionFilename,
        },
        PDF_VIEWER_ORIGIN
      );

      this.dataListener = null;
      this.originalRequest = null;
    };

    return true;
  }

  abortLoading() {
    if (this.originalRequest) {
      this.originalRequest.cancel(Cr.NS_BINDING_ABORTED);
      this.originalRequest = null;
    }
    this.dataListener = null;
  }
}

/**
 * Event listener to trigger chrome privileged code.
 */
class RequestListener {
  constructor(actions) {
    this.actions = actions;
  }

  // Receive an event and synchronously or asynchronously responds.
  receive(event) {
    var message = event.target;
    var doc = message.ownerDocument;
    var action = event.detail.action;
    var data = event.detail.data;
    var sync = event.detail.sync;
    var actions = this.actions;
    if (!(action in actions)) {
      log("Unknown action: " + action);
      return;
    }
    var response;
    if (sync) {
      response = actions[action].call(this.actions, data);
      event.detail.response = Cu.cloneInto(response, doc.defaultView);
    } else {
      if (!event.detail.responseExpected) {
        doc.documentElement.removeChild(message);
        response = null;
      } else {
        response = function sendResponse(aResponse) {
          try {
            var listener = doc.createEvent("CustomEvent");
            let detail = Cu.cloneInto({ response: aResponse }, doc.defaultView);
            listener.initCustomEvent("pdf.js.response", true, false, detail);
            return message.dispatchEvent(listener);
          } catch (e) {
            // doc is no longer accessible because the requestor is already
            // gone. unloaded content cannot receive the response anyway.
            return false;
          }
        };
      }
      actions[action].call(this.actions, data, response);
    }
  }
}

function PdfStreamConverter() {}

PdfStreamConverter.prototype = {
  QueryInterface: ChromeUtils.generateQI([
    "nsIStreamConverter",
    "nsIStreamListener",
    "nsIRequestObserver",
  ]),

  /*
   * This component works as such:
   * 1. asyncConvertData stores the listener
   * 2. onStartRequest creates a new channel, streams the viewer
   * 3. If range requests are supported:
   *      3.1. Leave the request open until the viewer is ready to switch to
   *           range requests.
   *
   *    If range rquests are not supported:
   *      3.1. Read the stream as it's loaded in onDataAvailable to send
   *           to the viewer
   *
   * The convert function just returns the stream, it's just the synchronous
   * version of asyncConvertData.
   */

  // nsIStreamConverter::convert
  convert(aFromStream, aFromType, aToType, aCtxt) {
    throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
  },

  // nsIStreamConverter::asyncConvertData
  asyncConvertData(aFromType, aToType, aListener, aCtxt) {
    if (aCtxt && aCtxt instanceof Ci.nsIChannel) {
      aCtxt.QueryInterface(Ci.nsIChannel);
    }
    // We need to check if we're supposed to convert here, because not all
    // asyncConvertData consumers will call getConvertedType first:
    this.getConvertedType(aFromType, aCtxt);

    // Store the listener passed to us
    this.listener = aListener;
  },

  _usableHandler(handlerInfo) {
    let { preferredApplicationHandler } = handlerInfo;
    if (
      !preferredApplicationHandler ||
      !(preferredApplicationHandler instanceof Ci.nsILocalHandlerApp)
    ) {
      return false;
    }
    preferredApplicationHandler.QueryInterface(Ci.nsILocalHandlerApp);
    // We have an app, grab the executable
    let { executable } = preferredApplicationHandler;
    if (!executable) {
      return false;
    }
    return !executable.equals(gOurBinary);
  },

  /*
   * Check if the user wants to use PDF.js. Returns true if PDF.js should
   * handle PDFs, and false if not. Will always return true on non-parent
   * processes.
   *
   * If the user has selected to open PDFs with a helper app, and we are that
   * helper app, or if the user has selected the OS default, and we are that
   * OS default, reset the preference back to pdf.js .
   *
   */
  _validateAndMaybeUpdatePDFPrefs() {
    let { processType, PROCESS_TYPE_DEFAULT } = Services.appinfo;
    // If we're not in the parent, or are the default, then just say yes.
    if (processType != PROCESS_TYPE_DEFAULT || PdfJs.cachedIsDefault()) {
      return true;
    }

    // OK, PDF.js might not be the default. Find out if we've misled the user
    // into making Firefox an external handler or if we're the OS default and
    // Firefox is set to use the OS default:
    let mime = Svc.mime.getFromTypeAndExtension(PDF_CONTENT_TYPE, "pdf");
    // The above might throw errors. We're deliberately letting those bubble
    // back up, where they'll tell the stream converter not to use us.

    if (!mime) {
      // This shouldn't happen, but we can't fix what isn't there. Assume
      // we're OK to handle with PDF.js
      return true;
    }

    const { saveToDisk, useHelperApp, useSystemDefault } = Ci.nsIHandlerInfo;
    let { preferredAction, alwaysAskBeforeHandling } = mime;
    // If the user has indicated they want to be asked or want to save to
    // disk, we shouldn't render inline immediately:
    if (alwaysAskBeforeHandling || preferredAction == saveToDisk) {
      return false;
    }
    // If we have usable helper app info, don't use PDF.js
    if (preferredAction == useHelperApp && this._usableHandler(mime)) {
      return false;
    }
    // If we want the OS default and that's not Firefox, don't use PDF.js
    if (preferredAction == useSystemDefault && !mime.isCurrentAppOSDefault()) {
      return false;
    }
    // Log that we're doing this to help debug issues if people end up being
    // surprised by this behaviour.
    Cu.reportError("Found unusable PDF preferences. Fixing back to PDF.js");

    mime.preferredAction = Ci.nsIHandlerInfo.handleInternally;
    mime.alwaysAskBeforeHandling = false;
    Svc.handlers.store(mime);
    return true;
  },

  getConvertedType(aFromType, aChannel) {
    const HTML = "text/html";
    let channelURI = aChannel?.URI;
    // We can be invoked for application/octet-stream; check if we want the
    // channel first:
    if (aFromType != "application/pdf") {
      let ext = channelURI?.QueryInterface(Ci.nsIURL).fileExtension;
      let isPDF = ext.toLowerCase() == "pdf";
      let browsingContext = aChannel?.loadInfo.targetBrowsingContext;
      let toplevelOctetStream =
        aFromType == "application/octet-stream" &&
        browsingContext &&
        !browsingContext.parent;
      if (
        !isPDF ||
        !toplevelOctetStream ||
        !getBoolPref(PREF_PREFIX + ".handleOctetStream", false)
      ) {
        throw new Components.Exception(
          "Ignore PDF.js for this download.",
          Cr.NS_ERROR_FAILURE
        );
      }
      // fall through, this appears to be a pdf.
    }

    if (this._validateAndMaybeUpdatePDFPrefs()) {
      return HTML;
    }
    // Hm, so normally, no pdfjs. However... if this is a file: channel loaded
    // with system principal, load it anyway:
    if (channelURI?.schemeIs("file")) {
      let triggeringPrincipal = aChannel.loadInfo?.triggeringPrincipal;
      if (triggeringPrincipal?.isSystemPrincipal) {
        return HTML;
      }
    }

    throw new Components.Exception("Can't use PDF.js", Cr.NS_ERROR_FAILURE);
  },

  // nsIStreamListener::onDataAvailable
  onDataAvailable(aRequest, aInputStream, aOffset, aCount) {
    if (!this.dataListener) {
      return;
    }

    var binaryStream = this.binaryStream;
    binaryStream.setInputStream(aInputStream);
    let chunk = new ArrayBuffer(aCount);
    binaryStream.readArrayBuffer(aCount, chunk);
    this.dataListener.append(new Uint8Array(chunk));
  },

  // nsIRequestObserver::onStartRequest
  onStartRequest(aRequest) {
    // Setup the request so we can use it below.
    var isHttpRequest = false;
    try {
      aRequest.QueryInterface(Ci.nsIHttpChannel);
      isHttpRequest = true;
    } catch (e) {}

    var rangeRequest = false;
    var streamRequest = false;
    if (isHttpRequest) {
      var contentEncoding = "identity";
      try {
        contentEncoding = aRequest.getResponseHeader("Content-Encoding");
      } catch (e) {}

      var acceptRanges;
      try {
        acceptRanges = aRequest.getResponseHeader("Accept-Ranges");
      } catch (e) {}

      var hash = aRequest.URI.ref;
      var isPDFBugEnabled = getBoolPref(PREF_PREFIX + ".pdfBugEnabled", false);
      rangeRequest =
        contentEncoding === "identity" &&
        acceptRanges === "bytes" &&
        aRequest.contentLength >= 0 &&
        !getBoolPref(PREF_PREFIX + ".disableRange", false) &&
        (!isPDFBugEnabled || !hash.toLowerCase().includes("disablerange=true"));
      streamRequest =
        contentEncoding === "identity" &&
        aRequest.contentLength >= 0 &&
        !getBoolPref(PREF_PREFIX + ".disableStream", false) &&
        (!isPDFBugEnabled ||
          !hash.toLowerCase().includes("disablestream=true"));
    }

    aRequest.QueryInterface(Ci.nsIChannel);

    aRequest.QueryInterface(Ci.nsIWritablePropertyBag);

    var contentDispositionFilename;
    try {
      contentDispositionFilename = aRequest.contentDispositionFilename;
    } catch (e) {}

    // Change the content type so we don't get stuck in a loop.
    aRequest.setProperty("contentType", aRequest.contentType);
    aRequest.contentType = "text/html";
    if (isHttpRequest) {
      // We trust PDF viewer, using no CSP
      aRequest.setResponseHeader("Content-Security-Policy", "", false);
      aRequest.setResponseHeader(
        "Content-Security-Policy-Report-Only",
        "",
        false
      );
      // The viewer does not need to handle HTTP Refresh header.
      aRequest.setResponseHeader("Refresh", "", false);
    }

    PdfJsTelemetry.onViewerIsUsed();
    PdfJsTelemetry.onDocumentSize(aRequest.contentLength);

    // Creating storage for PDF data
    var contentLength = aRequest.contentLength;
    this.dataListener = new PdfDataListener(contentLength);
    this.binaryStream = Cc["@mozilla.org/binaryinputstream;1"].createInstance(
      Ci.nsIBinaryInputStream
    );

    // Create a new channel that is viewer loaded as a resource.
    var channel = NetUtil.newChannel({
      uri: PDF_VIEWER_WEB_PAGE,
      loadUsingSystemPrincipal: true,
    });

    var listener = this.listener;
    var dataListener = this.dataListener;
    // Proxy all the request observer calls, when it gets to onStopRequest
    // we can get the dom window.  We also intentionally pass on the original
    // request(aRequest) below so we don't overwrite the original channel and
    // trigger an assertion.
    var proxy = {
      onStartRequest(request) {
        listener.onStartRequest(aRequest);
      },
      onDataAvailable(request, inputStream, offset, count) {
        listener.onDataAvailable(aRequest, inputStream, offset, count);
      },
      onStopRequest(request, statusCode) {
        var domWindow = getDOMWindow(channel, resourcePrincipal);
        if (!Components.isSuccessCode(statusCode) || !domWindow) {
          // The request may have been aborted and the document may have been
          // replaced with something that is not PDF.js, abort attaching.
          listener.onStopRequest(aRequest, statusCode);
          return;
        }
        var actions;
        if (rangeRequest || streamRequest) {
          actions = new RangedChromeActions(
            domWindow,
            contentDispositionFilename,
            aRequest,
            rangeRequest,
            streamRequest,
            dataListener
          );
        } else {
          actions = new StandardChromeActions(
            domWindow,
            contentDispositionFilename,
            aRequest,
            dataListener
          );
        }
        var requestListener = new RequestListener(actions);
        domWindow.document.addEventListener(
          PDFJS_EVENT_ID,
          function(event) {
            requestListener.receive(event);
          },
          false,
          true
        );

        let actor = getActor(domWindow);
        actor?.init(actions.supportsIntegratedFind());

        listener.onStopRequest(aRequest, statusCode);

        if (domWindow.windowGlobalChild.browsingContext.parent) {
          // This will need to be changed when fission supports object/embed (bug 1614524)
          var isObjectEmbed = domWindow.frameElement
            ? domWindow.frameElement.tagName == "OBJECT" ||
              domWindow.frameElement.tagName == "EMBED"
            : false;
          PdfJsTelemetry.onEmbed(isObjectEmbed);
        }
      },
    };

    // Keep the URL the same so the browser sees it as the same.
    channel.originalURI = aRequest.URI;
    channel.loadGroup = aRequest.loadGroup;
    channel.loadInfo.originAttributes = aRequest.loadInfo.originAttributes;

    // We can use the resource principal when data is fetched by the chrome,
    // e.g. useful for NoScript. Make make sure we reuse the origin attributes
    // from the request channel to keep isolation consistent.
    var uri = NetUtil.newURI(PDF_VIEWER_WEB_PAGE);
    var resourcePrincipal = Services.scriptSecurityManager.createContentPrincipal(
      uri,
      aRequest.loadInfo.originAttributes
    );
    // Remember the principal we would have had before we mess with it.
    let originalPrincipal = Services.scriptSecurityManager.getChannelResultPrincipal(
      aRequest
    );
    aRequest.owner = resourcePrincipal;
    aRequest.setProperty("noPDFJSPrincipal", originalPrincipal);

    channel.asyncOpen(proxy);
  },

  // nsIRequestObserver::onStopRequest
  onStopRequest(aRequest, aStatusCode) {
    if (!this.dataListener) {
      // Do nothing
      return;
    }

    if (Components.isSuccessCode(aStatusCode)) {
      this.dataListener.finish();
    } else {
      this.dataListener.error(aStatusCode);
    }
    delete this.dataListener;
    delete this.binaryStream;
  },
};
