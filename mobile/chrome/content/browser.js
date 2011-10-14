let Cc = Components.classes;
let Ci = Components.interfaces;
let Cu = Components.utils;
let Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/Services.jsm")

function dump(a) {
  Cc["@mozilla.org/consoleservice;1"].getService(Ci.nsIConsoleService).logStringMessage(a);
}

function sendMessageToJava(aMessage) {
  let bridge = Cc["@mozilla.org/android/bridge;1"].getService(Ci.nsIAndroidBridge);
  bridge.handleGeckoMessage(JSON.stringify(aMessage));
}


var BrowserApp = {
  _tabs: [],
  _selectedTab: null,

  deck: null,

  startup: function startup() {
    window.QueryInterface(Ci.nsIDOMChromeWindow).browserDOMWindow = new nsBrowserAccess();
    dump("zerdatime " + Date.now() + " - browser chrome startup finished.");

    this.deck = document.getElementById("browsers");
    BrowserEventHandler.init();

    Services.obs.addObserver(this, "session-back", false);
    Services.obs.addObserver(this, "session-reload", false);

    let uri = "about:support";
    try {
      uri = Services.prefs.getCharPref("browser.last.uri");
    } catch (e) {};
  
    // XXX maybe we don't do this if the launch was kicked off from external
    Services.io.offline = false;
    let newTab = this.addTab(uri);
    newTab.active = true;
  },

  shutdown: function shutdown() {
  },

  get tabs() {
    return this._tabs;
  },

  get selectedTab() {
    return this._selectedTab;
  },

  set selectedTab(aTab) {
    this._selectedTab = aTab;
    if (!aTab)
      return;

    this.deck.selectedPanel = aTab.browser;
  },

  get selectedBrowser() {
    if (this._selectedTab)
      return this._selectedTab.browser;
    return null;
  },

  getTabForBrowser: function getTabForBrowser(aBrowser) {
    let tabs = this._tabs;
    for (let i = 0; i < tabs.length; i++) {
      if (tabs[i].browser == aBrowser)
        return tabs[i];
    }
    return null;
  },

  getBrowserForWindow: function getBrowserForWindow(aWindow) {
    let tabs = this._tabs;
    for (let i = 0; i < tabs.length; i++) {
      if (tabs[i].browser.contentWindow == aWindow)
        return tabs[i].browser;
    }
    return null;
  },

  getBrowserForDocument: function getBrowserForDocument(aDocument) {
    let tabs = this._tabs;
    for (let i = 0; i < tabs.length; i++) {
      if (tabs[i].browser.contentDocument == aDocument)
        return tabs[i].browser;
    }
    return null;
  },

  loadURI: function loadURI(aURI, aParams) {
    let browser = this.selectedBrowser;
    if (!browser)
      return;

    let flags = aParams.flags || Ci.nsIWebNavigation.LOAD_FLAGS_NONE;
    let postData = ("postData" in aParams && aParams.postData) ? aParams.postData.value : null;
    let referrerURI = "referrerURI" in aParams ? aParams.referrerURI : null;
    let charset = "charset" in aParams ? aParams.charset : null;
    browser.loadURIWithFlags(aURI, flags, referrerURI, charset, postData);
  },

  addTab: function addTab(aURI) {
    let newTab = new Tab(aURI);
    this._tabs.push(newTab);
    return newTab;
  },

  closeTab: function closeTab(aTab) {
    if (aTab == this.selectedTab)
      this.selectedTab = null;

    aTab.destroy();
    this._tabs.splice(this._tabs.indexOf(aTab), 1);
  },

  observe: function(aSubject, aTopic, aData) {
    let browser = this.selectedBrowser;
    if (!browser)
      return;

    if (aTopic == "session-back")
      browser.goBack();
    else if (aTopic == "session-reload")
      browser.reload();
  }
}


function nsBrowserAccess() {
}

nsBrowserAccess.prototype = {
  openURI: function browser_openURI(aURI, aOpener, aWhere, aContext) {
    dump("nsBrowserAccess::openURI");
    let browser = BrowserApp.selectedBrowser;
    if (!browser)
      browser = BrowserApp.addTab("about:blank").browser;

    // Why does returning the browser.contentWindow not work here?
    Services.io.offline = false;
    browser.loadURI(aURI.spec, null, null);
    return null;
  },

  openURIInFrame: function browser_openURIInFrame(aURI, aOpener, aWhere, aContext) {
    dump("nsBrowserAccess::openURIInFrame");
    return null;
  },

  isTabContentWindow: function(aWindow) {
    return BrowserApp.getBrowserForWindow(aWindow) != null;
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIBrowserDOMWindow])
};

function Tab(aURL) {
  this.browser = null;
  this.create(aURL);
}

Tab.prototype = {
  create: function(aURL) {
    if (this.browser)
      return;

    this.browser = document.createElement("browser");
    this.browser.setAttribute("type", "content");
    BrowserApp.deck.appendChild(this.browser);
    this.browser.stop();

    let flags = Ci.nsIWebProgress.NOTIFY_STATE_ALL |
                Ci.nsIWebProgress.NOTIFY_LOCATION |
                Ci.nsIWebProgress.NOTIFY_PROGRESS;
    this.browser.addProgressListener(this, flags);
    this.browser.loadURI(aURL);
  },

  destroy: function() {
    if (!this.browser)
      return;

    this.browser.removeProgressListener(this);
    BrowserApp.deck.removeChild(this.browser);
    this.browser = null;
  },

  set active(aActive) {
    if (!this.browser)
      return;

    if (aActive) {
      this.browser.setAttribute("type", "content-primary");
      BrowserApp.selectedTab = this;
    } else {
      this.browser.setAttribute("type", "content");
    }
  },

  get active() {
    if (!this.browser)
      return false;
    return this.browser.getAttribute("type") == "content-primary";
  },

  onStateChange: function(aWebProgress, aRequest, aStateFlags, aStatus) {
    let windowID = aWebProgress.DOMWindow.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils).currentInnerWindowID;
    let message = {
      gecko: {
        type: "onStateChange",
        windowID: windowID,
        state: aStateFlags
      }
    };

    sendMessageToJava(message);
  },

  onLocationChange: function(aWebProgress, aRequest, aLocationURI) {
    try {
      let browser = BrowserApp.getBrowserForWindow(aWebProgress.DOMWindow);
      let uri = browser.currentURI.spec;
      let windowID = aWebProgress.DOMWindow.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils).currentInnerWindowID;

      dump("Setting Last uri to: " + uri);
      Services.prefs.setCharPref("browser.last.uri", uri);

      let message = {
        gecko: {
          type: "onLocationChange",
          windowID: windowID,
          uri: uri
        }
      };

      sendMessageToJava(message);

      if (uri == Services.prefs.getCharPref("browser.last.uri")) {
        let showMessage = {
          gecko: {
            type: "hideLoadingScreen",
            windowID: windowID
          }
        };

        sendMessageToJava(showMessage);
      }
    } catch (e) {
      dump("onLocationChange throws " + e);
    }
  },

  onSecurityChange: function(aBrowser, aWebProgress, aRequest, aState) {
    dump("progressListener.onSecurityChange");
  },

  onProgressChange: function(aWebProgress, aRequest, aCurSelfProgress, aMaxSelfProgress, aCurTotalProgress, aMaxTotalProgress) {
    let windowID = aWebProgress.DOMWindow.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils).currentInnerWindowID;
    let message = {
      gecko: {
        type: "onProgressChange",
        windowID: windowID,
        current: aCurTotalProgress,
        total: aMaxTotalProgress
      }
    };

    sendMessageToJava(message);
  },
  onStatusChange: function(aBrowser, aWebProgress, aRequest, aStatus, aMessage) {
    dump("progressListener.onStatusChange");
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener, Ci.nsISupportsWeakReference])
};


var BrowserEventHandler = {
  init: function init() {
    this.motionBuffer = [];
    this._updateLastPosition(0, 0);

    window.addEventListener("click", this, true);
    window.addEventListener("mousedown", this, true);
    window.addEventListener("mouseup", this, true);
    window.addEventListener("mousemove", this, true);

    BrowserApp.deck.addEventListener("MozMagnifyGestureStart", this, true);
    BrowserApp.deck.addEventListener("MozMagnifyGestureUpdate", this, true);
    BrowserApp.deck.addEventListener("DOMContentLoaded", this, true);
  },

  handleEvent: function(aEvent) {
    switch (aEvent.type) {
      case "DOMContentLoaded": {
        let browser = BrowserApp.getBrowserForDocument(aEvent.target);
        sendMessageToJava({
          gecko: {
            type: "DOMContentLoaded",
            windowID: 0,
            uri: browser.currentURI.spec,
            title: browser.contentTitle
          }
        });
        break;
      }
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
  
        // Set panning to true *after* calling updateLastPosition, so
        // that it can clear the motion buffer the first time we call
        // it.
        this.panning = true;

        aEvent.stopPropagation();
        aEvent.preventDefault();
        break;
      case "mousemove":
        let dx = aEvent.clientX - this.lastX;
        let dy = aEvent.clientY - this.lastY;
  
        BrowserApp.selectedBrowser.contentWindow.wrappedJSObject.scrollBy(-dx, -dy);
        this._updateLastPosition(aEvent.clientX, aEvent.clientY);
  
        aEvent.stopPropagation();
        aEvent.preventDefault();
        break;
      case "mouseup":
        this.panning = false;
        let isDrag = (Math.abs(aEvent.clientX - this.startX) > 10 ||
                      Math.abs(aEvent.clientY - this.startY) > 10);

        if (isDrag)
          this.blockClick = true;

        if (isDrag && Math.abs(this.motionBuffer[0].time -
                               this.motionBuffer[4].time) < 1000) {
          // We've exceeded the scroll threshold, start kinetic pan

          // Find the average velocity over the last few motion events
          this.panX = 0;
          this.panY = 0;
          for (let i = 1; i < 5; i++) {
              let timeDelta = this.motionBuffer[i].time -
                this.motionBuffer[i - 1].time;
              this.panX += (this.motionBuffer[i].x -
                            this.motionBuffer[i - 1].x) / timeDelta;
              this.panY += (this.motionBuffer[i].y -
                            this.motionBuffer[i - 1].y) / timeDelta;
          }
          this.panX /= 4;
          this.panY /= 4;

          // TODO: Use the kinetic scrolling prefs
          this.panDeceleration = 0.999;
          this.panAccumulatedDeltaX = 0;
          this.panAccumulatedDeltaY = 0;
          this.panLastTime = window.mozAnimationStartTime;

          // TODO: Find the deepest scrollable element at the
          // mouse coordinates
          BrowserApp.selectedBrowser.contentWindow.wrappedJSObject.
            scrollBy(this.panX, this.panY);

          let self = this;
          let callback = {
            onBeforePaint: function kineticPanCallback(timeStamp) {
              if (self.panning)
                return;

              let timeDelta = timeStamp - self.panLastTime;
              self.panLastTime = timeStamp;

              // Adjust deceleration
              self.panX *= Math.pow(self.panDeceleration, timeDelta);
              self.panY *= Math.pow(self.panDeceleration, timeDelta);

              // Calculate panning motion
              let dx = self.panX * timeDelta;
              let dy = self.panY * timeDelta;

              // We only want to set integer scroll coordinates
              self.panAccumulatedDeltaX += dx - Math.floor(dx);
              self.panAccumulatedDeltaY += dy - Math.floor(dy);

              dx = Math.floor(dx);
              dy = Math.floor(dy);

              if (Math.abs(self.panAccumulatedDeltaX) >= 1.0) {
                  let adx = Math.floor(self.panAccumulatedDeltaX);
                  dx += adx;
                  self.panAccumulatedDeltaX -= adx;
              }

              if (Math.abs(self.panAccumulatedDeltaY) >= 1.0) {
                  let ady = Math.floor(self.panAccumulatedDeltaY);
                  dy += ady;
                  self.panAccumulatedDeltaY -= ady;
              }

              BrowserApp.selectedBrowser.contentWindow.wrappedJSObject.
                scrollBy(-dx, -dy);

              // If we're moving at a rate slower than 0.015
              // pixels/ms, stop requesting frames.
              // This is roughly equivalent to a pixel every
              // 4 frames at 60fps.
              if (Math.abs(self.panX) >= 0.015 || Math.abs(self.panY) >= 0.015)
                window.mozRequestAnimationFrame(this);
            }
          };

          // If an axis is moving slower than 0.3 pixels per ms
          // (about five pixels per frame at 60fps), lock that axis
          if (Math.abs(this.panX) < 0.3)
            this.panX = 0;
          if (Math.abs(this.panY) < 0.3)
            this.panY = 0;

          // Don't start kinetic panning if we're not moving
          if (Math.abs(this.panX) > 0 || Math.abs(this.panY) > 0)
            window.mozRequestAnimationFrame(callback);
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
          BrowserApp.selectedBrowser.markupDocumentViewer.fullZoom += delta;
          this._pinchDelta = 0;
        }
        break;
    }
  },

  _updateLastPosition: function(x, y) {
    this.lastX = x;
    this.lastY = y;
    this.lastTime = Date.now();

    for (let i = 0; i < 4; i++) {
      if (this.panning) {
        this.motionBuffer[i] = this.motionBuffer[i + 1];
      } else {
        this.motionBuffer[i] =
          { x: this.lastX, y: this.lastY, time: this.lastTime };
      }
    }

    this.motionBuffer[4] =
      { x: this.lastX, y: this.lastY, time: this.lastTime };
  }
};
