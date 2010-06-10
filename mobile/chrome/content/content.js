// This stays here because otherwise it's hard to tell if there's a parsing error
dump("###################################### content loaded\n");

// how many milliseconds before the mousedown and the overlay of an element
const kTapOverlayTimeout = 200;

let Cc = Components.classes;
let Ci = Components.interfaces;
let gIOService = Cc["@mozilla.org/network/io-service;1"]
  .getService(Ci.nsIIOService);
let gFocusManager = Cc["@mozilla.org/focus-manager;1"]
  .getService(Ci.nsIFocusManager);
let gPrefService = Cc["@mozilla.org/preferences-service;1"]
  .getService(Ci.nsIPrefBranch2);

let XULDocument = Ci.nsIDOMXULDocument;
let HTMLHtmlElement = Ci.nsIDOMHTMLHtmlElement;
let HTMLIFrameElement = Ci.nsIDOMHTMLIFrameElement;
let HTMLFrameElement = Ci.nsIDOMHTMLFrameElement;
let HTMLTextAreaElement = Ci.nsIDOMHTMLTextAreaElement;
let HTMLInputElement = Ci.nsIDOMHTMLInputElement;
let HTMLSelectElement = Ci.nsIDOMHTMLSelectElement;
let HTMLLabelElement = Ci.nsIDOMHTMLLabelElement;
let HTMLButtonElement = Ci.nsIDOMHTMLButtonElement;
let HTMLOptGroupElement = Ci.nsIDOMHTMLOptGroupElement;
let HTMLOptionElement = Ci.nsIDOMHTMLOptionElement;

/** Send message to UI thread with browser guid as the first parameter. */
function sendMessage(name) {
  sendAsyncMessage(name, Array.prototype.slice.call(arguments, 1));
}

/** Watches for mouse click in content and redirect them to the best found target **/
const ElementTouchHelper = {
  get radius() {
    delete this.radius;
    return this.radius = { "top": gPrefService.getIntPref("browser.ui.touch.top"),
                           "right": gPrefService.getIntPref("browser.ui.touch.right"),
                           "bottom": gPrefService.getIntPref("browser.ui.touch.bottom"),
                           "left": gPrefService.getIntPref("browser.ui.touch.left")
                         };
  },

  get weight() {
    delete this.weight;
    return this.weight = { "visited": gPrefService.getIntPref("browser.ui.touch.weight.visited")
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
  x = x - scroll.x
  y = y - scroll.y;
  let elem = ElementTouchHelper.getClosest(cwu, x, y);

  // step through layers of IFRAMEs and FRAMES to find innermost element
  while (elem && (elem instanceof HTMLIFrameElement || elem instanceof HTMLFrameElement)) {
    // adjust client coordinates' origin to be top left of iframe viewport
    let rect = elem.getBoundingClientRect();
    x = x - rect.left;
    y = y - rect.top;
    let windowUtils = elem.contentDocument.defaultView.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
    elem = ElementTouchHelper.getClosest(windowUtils, x, y);
  }

  return elem;
}


function getBoundingContentRect(contentElem) {
  if (!contentElem)
    return new Rect(0, 0, 0, 0);

  let document = contentElem.ownerDocument;
  while(document.defaultView.frameElement)
    document = document.defaultView.frameElement.ownerDocument;

  let offset = Util.getScrollOffset(content);
  let r = contentElem.getBoundingClientRect();

  // step out of iframes and frames, offsetting scroll values
  for (let frame = contentElem.ownerDocument.defaultView; frame != content; frame = frame.parent) {
    // adjust client coordinates' origin to be top left of iframe viewport
    let rect = frame.frameElement.getBoundingClientRect();
    let left = frame.getComputedStyle(frame.frameElement, "").borderLeftWidth;
    let top = frame.getComputedStyle(frame.frameElement, "").borderTopWidth;
    offset.add(rect.left + parseInt(left), rect.top + parseInt(top));
  }

  return new Rect(r.left + offset.x, r.top + offset.y, r.width, r.height);
}


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

        sendAsyncMessage("MozApplicationManifest", {
          location: doc.documentURIObject.spec,
          manifest: doc.documentElement.getAttribute("manifest"),
          charset: doc.characterSet
        });
        break;
      }
    }
  },

  /** Next scroll size change event will invalidate all previous content. See constructor. */
  emptyPage: function emptyPage() {
    this._incremental = false;
  },

  startCoalescing: function startCoalescing() {
    this._timer.interval(1000);
  },
  
  stopCoalescing: function stopCoalescing() {
    this._timer.flush();
  },

  sizeChange: function sizeChange(scrollOffset, x, y, width, height) {
    // Adjust width and height from the incoming event properties so that we
    // ignore changes to width and height contributed by growth in page
    // quadrants other than x > 0 && y > 0.
    var x = x + scrollOffset.x;
    var y = y + scrollOffset.y;
    this._pendingSizeChange = {
      width: width + (x < 0 ? x : 0),
      height: height + (y < 0 ? y : 0)
    };

    // Clear any pending dirty rectangles since entire viewport will be invalidated
    // anyways.
    var rect = this._pendingDirtyRect;
    rect.top = rect.bottom;
    rect.left = rect.right;

    if (!this._timer.isPending())
      this.flush()
  },

  dirty: function dirty(scrollOffset, clientRects) {
    if (!this._pendingSizeChange) {
      var unionRect = this._pendingDirtyRect;
      for (var i = clientRects.length - 1; i >= 0; i--) {
        var e = clientRects.item(i);
        unionRect.expandToContain(new Rect(
          e.left + scrollOffset.x, e.top + scrollOffset.y, e.width, e.height));
      }

      if (!this._timer.isPending())
        this.flush()
    }
  },

  flush: function flush() {
    var dirtyRect = this._pendingDirtyRect;
    var sizeChange = this._pendingSizeChange;
    if (sizeChange) {
      sendMessage("FennecMozScrolledAreaChanged", sizeChange.width, sizeChange.height);
      if (!this._incremental)
        sendMessage("FennecMozAfterPaint", [new Rect(0, 0, sizeChange.width, sizeChange.height)]);
      this._pendingSizeChange = null;
      // After first size change has been issued, assume subsequent size changes are only incremental
      // changes to the current page.
      this._incremental = true;
    }
    else if (!dirtyRect.isEmpty()) {
      // No size change has occurred, but areas have been dirtied.
      sendMessage("FennecMozAfterPaint", [dirtyRect]);
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
    var win = aWebProgress.DOMWindow;
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


/**
 * Responsible for opening up form assistant and sending messages to the FormHelper when user
 * types keys to navigate.
 */
function ContentFormManager() {
  this._navigator = null;

  addMessageListener("FennecClosedFormAssist", this);
  addMessageListener("FennecFormPrevious", this);
  addMessageListener("FennecFormNext", this);
  addMessageListener("FennecFormChoiceSelect", this);
  addMessageListener("FennecFormChoiceChange", this);
}

ContentFormManager.prototype = {
  formAssist: function(element) {
    if (!element)
      return false;

    let wrapper = new BasicWrapper(element);
    if (!wrapper.canAssist())
      return false;

    let navigationEnabled = gPrefService.getBoolPref("formhelper.enabled");
    if (!navigationEnabled && !wrapper.hasChoices())
      return false;

    let navigator = new FormNavigator(this, element, navigationEnabled);
    if (!navigator.getCurrent())
      return false;

    sendMessage("FennecFormAssist", navigator.getJSON());

    if (!this._navigator)
      addEventListener("keyup", this, false);
    this._navigator = navigator;

    return true;
  },

  closeFormAssistant: function() {
    if (this._navigator) {
      sendMessage("FennecCloseFormAssist");
      this.closedFormAssistant();
    }
  },

  closedFormAssistant: function() {
    if (this._navigator) {
      removeEventListener("keyup", this, false);
      this._navigator = null;
    }
  },

  receiveFennecClosedFormAssist: function() {
    this.closedFormAssistant();
  },

  receiveFennecFormPrevious: function() {
    if (this._navigator) {
      this._navigator.goToPrevious();
      sendMessage("FennecFormAssist", this._navigator.getJSON());
    }
  },

  receiveFennecFormNext: function() {
    if (this._navigator) {
      this._navigator.goToNext();
      sendMessage("FennecFormAssist", this._navigator.getJSON());
    }
  },

  receiveFennecFormChoiceSelect: function(message, index, selected, clearAll) {
    if (this._navigator) {
      let current = this._navigator.getCurrent();
      if (current)
        current.choiceSelect(index, selected, clearAll);
    }
  },

  receiveFennecFormChoiceChange: function() {
    if (this._navigator) {
      let current = this._navigator.getCurrent();
      if (current)
        current.choiceChange();
    }
  },

  handleEvent: function formHelperHandleEvent(aEvent) {
    let currentWrapper = this._navigator.getCurrent();
    let currentElement = currentWrapper.element;

    switch (aEvent.keyCode) {
      case aEvent.DOM_VK_DOWN:
        if (currentElement instanceof HTMLTextAreaElement) {
          let existSelection = currentElement.selectionEnd - currentElement.selectionStart;
          let isEnd = (currentElement.textLength == currentElement.selectionEnd);
          if (!isEnd || existSelection)
            return;
        }

        this._navigator.goToNext();
        break;

      case aEvent.DOM_VK_UP:
        if (currentElement instanceof HTMLTextAreaElement) {
          let existSelection = currentElement.selectionEnd - currentElement.selectionStart;
          let isStart = (currentElement.selectionEnd == 0);
          if (!isStart || existSelection)
            return;
        }

        this._navigator.goToPrevious();
        break;

      case aEvent.DOM_VK_RETURN:
        break;

      default:
        let target = aEvent.target;
        if (currentWrapper.canAutocomplete())
          sendMessage("FennecFormAutocomplete", currentWrapper.getAutocompleteSuggestions());
        break;
    }

    let caretRect = currentWrapper.getCaretRect();
    if (!caretRect.isEmpty()) {
      sendMessage("FennecCaretRect", caretRect);
    }
  },

  _getRectForCaret: function _getRectForCaret() {
    let currentElement = this.getCurrent();
    let rect = currentElement.getCaretRect();
    return null;
  },
  
};


/**
 * Responsible for iterating through form elements.
 * The navigable list is generated on instantiation, so construct new FormNavigators when
 * the current document is changed or the list of form elements needs to be regenerated.
 */
function FormNavigator(manager, element, showNavigation) {
  this._manager = manager;
  this._showNavigation = showNavigation;
  
  if (showNavigation) {
    this._wrappers = [];
    this._currentIndex = -1;
    this._getAllWrappers(element);
  } else {
    this._wrappers = [element];
    this._currentIndex = 0;
  }
}

FormNavigator.prototype = {
  endNavigation: function() {
    this._manager.closedFormAssistant();
  },

  getCurrent: function() {
    return this._wrappers[this._currentIndex];
  },

  getJSON: function() {
    return {
      hasNext: !!this.getNext(),
      hasPrevious: !!this.getPrevious(),
      current: this.getCurrent().getJSON(),
      showNavigation: this._showNavigation
    };
  },

  getPrevious: function() {
    return this._wrappers[this._currentIndex - 1];
  },

  getNext: function() {
    return this._wrappers[this._currentIndex + 1];
  },

  goToPrevious: function() {
    return this._setIndex(this._currentIndex - 1);
  },

  goToNext: function() {
    return this._setIndex(this._currentIndex + 1);
  },

  _getAllWrappers: function(element) {
    // XXX good candidate for tracing if possible.  The tough ones are length and
    // canNavigateTo / isVisible.

    let document = element.ownerDocument;
    if (!document)
      return;

    let elements = this._wrappers;

    // get all the documents
    let documents = [document];
    let iframes = document.querySelectorAll("iframe, frame");
    for (let i = 0; i < iframes.length; i++)
      documents.push(iframes[i].contentDocument);

    for (let i = 0; i < documents.length; i++) {
      let nodes = documents[i].querySelectorAll("input, button, select, textarea, [role=button]");
      nodes = this._filterRadioButtons(nodes);

      for (let j = 0; j < nodes.length; j++) {
        let node = nodes[j];
        let wrapper = new BasicWrapper(node);
        if (wrapper.canNavigateTo() && wrapper.isVisible()) {
          elements.push(wrapper);
          if (node == element)
            this._setIndex(elements.length - 1);
        }
      }
    }

    function orderByTabIndex(a, b) {
      // for an explanation on tabbing navigation see
      // http://www.w3.org/TR/html401/interact/forms.html#h-17.11.1
      // In resume tab index navigation order is 1, 2, 3, ..., 32767, 0
      if (a.tabIndex == 0 || b.tabIndex == 0)
        return b.tabIndex;

      return a.tabIndex > b.tabIndex;
    }
    elements = elements.sort(orderByTabIndex);
  },

  /**
   * For each radio button group, remove all but the checked button
   * if there is one, or the first button otherwise.
   */
  _filterRadioButtons: function(nodes) {
    // First pass: Find the checked or first element in each group.
    let chosenRadios = {};
    for (let i=0; i < nodes.length; i++) {
      let node = nodes[i];
      if (node.type == "radio" && (!chosenRadios.hasOwnProperty(node.name) || node.checked))
        chosenRadios[node.name] = node;
    }

    // Second pass: Exclude all other radio buttons from the list.
    var result = [];
    for (let i=0; i < nodes.length; i++) {
      let node = nodes[i];
      if (node.type == "radio" && chosenRadios[node.name] != node)
        continue;
      result.push(node);
    }
    return result;
  },

  _setIndex: function(i) {
    let element = this._wrappers[i];
    if (element) {
      gFocusManager.setFocus(element.element, Ci.nsIFocusManager.FLAG_NOSCROLL);
      this._currentIndex = i;
      return true;
    } else {
      return false;
    }
  }
};


/** Can't think of a good description of this class.  It probably does too much? */
function Content() {
  this._iconURI = null;

  addMessageListener("Browser:Blur", this);
  addMessageListener("Browser:Focus", this);
  addMessageListener("Browser:Mousedown", this);
  addMessageListener("Browser:Mouseup", this);
  addMessageListener("Browser:CancelMouse", this);
  addMessageListener("Browser:SaveAs", this);

  this._coalescer = new Coalescer();
  addEventListener("MozAfterPaint", this._coalescer, false);
  addEventListener("MozScrolledAreaChanged", this._coalescer, false);
  addEventListener("MozApplicationManifest", this._coalescer, false);

  this._progressController = new ProgressController(this);
  this._progressController.start();

  this._contentFormManager = new ContentFormManager();

  this._mousedownTimeout = new Util.Timeout();
}

Content.prototype = {
  receiveMessage: function receiveMessage(aMessage) {
    let json = aMessage.json;

    switch (aMessage.name) {
      case "Browser:Blur": 
        docShell.isOffScreenBrowser = false;
        this._selected = false;
        break;

      case "Browser:Focus":
        docShell.isOffScreenBrowser = true;
        this._selected = true;
        break;

      case "Browser:Mousedown":
        this._mousedownTimeout.once(kTapOverlayTimeout, function() {
          let element = elementFromPoint(x, y);
          gFocusManager.setFocus(element, Ci.nsIFocusManager.FLAG_NOSCROLL);
        });
        break;

      case "Browser:Mouseup":
        this._mousedownTimeout.flush();

        let element = elementFromPoint(x, y);
        if (!this._contentFormManager.formAssist(element)) {
          this._sendMouseEvent("mousedown", element, x, y);
          this._sendMouseEvent("mouseup", element, x, y);
        }
        break;

      case "Browser:CancelMouse":
        this._mousedownTimeout.clear();
        // XXX there must be a better way than this to cancel the mouseover/focus?
        this._sendMouseEvent("mouseup", null, -1000, -1000);
        try {
          content.document.activeElement.blur();
        }
        catch(e) {}
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
    }
  },

  _sendMouseEvent: function _sendMouseEvent(name, element, x, y) {
    let windowUtils = Util.getWindowUtils(content);
    let scrollOffset = Util.getScrollOffset(content);

    // the element can be out of the cX/cY point because of the touch radius
    let rect = getBoundingContentRect(element);
    if (!rect.isEmpty() && !(element instanceof HTMLHtmlElement) ||
        x < rect.left || x > rect.right ||
        y < rect.top || y > rect.bottom) {
      let point = rect.center();
      x = point.x;
      y = point.y;
    }

    windowUtils.sendMouseEvent(name, x - scrollOffset.x, y - scrollOffset.y, 0, 1, 0, true);
  },

  startLoading: function startLoading() {
    this._loading = true;
    this._iconURI = null;
    this._coalescer.emptyPage();
    this._coalescer.startCoalescing();
  },

  stopLoading: function stopLoading() {
    this._loading = false;
    this._coalescer.stopCoalescing();
  },

  isSelected: function isSelected() {
    return this._selected;
  }
};

let contentObject = new Content();

let ViewportHandler = {
  metadata: null,

  init: function init() {
    addEventListener("DOMContentLoaded", this, false);
    addEventListener("DOMMetaAdded", this, false);
    addEventListener("pageshow", this, false);

    this.progresscontroller = new ProgressController(this)
    this.progresscontroller.start();
  },

  handleEvent: function handleEvent(aEvent) {
    switch (aEvent.type) {
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

  startLoading: function() {
    this.metadata = null;
    sendAsyncMessage("FennecViewportMetadata", {});
  },

  stopLoading: function() {
  },

  updateMetadata: function notify() {
    this.metadata = this.getViewportMetadata();
    sendAsyncMessage("FennecViewportMetadata", this.metadata);
  },

  getViewportMetadata: function getViewportMetadata() {
    let dpiScale = gPrefService.getIntPref("zoom.dpiScale") / 100;

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
  },
};

ViewportHandler.init();
