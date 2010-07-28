// This stays here because otherwise it's hard to tell if there's a parsing error
dump("###################################### content loaded\n");

// how many milliseconds before the mousedown and the overlay of an element
const kTapOverlayTimeout = 200;

let Cc = Components.classes;
let Ci = Components.interfaces;
let Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");

let gFocusManager = Cc["@mozilla.org/focus-manager;1"]
  .getService(Ci.nsIFocusManager);

let XULDocument = Ci.nsIDOMXULDocument;
let HTMLHtmlElement = Ci.nsIDOMHTMLHtmlElement;
let HTMLIFrameElement = Ci.nsIDOMHTMLIFrameElement;
let HTMLFrameElement = Ci.nsIDOMHTMLFrameElement;


/** Watches for mouse click in content and redirect them to the best found target **/
const ElementTouchHelper = {
  get radius() {
    let prefs = Services.prefs;
    delete this.radius;
    return this.radius = { "top": prefs.getIntPref("browser.ui.touch.top"),
                           "right": prefs.getIntPref("browser.ui.touch.right"),
                           "bottom": prefs.getIntPref("browser.ui.touch.bottom"),
                           "left": prefs.getIntPref("browser.ui.touch.left")
                         };
  },

  get weight() {
    delete this.weight;
    return this.weight = { "visited": Services.prefs.getIntPref("browser.ui.touch.weight.visited")
                         };
  },

  /* Retrieve the closest element to a point by looking at borders position */
  getClosest: function getClosest(aWindowUtils, aX, aY) {
    let target = aWindowUtils.elementFromPoint(aX, aY,
                                               true,   /* ignore root scroll frame*/
                                               false); /* don't flush layout */

    let nodes = aWindowUtils.nodesFromRect(aX, aY, this.radius.top,
                                                   this.radius.right,
                                                   this.radius.bottom,
                                                   this.radius.left, true, false);

    // return early if the click is just over a clickable element
    if (this._isElementClickable(target, nodes))
      return target;

    let threshold = Number.POSITIVE_INFINITY;
    for (let i = 0; i < nodes.length; i++) {
      let current = nodes[i];
      if (!current.mozMatchesSelector || !this._isElementClickable(current))
        continue;

      let rect = current.getBoundingClientRect();
      let distance = this._computeDistanceFromRect(aX, aY, rect);

      // increase a little bit the weight for already visited items
      if (current && current.mozMatchesSelector("*:visited"))
        distance *= (this.weight.visited / 100);

      if (distance < threshold) {
        target = current;
        threshold = distance;
      }
    }

    return target;
  },

  _isElementClickable: function _isElementClickable(aElement, aElementsInRect) {
    let isClickable = this._hasMouseListener(aElement);

    // If possible looks in the parents node to find a target
    if (aElement && !isClickable && aElementsInRect) {
      let parentNode = aElement.parentNode;
      let count = aElementsInRect.length;
      for (let i = 0; i < count && parentNode; i++) {
        if (aElementsInRect[i] != parentNode)
          continue;

        isClickable = this._hasMouseListener(parentNode);
        if (isClickable)
          break;

        parentNode = parentNode.parentNode;
      }
    }

    return aElement && (isClickable || aElement.mozMatchesSelector("a,*:link,*:visited,*[role=button],button,input,select,label"));
  },

  _computeDistanceFromRect: function _computeDistanceFromRect(aX, aY, aRect) {
    let x = 0, y = 0;
    let xmost = aRect.left + aRect.width;
    let ymost = aRect.top + aRect.height;

    // compute horizontal distance from left/right border depending if X is
    // before/inside/after the element's rectangle
    if (aRect.left < aX && aX < xmost)
      x = Math.min(xmost - aX, aX - aRect.left);
    else if (aX < aRect.left)
      x = aRect.left - aX;
    else if (aX > xmost)
      x = aX - xmost;

    // compute vertical distance from top/bottom border depending if Y is
    // above/inside/below the element's rectangle
    if (aRect.top < aY && aY < ymost)
      y = Math.min(ymost - aY, aY - aRect.top);
    else if (aY < aRect.top)
      y = aRect.top - aY;
    if (aY > ymost)
      y = aY - ymost;

    return Math.sqrt(Math.pow(x, 2) + Math.pow(y, 2));
  },

  _els: Cc["@mozilla.org/eventlistenerservice;1"].getService(Ci.nsIEventListenerService),
  _clickableEvents: ["mousedown", "mouseup", "click"],
  _hasMouseListener: function _hasMouseListener(aElement) {
    let els = this._els;
    let listeners = els.getListenerInfoFor(aElement, {});
    for (let i = 0; i < listeners.length; i++) {
      if (this._clickableEvents.indexOf(listeners[i].type) != -1)
        return true;
    }
    return false;
  }
};


/**
 * @param x,y Browser coordinates
 * @return Element at position, null if no active browser or no element found
 */
function elementFromPoint(x, y) {
  // browser's elementFromPoint expect browser-relative client coordinates.
  // subtract browser's scroll values to adjust
  let cwu = Util.getWindowUtils(content);
  let scroll = Util.getScrollOffset(content);
  x = x - scroll.x;
  y = y - scroll.y;
  let elem = ElementTouchHelper.getClosest(cwu, x, y);

  // step through layers of IFRAMEs and FRAMES to find innermost element
  while (elem && (elem instanceof HTMLIFrameElement || elem instanceof HTMLFrameElement)) {
    // adjust client coordinates' origin to be top left of iframe viewport
    let rect = elem.getBoundingClientRect();
    x -= rect.left;
    y -= rect.top;
    let windowUtils = elem.contentDocument.defaultView.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
    elem = ElementTouchHelper.getClosest(windowUtils, x, y);
  }

  return elem;
}

function getBoundingContentRect(aElement) {
  if (!aElement)
    return new Rect(0, 0, 0, 0);

  let document = aElement.ownerDocument;
  while(document.defaultView.frameElement)
    document = document.defaultView.frameElement.ownerDocument;

  let offset = Util.getScrollOffset(content);
  let r = aElement.getBoundingClientRect();

  // step out of iframes and frames, offsetting scroll values
  for (let frame = aElement.ownerDocument.defaultView; frame != content; frame = frame.parent) {
    // adjust client coordinates' origin to be top left of iframe viewport
    let rect = frame.frameElement.getBoundingClientRect();
    let left = frame.getComputedStyle(frame.frameElement, "").borderLeftWidth;
    let top = frame.getComputedStyle(frame.frameElement, "").borderTopWidth;
    offset.add(rect.left + parseInt(left), rect.top + parseInt(top));
  }

  return new Rect(r.left + offset.x, r.top + offset.y, r.width, r.height);
}

function getContentClientRects(aElement) {
  let offset = Util.getScrollOffset(content);
  let nativeRects = aElement.getClientRects();
  // step out of iframes and frames, offsetting scroll values
  for (let frame = aElement.ownerDocument.defaultView; frame != content; frame = frame.parent) {
    // adjust client coordinates' origin to be top left of iframe viewport
    let rect = frame.frameElement.getBoundingClientRect();
    let left = frame.getComputedStyle(frame.frameElement, "").borderLeftWidth;
    let top = frame.getComputedStyle(frame.frameElement, "").borderTopWidth;
    offset.add(rect.left + parseInt(left), rect.top + parseInt(top));
  }

  let result = [];
  for (let i = nativeRects.length - 1; i >= 0; i--) {
    let r = nativeRects[i];
    result.push({ left: r.left + offset.x,
                  top: r.top + offset.y,
                  width: r.width,
                  height: r.height
                });
  }
  return result;
};

/** Reponsible for sending messages about viewport size changes and painting. */
function Coalescer() {
  this._pendingDirtyRect = new Rect(0, 0, 0, 0);
  this._pendingSizeChange = null;
  this._timer = new Util.Timeout(this);

  // XXX When moving back and forward in docShell history, MozAfterPaint does not get called properly and
  // some dirty rectangles are never flagged properly.  To fix this, coalescer will fake a paint event that
  // dirties the entire viewport.
  this._incremental = false;
}

Coalescer.prototype = {
  start: function start() {
    this._emptyPage();
    this._timer.interval(1000);
  },

  stop: function stop() {
    this._timer.flush();
  },

  notify: function notify() {
    this.flush();
  },

  handleEvent: function handleEvent(aEvent) {
    switch (aEvent.type) {
      case "MozAfterPaint": {
        let win = aEvent.originalTarget;
        let scrollOffset = Util.getScrollOffset(win);
        this.dirty(scrollOffset, aEvent.clientRects);
        break;
      }
      case "MozScrolledAreaChanged": {
        // XXX if it's possible to get a scroll area change with the same values,
        // it would be optimal if this didn't send the same message twice.
        let doc = aEvent.originalTarget;
        let win = doc.defaultView;
        let scrollOffset = Util.getScrollOffset(win);
        if (win.parent != win) // We are only interested in root scroll pane changes
          return;
        this.sizeChange(scrollOffset, aEvent.x, aEvent.y, aEvent.width, aEvent.height);
        break;
      }
      case "MozApplicationManifest": {
        let doc = aEvent.originalTarget;
        sendAsyncMessage("Browser:MozApplicationManifest", {
          location: doc.documentURIObject.spec,
          manifest: doc.documentElement.getAttribute("manifest"),
          charset: doc.characterSet
        });
        break;
      }
      case "scroll":
        let scroll = Util.getScrollOffset(content);
        sendAsyncMessage("Browser:PageScroll", { scrollX: scroll.x, scrollY: scroll.y });
        break;
    }
  },

  /** Next scroll size change event will invalidate all previous content. See constructor. */
  _emptyPage: function _emptyPage() {
    this._incremental = false;
  },

  sizeChange: function sizeChange(scrollOffset, x, y, width, height) {
    // Adjust width and height from the incoming event properties so that we
    // ignore changes to width and height contributed by growth in page
    // quadrants other than x > 0 && y > 0.
    x = x + scrollOffset.x;
    y = y + scrollOffset.y;
    this._pendingSizeChange = {
      width: width + (x < 0 ? x : 0),
      height: height + (y < 0 ? y : 0)
    };

    // Clear any pending dirty rectangles since entire viewport will be invalidated
    // anyways.
    let rect = this._pendingDirtyRect;
    rect.top = rect.bottom;
    rect.left = rect.right;

    if (!this._timer.isPending())
      this.flush();
  },

  dirty: function dirty(scrollOffset, clientRects) {
    if (!this._pendingSizeChange) {
      let unionRect = this._pendingDirtyRect;
      for (let i = clientRects.length - 1; i >= 0; i--) {
        let e = clientRects.item(i);
        unionRect.expandToContain(new Rect(
          e.left + scrollOffset.x, e.top + scrollOffset.y, e.width, e.height));
      }

      if (!this._timer.isPending())
        this.flush();
    }
  },

  flush: function flush() {
    let dirtyRect = this._pendingDirtyRect;
    let sizeChange = this._pendingSizeChange;
    if (sizeChange) {
      sendAsyncMessage("Browser:MozScrolledAreaChanged", { width: sizeChange.width, height: sizeChange.height });
      if (!this._incremental)
        sendAsyncMessage("Browser:MozAfterPaint", { rects: [ { left: 0, top: 0, right: sizeChange.width, bottom: sizeChange.height } ] });

      this._pendingSizeChange = null;

      // After first size change has been issued, assume subsequent size changes are only incremental
      // changes to the current page.
      this._incremental = true;
    }
    else if (!dirtyRect.isEmpty()) {
      // No size change has occurred, but areas have been dirtied.
      sendAsyncMessage("Browser:MozAfterPaint", { rects: [dirtyRect] });

      // Reset the rect to empty
      dirtyRect.top = dirtyRect.bottom;
      dirtyRect.left = dirtyRect.right;
    }
  }
};


/**
 * Responsible for sending messages about security, location, and page load state.
 * @param loadingController Object with methods startLoading and stopLoading
 */
function ProgressController(loadingController) {
  this._webNavigation = docShell.QueryInterface(Ci.nsIWebNavigation);
  this._overrideService = null;
  this._hostChanged = false;
  this._state = null;
  this._loadingController = loadingController || this._defaultLoadingController;
}

ProgressController.prototype = {
  // Default loading callbacks do nothing
  _defaultLoadingController: {
    startLoading: function() {},
    stopLoading: function() {}
  },

  onStateChange: function onStateChange(aWebProgress, aRequest, aStateFlags, aStatus) {
    // ignore notification that aren't about the main document (iframes, etc)
    let win = aWebProgress.DOMWindow;
    if (win != win.parent)
      return;

    // If you want to observe other state flags, be sure they're listed in the
    // Tab._createBrowser's call to addProgressListener
    if (aStateFlags & Ci.nsIWebProgressListener.STATE_IS_NETWORK) {
      if (aStateFlags & Ci.nsIWebProgressListener.STATE_START) {
        this._loadingController.startLoading();
      }
      else if (aStateFlags & Ci.nsIWebProgressListener.STATE_STOP) {
        this._loadingController.stopLoading();
      }
    }
  },

  /** This method is called to indicate progress changes for the currently loading page. */
  onProgressChange: function onProgressChange(aWebProgress, aRequest, aCurSelf, aMaxSelf, aCurTotal, aMaxTotal) {
  },

  /** This method is called to indicate a change to the current location. */
  onLocationChange: function onLocationChange(aWebProgress, aRequest, aLocationURI) {
  },

  /**
   * This method is called to indicate a status changes for the currently
   * loading page.  The message is already formatted for display.
   */
  onStatusChange: function onStatusChange(aWebProgress, aRequest, aStatus, aMessage) {
  },

  /** This method is called when the security state of the browser changes. */
  onSecurityChange: function onSecurityChange(aWebProgress, aRequest, aState) {
  },

  QueryInterface: function QueryInterface(aIID) {
    if (aIID.equals(Ci.nsIWebProgressListener) ||
        aIID.equals(Ci.nsISupportsWeakReference) ||
        aIID.equals(Ci.nsISupports)) {
        return this;
    }

    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  start: function start() {
    let flags = Ci.nsIWebProgress.NOTIFY_STATE_NETWORK;
    let webProgress = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIWebProgress);
    webProgress.addProgressListener(this, flags);
  },

  stop: function stop() {
    let webProgress = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIWebProgress);
    webProgress.removeProgressListener(this);
  }
};


/** Can't think of a good description of this class.  It probably does too much? */
function Content() {
  addMessageListener("Browser:Blur", this);
  addMessageListener("Browser:Focus", this);
  addMessageListener("Browser:MouseDown", this);
  addMessageListener("Browser:MouseUp", this);
  addMessageListener("Browser:MouseCancel", this);
  addMessageListener("Browser:SaveAs", this);
  addMessageListener("Browser:ZoomToPoint", this);

  this._coalescer = new Coalescer();
  addEventListener("MozAfterPaint", this._coalescer, false);
  addEventListener("MozScrolledAreaChanged", this._coalescer, false);
  addEventListener("MozApplicationManifest", this._coalescer, false);
  addEventListener("scroll", this._coalescer, false);

  this._progressController = new ProgressController(this);
  this._progressController.start();

  this._formAssistant = new FormAssistant();
  this._overlayTimeout = new Util.Timeout();
  this._contextTimeout = new Util.Timeout();
}

Content.prototype = {
  receiveMessage: function receiveMessage(aMessage) {
    let json = aMessage.json;
    let x = json.x;
    let y = json.y;
    let modifiers = json.modifiers;

    switch (aMessage.name) {
      case "Browser:Blur":
        docShell.isOffScreenBrowser = false;
        this._selected = false;
        break;

      case "Browser:Focus":
        docShell.isOffScreenBrowser = true;
        this._selected = true;
        break;

      case "Browser:MouseDown":
        this._overlayTimeout.clear();
        this._overlayTimeout.clear();

        let element = elementFromPoint(x, y);
        if (!element)
          return;

        if (element.mozMatchesSelector("*:link,*:visited,*:link *,*:visited *,*[role=button],button,input,option,select,textarea,label")) {
          this._overlayTimeout.once(kTapOverlayTimeout, function() {
            let rects = getContentClientRects(element);
            sendAsyncMessage("Browser:Highlight", { rects: rects });
          });
        }

        // We add a few milliseconds because of how the InputHandler wait before
        // dispatching a single click (default: 500)
        this._contextTimeout.once(500 + 200, function() {
          let event = content.document.createEvent("PopupEvents");
          event.initEvent("contextmenu", true, true);
          element.dispatchEvent(event);
        });
        break;

      case "Browser:MouseUp": {
        let element = elementFromPoint(x, y);
        if (modifiers == Ci.nsIDOMNSEvent.CONTROL_MASK) {
          let uri = Util.getHrefForElement(element);
          if (uri)
            sendAsyncMessage("Browser:OpenURI", { uri: uri });
        } else if (!this._formAssistant.open(element)) {
          sendAsyncMessage("FindAssist:Hide", { });
          this._sendMouseEvent("mousedown", element, x, y);
          this._sendMouseEvent("mouseup", element, x, y);
        }
        break;
      }

      case "Browser:MouseCancel":
        this._overlayTimeout.clear();
        this._contextTimeout.clear();
        break;

      case "Browser:SaveAs":
        if (json.type != Ci.nsIPrintSettings.kOutputFormatPDF)
          return;

        let printSettings = Cc["@mozilla.org/gfx/printsettings-service;1"]
                              .getService(Ci.nsIPrintSettingsService)
                              .newPrintSettings;
        printSettings.printSilent = true;
        printSettings.showPrintProgress = false;
        printSettings.printBGImages = true;
        printSettings.printBGColors = true;
        printSettings.printToFile = true;
        printSettings.toFileName = json.filePath;
        printSettings.printFrameType = Ci.nsIPrintSettings.kFramesAsIs;
        printSettings.outputFormat = Ci.nsIPrintSettings.kOutputFormatPDF;

        //XXX we probably need a preference here, the header can be useful
        printSettings.footerStrCenter = "";
        printSettings.footerStrLeft   = "";
        printSettings.footerStrRight  = "";
        printSettings.headerStrCenter = "";
        printSettings.headerStrLeft   = "";
        printSettings.headerStrRight  = "";

        let listener = {
          onStateChange: function(aWebProgress, aRequest, aStateFlags, aStatus) {
            if (aStateFlags & Ci.nsIWebProgressListener.STATE_STOP) {
              sendAsyncMessage("Browser:SaveAs:Return", { type: json.type, id: json.id, referrer: json.referrer });
            }
          },
          onProgressChange : function(aWebProgress, aRequest, aCurSelfProgress, aMaxSelfProgress, aCurTotalProgress, aMaxTotalProgress) {},

          // stubs for the nsIWebProgressListener interfaces which nsIWebBrowserPrint doesn't use.
          onLocationChange : function() { throw "Unexpected onLocationChange"; },
          onStatusChange   : function() { throw "Unexpected onStatusChange";   },
          onSecurityChange : function() { throw "Unexpected onSecurityChange"; }
        };

        let webBrowserPrint = content.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIWebBrowserPrint);
        webBrowserPrint.print(printSettings, listener);
        break;

      case "Browser:ZoomToPoint": {
        let rect = null;
        let element = elementFromPoint(x, y);
        let win = element.ownerDocument.defaultView;
        while (element && win.getComputedStyle(element,null).display == "inline")
          element = element.parentNode;
        if (element)
          rect = getBoundingContentRect(element);
        sendAsyncMessage("Browser:ZoomToPoint:Return", { x: x, y: y, rect: rect });
        break;
      }
    }
  },

  _sendMouseEvent: function _sendMouseEvent(aName, aElement, aX, aY) {
    // the element can be out of the aX/aY point because of the touch radius
    if (!(aElement instanceof HTMLHtmlElement)) {
      let isTouchClick = true;
      let rects = getContentClientRects(aElement);
      for (let i = 0; i < rects.length; i++) {
        let rect = rects[i];
        if ((aX > rect.left && aX < (rect.left + rect.width)) &&
            (aY > rect.top && aY < (rect.top + rect.height))) {
          isTouchClick = false;
          break;
        }
      }

      if (isTouchClick) {
        let rect = rects[0];
        let point = (new Rect(rect.left, rect.top, rect.width, rect.height)).center();
        aX = point.x;
        aY = point.y;
      }
    }

    let scrollOffset = Util.getScrollOffset(content);
    let windowUtils = Util.getWindowUtils(content);
    windowUtils.sendMouseEvent(aName, aX - scrollOffset.x, aY - scrollOffset.y, 0, 1, 0, true);
  },

  startLoading: function startLoading() {
    this._contextTimeout.clear();
    this._overlayTimeout.clear();
    this._loading = true;
    this._coalescer.start();
  },

  stopLoading: function stopLoading() {
    this._loading = false;
    this._coalescer.stop();
  },

  isSelected: function isSelected() {
    return this._selected;
  }
};

let contentObject = new Content();

let ViewportHandler = {
  metadata: null,

  init: function init() {
    addEventListener("DOMWindowCreated", this, false);
    addEventListener("DOMMetaAdded", this, false);
    addEventListener("DOMContentLoaded", this, false);
    addEventListener("pageshow", this, false);
  },

  handleEvent: function handleEvent(aEvent) {
    switch (aEvent.type) {
      case "DOMWindowCreated":
        this.resetMetadata();
        break;

      case "DOMMetaAdded":
        let target = aEvent.originalTarget;
        let isRootDocument = (target.ownerDocument == content.document);
        if (isRootDocument && target.name == "viewport")
          this.updateMetadata();
        break;

      case "DOMContentLoaded":
      case "pageshow":
        if (!this.metadata)
          this.updateMetadata();
        break;
    }
  },

  resetMetadata: function resetMetadata() {
    this.metadata = null;
    sendAsyncMessage("Browser:ViewportMetadata", {});
  },

  updateMetadata: function updateMetadata() {
    this.metadata = this.getViewportMetadata();
    sendAsyncMessage("Browser:ViewportMetadata", this.metadata);
  },

  getViewportMetadata: function getViewportMetadata() {
    let dpiScale = Services.prefs.getIntPref("zoom.dpiScale") / 100;

    let doctype = content.document.doctype;
    if (doctype && /(WAP|WML|Mobile)/.test(doctype.publicId))
      return { defaultZoom: dpiScale, autoSize: true };

    let windowUtils = Util.getWindowUtils(content);
    let handheldFriendly = windowUtils.getDocumentMetadata("HandheldFriendly");
    if (handheldFriendly == "true")
      return { defaultZoom: dpiScale, autoSize: true };

    if (content.document instanceof XULDocument)
      return { defaultZoom: 1.0, autoSize: true, allowZoom: false };

    // viewport details found here
    // http://developer.apple.com/safari/library/documentation/AppleApplications/Reference/SafariHTMLRef/Articles/MetaTags.html
    // http://developer.apple.com/safari/library/documentation/AppleApplications/Reference/SafariWebContent/UsingtheViewport/UsingtheViewport.html

    // Note: These values will be NaN if parseFloat or parseInt doesn't find a number.
    // Remember that NaN is contagious: Math.max(1, NaN) == Math.min(1, NaN) == NaN.
    let viewportScale = parseFloat(windowUtils.getDocumentMetadata("viewport-initial-scale"));
    let viewportMinScale = parseFloat(windowUtils.getDocumentMetadata("viewport-minimum-scale"));
    let viewportMaxScale = parseFloat(windowUtils.getDocumentMetadata("viewport-maximum-scale"));
    let viewportWidthStr = windowUtils.getDocumentMetadata("viewport-width");
    let viewportHeightStr = windowUtils.getDocumentMetadata("viewport-height");

    viewportScale = Util.clamp(viewportScale, kViewportMinScale, kViewportMaxScale);
    viewportMinScale = Util.clamp(viewportMinScale, kViewportMinScale, kViewportMaxScale);
    viewportMaxScale = Util.clamp(viewportMaxScale, kViewportMinScale, kViewportMaxScale);

    // If initial scale is 1.0 and width is not set, assume width=device-width
    let autoSize = (viewportWidthStr == "device-width" ||
                    viewportHeightStr == "device-height" ||
                    (viewportScale == 1.0 && !viewportWidthStr));

    let viewportWidth = Util.clamp(parseInt(viewportWidthStr), kViewportMinWidth, kViewportMaxWidth);
    let viewportHeight = Util.clamp(parseInt(viewportHeightStr), kViewportMinHeight, kViewportMaxHeight);

    // Zoom level is the final (device pixel : CSS pixel) ratio for content.
    // Since web content specifies scale as (reference pixel : CSS pixel) ratio,
    // multiply the requested scale by a constant (device pixel : reference pixel)
    // factor to account for high DPI devices.
    //
    // See bug 561445 or any of the examples of chrome/tests/browser_viewport_XX.html
    // for more information and examples.
    let defaultZoom = viewportScale * dpiScale;
    let minZoom = viewportMinScale * dpiScale;
    let maxZoom = viewportMaxScale * dpiScale;

    return {
      defaultZoom: defaultZoom,
      minZoom: minZoom,
      maxZoom: maxZoom,
      width: viewportWidth,
      height: viewportHeight,
      autoSize: autoSize,
      allowZoom: windowUtils.getDocumentMetadata("viewport-user-scalable") != "no"
    };
  }
};

ViewportHandler.init();


const kXLinkNamespace = "http://www.w3.org/1999/xlink";

var ContextHandler = {
  _getLinkURL: function ch_getLinkURL(aLink) {
    let href = aLink.href;  
    if (href)
      return href;

    href = aLink.getAttributeNS(kXLinkNamespace, "href");
    if (!href || !href.match(/\S/)) {
      // Without this we try to save as the current doc,
      // for example, HTML case also throws if empty
      throw "Empty href";
    }

    return Util.makeURLAbsolute(aLink.baseURI, href);
  },

  _getURI: function ch_getURI(aURL) {
    try {
      return Util.makeURI(aURL);
    } catch (ex) { }

    return null;
  },

  _getProtocol: function ch_getProtocol(aURI) {
    if (aURI)
      return aURI.scheme;
    return null;
  },

  _isSaveable: function ch_isSaveable(aProtocol) {
    // We don't do the Right Thing for news/snews yet, so turn them off until we do
    return aProtocol && !(aProtocol == "mailto" || aProtocol == "javascript" || aProtocol == "news" || aProtocol == "snews");
  },

  _isVoice: function ch_isVoice(aProtocol) {
    // Collection of protocols related to voice or data links
    return aProtocol && (aProtocol == "tel" || aProtocol == "callto" || aProtocol == "sip" || aProtocol == "voipto");
  },

  init: function ch_init() {
    addEventListener("contextmenu", this, false);
  },

  handleEvent: function ch_handleEvent(aEvent) {
    if (aEvent.getPreventDefault())
      return;

    let state = {
      onLink: false,
      onSaveableLink: false,
      onVoiceLink: false,
      onImage: false,
      onLoadedImage: false,
      linkURL: "",
      linkProtocol: null,
      mediaURL: ""
    };

    let popupNode = aEvent.originalTarget;

    // Do checks for nodes that never have children.
    if (popupNode.nodeType == Ci.nsIDOMNode.ELEMENT_NODE) {
      // See if the user clicked on an image.
      if (popupNode instanceof Ci.nsIImageLoadingContent && popupNode.currentURI) {
        state.onImage = true;
        state.mediaURL = popupNode.currentURI.spec;

        let request = popupNode.getRequest(Ci.nsIImageLoadingContent.CURRENT_REQUEST);
        if (request && (request.imageStatus & request.STATUS_SIZE_AVAILABLE))
          state.onLoadedImage = true;
      }
    }

    let elem = popupNode;
    while (elem) {
      if (elem.nodeType == Ci.nsIDOMNode.ELEMENT_NODE) {
        // Link?
        if (!this.onLink &&
             ((elem instanceof Ci.nsIDOMHTMLAnchorElement && elem.href) ||
              (elem instanceof Ci.nsIDOMHTMLAreaElement && elem.href) ||
              elem instanceof Ci.nsIDOMHTMLLinkElement ||
              elem.getAttributeNS(kXLinkNamespace, "type") == "simple")) {

          // Target is a link or a descendant of a link.
          state.linkURL = this._getLinkURL(elem);
          state.linkProtocol = this._getProtocol(this._getURI(state.linkURL));
          state.onLink = true;
          state.onSaveableLink = this._isSaveable(state.linkProtocol);
          state.onVoiceLink = this._isVoice(state.linkProtocol);
        }
      }

      elem = elem.parentNode;
    }
    
    sendAsyncMessage("Browser:ContextMenu", state);
  }
};

ContextHandler.init();


var FormSubmitObserver = {
  
  init: function init(){
    addMessageListener("Browser:TabOpen", this);
    addMessageListener("Browser:TabClose", this);
  },

  receiveMessage: function findHandlerReceiveMessage(aMessage) {
    let json = aMessage.json;
    switch (aMessage.name) {
      case "Browser:TabOpen":
        Services.obs.addObserver(this, "formsubmit", false);
        break;
      case "Browser:TabClose":
        Services.obs.removeObserver(this, "formsubmit", false);
        break;
    }
  },

  notify: function notify(aFormElement, aWindow, aActionURI, aCancelSubmit) {
    // Do not notify unless this is the window where the submit occurred
    if (aWindow == content)
      // We don't need to send any data along
      sendAsyncMessage("Browser:FormSubmit", {});
  },

  QueryInterface : function(aIID) {
    if (!aIID.equals(Ci.nsIFormSubmitObserver) &&
        !aIID.equals(Ci.nsISupportsWeakReference) &&
        !aIID.equals(Ci.nsISupports))
      throw Components.results.NS_ERROR_NO_INTERFACE;
    return this;
  }
};

FormSubmitObserver.init();

var FindHandler = {
  get _fastFind() {
    delete this._fastFind;
    this._fastFind = Cc["@mozilla.org/typeaheadfind;1"].createInstance(Ci.nsITypeAheadFind);
    this._fastFind.init(docShell);
    return this._fastFind;
  },

  get _selectionController() {
    delete this._selectionController;
    return this._selectionController = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                                               .getInterface(Ci.nsISelectionDisplay)
                                               .QueryInterface(Ci.nsISelectionController);
  },

  init: function findHandlerInit() {
    addMessageListener("FindAssist:Find", this);
    addMessageListener("FindAssist:Next", this);
    addMessageListener("FindAssist:Previous", this);
  },

  receiveMessage: function findHandlerReceiveMessage(aMessage) {
    let findResult = Ci.nsITypeAheadFind.FIND_NOTFOUND;
    let json = aMessage.json;
    switch (aMessage.name) {
      case "FindAssist:Find":
        findResult = this._fastFind.find(json.searchString, false);
        break;

      case "FindAssist:Previous":
        findResult = this._fastFind.findAgain(true, false);
        break;

      case "FindAssist:Next":
        findResult = this._fastFind.findAgain(false, false);
        break;
    }

    if (findResult == Ci.nsITypeAheadFind.FIND_NOTFOUND) {
      sendAsyncMessage("FindAssist:Show", { rect: null , result: findResult });
      return;
    }

    let controller = this._selectionController.getSelection(Ci.nsISelectionController.SELECTION_NORMAL);
    if (!controller.rangeCount) {
      // The selection can be into an input or a textarea element
      let nodes = content.document.querySelectorAll("input[type='text'], textarea");
      for (let i = 0; i < nodes.length; i++) {
        let node = nodes[i];
        if (node instanceof Ci.nsIDOMNSEditableElement && node.editor) {
          controller = node.editor.selectionController.getSelection(Ci.nsISelectionController.SELECTION_NORMAL);
          if (controller.rangeCount)
            break;
        }
      }
    }

    let range = controller.getRangeAt(0);
    let scroll = Util.getScrollOffset(content);
    let rect = range.getBoundingClientRect();
    rect = new Rect(scroll.x + rect.left, scroll.y + rect.top, rect.width, rect.height);
    sendAsyncMessage("FindAssist:Show", { rect: rect.isEmpty() ? null: rect , result: findResult });
  }
};

FindHandler.init();
