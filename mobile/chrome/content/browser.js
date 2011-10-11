let Cc = Components.classes;
let Ci = Components.interfaces;
let Cu = Components.utils;
let Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/Services.jsm")

function dump(a) {
    Cc["@mozilla.org/consoleservice;1"]
        .getService(Ci.nsIConsoleService)
        .logStringMessage(a);
}

var fennecProgressListener = null;
var fennecEventHandler = null;
var responses = {};

function startup() {

    var httpRequestObserver = {  

      observe: function(subject, topic, data) {  

          if (topic == "http-on-examine-response") {  
              var httpChannel = subject.QueryInterface(Components.interfaces.nsIHttpChannel);  
              responses[subject.URI.spec] = httpChannel.responseStatus;
          } 
      },  
      
      get observerService() {  
          return Components.classes["@mozilla.org/observer-service;1"]  
                           .getService(Components.interfaces.nsIObserverService);  
      },  
      
      register: function() {  
          this.observerService.addObserver(this, "http-on-examine-response", false);  
      },  
      
      unregister: function()  {  
          this.observerService.removeObserver(this, "http-on-examine-response");  
      }  

    };
    httpRequestObserver.register();

    window.QueryInterface(Ci.nsIDOMChromeWindow).browserDOMWindow = new nsBrowserAccess(this);
    dump("zerdatime " + Date.now() + " - browser chrome startup finished.");

    fennecProgressListener = new FennecProgressListener(this);

    let frame = document.getElementById('home');
    frame.addProgressListener(fennecProgressListener, Ci.nsIWebProgress.NOTIFY_ALL);

    fennecEventHandler = new FennecEventHandler(frame);
    window.addEventListener("click", fennecEventHandler, true);
    window.addEventListener("mousedown", fennecEventHandler, true);
    window.addEventListener("mouseup", fennecEventHandler, true);
    window.addEventListener("mousemove", fennecEventHandler, true);

    frame.addEventListener("MozMagnifyGestureStart", fennecEventHandler, true);
    frame.addEventListener("MozMagnifyGestureUpdate", fennecEventHandler, true);
 
    let uri = "about:support";
    try {
        uri = Services.prefs.getCharPref("browser.last.uri");
    } catch (e){};
    // XXX maybe we don't do this if the launch was kicked off from external
    frame.loadURI(uri, null, null);
}


function nsBrowserAccess(chromeWindow) {
    this.chrome = chromeWindow;
}

nsBrowserAccess.prototype = {

  openURI: function browser_openURI(aURI, aOpener, aWhere, aContext) {
      let frame = this.chrome.document.getElementById("home");
      frame.loadURI(aURI.spec, null, null);
      return null;
  },

  openURIInFrame: function browser_openURIInFrame(aURI, aOpener, aWhere, aContext) {
      dump("nsBrowserAccess::openURIInFrame");
      return null;
  },

  isTabContentWindow: function(aWindow) {
    return !(this.chrome == aWindow);
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIBrowserDOMWindow]),
};


function SendMessageToJava(obj) {
    Cc["@mozilla.org/android/bridge;1"]
        .getService(Ci.nsIAndroidBridge)
        .handleGeckoMessage(JSON.stringify(obj));
}

function FennecProgressListener(chromeWindow){
    this.chrome = chromeWindow;
}

FennecProgressListener.prototype = {

    onStateChange: function(aWebProgress, aRequest, aStateFlags, aStatus) {

      let windowID = 0;//aWebProgress.DOMWindow.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils).currentInnerWindowID;

      let state = "start";
      if (aStateFlags & Ci.nsIWebProgressListener.STATE_STOP)
          state = "stop";
      
      let stateIs = "network"
      if (aStateFlags & Ci.nsIWebProgressListener.STATE_IS_DOCUMENT)
          stateIs = "document";

      let message = {
          "gecko": {
              "type"            : "onStateChange",
              "windowID"        : windowID,
              "state"           : state,
              "stateIs"         : stateIs
          }};
            
      SendMessageToJava(message);

      if (state == "start" && stateIs == "document") {
          let browser = this.chrome.document.getElementById("home");
          let uri = browser.currentURI.spec;
          
          //TODO: add the event listener when creating a new tab
          browser.contentDocument.addEventListener("DOMTitleChanged", function () {

              let stat = 0;
              if (responses[uri]) {
                  stat = responses[uri];
                  responses = {};
              }

              SendMessageToJava({
                  gecko: {
                      type: "DOMContentLoaded",
                      windowID: windowID,
                      uri: uri,
                      title: browser.contentTitle,
                      stat: stat
                  }
              });
          }, false);
      }

    },

    onLocationChange: function (aWebProgress, aRequest, aLocationURI) {
        try {

            let browser = this.chrome.document.getElementById("home");
            let uri = browser.currentURI.spec;
            let windowID = 0;

            dump("Setting Last uri to: " + uri);
            Services.prefs.setCharPref("browser.last.uri", uri);

            let message = {
                "gecko": {
                    "type"       :   "onLocationChange", 
                    "windowID"   :   windowID,
                    "uri"        :   uri
                }
            };
            
            SendMessageToJava(message);
            
            if (uri == Services.prefs.getCharPref("browser.last.uri")) {
                let showMessage = {
                    "gecko": {
                        "type"       : "hideLoadingScreen",
                        "windowID"   : windowID
                    }};
                
                SendMessageToJava(showMessage);
            }

        } catch (e) {dump("onLocationChange throws " + e); }
    },

    onSecurityChange: function (aBrowser, aWebProgress, aRequest, aState) { dump("progressListener.onSecurityChange"); },

    onProgressChange: function (aWebProgress, aRequest, aCurSelfProgress, aMaxSelfProgress, aCurTotalProgress, aMaxTotalProgress) { 
        let windowID = 0;//aWebProgress.DOMWindow.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils).currentInnerWindowID;
        let message = { 
            "gecko": {
                "type"       :   "onProgressChange", 
                "windowID"   :   windowID,
                "current"    :   aCurTotalProgress,
                "total"      :   aMaxTotalProgress
            }};

        SendMessageToJava(message);
    },
    onStatusChange: function (aBrowser, aWebProgress, aRequest, aStatus, aMessage) { dump("progressListener.onStatusChange");},

    QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener,
                                           Ci.nsISupportsWeakReference]),
};

function FennecEventHandler(browser) {
    this.browser = browser;
    this._updateLastPosition(0, 0);
}

FennecEventHandler.prototype = {

    handleEvent: function(aEvent) {
        switch (aEvent.type) {
            case "click":
                if (this.blockClick) {
                    aEvent.stopPropagation();
                    aEvent.preventDefault();
                }

                break;
            case "mousedown":
                this.startX = aEvent.clientX;
                this.startY = aEvent.clientY;
                this.blockClick = false;

                this._updateLastPosition(aEvent.clientX, aEvent.clientY);

                aEvent.stopPropagation();
                aEvent.preventDefault();
                break;
            case "mousemove":
                let dx = aEvent.clientX - this.lastX;
                let dy = aEvent.clientY - this.lastY;

                this.browser.contentWindow.wrappedJSObject.scrollBy(-dx, -dy);
                this._updateLastPosition(aEvent.clientX, aEvent.clientY);

                aEvent.stopPropagation();
                aEvent.preventDefault();
                break;
            case "mouseup":
                if (Math.abs(aEvent.clientX - this.startX) > 10 ||
                    Math.abs(aEvent.clientY - this.startY) > 10) {
                    this.blockClick = true;
                }
                aEvent.stopPropagation();
                aEvent.preventDefault();
                break;
            case "MozMagnifyGestureStart":
                this._pinchDelta = 0;
                break;
            case "MozMagnifyGestureUpdate":
                if (!aEvent.delta)
                    break;

                this._pinchDelta += (aEvent.delta / 100);

                if (Math.abs(this._pinchDelta) >= 1) {
                    let delta = Math.round(this._pinchDelta);
                    this.browser.markupDocumentViewer.fullZoom += delta;
                    this._pinchDelta = 0;
                }
                break;
        }
    },

    _updateLastPosition: function(x, y) {
        this.lastX = x;
        this.lastY = y;
    }

};
