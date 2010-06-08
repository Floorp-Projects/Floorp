let Cc = Components.classes;
let Ci = Components.interfaces;

dump("!! remote browser loaded\n")

let WebProgressListener = {
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
      isRootWindow: aWebProgress.DOMWindow == content,
      stateFlags: aStateFlags,
      status: aStatus,
      notifyFlags: notifyFlags
    };
    sendAsyncMessage("WebProgress:StateChange", json);
  },

  onProgressChange: function onProgressChange(aWebProgress, aRequest, aCurSelf, aMaxSelf, aCurTotal, aMaxTotal) {
    let json = {
      isRootWindow: aWebProgress.DOMWindow == content,
      curSelf: aCurSelf,
      maxSelf: aMaxSelf,
      curTotal: aCurTotal,
      maxTotal: aMaxTotal
    };
    sendAsyncMessage("WebProgress:ProgressChange", json);
  },

  onLocationChange: function onLocationChange(aWebProgress, aRequest, aLocationURI) {
    let location = aLocationURI ? aLocationURI.spec : "";
    let json = {
      isRootWindow: aWebProgress.DOMWindow == content,
      documentURI: aWebProgress.DOMWindow.document.documentURIObject.spec,
      location: location,
      canGoBack: docShell.canGoBack,
      canGoForward: docShell.canGoForward
    };
    sendAsyncMessage("WebProgress:LocationChange", json);
  },

  onStatusChange: function onStatusChange(aWebProgress, aRequest, aStatus, aMessage) {
    let json = {
      isRootWindow: aWebProgress.DOMWindow == content,
      status: aStatus,
      message: aMessage
    };
    sendAsyncMessage("WebProgress:StatusChange", json);
  },

  onSecurityChange: function onSecurityChange(aWebProgress, aRequest, aState) {
    let json = {
      isRootWindow: aWebProgress.DOMWindow == content,
      identity: this._getIdentityData(),
      state: aState
    };
    sendAsyncMessage("WebProgress:SecurityChange", json);
  },

  QueryInterface: function QueryInterface(aIID) {
    if (aIID.equals(Ci.nsIWebProgressListener) ||
        aIID.equals(Ci.nsISupportsWeakReference) ||
        aIID.equals(Ci.nsISupports)) {
        return this;
    }

    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  /**
   * Helper to parse out the important parts of the SSL cert for use in constructing
   * identity UI strings
   */
  _getIdentityData: function() {
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
  }
};

let webProgress = docShell.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIWebProgress);
webProgress.addProgressListener(WebProgressListener, Ci.nsIWebProgress.NOTIFY_ALL);

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
          location: document.documentURIObject.spec,
          href: target.href,
          charset: document.characterSet,
          title: target.title,
          rel: target.rel,
          type: target.type
        };

        sendAsyncMessage("DOMLinkAdded", json);
        break;

      case "DOMWillOpenModalDialog":
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
