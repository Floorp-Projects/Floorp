let Cc = Components.classes;
let Ci = Components.interfaces;

dump("!! remote browser loaded\n")

let WebProgressListener = {
  _notifyFlags: [],
  _calculatedNotifyFlags: 0,

  init: function() {
    let webProgress = docShell.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIWebProgress);
    webProgress.addProgressListener(this, Ci.nsIWebProgress.NOTIFY_ALL);

    addMessageListener("WebProgress:AddProgressListener", this);
    addMessageListener("WebProgress:RemoveProgressListener", this);
  },

  onStateChange: function onStateChange(aWebProgress, aRequest, aStateFlags, aStatus) {
    let webProgress = Ci.nsIWebProgressListener;
    let notifyFlags = 0;
    if (aStateFlags & webProgress.STATE_IS_REQUEST)
      notifyFlags |= Ci.nsIWebProgress.NOTIFY_STATE_REQUEST;
    if (aStateFlags & webProgress.STATE_IS_DOCUMENT)
      notifyFlags |= Ci.nsIWebProgress.NOTIFY_STATE_DOCUMENT;
    if (aStateFlags & webProgress.STATE_IS_NETWORK)
      notifyFlags |= Ci.nsIWebProgress.NOTIFY_STATE_NETWORK;
    if (aStateFlags & webProgress.STATE_IS_WINDOW)
      notifyFlags |= Ci.nsIWebProgress.NOTIFY_STATE_WINDOW;

    let json = {
      windowId: aWebProgress.DOMWindow.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils).currentInnerWindowID,
      stateFlags: aStateFlags,
      status: aStatus,
      notifyFlags: notifyFlags
    };

    sendAsyncMessage("WebProgress:StateChange", json);
  },

  onProgressChange: function onProgressChange(aWebProgress, aRequest, aCurSelf, aMaxSelf, aCurTotal, aMaxTotal) {
    let json = {
      windowId: aWebProgress.DOMWindow.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils).currentInnerWindowID,
      curSelf: aCurSelf,
      maxSelf: aMaxSelf,
      curTotal: aCurTotal,
      maxTotal: aMaxTotal
    };

    if (this._calculatedNotifyFlags & Ci.nsIWebProgress.NOTIFY_PROGRESS)
      sendAsyncMessage("WebProgress:ProgressChange", json);
  },

  onLocationChange: function onLocationChange(aWebProgress, aRequest, aLocationURI) {
    let location = aLocationURI ? aLocationURI.spec : "";
    let json = {
      windowId: aWebProgress.DOMWindow.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils).currentInnerWindowID,
      documentURI: aWebProgress.DOMWindow.document.documentURIObject.spec,
      location: location,
      canGoBack: docShell.canGoBack,
      canGoForward: docShell.canGoForward
    };
    sendAsyncMessage("WebProgress:LocationChange", json);
  },

  onStatusChange: function onStatusChange(aWebProgress, aRequest, aStatus, aMessage) {
    let json = {
      windowId: aWebProgress.DOMWindow.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils).currentInnerWindowID,
      status: aStatus,
      message: aMessage
    };

    if (this._calculatedNotifyFlags & Ci.nsIWebProgress.NOTIFY_STATUS)
      sendAsyncMessage("WebProgress:StatusChange", json);
  },

  onSecurityChange: function onSecurityChange(aWebProgress, aRequest, aState) {
    let data = SecurityUI.getIdentityData();
    let status = {
      serverCert: data
    };

    let json = {
      windowId: aWebProgress.DOMWindow.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils).currentInnerWindowID,
      SSLStatus: status,
      state: aState
    };
    sendAsyncMessage("WebProgress:SecurityChange", json);
  },

  receiveMessage: function(aMessage) {
    switch (aMessage.name) {
      case "WebProgress:AddProgressListener":
        this._notifyFlags.push(aMessage.json.notifyFlags);
        this._calculatedNotifyFlags |= aMessage.json.notifyFlags;
        break;
      case "WebProgress.RemoveProgressListener":
        let index = this._notifyFlags.indexOf(aMessage.json.notifyFlags);
        if (index != -1) {
          this._notifyFlags.splice(index, 1);
          this._calculatedNotifyFlags = this._notifyFlags.reduce(function(a, b) {
            return a | b;
          }, 0);
        }
        break;
    }
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
  /**
   * Helper to parse out the important parts of the SSL cert for use in constructing
   * identity UI strings
   */
  getIdentityData: function() {
    let result = {};
    let status = docShell.securityUI.QueryInterface(Ci.nsISSLStatusProvider).SSLStatus;

    if (status) {
      status = status.QueryInterface(Ci.nsISSLStatus);
      let cert = status.serverCert;

      // Human readable name of Subject
      result.subjectOrg = cert.organization;

      // SubjectName fields, broken up for individual access
      if (cert.subjectName) {
        result.subjectNameFields = {};
        cert.subjectName.split(",").forEach(function(v) {
          var field = v.split("=");
          if (field[1])
            this[field[0]] = field[1];
        }, result.subjectNameFields);

        // Call out city, state, and country specifically
        result.city = result.subjectNameFields.L;
        result.state = result.subjectNameFields.ST;
        result.country = result.subjectNameFields.C;
      }

      // Human readable name of Certificate Authority
      result.caOrg =  cert.issuerOrganization || cert.issuerCommonName;

      if (!this._overrideService)
        this._overrideService = Cc["@mozilla.org/security/certoverride;1"].getService(Ci.nsICertOverrideService);

      // Check whether this site is a security exception. XPConnect does the right
      // thing here in terms of converting _lastLocation.port from string to int, but
      // the overrideService doesn't like undefined ports, so make sure we have
      // something in the default case (bug 432241).
      result.isException = !!this._overrideService.hasMatchingOverride(content.location.hostname, (content.location.port || 443), cert, {}, {});
    }

    return result;
  },

  init: function() {
    addMessageListener("SecurityUI:Init", this);
  },

  receiveMessage: function(aMessage) {
    const SECUREBROWSERUI_CONTRACTID = "@mozilla.org/secure_browser_ui;1";
    let securityUI = Cc[SECUREBROWSERUI_CONTRACTID].createInstance(Ci.nsISecureBrowserUI);
    securityUI.init(content);
  }
};

SecurityUI.init();


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
        let util = aEvent.target.defaultView.QueryInterface(Ci.nsIInterfaceRequestor)
                                            .getInterface(Ci.nsIDOMWindowUtils);

        let json = {
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
        for (rv in retvals) {
          if (rv.preventDefault) {
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
  init: function() {
    addMessageListener("Content:ScrollTo", this);
    addMessageListener("Content:ScrollBy", this);
    addMessageListener("Content:SetResolution", this);
    addMessageListener("Content:SetCacheViewport", this);
    addMessageListener("Content:SetCssViewportSize", this);

    addEventListener("scroll", this, false);
    addEventListener("MozScrolledAreaChanged", this, false);
  },

  receiveMessage: function(aMessage) {
    let json = aMessage.json;
    switch (aMessage.name) {
      case "Content:ScrollTo":
        content.scrollTo(json.x, json.y);
        break;

      case "Content:ScrollBy":
        content.scrollBy(json.dx, json.dy);
        break;

      case "Content:SetResolution": {
        let cwu = Util.getWindowUtils(content);
        cwu.setResolution(json.zoomLevel, json.zoomLevel);
        sendAsyncMessage("Content:SetResolution:Return", { zoomLevel: json.zoomLevel });
        break;
      }

      case "Content:SetCacheViewport": {
        let displayport = new Rect(json.x, json.y, json.w, json.h);
        if (displayport.isEmpty())
          break;

        let cwu = Util.getWindowUtils(content);
        cwu.setDisplayPort(displayport.x, displayport.y, displayport.width, displayport.height);
        break;
      }

      case "Content:SetCssViewportSize": {
        let cwu = Util.getWindowUtils(content);
        cwu.setCSSViewport(json.width, json.height);
        break;
      }
    }
  },

  handleEvent: function(aEvent) {
    switch (aEvent.type) {
      case "scroll":
        Util.dumpLn("XXX stub");
        break;

      case "MozScrolledAreaChanged": {
        let doc = aEvent.originalTarget;
        let win = doc.defaultView;
        // XXX need to make some things in Util as its own module!
        let scrollOffset = Util.getScrollOffset(win);
        if (win.parent != win) // We are only interested in root scroll pane changes
          return;

        // Adjust width and height from the incoming event properties so that we
        // ignore changes to width and height contributed by growth in page
        // quadrants other than x > 0 && y > 0.
        let x = aEvent.x + scrollOffset.x;
        let y = aEvent.y + scrollOffset.y;
        let width = aEvent.width + (x < 0 ? x : 0);
        let height = aEvent.height + (y < 0 ? y : 0);

        sendAsyncMessage("MozScrolledAreaChanged", {
          width: width,
          height: height,
          viewportWidth: content.innerWidth,
          viewportHeight: content.innerHeight
        });

        break;
      }
    }
  }
};

ContentScroll.init();


function PromptRemoter() {
  addEventListener("DOMWindowCreated", this, false);
}

PromptRemoter.prototype = {
  handleEvent: function handleEvent(aEvent) {
    var window = aEvent.originalTarget.defaultView.content;
    // Need to make sure we are called on what we care about -
    // content windows. DOMWindowCreated is called on *all* HTMLDocuments,
    // some of which belong to ChromeWindows or lack defaultView.content
    // altogether.
    //
    // Note about the syntax used here: |"wrappedJSObject" in window|
    // will silently fail, without even letting us catch it as an
    // exception, and checking in the way that we do check in some
    // cases still throws an exception; see bug 582108 about both.
    try {
      if (!window || !window.wrappedJSObject) {
        return;
      }
    }
    catch(e) {
      return;
    }

    function bringTabToFront() {
      let event = window.document.createEvent("Events");
      event.initEvent("DOMWillOpenModalDialog", true, false);
      window.dispatchEvent(event);
    }

    function informClosedFrontTab() {
      let event = window.document.createEvent("Events");
      event.initEvent("DOMModalDialogClosed", true, false);
      window.dispatchEvent(event);
    }

    window.wrappedJSObject.alert = function(aMessage) {
      bringTabToFront();
      sendSyncMessage("Prompt:Alert", {
        message: aMessage
      });
      informClosedFrontTab();
    }

    window.wrappedJSObject.confirm = function(aMessage) {
      bringTabToFront();
      return sendSyncMessage("Prompt:Confirm", {
        message: aMessage
      });
      informClosedFrontTab();
    }

    window.wrappedJSObject.prompt = function(aText, aValue) {
      bringTabToFront();
      return sendSyncMessage("Prompt:Prompt", {
        text: aText,
        value: aValue
      });
      informClosedFrontTab();
    }
  },
};

new PromptRemoter();

