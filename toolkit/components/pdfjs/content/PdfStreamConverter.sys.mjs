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

const PDFJS_EVENT_ID = "pdf.js.message";
const PDF_VIEWER_ORIGIN = "resource://pdf.js";
const PDF_VIEWER_WEB_PAGE = "resource://pdf.js/web/viewer.html";
const MAX_NUMBER_OF_PREFS = 50;
const PDF_CONTENT_TYPE = "application/pdf";

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";
import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  NetUtil: "resource://gre/modules/NetUtil.sys.mjs",
  NetworkManager: "resource://pdf.js/PdfJsNetwork.sys.mjs",
  PdfJs: "resource://pdf.js/PdfJs.sys.mjs",
  PdfJsTelemetry: "resource://pdf.js/PdfJsTelemetry.sys.mjs",
  PdfSandbox: "resource://pdf.js/PdfSandbox.sys.mjs",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
});

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

ChromeUtils.defineLazyGetter(lazy, "gOurBinary", () => {
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

function log(aMsg) {
  if (!Services.prefs.getBoolPref("pdfjs.pdfBugEnabled", false)) {
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
    const actorName =
      AppConstants.platform === "android" ? "GeckoViewPdfjs" : "Pdfjs";
    return window.windowGlobalChild.getActor(actorName);
  } catch (ex) {
    return null;
  }
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

    if (!Services.prefs.getBoolPref("pdfjs.enableScripting", false)) {
      return sendResp(false);
    }

    if (this.sandbox !== null) {
      return sendResp(true);
    }

    try {
      this.sandbox = new lazy.PdfSandbox(this.domWindow, data);
    } catch (err) {
      // If there's an error here, it means that something is really wrong
      // on pdf.js side during sandbox initialization phase.
      console.error(err);
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

  dispatchAsyncEventInSandbox(event, sendResponse) {
    this.dispatchEventInSandbox(event);
    sendResponse();
  }

  destroySandbox() {
    if (this.sandbox) {
      this.domWindow.removeEventListener("unload", this.unloadListener);
      this.sandbox.destroy();
      this.sandbox = null;
    }
  }

  isInPrivateBrowsing() {
    return lazy.PrivateBrowsingUtils.isContentWindowPrivate(this.domWindow);
  }

  getWindowOriginAttributes() {
    try {
      return this.domWindow.document.nodePrincipal.originAttributes;
    } catch (err) {
      return {};
    }
  }

  download(data, sendResponse) {
    const { originalUrl, options } = data;
    const blobUrl = data.blobUrl || originalUrl;
    let { filename } = data;
    if (
      typeof filename !== "string" ||
      (!/\.pdf$/i.test(filename) && !data.isAttachment)
    ) {
      filename = "document.pdf";
    }

    const actor = getActor(this.domWindow);
    actor.sendAsyncMessage("PDFJS:Parent:saveURL", {
      blobUrl,
      originalUrl,
      filename,
      options: options || {},
    });
  }

  getLocaleProperties(_data, sendResponse) {
    const { requestedLocale, defaultLocale, isAppLocaleRTL } = Services.locale;
    sendResponse({
      lang: requestedLocale || defaultLocale,
      isRTL: isAppLocaleRTL,
    });
  }

  supportsIntegratedFind() {
    // Integrated find is only supported when we're not in a frame
    return this.domWindow.windowGlobalChild.browsingContext.parent === null;
  }

  getBrowserPrefs() {
    return {
      canvasMaxAreaInBytes: Services.prefs.getIntPref("gfx.max-alloc-size"),
      isInAutomation: Cu.isInAutomation,
      supportsDocumentFonts:
        !!Services.prefs.getIntPref("browser.display.use_document_fonts") &&
        Services.prefs.getBoolPref("gfx.downloadable_fonts.enabled"),
      supportsIntegratedFind: this.supportsIntegratedFind(),
      supportsMouseWheelZoomCtrlKey:
        Services.prefs.getIntPref("mousewheel.with_control.action") === 3,
      supportsMouseWheelZoomMetaKey:
        Services.prefs.getIntPref("mousewheel.with_meta.action") === 3,
      supportsPinchToZoom: Services.prefs.getBoolPref("apz.allow_zooming"),
    };
  }

  isMobile() {
    return AppConstants.platform === "android";
  }

  getNimbusExperimentData(_data, sendResponse) {
    if (!this.isMobile()) {
      sendResponse(null);
      return;
    }
    const actor = getActor(this.domWindow);
    actor.sendAsyncMessage("PDFJS:Parent:getNimbus");
    Services.obs.addObserver(
      {
        observe(aSubject, aTopic, aData) {
          if (aTopic === "pdfjs-getNimbus") {
            Services.obs.removeObserver(this, aTopic);
            sendResponse(aSubject && JSON.stringify(aSubject.wrappedJSObject));
          }
        },
      },
      "pdfjs-getNimbus"
    );
  }

  reportTelemetry(data) {
    const probeInfo = JSON.parse(data);
    const { type } = probeInfo;
    switch (type) {
      case "pageInfo":
        lazy.PdfJsTelemetry.onTimeToView(probeInfo.timestamp);
        break;
      case "editing":
        lazy.PdfJsTelemetry.onEditing(probeInfo);
        break;
      case "buttons":
      case "gv-buttons":
        const id = probeInfo.data.id.replace(
          /([A-Z])/g,
          c => `_${c.toLowerCase()}`
        );
        if (type === "buttons") {
          lazy.PdfJsTelemetry.onButtons(id);
        } else {
          lazy.PdfJsTelemetry.onGeckoview(id);
        }
        break;
    }
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

  getPreferences(prefs, sendResponse) {
    var defaultBranch = Services.prefs.getDefaultBranch("pdfjs.");
    var currentPrefs = {},
      numberOfPrefs = 0;
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
      const prefName = `pdfjs.${key}`,
        prefValue = prefs[key];
      switch (typeof prefValue) {
        case "boolean":
          currentPrefs[key] = Services.prefs.getBoolPref(prefName, prefValue);
          break;
        case "number":
          currentPrefs[key] = Services.prefs.getIntPref(prefName, prefValue);
          break;
        case "string":
          currentPrefs[key] = Services.prefs.getStringPref(prefName, prefValue);
          break;
      }
    }

    const res = {
      browserPrefs: this.getBrowserPrefs(),
      prefs: currentPrefs,
    };
    sendResponse?.(res);
    return res;
  }

  /**
   * Set the different editor states in order to be able to update the context
   * menu.
   * @param {Object} details
   */
  updateEditorStates({ details }) {
    const doc = this.domWindow.document;
    if (!doc.editorStates) {
      doc.editorStates = {
        isEditing: false,
        isEmpty: true,
        hasSomethingToUndo: false,
        hasSomethingToRedo: false,
        hasSelectedEditor: false,
      };
    }
    const { editorStates } = doc;
    for (const [key, value] of Object.entries(details)) {
      if (typeof value === "boolean" && key in editorStates) {
        editorStates[key] = value;
      }
    }
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

    this.networkManager = new lazy.NetworkManager(this.pdfUrl, {
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
        const chunk = this.dataListener.readData();

        this.domWindow.postMessage(
          {
            pdfjsLoadAction: "progressiveRead",
            loaded,
            total,
            chunk,
          },
          PDF_VIEWER_ORIGIN,
          chunk ? [chunk.buffer] : undefined
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
      PDF_VIEWER_ORIGIN,
      data ? [data.buffer] : undefined
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
      onDone: function RangedChromeActions_onDone({ begin, chunk }) {
        domWindow.postMessage(
          {
            pdfjsLoadAction: "range",
            begin,
            chunk,
          },
          PDF_VIEWER_ORIGIN,
          chunk ? [chunk.buffer] : undefined
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
        PDF_VIEWER_ORIGIN,
        data ? [data.buffer] : undefined
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

export function PdfStreamConverter() {}

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
    return !executable.equals(lazy.gOurBinary);
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
    if (processType != PROCESS_TYPE_DEFAULT || lazy.PdfJs.cachedIsDefault()) {
      return { shouldOpen: true };
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
      return { shouldOpen: true };
    }

    const { saveToDisk, useHelperApp, useSystemDefault } = Ci.nsIHandlerInfo;
    let { preferredAction, alwaysAskBeforeHandling } = mime;
    // return this info so getConvertedType can use it.
    let rv = { alwaysAskBeforeHandling, shouldOpen: false };
    // If the user has indicated they want to be asked or want to save to
    // disk, we shouldn't render inline immediately:
    if (alwaysAskBeforeHandling || preferredAction == saveToDisk) {
      return rv;
    }
    // If we have usable helper app info, don't use PDF.js
    if (preferredAction == useHelperApp && this._usableHandler(mime)) {
      return rv;
    }
    // If we want the OS default and that's not Firefox, don't use PDF.js
    if (preferredAction == useSystemDefault && !mime.isCurrentAppOSDefault()) {
      return rv;
    }
    rv.shouldOpen = true;
    // Log that we're doing this to help debug issues if people end up being
    // surprised by this behaviour.
    console.error("Found unusable PDF preferences. Fixing back to PDF.js");

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
      // Check if the filename has a PDF extension.
      let isPDF = false;
      try {
        isPDF = aChannel.contentDispositionFilename.endsWith(".pdf");
      } catch (ex) {}
      if (!isPDF) {
        isPDF =
          channelURI?.QueryInterface(Ci.nsIURL).fileExtension.toLowerCase() ==
          "pdf";
      }

      let browsingContext = aChannel?.loadInfo.targetBrowsingContext;
      let toplevelOctetStream =
        aFromType == "application/octet-stream" &&
        browsingContext &&
        !browsingContext.parent;
      if (
        !isPDF ||
        !toplevelOctetStream ||
        !Services.prefs.getBoolPref("pdfjs.handleOctetStream", false)
      ) {
        throw new Components.Exception(
          "Ignore PDF.js for this download.",
          Cr.NS_ERROR_FAILURE
        );
      }
      // fall through, this appears to be a pdf.
    }

    let { alwaysAskBeforeHandling, shouldOpen } =
      this._validateAndMaybeUpdatePDFPrefs();

    if (shouldOpen) {
      return HTML;
    }
    // Hm, so normally, no pdfjs. However... if this is a file: channel there
    // are some edge-cases.
    if (channelURI?.schemeIs("file")) {
      // If we're loaded with system principal, we were likely handed the PDF
      // by the OS or directly from the URL bar. Assume we should load it:
      let triggeringPrincipal = aChannel.loadInfo?.triggeringPrincipal;
      if (triggeringPrincipal?.isSystemPrincipal) {
        return HTML;
      }

      // If we're loading from a file: link, load it in PDF.js unless the user
      // has told us they always want to open/save PDFs.
      // This is because handing off the choice to open in Firefox itself
      // through the dialog doesn't work properly and making it work is
      // non-trivial (see https://bugzilla.mozilla.org/show_bug.cgi?id=1680147#c3 )
      // - and anyway, opening the file is what we do for *all*
      // other file types we handle internally (and users can then use other UI
      // to save or open it with other apps from there).
      if (triggeringPrincipal?.schemeIs("file") && alwaysAskBeforeHandling) {
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
      const isPDFBugEnabled = Services.prefs.getBoolPref(
        "pdfjs.pdfBugEnabled",
        false
      );
      rangeRequest =
        contentEncoding === "identity" &&
        acceptRanges === "bytes" &&
        aRequest.contentLength >= 0 &&
        !Services.prefs.getBoolPref("pdfjs.disableRange", false) &&
        (!isPDFBugEnabled || !hash.toLowerCase().includes("disablerange=true"));
      streamRequest =
        contentEncoding === "identity" &&
        aRequest.contentLength >= 0 &&
        !Services.prefs.getBoolPref("pdfjs.disableStream", false) &&
        (!isPDFBugEnabled ||
          !hash.toLowerCase().includes("disablestream=true"));
    }

    aRequest.QueryInterface(Ci.nsIChannel);

    aRequest.QueryInterface(Ci.nsIWritablePropertyBag);

    var contentDispositionFilename;
    try {
      contentDispositionFilename = aRequest.contentDispositionFilename;
    } catch (e) {}

    if (
      contentDispositionFilename &&
      !/\.pdf$/i.test(contentDispositionFilename)
    ) {
      contentDispositionFilename += ".pdf";
    }

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

    lazy.PdfJsTelemetry.onViewerIsUsed();

    // The document will be loaded via the stream converter as html,
    // but since we may have come here via a download or attachment
    // that was opened directly, force the content disposition to be
    // inline so that the html document will be loaded normally instead
    // of going to the helper service.
    aRequest.contentDisposition = Ci.nsIChannel.DISPOSITION_FORCE_INLINE;

    // Creating storage for PDF data
    var contentLength = aRequest.contentLength;
    this.dataListener = new PdfDataListener(contentLength);
    this.binaryStream = Cc["@mozilla.org/binaryinputstream;1"].createInstance(
      Ci.nsIBinaryInputStream
    );

    // Create a new channel that is viewer loaded as a resource.
    var channel = lazy.NetUtil.newChannel({
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
          function (event) {
            requestListener.receive(event);
          },
          false,
          true
        );

        let actor = getActor(domWindow);
        actor?.init(actions.supportsIntegratedFind());
        actor?.sendAsyncMessage("PDFJS:Parent:recordExposure");
        listener.onStopRequest(aRequest, statusCode);
      },
    };

    // Keep the URL the same so the browser sees it as the same.
    channel.originalURI = aRequest.URI;
    channel.loadGroup = aRequest.loadGroup;
    channel.loadInfo.originAttributes = aRequest.loadInfo.originAttributes;

    // We can use the resource principal when data is fetched by the chrome,
    // e.g. useful for NoScript. Make make sure we reuse the origin attributes
    // from the request channel to keep isolation consistent.
    var uri = lazy.NetUtil.newURI(PDF_VIEWER_WEB_PAGE);
    var resourcePrincipal =
      Services.scriptSecurityManager.createContentPrincipal(
        uri,
        aRequest.loadInfo.originAttributes
      );
    // Remember the principal we would have had before we mess with it.
    let originalPrincipal =
      Services.scriptSecurityManager.getChannelResultPrincipal(aRequest);
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
