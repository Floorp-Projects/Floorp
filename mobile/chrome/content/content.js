// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
// This stays here because otherwise it's hard to tell if there's a parsing error
dump("###################################### content loaded\n");

let Cc = Components.classes;
let Ci = Components.interfaces;
let Cu = Components.utils;
let Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "Services", function() {
  Cu.import("resource://gre/modules/Services.jsm");
  return Services;
});

XPCOMUtils.defineLazyGetter(this, "Rect", function() {
  Cu.import("resource://gre/modules/Geometry.jsm");
  return Rect;
});

XPCOMUtils.defineLazyGetter(this, "Point", function() {
  Cu.import("resource://gre/modules/Geometry.jsm");
  return Point;
});

XPCOMUtils.defineLazyServiceGetter(this, "gFocusManager",
  "@mozilla.org/focus-manager;1", "nsIFocusManager");
XPCOMUtils.defineLazyServiceGetter(this, "gDOMUtils",
  "@mozilla.org/inspector/dom-utils;1", "inIDOMUtils");

let XULDocument = Ci.nsIDOMXULDocument;
let HTMLHtmlElement = Ci.nsIDOMHTMLHtmlElement;
let HTMLIFrameElement = Ci.nsIDOMHTMLIFrameElement;
let HTMLFrameElement = Ci.nsIDOMHTMLFrameElement;
let HTMLFrameSetElement = Ci.nsIDOMHTMLFrameSetElement;
let HTMLSelectElement = Ci.nsIDOMHTMLSelectElement;
let HTMLOptionElement = Ci.nsIDOMHTMLOptionElement;

// Blindly copied from Safari documentation for now.
const kViewportMinScale  = 0;
const kViewportMaxScale  = 10;
const kViewportMinWidth  = 200;
const kViewportMaxWidth  = 10000;
const kViewportMinHeight = 223;
const kViewportMaxHeight = 10000;

const kReferenceDpi = 240; // standard "pixel" size used in some preferences

const kStateActive = 0x00000001; // :active pseudoclass for elements

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
    if (!this.dpiRatio)
      this.dpiRatio = aWindowUtils.displayDPI / kReferenceDpi;

    let dpiRatio = this.dpiRatio;

    let target = aWindowUtils.elementFromPoint(aX, aY,
                                               true,   /* ignore root scroll frame*/
                                               false); /* don't flush layout */

    // return early if the click is just over a clickable element
    if (this._isElementClickable(target))
      return target;

    let nodes = aWindowUtils.nodesFromRect(aX, aY, this.radius.top * dpiRatio,
                                                   this.radius.right * dpiRatio,
                                                   this.radius.bottom * dpiRatio,
                                                   this.radius.left * dpiRatio, true, false);

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

  _isElementClickable: function _isElementClickable(aElement) {
    const selector = "a,:link,:visited,[role=button],button,input,select,textarea,label";
    for (let elem = aElement; elem; elem = elem.parentNode) {
      if (this._hasMouseListener(elem))
        return true;
      if (elem.mozMatchesSelector && elem.mozMatchesSelector(selector))
        return true;
    }
    return false;
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
  let scroll = ContentScroll.getScrollOffset(content);
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

  let offset = ContentScroll.getScrollOffset(content);
  offset = new Point(offset.x, offset.y);

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

function getOverflowContentBoundingRect(aElement) {
  let r = getBoundingContentRect(aElement);

  // If the overflow is hidden don't bother calculating it
  let computedStyle = aElement.ownerDocument.defaultView.getComputedStyle(aElement);
  let blockDisplays = ["block", "inline-block", "list-item"];
  if ((blockDisplays.indexOf(computedStyle.getPropertyValue("display")) != -1 && computedStyle.getPropertyValue("overflow") == "hidden") || aElement instanceof HTMLSelectElement)
    return r;

  for (let i = 0; i < aElement.childElementCount; i++)
    r = r.union(getBoundingContentRect(aElement.children[i]));

  return r;
}

function getContentClientRects(aElement) {
  let offset = ContentScroll.getScrollOffset(content);
  offset = new Point(offset.x, offset.y);

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


let Content = {
  get formAssistant() {
    delete this.formAssistant;
    return this.formAssistant = new FormAssistant();
  },

  init: function init() {
    this._isZoomedToElement = false;

    addMessageListener("Browser:Blur", this);
    addMessageListener("Browser:MouseOver", this);
    addMessageListener("Browser:MouseLong", this);
    addMessageListener("Browser:MouseDown", this);
    addMessageListener("Browser:MouseClick", this);
    addMessageListener("Browser:MouseCancel", this);
    addMessageListener("Browser:SaveAs", this);
    addMessageListener("Browser:ZoomToPoint", this);
    addMessageListener("Browser:MozApplicationCache:Fetch", this);
    addMessageListener("Browser:SetCharset", this);
    addMessageListener("Browser:ContextCommand", this);
    addMessageListener("Browser:CanUnload", this);
    addMessageListener("Browser:CanCaptureMouse", this);

    if (Util.isParentProcess())
      addEventListener("DOMActivate", this, true);

    addEventListener("MozApplicationManifest", this, false);
    addEventListener("DOMContentLoaded", this, false);
    addEventListener("pagehide", this, false);
    addEventListener("keypress", this, false, false);

    // Attach a listener to watch for "click" events bubbling up from error
    // pages and other similar page. This lets us fix bugs like 401575 which
    // require error page UI to do privileged things, without letting error
    // pages have any privilege themselves.
    addEventListener("click", this, false);

    docShell.QueryInterface(Ci.nsIDocShellHistory).useGlobalHistory = true;
  },

  handleEvent: function handleEvent(aEvent) {
    switch (aEvent.type) {
      // If the keypress is a trusted event and has not been consume by content
      // let's send it back to the chrome process to have it handle shortcuts
      case "keypress":
        let timer = new Util.Timeout(function() {
          let eventData = {
            ctrlKey: aEvent.ctrlKey,
            altKey: aEvent.altKey,
            shiftKey: aEvent.shiftKey,
            metaKey: aEvent.metaKey,
            keyCode: aEvent.keyCode,
            charCode: aEvent.charCode,
            preventDefault: aEvent.getPreventDefault()
          };
          sendAsyncMessage("Browser:KeyPress", eventData);
        });
        timer.once(0);
        break;

      case "DOMActivate": {
        // In a local tab, open remote links in new tabs.
        let target = aEvent.originalTarget;
        let href = Util.getHrefForElement(target);
        if (/^http(s?):/.test(href)) {
          aEvent.preventDefault();
          sendAsyncMessage("Browser:OpenURI", { uri: href,
                                                referrer: target.ownerDocument.documentURIObject.spec,
                                                bringFront: true });
        }
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

      case "click": {
        // Don't trust synthetic events
        if (!aEvent.isTrusted)
          return;

        let ot = aEvent.originalTarget;
        let errorDoc = ot.ownerDocument;

        // If the event came from an ssl error page, it is probably either the "Add
        // Exception…" or "Get me out of here!" button
        if (/^about:certerror\?e=nssBadCert/.test(errorDoc.documentURI)) {
          let perm = errorDoc.getElementById("permanentExceptionButton");
          let temp = errorDoc.getElementById("temporaryExceptionButton");
          if (ot == temp || ot == perm) {
            let action = (ot == perm ? "permanent" : "temporary");
            sendAsyncMessage("Browser:CertException", { url: errorDoc.location.href, action: action });
          } else if (ot == errorDoc.getElementById("getMeOutOfHereButton")) {
            sendAsyncMessage("Browser:CertException", { url: errorDoc.location.href, action: "leave" });
          }
        } else if (/^about:blocked/.test(errorDoc.documentURI)) {
          // The event came from a button on a malware/phishing block page
          // First check whether it's malware or phishing, so that we can
          // use the right strings/links
          let isMalware = /e=malwareBlocked/.test(errorDoc.documentURI);
    
          if (ot == errorDoc.getElementById("getMeOutButton")) {
            sendAsyncMessage("Browser:BlockedSite", { url: errorDoc.location.href, action: "leave" });
          } else if (ot == errorDoc.getElementById("reportButton")) {
            // This is the "Why is this site blocked" button.  For malware,
            // we can fetch a site-specific report, for phishing, we redirect
            // to the generic page describing phishing protection.
            let action = isMalware ? "report-malware" : "report-phising";
            sendAsyncMessage("Browser:BlockedSite", { url: errorDoc.location.href, action: action });
          } else if (ot == errorDoc.getElementById("ignoreWarningButton")) {
            // Allow users to override and continue through to the site,
            // but add a notify bar as a reminder, so that they don't lose
            // track after, e.g., tab switching.
            let webNav = docShell.QueryInterface(Ci.nsIWebNavigation);
            webNav.loadURI(content.location, Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_CLASSIFIER, null, null, null);
            
            // TODO: We'll need to impl notifications in the parent process and use the preference code found here:
            //       http://hg.mozilla.org/mozilla-central/file/855e5cd3c884/browser/base/content/browser.js#l2672
            //       http://hg.mozilla.org/mozilla-central/file/855e5cd3c884/browser/components/safebrowsing/content/globalstore.js
          }
        }
        break;
      }

      case "DOMContentLoaded":
        this._maybeNotifyErroPage();
        break;

      case "pagehide":
        if (aEvent.target == content.document)
          this._resetFontSize();          
        break;
    }
  },

  receiveMessage: function receiveMessage(aMessage) {
    let json = aMessage.json;
    let x = json.x;
    let y = json.y;
    let modifiers = json.modifiers;

    switch (aMessage.name) {
      case "Browser:ContextCommand": {
        let wrappedTarget = elementFromPoint(x, y);
        if (!wrappedTarget || !(wrappedTarget instanceof Ci.nsIDOMNSEditableElement))
          break;
        let target = wrappedTarget.QueryInterface(Ci.nsIDOMNSEditableElement);
        if (!target)
          break;
        switch (json.command) {
          case "select-all":
            target.editor.selectAll();
            break;
          case "paste":
            target.editor.paste(Ci.nsIClipboard.kGlobalClipboard);
            break;
        }
        target.focus();
        break;
      }

      case "Browser:Blur":
        gFocusManager.clearFocus(content);
        break;

      case "Browser:CanUnload":
        let canUnload = docShell.contentViewer.permitUnload();
        sendSyncMessage("Browser:CanUnload:Return", { permit: canUnload });
        break;

      case "Browser:MouseOver": {
        let element = elementFromPoint(x, y);
        if (!element)
          return;

        // Sending a mousemove force the dispatching of mouseover/mouseout
        this._sendMouseEvent("mousemove", element, x, y);
        break;
      }

      case "Browser:MouseDown": {
        let element = elementFromPoint(x, y);
        if (!element)
          return;

        // There is no need to have a feedback for disabled element
        let isDisabled = element instanceof HTMLOptionElement ? (element.disabled || element.parentNode.disabled) : element.disabled;
        if (isDisabled)
          return;

        // Calculate the rect of the active area
        this._doTapHighlight(element);
        break;
      }

      case "Browser:MouseCancel": {
        this._cancelTapHighlight();
        break;
      }

      case "Browser:MouseLong": {
        let element = elementFromPoint(x, y);
        if (!element)
          return;

#ifdef MOZ_PLATFORM_MAEMO
        if (element instanceof Ci.nsIDOMHTMLEmbedElement) {
          // Generate a right click mouse event to make possible to show
          // context menu for plugins:
          this._sendMouseEvent("mousedown", element, x, y, 2);
          this._sendMouseEvent("mouseup", element, x, y, 2);
          break;
        }
#endif

        ContextHandler.messageId = json.messageId;

        let event = content.document.createEvent("MouseEvent");
        event.initMouseEvent("contextmenu", true, true, content,
                             0, x, y, x, y, false, false, false, false,
                             0, null);
        event.x = x;
        event.y = y;
        element.dispatchEvent(event);
        break;
      }

      case "Browser:MouseClick": {
        this.formAssistant.focusSync = true;
        let element = elementFromPoint(x, y);
        if (modifiers == Ci.nsIDOMNSEvent.CONTROL_MASK) {
          let uri = Util.getHrefForElement(element);
          if (uri)
            sendAsyncMessage("Browser:OpenURI", { uri: uri,
                                                  referrer: element.ownerDocument.documentURIObject.spec });
          break;
        }

        if (!this.formAssistant.open(element, x, y))
          sendAsyncMessage("FindAssist:Hide", { });

        if (this._highlightElement) {
          this._sendMouseEvent("mousemove", this._highlightElement, x, y);
          this._sendMouseEvent("mousedown", this._highlightElement, x, y);
          this._sendMouseEvent("mouseup", this._highlightElement, x, y);
        }
        this._cancelTapHighlight();
        ContextHandler.reset();
        this.formAssistant.focusSync = false;
        break;
      }

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
        if (this._isZoomedToElement) {
          this._resetFontSize();
        } else {
          this._isZoomedToElement = true;
          let element = elementFromPoint(x, y);
          let win = element.ownerDocument.defaultView;
          while (element && win.getComputedStyle(element,null).display == "inline")
            element = element.parentNode;
          if (element) {
            rect = getBoundingContentRect(element);
            if (Services.prefs.getBoolPref("browser.ui.zoom.reflow")) {
              sendAsyncMessage("Browser:ZoomToPoint:Return", { x: x, y: y, zoomTo: rect });

              let fontSize = Services.prefs.getIntPref("browser.ui.zoom.reflow.fontSize");
              this._setMinFontSize(Math.max(1, rect.width / json.width) * fontSize);

              let oldRect = rect;
              rect = getBoundingContentRect(element);
              y += rect.top - oldRect.top;
            }
          }
        }
        content.setTimeout(function() {
          sendAsyncMessage("Browser:ZoomToPoint:Return", { x: x, y: y, zoomTo: rect });
        }, 0);
        break;
      }

      case "Browser:MozApplicationCache:Fetch": {
        let currentURI = Services.io.newURI(json.location, json.charset, null);
        let manifestURI = Services.io.newURI(json.manifest, json.charset, currentURI);
        let updateService = Cc["@mozilla.org/offlinecacheupdate-service;1"]
                            .getService(Ci.nsIOfflineCacheUpdateService);
        updateService.scheduleUpdate(manifestURI, currentURI, content);
        break;
      }

      case "Browser:SetCharset": {
        let docCharset = docShell.QueryInterface(Ci.nsIDocCharset);
        docCharset.charset = json.charset;

        let webNav = docShell.QueryInterface(Ci.nsIWebNavigation);
        webNav.reload(Ci.nsIWebNavigation.LOAD_FLAGS_CHARSET_CHANGE);
        break;
      }

      case "Browser:CanCaptureMouse": {
        sendAsyncMessage("Browser:CanCaptureMouse:Return", {
          contentMightCaptureMouse: content.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils).mayHaveTouchEventListeners
        });
        break;
      }
    }
  },

  _maybeNotifyErroPage: function _maybeNotifyErroPage() {
    // Notify browser that an error page is being shown instead
    // of the target location. Necessary to get proper thumbnail
    // updates on chrome for error pages.
    if (content.location.href !== content.document.documentURI)
      sendAsyncMessage("Browser:ErrorPage", null);
  },

  _resetFontSize: function _resetFontSize() {
    this._isZoomedToElement = false;
    this._setMinFontSize(0);
  },

  _highlightElement: null,

  _doTapHighlight: function _doTapHighlight(aElement) {
    gDOMUtils.setContentState(aElement, kStateActive);
    this._highlightElement = aElement;
  },

  _cancelTapHighlight: function _cancelTapHighlight(aElement) {
    gDOMUtils.setContentState(content.document.documentElement, kStateActive);
    this._highlightElement = null;
  },

  _sendMouseEvent: function _sendMouseEvent(aName, aElement, aX, aY, aButton) {
    // the element can be out of the aX/aY point because of the touch radius
    // if outside, we gracefully move the touch point to the center of the element
    if (!(aElement instanceof HTMLHtmlElement)) {
      let isTouchClick = true;
      let rects = getContentClientRects(aElement);
      for (let i = 0; i < rects.length; i++) {
        let rect = rects[i];
        // We might be able to deal with fractional pixels, but mouse events won't.
        // Deflate the bounds in by 1 pixel to deal with any fractional scroll offset issues.
        let inBounds = 
          (aX > rect.left + 1 && aX < (rect.left + rect.width - 1)) &&
          (aY > rect.top + 1 && aY < (rect.top + rect.height - 1));
        if (inBounds) {
          isTouchClick = false;
          break;
        }
      }

      if (isTouchClick) {
        let rect = new Rect(rects[0].left, rects[0].top, rects[0].width, rects[0].height);
        if (rect.isEmpty())
          return;

        let point = rect.center();
        aX = point.x;
        aY = point.y;
      }
    }

    let scrollOffset = ContentScroll.getScrollOffset(content);
    let windowUtils = Util.getWindowUtils(content);
    aButton = aButton || 0;
    windowUtils.sendMouseEventToWindow(aName, aX - scrollOffset.x, aY - scrollOffset.y, aButton, 1, 0, true);
  },

  _setMinFontSize: function _setMinFontSize(aSize) {
    let viewer = docShell.contentViewer.QueryInterface(Ci.nsIMarkupDocumentViewer);
    if (viewer)
      viewer.minFontSize = aSize;
  }
};

Content.init();

let ViewportHandler = {
  init: function init() {
    addEventListener("DOMWindowCreated", this, false);
    addEventListener("DOMMetaAdded", this, false);
    addEventListener("DOMContentLoaded", this, false);
    addEventListener("pageshow", this, false);
  },

  handleEvent: function handleEvent(aEvent) {
    let target = aEvent.originalTarget;
    let isRootDocument = (target == content.document || target.ownerDocument == content.document);
    if (!isRootDocument)
      return;

    switch (aEvent.type) {
      case "DOMWindowCreated":
        this.resetMetadata();
        break;

      case "DOMMetaAdded":
        if (target.name == "viewport")
          this.updateMetadata();
        break;

      case "DOMContentLoaded":
      case "pageshow":
        this.updateMetadata();
        break;
    }
  },

  resetMetadata: function resetMetadata() {
    sendAsyncMessage("Browser:ViewportMetadata", null);
  },

  updateMetadata: function updateMetadata() {
    sendAsyncMessage("Browser:ViewportMetadata", this.getViewportMetadata());
  },

  /**
   * Returns an object with the page's preferred viewport properties:
   *   defaultZoom (optional float): The initial scale when the page is loaded.
   *   minZoom (optional float): The minimum zoom level.
   *   maxZoom (optional float): The maximum zoom level.
   *   width (optional int): The CSS viewport width in px.
   *   height (optional int): The CSS viewport height in px.
   *   autoSize (boolean): Resize the CSS viewport when the window resizes.
   *   allowZoom (boolean): Let the user zoom in or out.
   *   autoScale (boolean): Adjust the viewport properties to account for display density.
   */
  getViewportMetadata: function getViewportMetadata() {
    let doctype = content.document.doctype;
    if (doctype && /(WAP|WML|Mobile)/.test(doctype.publicId))
      return { defaultZoom: 1, autoSize: true, allowZoom: true, autoScale: true };

    let windowUtils = Util.getWindowUtils(content);
    let handheldFriendly = windowUtils.getDocumentMetadata("HandheldFriendly");
    if (handheldFriendly == "true")
      return { defaultZoom: 1, autoSize: true, allowZoom: true, autoScale: true };

    if (content.document instanceof XULDocument)
      return { defaultZoom: 1, autoSize: true, allowZoom: false, autoScale: false };

    // HACK: Since we can't set the scale in local tabs (bug 597081), we force
    // them to device-width and scale=1 so they will lay out reasonably.
    if (Util.isParentProcess())
      return { defaultZoom: 1, autoSize: true, allowZoom: false, autoScale: false };

    // HACK: Since we can't set the scale correctly in frameset pages yet (bug 645756), we force
    // them to device-width and scale=1 so they will lay out reasonably.
    if (content.frames.length > 0 && (content.document.body instanceof HTMLFrameSetElement))
      return { defaultZoom: 1, autoSize: true, allowZoom: false, autoScale: false };

    // viewport details found here
    // http://developer.apple.com/safari/library/documentation/AppleApplications/Reference/SafariHTMLRef/Articles/MetaTags.html
    // http://developer.apple.com/safari/library/documentation/AppleApplications/Reference/SafariWebContent/UsingtheViewport/UsingtheViewport.html

    // Note: These values will be NaN if parseFloat or parseInt doesn't find a number.
    // Remember that NaN is contagious: Math.max(1, NaN) == Math.min(1, NaN) == NaN.
    let scale = parseFloat(windowUtils.getDocumentMetadata("viewport-initial-scale"));
    let minScale = parseFloat(windowUtils.getDocumentMetadata("viewport-minimum-scale"));
    let maxScale = parseFloat(windowUtils.getDocumentMetadata("viewport-maximum-scale"));

    let widthStr = windowUtils.getDocumentMetadata("viewport-width");
    let heightStr = windowUtils.getDocumentMetadata("viewport-height");
    let width = Util.clamp(parseInt(widthStr), kViewportMinWidth, kViewportMaxWidth);
    let height = Util.clamp(parseInt(heightStr), kViewportMinHeight, kViewportMaxHeight);

    let allowZoomStr = windowUtils.getDocumentMetadata("viewport-user-scalable");
    let allowZoom = !/^(0|no|false)$/.test(allowZoomStr); // WebKit allows 0, "no", or "false"

    scale = Util.clamp(scale, kViewportMinScale, kViewportMaxScale);
    minScale = Util.clamp(minScale, kViewportMinScale, kViewportMaxScale);
    maxScale = Util.clamp(maxScale, kViewportMinScale, kViewportMaxScale);

    // If initial scale is 1.0 and width is not set, assume width=device-width
    let autoSize = (widthStr == "device-width" ||
                    (!widthStr && (heightStr == "device-height" || scale == 1.0)));

    return {
      defaultZoom: scale,
      minZoom: minScale,
      maxZoom: maxScale,
      width: width,
      height: height,
      autoSize: autoSize,
      allowZoom: allowZoom,
      autoScale: true
    };
  }
};

ViewportHandler.init();


const kXLinkNamespace = "http://www.w3.org/1999/xlink";

var ContextHandler = {
  _types: [],

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

  init: function ch_init() {
    addEventListener("contextmenu", this, false);
    addEventListener("pagehide", this, false);
    addMessageListener("Browser:ContextCommand", this, false);
    this.popupNode = null;
  },

  reset: function ch_reset() {
    this.popupNode = null;
  },

  handleEvent: function ch_handleEvent(aEvent) {
    switch (aEvent.type) {
      case "contextmenu":
        this.onContextMenu(aEvent);
        break;
      case "pagehide":
        this.reset();
        break;
    }
  },

  onContextMenu: function ch_onContextMenu(aEvent) {
    if (aEvent.getPreventDefault())
      return;

    let state = {
      types: [],
      label: "",
      linkURL: "",
      linkTitle: "",
      linkProtocol: null,
      mediaURL: "",
      x: aEvent.x,
      y: aEvent.y
    };

    let popupNode = this.popupNode = aEvent.originalTarget;

    // Do checks for nodes that never have children.
    if (popupNode.nodeType == Ci.nsIDOMNode.ELEMENT_NODE) {
      // See if the user clicked on an image.
      if (popupNode instanceof Ci.nsIImageLoadingContent && popupNode.currentURI) {
        state.types.push("image");
        state.label = state.mediaURL = popupNode.currentURI.spec;

        // Retrieve the type of image from the cache since the url can fail to
        // provide valuable informations
        try {
          let imageCache = Cc["@mozilla.org/image/cache;1"].getService(Ci.imgICache);
          let props = imageCache.findEntryProperties(popupNode.currentURI, content.document.characterSet);
          if (props) {
            state.contentType = String(props.get("type", Ci.nsISupportsCString));
            state.contentDisposition = String(props.get("content-disposition", Ci.nsISupportsCString));
          }
        } catch (e) {
          // Failure to get type and content-disposition off the image is non-fatal
        }

      } else if (popupNode instanceof Ci.nsIDOMHTMLMediaElement) {
        state.label = state.mediaURL = (popupNode.currentSrc || popupNode.src);
        state.types.push((popupNode.paused || popupNode.ended) ? "media-paused" : "media-playing");
        if (popupNode instanceof Ci.nsIDOMHTMLVideoElement)
          state.types.push("video");
      }
    }

    let elem = popupNode;
    while (elem) {
      if (elem.nodeType == Ci.nsIDOMNode.ELEMENT_NODE) {
        // Link?
        if ((elem instanceof Ci.nsIDOMHTMLAnchorElement && elem.href) ||
            (elem instanceof Ci.nsIDOMHTMLAreaElement && elem.href) ||
            elem instanceof Ci.nsIDOMHTMLLinkElement ||
            elem.getAttributeNS(kXLinkNamespace, "type") == "simple") {

          // Target is a link or a descendant of a link.
          state.types.push("link");
          state.label = state.linkURL = this._getLinkURL(elem);
          state.linkTitle = popupNode.textContent || popupNode.title;
          state.linkProtocol = this._getProtocol(this._getURI(state.linkURL));
          break;
        } else if ((elem instanceof Ci.nsIDOMHTMLInputElement &&
                    elem.mozIsTextField(false)) || elem instanceof Ci.nsIDOMHTMLTextAreaElement) {
          let selectionStart = elem.selectionStart;
          let selectionEnd = elem.selectionEnd;

          state.types.push("input-text");

          // Don't include "copy" for password fields.
          if (!(elem instanceof Ci.nsIDOMHTMLInputElement) || elem.mozIsTextField(true)) {
            if (selectionStart != selectionEnd) {
              state.types.push("copy");
              state.string = elem.value.slice(selectionStart, selectionEnd);
            } else if (elem.value) {
              state.types.push("copy-all");
              state.string = elem.value;
            }
          }

          if (selectionStart > 0 || selectionEnd < elem.textLength)
            state.types.push("select-all");

          let clipboard = Cc["@mozilla.org/widget/clipboard;1"].getService(Ci.nsIClipboard);
          let flavors = ["text/unicode"];
          let hasData = clipboard.hasDataMatchingFlavors(flavors, flavors.length, Ci.nsIClipboard.kGlobalClipboard);

          if (hasData && !elem.readOnly)
            state.types.push("paste");
          break;
        } else if (elem instanceof Ci.nsIDOMHTMLParagraphElement ||
                   elem instanceof Ci.nsIDOMHTMLDivElement ||
                   elem instanceof Ci.nsIDOMHTMLLIElement ||
                   elem instanceof Ci.nsIDOMHTMLPreElement ||
                   elem instanceof Ci.nsIDOMHTMLHeadingElement ||
                   elem instanceof Ci.nsIDOMHTMLTableCellElement) {
          state.types.push("content-text");
          break;
        }
      }

      elem = elem.parentNode;
    }

    for (let i = 0; i < this._types.length; i++)
      if (this._types[i].handler(state, popupNode))
        state.types.push(this._types[i].name);

    state.messageId = this.messageId;

    sendAsyncMessage("Browser:ContextMenu", state);
  },

  receiveMessage: function ch_receiveMessage(aMessage) {
    let node = this.popupNode;
    let command = aMessage.json.command;

    switch (command) {
      case "play":
      case "pause":
        if (node instanceof Ci.nsIDOMHTMLMediaElement)
          node[command]();
        break;

      case "fullscreen":
        if (node instanceof Ci.nsIDOMHTMLVideoElement) {
          node.pause();
          Cu.import("resource:///modules/video.jsm");
          Video.fullScreenSourceElement = node;
          sendAsyncMessage("Browser:FullScreenVideo:Start");
        }
        break;
    }
  },

  /**
   * For add-ons to add new types and data to the ContextMenu message.
   *
   * @param aName A string to identify the new type.
   * @param aHandler A function that takes a state object and a target element.
   *    If aHandler returns true, then aName will be added to the list of types.
   *    The function may also modify the state object.
   */
  registerType: function registerType(aName, aHandler) {
    this._types.push({name: aName, handler: aHandler});
  },

  /** Remove all handlers registered for a given type. */
  unregisterType: function unregisterType(aName) {
    this._types = this._types.filter(function(type) type.name != aName);
  }
};

ContextHandler.init();

ContextHandler.registerType("mailto", function(aState, aElement) {
  return aState.linkProtocol == "mailto";
});

ContextHandler.registerType("callto", function(aState, aElement) {
  let protocol = aState.linkProtocol;
  return protocol == "tel" || protocol == "callto" || protocol == "sip" || protocol == "voipto";
});

ContextHandler.registerType("link-openable", function(aState, aElement) {
  return Util.isOpenableScheme(aState.linkProtocol);
});

ContextHandler.registerType("link-shareable", function(aState, aElement) {
  return Util.isShareableScheme(aState.linkProtocol);
});

["image", "video"].forEach(function(aType) {
  ContextHandler.registerType(aType+"-shareable", function(aState, aElement) {
    if (aState.types.indexOf(aType) == -1)
      return false;

    let protocol = ContextHandler._getProtocol(ContextHandler._getURI(aState.mediaURL));
    return Util.isShareableScheme(protocol);
  });
});

ContextHandler.registerType("image-loaded", function(aState, aElement) {
  if (aState.types.indexOf("image") != -1) {
    let request = aElement.getRequest(Ci.nsIImageLoadingContent.CURRENT_REQUEST);
    if (request && (request.imageStatus & request.STATUS_SIZE_AVAILABLE))
      return true;
  }
  return false;
});

var FormSubmitObserver = {
  init: function init(){
    addMessageListener("Browser:TabOpen", this);
    addMessageListener("Browser:TabClose", this);

    addEventListener("pageshow", this, false);

    Services.obs.addObserver(this, "invalidformsubmit", false);
  },

  handleEvent: function handleEvent(aEvent) {
    let target = aEvent.originalTarget;
    let isRootDocument = (target == content.document || target.ownerDocument == content.document);
    if (!isRootDocument)
      return;

    // Reset invalid submit state on each pageshow
    if (aEvent.type == "pageshow")
      Content.formAssistant.invalidSubmit = false;
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

  notifyInvalidSubmit: function notifyInvalidSubmit(aFormElement, aInvalidElements) {
    if (!aInvalidElements.length)
      return;

    let element = aInvalidElements.queryElementAt(0, Ci.nsISupports);
    if (!(element instanceof HTMLInputElement ||
          element instanceof HTMLTextAreaElement ||
          element instanceof HTMLSelectElement ||
          element instanceof HTMLButtonElement)) {
      return;
    }

    Content.formAssistant.invalidSubmit = true;
    Content.formAssistant.open(element);
  },

  QueryInterface : function(aIID) {
    if (!aIID.equals(Ci.nsIFormSubmitObserver) &&
        !aIID.equals(Ci.nsISupportsWeakReference) &&
        !aIID.equals(Ci.nsISupports))
      throw Cr.NS_ERROR_NO_INTERFACE;
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

    let selection = this._fastFind.currentWindow.getSelection();
    if (!selection.rangeCount || selection.isCollapsed) {
      // The selection can be into an input or a textarea element
      let nodes = content.document.querySelectorAll("input[type='text'], textarea");
      for (let i = 0; i < nodes.length; i++) {
        let node = nodes[i];
        if (node instanceof Ci.nsIDOMNSEditableElement && node.editor) {
          selection = node.editor.selectionController.getSelection(Ci.nsISelectionController.SELECTION_NORMAL);
          if (selection.rangeCount && !selection.isCollapsed)
            break;
        }
      }
    }

    let scroll = ContentScroll.getScrollOffset(content);
    for (let frame = this._fastFind.currentWindow; frame != content; frame = frame.parent) {
      let rect = frame.frameElement.getBoundingClientRect();
      let left = frame.getComputedStyle(frame.frameElement, "").borderLeftWidth;
      let top = frame.getComputedStyle(frame.frameElement, "").borderTopWidth;
      scroll.add(rect.left + parseInt(left), rect.top + parseInt(top));
    }

    let rangeRect = selection.getRangeAt(0).getBoundingClientRect();
    let rect = new Rect(scroll.x + rangeRect.left, scroll.y + rangeRect.top, rangeRect.width, rangeRect.height);

    // Ensure the potential "scroll" event fired during a search as already fired
    let timer = new Util.Timeout(function() {
      sendAsyncMessage("FindAssist:Show", { rect: rect.isEmpty() ? null: rect , result: findResult });
    });
    timer.once(0);
  }
};

FindHandler.init();

var ConsoleAPIObserver = {
  init: function init() {
    addMessageListener("Browser:TabOpen", this);
    addMessageListener("Browser:TabClose", this);
  },

  receiveMessage: function receiveMessage(aMessage) {
    let json = aMessage.json;
    switch (aMessage.name) {
      case "Browser:TabOpen":
        Services.obs.addObserver(this, "console-api-log-event", false);
        break;
      case "Browser:TabClose":
        Services.obs.removeObserver(this, "console-api-log-event", false);
        break;
    }
  },

  observe: function observe(aMessage, aTopic, aData) {
    let contentWindowId = content.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils).outerWindowID;
    aMessage = aMessage.wrappedJSObject;
    if (aMessage.ID != contentWindowId)
      return;

    let mappedArguments = Array.map(aMessage.arguments, this.formatResult, this);
    let joinedArguments = Array.join(mappedArguments, " ");

    if (aMessage.level == "error" || aMessage.level == "warn") {
      let flag = (aMessage.level == "error" ? Ci.nsIScriptError.errorFlag : Ci.nsIScriptError.warningFlag);
      let consoleMsg = Cc["@mozilla.org/scripterror;1"].createInstance(Ci.nsIScriptError);
      consoleMsg.init(joinedArguments, null, null, 0, 0, flag, "content javascript");
      Services.console.logMessage(consoleMsg);
    } else if (aMessage.level == "trace") {
      let bundle = Services.strings.createBundle("chrome://global/locale/headsUpDisplay.properties");
      let args = aMessage.arguments;
      let filename = this.abbreviateSourceURL(args[0].filename);
      let functionName = args[0].functionName || bundle.GetStringFromName("stacktrace.anonymousFunction");
      let lineNumber = args[0].lineNumber;

      let body = bundle.formatStringFromName("stacktrace.outputMessage", [filename, functionName, lineNumber], 3);
      body += "\n";
      args.forEach(function(aFrame) {
        body += aFrame.filename + " :: " + aFrame.functionName + " :: " + aFrame.lineNumber + "\n";
      });

      Services.console.logStringMessage(body);
    } else {
      Services.console.logStringMessage(joinedArguments);
    }
  },

  getResultType: function getResultType(aResult) {
    let type = aResult === null ? "null" : typeof aResult;
    if (type == "object" && aResult.constructor && aResult.constructor.name)
      type = aResult.constructor.name;
    return type.toLowerCase();
  },

  formatResult: function formatResult(aResult) {
    let output = "";
    let type = this.getResultType(aResult);
    switch (type) {
      case "string":
      case "boolean":
      case "date":
      case "error":
      case "number":
      case "regexp":
        output = aResult.toString();
        break;
      case "null":
      case "undefined":
        output = type;
        break;
      default:
        if (aResult.toSource) {
          try {
            output = aResult.toSource();
          } catch (ex) { }
        }
        if (!output || output == "({})") {
          output = aResult.toString();
        }
        break;
    }

    return output;
  },

  abbreviateSourceURL: function abbreviateSourceURL(aSourceURL) {
    // Remove any query parameters.
    let hookIndex = aSourceURL.indexOf("?");
    if (hookIndex > -1)
      aSourceURL = aSourceURL.substring(0, hookIndex);

    // Remove a trailing "/".
    if (aSourceURL[aSourceURL.length - 1] == "/")
      aSourceURL = aSourceURL.substring(0, aSourceURL.length - 1);

    // Remove all but the last path component.
    let slashIndex = aSourceURL.lastIndexOf("/");
    if (slashIndex > -1)
      aSourceURL = aSourceURL.substring(slashIndex + 1);

    return aSourceURL;
  }
};

ConsoleAPIObserver.init();

var TouchEventHandler = {
  element: null,
  isCancellable: true,

  init: function() {
    addMessageListener("Browser:MouseUp", this);
    addMessageListener("Browser:MouseDown", this);
    addMessageListener("Browser:MouseMove", this);
  },

  receiveMessage: function(aMessage) {
    let json = aMessage.json;
    if (Util.isParentProcess())
      return;

    if (!content.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils).mayHaveTouchEventListeners) {
      sendAsyncMessage("Browser:CaptureEvents", {
        type: null,
        messageId: json.messageId,
        click: false, panning: false,
        contentMightCaptureMouse: false
      });
      return;
    }

    let type;
    switch (aMessage.name) {
      case "Browser:MouseDown":
        this.isCancellable = true;
        this.element = elementFromPoint(json.x, json.y);
        type = "touchstart";
        break;

      case "Browser:MouseUp":
        this.isCancellable = false;
        type = "touchend";
        break;

      case "Browser:MouseMove":
        type = "touchmove";
        break;
    }

    if (!this.element)
      return;

    let cancelled = !this.sendEvent(type, json, this.element);
    if (type == "touchend")
      this.element = null;

    if (this.isCancellable) {
      sendAsyncMessage("Browser:CaptureEvents", { messageId: json.messageId,
                                                  type: type,
                                                  contentMightCaptureMouse: true,
                                                  click: cancelled && aMessage.name == "Browser:MouseDown",
                                                  panning: cancelled });
      // Panning can be cancelled only during the "touchstart" event and the
      // first "touchmove" event.  After it's cancelled, it stays cancelled
      // until the next touchstart event.
      if (cancelled || aMessage.name == "Browser:MouseMove")
        this.isCancellable = false;
    }
  },

  sendEvent: function(aName, aData, aElement) {
    if (!Services.prefs.getBoolPref("dom.w3c_touch_events.enabled"))
      return true;

    let evt = content.document.createEvent("touchevent");
    let point = content.document.createTouch(content, aElement, 0,
                                             aData.x, aData.y, aData.x, aData.y, aData.x, aData.y,
                                             1, 1, 0, 0);
    let touches = content.document.createTouchList(point);
    if (aName == "touchend") {
      let empty = content.document.createTouchList();
      evt.initTouchEvent(aName, true, true, content, 0, true, true, true, true, empty, empty, touches);      
    } else {
      evt.initTouchEvent(aName, true, true, content, 0, true, true, true, true, touches, touches, touches);
    }
    return aElement.dispatchEvent(evt);
  }
};

TouchEventHandler.init();

var SelectionHandler = {
  cache: {},
  selectedText: "",
  contentWindow: null,
  
  init: function sh_init() {
    addMessageListener("Browser:SelectionStart", this);
    addMessageListener("Browser:SelectionEnd", this);
    addMessageListener("Browser:SelectionMove", this);
  },

  getCurrentWindowAndOffset: function(x, y, offset) {
    let utils = Util.getWindowUtils(content);
    let elem = utils.elementFromPoint(x, y, true, false);
    while (elem && (elem instanceof HTMLIFrameElement || elem instanceof HTMLFrameElement)) {
      // adjust client coordinates' origin to be top left of iframe viewport
      let rect = elem.getBoundingClientRect();
      scrollOffset = ContentScroll.getScrollOffset(elem.ownerDocument.defaultView);
      offset.x += rect.left;
      x -= rect.left;
      
      offset.y += rect.top + scrollOffset.y;
      y -= rect.top + scrollOffset.y;
      utils = elem.contentDocument.defaultView.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
      elem = utils.elementFromPoint(x, y, true, false);
    }
    if (!elem)
      return {};
    
    return { contentWindow: elem.ownerDocument.defaultView, offset: offset };
  },

  receiveMessage: function sh_receiveMessage(aMessage) {
    let scrollOffset = ContentScroll.getScrollOffset(content);
    let utils = Util.getWindowUtils(content);
    let json = aMessage.json;

    switch (aMessage.name) {
      case "Browser:SelectionStart": {
        // Clear out the text cache
        this.selectedText = "";

        // if this is an iframe, dig down to find the document that was clicked
        let x = json.x - scrollOffset.x;
        let y = json.y - scrollOffset.y;
        let { contentWindow: contentWindow, offset: offset } = this.getCurrentWindowAndOffset(x, y, scrollOffset);
        if (!contentWindow)
          return;

        let currentDocShell = contentWindow.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIWebNavigation).QueryInterface(Ci.nsIDocShell);

        // Remove any previous selected or created ranges. Tapping anywhere on a
        // page will create an empty range.
        let selection = contentWindow.getSelection();
        selection.removeAllRanges();

          // Position the caret using a fake mouse click
          utils.sendMouseEventToWindow("mousedown", x, y, 0, 1, 0, true);
          utils.sendMouseEventToWindow("mouseup", x, y, 0, 1, 0, true);

        try {
          let selcon = currentDocShell.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsISelectionDisplay).QueryInterface(Ci.nsISelectionController);

          // Select the word nearest the caret
          selcon.wordMove(false, false);
          selcon.wordMove(true, true);
        } catch(e) {
          // If we couldn't select the word at the given point, bail
          return;
        }

        // Find the selected text rect and send it back so the handles can position correctly
        if (selection.rangeCount == 0)
          return;

        let range = selection.getRangeAt(0).QueryInterface(Ci.nsIDOMNSRange);
        if (!range)
          return;

        // Cache the selected text since the selection might be gone by the time we get the "end" message
        this.selectedText = selection.toString().trim();

        // If the range didn't have any text, let's bail
        if (!this.selectedText.length) {
          selection.collapseToStart();
          return;
        }

        this.cache = this._extractFromRange(range, offset);
        this.contentWindow = contentWindow;

        sendAsyncMessage("Browser:SelectionRange", this.cache);
        break;
      }

      case "Browser:SelectionEnd": {
        let tap = { x: json.x - this.cache.offset.x, y: json.y - this.cache.offset.y };
        pointInSelection = (tap.x > this.cache.rect.left && tap.x < this.cache.rect.right) && (tap.y > this.cache.rect.top && tap.y < this.cache.rect.bottom);

        try {
          // The selection might already be gone
          if (this.contentWindow)
            this.contentWindow.getSelection().removeAllRanges();
          this.contentWindow = null;
        } catch(e) {
          Cu.reportError(e);
        }

        if (pointInSelection && this.selectedText.length) {
          let clipboard = Cc["@mozilla.org/widget/clipboardhelper;1"].getService(Ci.nsIClipboardHelper);
          clipboard.copyString(this.selectedText);
          sendAsyncMessage("Browser:SelectionCopied", { succeeded: true });
        } else {
          sendAsyncMessage("Browser:SelectionCopied", { succeeded: false });
        }
        break;
      }

      case "Browser:SelectionMove": {
        if (!this.contentWindow)
          return;

        // Hack to avoid setting focus in a textbox [Bugs 654352 & 667243]
        let elemUnder = elementFromPoint(json.x - scrollOffset.x, json.y - scrollOffset.y);
        if (elemUnder && elemUnder instanceof Ci.nsIDOMHTMLInputElement || elemUnder instanceof Ci.nsIDOMHTMLTextAreaElement)

        // Limit the selection to the initial content window (don't leave or enter iframes)
        if (elemUnder && elemUnder.ownerDocument.defaultView != this.contentWindow)
          return;
        
        // Use fake mouse events to update the selection
        if (json.type == "end") {
          // Keep the cache in "client" coordinates, but translate for the mouse event
          this.cache.end = { x: json.x, y: json.y };
          let end = { x: this.cache.end.x - scrollOffset.x, y: this.cache.end.y - scrollOffset.y };
          utils.sendMouseEventToWindow("mousedown", end.x, end.y, 0, 1, Ci.nsIDOMNSEvent.SHIFT_MASK, true);
          utils.sendMouseEventToWindow("mouseup", end.x, end.y, 0, 1, Ci.nsIDOMNSEvent.SHIFT_MASK, true);
        } else {
          // Keep the cache in "client" coordinates, but translate for the mouse event
          this.cache.start = { x: json.x, y: json.y };
          let start = { x: this.cache.start.x - scrollOffset.x, y: this.cache.start.y - scrollOffset.y };
          let end = { x: this.cache.end.x - scrollOffset.x, y: this.cache.end.y - scrollOffset.y };
        
          utils.sendMouseEventToWindow("mousedown", start.x, start.y, 0, 0, 0, true);
          utils.sendMouseEventToWindow("mouseup", start.x, start.y, 0, 0, 0, true);
        
          utils.sendMouseEventToWindow("mousedown", end.x, end.y, 0, 1, Ci.nsIDOMNSEvent.SHIFT_MASK, true);
          utils.sendMouseEventToWindow("mouseup", end.x, end.y, 0, 1, Ci.nsIDOMNSEvent.SHIFT_MASK, true);
        }

        // Cache the selected text since the selection might be gone by the time we get the "end" message
        let selection = this.contentWindow.getSelection()
        this.selectedText = selection.toString().trim();

        // Update the rect we use to test when finishing the clipboard operation
        let range = selection.getRangeAt(0).QueryInterface(Ci.nsIDOMNSRange);
        this.cache.rect = this._extractFromRange(range, this.cache.offset).rect;
        break;
      }
    }
  },

  _extractFromRange: function sh_extractFromRange(aRange, aOffset) {
    let cache = { start: {}, end: {}, rect: { left: Number.MAX_VALUE, top: Number.MAX_VALUE, right: 0, bottom: 0 } };
    let rects = aRange.getClientRects();
    for (let i=0; i<rects.length; i++) {
      if (i == 0) {
        cache.start.x = rects[i].left + aOffset.x;
        cache.start.y = rects[i].bottom + aOffset.y;
      }
      cache.end.x = rects[i].right + aOffset.x;
      cache.end.y = rects[i].bottom + aOffset.y;
    }

    // Keep the handles from being positioned completely out of the selection range
    const HANDLE_VERTICAL_MARGIN = 4;
    cache.start.y -= HANDLE_VERTICAL_MARGIN;
    cache.end.y -= HANDLE_VERTICAL_MARGIN;

    cache.rect = aRange.getBoundingClientRect();
    cache.rect.left += aOffset.x;
    cache.rect.top += aOffset.y;
    cache.rect.right += aOffset.x;
    cache.rect.bottom += aOffset.y;
    cache.offset = aOffset;

    return cache;
  }
};

SelectionHandler.init();
