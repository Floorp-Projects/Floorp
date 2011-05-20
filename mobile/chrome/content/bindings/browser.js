// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
let Cc = Components.classes;
let Ci = Components.interfaces;

dump("!! remote browser loaded\n");

let WebProgressListener = {
  init: function() {
    let flags = Ci.nsIWebProgress.NOTIFY_LOCATION |
                Ci.nsIWebProgress.NOTIFY_SECURITY |
                Ci.nsIWebProgress.NOTIFY_STATE_NETWORK | Ci.nsIWebProgress.NOTIFY_STATE_DOCUMENT;

    let webProgress = docShell.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIWebProgress);
    webProgress.addProgressListener(this, flags);
  },

  onStateChange: function onStateChange(aWebProgress, aRequest, aStateFlags, aStatus) {
    if (content != aWebProgress.DOMWindow)
      return;

    let json = {
      contentWindowId: content.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils).currentInnerWindowID,
      stateFlags: aStateFlags,
      status: aStatus
    };

    sendAsyncMessage("Content:StateChange", json);
  },

  onProgressChange: function onProgressChange(aWebProgress, aRequest, aCurSelf, aMaxSelf, aCurTotal, aMaxTotal) {
  },

  onLocationChange: function onLocationChange(aWebProgress, aRequest, aLocationURI) {
    if (content != aWebProgress.DOMWindow)
      return;

    let spec = aLocationURI ? aLocationURI.spec : "";
    let location = spec.split("#")[0];

    let charset = content.document.characterSet;

    let json = {
      contentWindowId: content.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils).currentInnerWindowID,
      documentURI:     aWebProgress.DOMWindow.document.documentURIObject.spec,
      location:        spec,
      canGoBack:       docShell.canGoBack,
      canGoForward:    docShell.canGoForward,
      charset:         charset.toString()
    };

    sendAsyncMessage("Content:LocationChange", json);

    // When a new page is loaded fire a message for the first paint
    addEventListener("MozAfterPaint", function(aEvent) {
      removeEventListener("MozAfterPaint", arguments.callee, true);

      let scrollOffset = ContentScroll.getScrollOffset(content);
      sendAsyncMessage("Browser:FirstPaint", scrollOffset);
    }, true);
  },

  onStatusChange: function onStatusChange(aWebProgress, aRequest, aStatus, aMessage) {
  },

  onSecurityChange: function onSecurityChange(aWebProgress, aRequest, aState) {
    if (content != aWebProgress.DOMWindow)
      return;

    let serialization = SecurityUI.getSSLStatusAsString();

    let json = {
      contentWindowId: content.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils).currentInnerWindowID,
      SSLStatusAsString: serialization,
      state: aState
    };

    sendAsyncMessage("Content:SecurityChange", json);
  },

  QueryInterface: function QueryInterface(aIID) {
    if (aIID.equals(Ci.nsIWebProgressListener) ||
        aIID.equals(Ci.nsISupportsWeakReference) ||
        aIID.equals(Ci.nsISupports)) {
        return this;
    }

    throw Components.results.NS_ERROR_NO_INTERFACE;
  }
};

WebProgressListener.init();


let SecurityUI = {
  getSSLStatusAsString: function() {
    let status = docShell.securityUI.QueryInterface(Ci.nsISSLStatusProvider).SSLStatus;

    if (status) {
      let serhelper = Cc["@mozilla.org/network/serialization-helper;1"]
                      .getService(Ci.nsISerializationHelper);

      status.QueryInterface(Ci.nsISerializable);
      return serhelper.serializeToString(status);
    }

    return null;
  }
};

let WebNavigation =  {
  _webNavigation: docShell.QueryInterface(Ci.nsIWebNavigation),

  init: function() {
    addMessageListener("WebNavigation:GoBack", this);
    addMessageListener("WebNavigation:GoForward", this);
    addMessageListener("WebNavigation:GotoIndex", this);
    addMessageListener("WebNavigation:LoadURI", this);
    addMessageListener("WebNavigation:Reload", this);
    addMessageListener("WebNavigation:Stop", this);
  },

  receiveMessage: function(message) {
    switch (message.name) {
      case "WebNavigation:GoBack":
        this.goBack(message);
        break;
      case "WebNavigation:GoForward":
        this.goForward(message);
        break;
      case "WebNavigation:GotoIndex":
        this.gotoIndex(message);
        break;
      case "WebNavigation:LoadURI":
        this.loadURI(message);
        break;
      case "WebNavigation:Reload":
        this.reload(message);
        break;
      case "WebNavigation:Stop":
        this.stop(message);
        break;
    }
  },

  goBack: function() {
    this._webNavigation.goBack();
  },

  goForward: function() {
    this._webNavigation.goForward();
  },

  gotoIndex: function(message) {
    this._webNavigation.gotoIndex(message.index);
  },

  loadURI: function(message) {
    let flags = message.json.flags || this._webNavigation.LOAD_FLAGS_NONE;
    this._webNavigation.loadURI(message.json.uri, flags, null, null, null);
  },

  reload: function(message) {
    let flags = message.json.flags || this._webNavigation.LOAD_FLAGS_NONE;
    this._webNavigation.reload(flags);
  },

  stop: function(message) {
    let flags = message.json.flags || this._webNavigation.STOP_ALL;
    this._webNavigation.stop(flags);
  }
};

WebNavigation.init();


let DOMEvents =  {
  init: function() {
    addEventListener("DOMContentLoaded", this, false);
    addEventListener("DOMTitleChanged", this, false);
    addEventListener("DOMLinkAdded", this, false);
    addEventListener("DOMWillOpenModalDialog", this, false);
    addEventListener("DOMModalDialogClosed", this, true);
    addEventListener("DOMWindowClose", this, false);
    addEventListener("DOMPopupBlocked", this, false);
    addEventListener("pageshow", this, false);
    addEventListener("pagehide", this, false);
  },

  handleEvent: function(aEvent) {
    let document = content.document;
    switch (aEvent.type) {
      case "DOMContentLoaded":
        if (document.documentURIObject.spec == "about:blank")
          return;

        sendAsyncMessage("DOMContentLoaded", { });
        break;

      case "pageshow":
      case "pagehide": {
        if (aEvent.target.defaultView != content)
          break;

        let util = aEvent.target.defaultView.QueryInterface(Ci.nsIInterfaceRequestor)
                                            .getInterface(Ci.nsIDOMWindowUtils);

        let json = {
          contentWindowWidth: content.innerWidth,
          contentWindowHeight: content.innerHeight,
          windowId: util.outerWindowID,
          persisted: aEvent.persisted
        };

        sendAsyncMessage(aEvent.type, json);
        break;
      }

      case "DOMPopupBlocked": {
        let util = aEvent.requestingWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                                          .getInterface(Ci.nsIDOMWindowUtils);
        let json = {
          windowId: util.outerWindowID,
          popupWindowURI: {
            spec: aEvent.popupWindowURI.spec,
            charset: aEvent.popupWindowURI.originCharset
          },
          popupWindowFeatures: aEvent.popupWindowFeatures,
          popupWindowName: aEvent.popupWindowName
        };

        sendAsyncMessage("DOMPopupBlocked", json);
        break;
      }

      case "DOMTitleChanged":
        sendAsyncMessage("DOMTitleChanged", { title: document.title });
        break;

      case "DOMLinkAdded":
        let target = aEvent.originalTarget;
        if (!target.href || target.disabled)
          return;

        let json = {
          windowId: target.ownerDocument.defaultView.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils).currentInnerWindowID,
          href: target.href,
          charset: document.characterSet,
          title: target.title,
          rel: target.rel,
          type: target.type
        };

        sendAsyncMessage("DOMLinkAdded", json);
        break;

      case "DOMWillOpenModalDialog":
      case "DOMModalDialogClosed":
      case "DOMWindowClose":
        let retvals = sendSyncMessage(aEvent.type, { });
        for (let i in retvals) {
          if (retvals[i].preventDefault) {
            aEvent.preventDefault();
            break;
          }
        }
        break;
    }
  }
};

DOMEvents.init();

let ContentScroll =  {
  _scrollOffset: { x: 0, y: 0 },

  init: function() {
    addMessageListener("Content:SetCacheViewport", this);
    addMessageListener("Content:SetWindowSize", this);

    addEventListener("scroll", this, false);
    addEventListener("pagehide", this, false);
    addEventListener("MozScrolledAreaChanged", this, false);
  },

  getScrollOffset: function(aWindow) {
    let cwu = aWindow.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
    let scrollX = {}, scrollY = {};
    cwu.getScrollXY(false, scrollX, scrollY);
    return { x: scrollX.value, y: scrollY.value };
  },

  getScrollOffsetForElement: function(aElement) {
    if (aElement.parentNode == aElement.ownerDocument)
      return this.getScrollOffset(aElement.ownerDocument.defaultView);
    return { x: aElement.scrollLeft, y: aElement.scrollTop };
  },

  setScrollOffsetForElement: function(aElement, aLeft, aTop) {
    if (aElement.parentNode == aElement.ownerDocument) {
      aElement.ownerDocument.defaultView.scrollTo(aLeft, aTop);
    } else {
      aElement.scrollLeft = aLeft;
      aElement.scrollTop = aTop;
    }
  },

  receiveMessage: function(aMessage) {
    let json = aMessage.json;
    switch (aMessage.name) {
      case "Content:SetCacheViewport": {
        // Set resolution for root view
        let rootCwu = content.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
        if (json.id == 1)
          rootCwu.setResolution(json.scale, json.scale);

        let displayport = new Rect(json.x, json.y, json.w, json.h);
        if (displayport.isEmpty())
          break;

        // Map ID to element
        let element = rootCwu.findElementWithViewId(json.id);
        if (!element)
          break;

        let binding = element.ownerDocument.getBindingParent(element);
        if (binding instanceof Ci.nsIDOMHTMLInputElement && binding.mozIsTextField(false))
          break;

        // Set the scroll offset for this element if specified
        if (json.scrollX >= 0 && json.scrollY >= 0) {
          this.setScrollOffsetForElement(element, json.scrollX, json.scrollY)
          if (json.id == 1)
            this._scrollOffset = this.getScrollOffset(content);
        }

        // Set displayport. We want to set this after setting the scroll offset, because
        // it is calculated based on the scroll offset.
        let scrollOffset = this.getScrollOffsetForElement(element);
        let x = displayport.x - scrollOffset.x;
        let y = displayport.y - scrollOffset.y;

        if (json.id == 1) {
          x = Math.round(x * json.scale) / json.scale;
          y = Math.round(y * json.scale) / json.scale;
        }

        let win = element.ownerDocument.defaultView;
        let winCwu = win.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
        winCwu.setDisplayPortForElement(x, y, displayport.width, displayport.height, element);

        // XXX If we scrolled during this displayport update, then it is the
        //     end of a pan. Due to bug 637852, there may be seaming issues
        //     with the visible content, so we need to redraw.
        if (json.id == 1 && json.scrollX >= 0 && json.scrollY >= 0)
          win.setTimeout(
            function() {
              winCwu.redraw();
            }, 0);

        break;
      }

      case "Content:SetWindowSize": {
        let cwu = content.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
        cwu.setCSSViewport(json.width, json.height);
        break;
      }
    }
  },

  handleEvent: function(aEvent) {
    switch (aEvent.type) {
      case "pagehide":
        this._scrollOffset = { x: 0, y: 0 };
        break;

      case "scroll": {
        let doc = aEvent.target;
        if (doc != content.document)
          break;

        this.sendScroll();
        break;
      }

      case "MozScrolledAreaChanged": {
        let doc = aEvent.originalTarget;
        if (content != doc.defaultView) // We are only interested in root scroll pane changes
          return;

        sendAsyncMessage("MozScrolledAreaChanged", {
          width: aEvent.width,
          height: aEvent.height,
          left: aEvent.x
        });

        break;
      }
    }
  },

  sendScroll: function sendScroll() {
    let scrollOffset = this.getScrollOffset(content);
    if (this._scrollOffset.x == scrollOffset.x && this._scrollOffset.y == scrollOffset.y)
      return;

    this._scrollOffset = scrollOffset;
    sendAsyncMessage("scroll", scrollOffset);
  }
};

ContentScroll.init();

let ContentActive =  {
  init: function() {
    addMessageListener("Content:Activate", this);
    addMessageListener("Content:Deactivate", this);
  },

  receiveMessage: function(aMessage) {
    let json = aMessage.json;
    switch (aMessage.name) {
      case "Content:Deactivate":
        docShell.isActive = false;
        let cwu = content.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
        cwu.setDisplayPortForElement(0,0,0,0,content.document.documentElement);
        break;

      case "Content:Activate":
        docShell.isActive = true;
        break;
    }
  }
};

ContentActive.init();

/**
 * Helper class for IndexedDB, child part. Listens using
 * the observer service for events regarding IndexedDB
 * prompts, and sends messages to the parent to actually
 * show the prompts.
 */
let IndexedDB = {
  _permissionsPrompt: "indexedDB-permissions-prompt",
  _permissionsResponse: "indexedDB-permissions-response",

  _quotaPrompt: "indexedDB-quota-prompt",
  _quotaResponse: "indexedDB-quota-response",
  _quotaCancel: "indexedDB-quota-cancel",

  waitingObservers: [],

  init: function IndexedDBPromptHelper_init() {
    let os = Services.obs;
    os.addObserver(this, this._permissionsPrompt, false);
    os.addObserver(this, this._quotaPrompt, false);
    os.addObserver(this, this._quotaCancel, false);
    addMessageListener("IndexedDB:Response", this);
  },

  observe: function IndexedDBPromptHelper_observe(aSubject, aTopic, aData) {
    if (aTopic != this._permissionsPrompt && aTopic != this._quotaPrompt && aTopic != this._quotaCancel) {
      throw new Error("Unexpected topic!");
    }

    let requestor = aSubject.QueryInterface(Ci.nsIInterfaceRequestor);
    let observer = requestor.getInterface(Ci.nsIObserver);

    let contentWindow = requestor.getInterface(Ci.nsIDOMWindow);
    let contentDocument = contentWindow.document;

    if (aTopic == this._quotaCancel) {
      observer.observe(null, this._quotaResponse, Ci.nsIPermissionManager.UNKNOWN_ACTION);
      return;
    }

    // Remote to parent
    sendAsyncMessage("IndexedDB:Prompt", {
      topic: aTopic,
      host: contentDocument.documentURIObject.asciiHost,
      location: contentDocument.location.toString(),
      data: aData,
      observerId: this.addWaitingObserver(observer),
    });
  },

  receiveMessage: function(aMessage) {
    let payload = aMessage.json;
    switch (aMessage.name) {
      case "IndexedDB:Response":
        let observer = this.getAndRemoveWaitingObserver(payload.observerId);
        observer.observe(null, payload.responseTopic, payload.permission);
    }
  },

  addWaitingObserver: function(aObserver) {
    let observerId = 0;
    while (observerId in this.waitingObservers)
      observerId++;
    this.waitingObservers[observerId] = aObserver;
    return observerId;
  },

  getAndRemoveWaitingObserver: function(aObserverId) {
    let observer = this.waitingObservers[aObserverId];
    delete this.waitingObservers[aObserverId];
    return observer;
  },
};

IndexedDB.init();

