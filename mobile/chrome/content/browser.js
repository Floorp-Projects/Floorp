let Cc = Components.classes;
let Ci = Components.interfaces;
let Cu = Components.utils;
let Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/Services.jsm")

// The ratio of velocity that is retained every ms.
const kPanDeceleration = 0.999;

// The number of ms to consider events over for a swipe gesture.
const kSwipeLength = 1000;

// Minimum speed to move during kinetic panning. 0.015 pixels/ms is roughly
// equivalent to a pixel every 4 frames at 60fps.
const kMinKineticSpeed = 0.015;

// Maximum kinetic panning speed. 3 pixels/ms is equivalent to 50 pixels per
// frame at 60fps.
const kMaxKineticSpeed = 3;

// The maximum magnitude of disparity allowed between axes acceleration. If
// it's larger than this, lock the slow-moving axis.
const kAxisLockRatio = 5;

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

    Services.obs.addObserver(this, "tab-add", false);
    Services.obs.addObserver(this, "tab-load", false);
    Services.obs.addObserver(this, "tab-select", false);
    Services.obs.addObserver(this, "session-back", false);
    Services.obs.addObserver(this, "session-reload", false);

    let uri = "about:support";
    if ("arguments" in window && window.arguments[0])
      uri = window.arguments[0];

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

  getTabForId: function getTabForId(aId) {
    let tabs = this._tabs;
    for (let i=0; i < tabs.length; i++) {
       if (tabs[i].id == aId)
         return tabs[i];
    }
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

  selectTab: function selectTab(aTabId) {
    let tab = this.getTabForId(aTabId);
    if (tab != null) {
      this.selectedTab = tab;
      tab.active = true;
    }
  },

  observe: function(aSubject, aTopic, aData) {
    let browser = this.selectedBrowser;
    if (!browser)
      return;

    if (aTopic == "session-back")
      browser.goBack();
    else if (aTopic == "session-reload")
      browser.reload();
    else if (aTopic == "tab-add") {
      let newTab = this.addTab(aData);
      newTab.active = true;
    } else if (aTopic == "tab-load") 
      browser.loadURI(aData);
    else if (aTopic == "tab-select") 
      this.selectTab(parseInt(aData));
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


let gTabIDFactory = 0;

function Tab(aURL) {
  this.browser = null;
  this.id = 0;
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

    this.id = ++gTabIDFactory;
    let message = {
      gecko: {
        type: "onCreateTab",
        tabID: this.id,
        uri: aURL
      }
    };

    sendMessageToJava(message);
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
    let browser = BrowserApp.getBrowserForWindow(aWebProgress.DOMWindow);
    let windowID = aWebProgress.DOMWindow.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils).currentInnerWindowID;
    
    let uri = "";
    if (browser)
      uri = browser.currentURI.spec;

    let message = {
      gecko: {
        type: "onStateChange",
        tabID: this.id,
        windowID: windowID,
        uri: uri,
        state: aStateFlags
      }
    };

    sendMessageToJava(message);
  },

  onLocationChange: function(aWebProgress, aRequest, aLocationURI) {
    let browser = BrowserApp.getBrowserForWindow(aWebProgress.DOMWindow);
    let uri = browser.currentURI.spec;
    let windowID = aWebProgress.DOMWindow.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils).currentInnerWindowID;

    let message = {
      gecko: {
        type: "onLocationChange",
        tabID: this.id,
        windowID: windowID,
        uri: uri
      }
    };

    sendMessageToJava(message);
  },

  onSecurityChange: function(aBrowser, aWebProgress, aRequest, aState) {
    dump("progressListener.onSecurityChange");
  },

  onProgressChange: function(aWebProgress, aRequest, aCurSelfProgress, aMaxSelfProgress, aCurTotalProgress, aMaxTotalProgress) {
    let windowID = aWebProgress.DOMWindow.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils).currentInnerWindowID;
    let message = {
      gecko: {
        type: "onProgressChange",
        tabID: this.id,
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
    window.addEventListener("click", this, true);
    window.addEventListener("mousedown", this, true);
    window.addEventListener("mouseup", this, true);
    window.addEventListener("mousemove", this, true);

    BrowserApp.deck.addEventListener("MozMagnifyGestureStart", this, true);
    BrowserApp.deck.addEventListener("MozMagnifyGestureUpdate", this, true);
    BrowserApp.deck.addEventListener("DOMContentLoaded", this, true);
    BrowserApp.deck.addEventListener("DOMLinkAdded", this, true);
    BrowserApp.deck.addEventListener("DOMTitleChanged", this, true);
  },

  handleEvent: function(aEvent) {
    switch (aEvent.type) {
      case "DOMContentLoaded": {
        let browser = BrowserApp.getBrowserForDocument(aEvent.target);
        let tabID = BrowserApp.getTabForBrowser(browser).id;
        let uri = browser.currentURI.spec;

        dump("Setting Last uri to: " + uri);
        Services.prefs.setCharPref("browser.last.uri", uri);

        sendMessageToJava({
          gecko: {
            type: "DOMContentLoaded",
            tabID: tabID,
            windowID: 0,
            uri: uri,
            title: browser.contentTitle
          }
        });
        break;
      }

      case "DOMLinkAdded": {
        let target = aEvent.originalTarget;
        if (!target.href || target.disabled)
          return;
        
        let json = {
          type: "DOMLinkAdded",
          windowId: target.ownerDocument.defaultView.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils).currentInnerWindowID,
          href: target.href,
          charset: target.ownerDocument.characterSet,
          title: target.title,
          rel: target.rel
        };
        
        // rel=icon can also have a sizes attribute
        if (target.hasAttribute("sizes"))
          json.sizes = target.getAttribute("sizes");

        sendMessageToJava({ gecko: json });
        break;
      }

      case "DOMTitleChanged": {
        let browser = BrowserApp.getBrowserForDocument(aEvent.target);
        let tabID = BrowserApp.getTabForBrowser(browser).id;
        sendMessageToJava({
          gecko: {
            type: "DOMTitleChanged",
            tabID: tabID,
            title: aEvent.target.title
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
        this.firstMovement = true;
        this.firstXMovement = 0;
        this.firstYMovement = 0;

        this.motionBuffer = [];
        this._updateLastPosition(aEvent.clientX, aEvent.clientY, 0, 0);
        this.panElement = this._findScrollableElement(aEvent.originalTarget,
                                                      true);

        if (this.panElement)
          this.panning = true;

        // We always block the event to stop text selection happening when
        // there's nothing scrollable on the page
        aEvent.stopPropagation();
        aEvent.preventDefault();
        break;

      case "mousemove":
        if (!this.panning)
          break;

        let dx = aEvent.clientX - this.lastX;
        let dy = aEvent.clientY - this.lastY;

        // If this is the first panning motion, check if we can
        // move in the given direction. If we can't, find an
        // ancestor that can.
        // We do this per-axis, as often the very first movement doesn't
        // reflect the direction of movement for both axes.
        if (this.firstXMovement == 0) {
          this.firstXMovement = dx;
        }
        if (this.firstYMovement == 0) {
          this.firstYMovement = dy;
        }
        if (this.firstMovement &&
            this.firstXMovement != 0 &&
            this.firstYMovement != 0) {
          this.firstMovement = false;
          let originalElement = this.panElement;

          // See if we've reached the extents of this element and if so,
          // find an element above it that can be scrolled.
          while (this.panElement &&
                 !this._elementCanScroll(this.panElement,
                                         -this.firstXMovement,
                                         -this.firstYMovement)) {
            this.panElement =
              this._findScrollableElement(this.panElement, false);
          }

          // If there are no scrollable elements above the element whose
          // extents we've reached, let that element be scrolled.
          if (!this.panElement)
            this.panElement = originalElement;
        }

        this._scrollElementBy(this.panElement, -dx, -dy);

        // Note, it's important that this happens after the scrollElementBy,
        // as this will modify the clientX/clientY to be relative to the
        // correct element.
        this._updateLastPosition(aEvent.clientX, aEvent.clientY, dx, dy);

        aEvent.stopPropagation();
        aEvent.preventDefault();
        break;

      case "mouseup":
        if (!this.panning)
          break;

        this.panning = false;
        let isDrag = (Math.abs(aEvent.clientX - this.startX) > 10 ||
                      Math.abs(aEvent.clientY - this.startY) > 10);

        if (isDrag) {
          this.blockClick = true;
          aEvent.stopPropagation();
          aEvent.preventDefault();

          // Find the average velocity over the last few motion events
          this.panLastTime = window.mozAnimationStartTime;
          this.panX = 0;
          this.panY = 0;
          let nEvents = 0;
          for (let i = 0; i < this.motionBuffer.length - 1; i++) {
              if (this.motionBuffer[i].time < this.panLastTime - kSwipeLength)
                break;

              let timeDelta = this.motionBuffer[i].time -
                this.motionBuffer[i + 1].time;
              if (timeDelta <= 0)
                continue;

              this.panX += this.motionBuffer[i].dx / timeDelta;
              this.panY += this.motionBuffer[i].dy / timeDelta;
              nEvents ++;
          }
          if (nEvents == 0)
            break;

          this.panX /= nEvents;
          this.panY /= nEvents;
          if (Math.abs(this.panX) > kMaxKineticSpeed)
            this.panX = (this.panX > 0) ? kMaxKineticSpeed : -kMaxKineticSpeed;
          if (Math.abs(this.panY) > kMaxKineticSpeed)
            this.panY = (this.panY > 0) ? kMaxKineticSpeed : -kMaxKineticSpeed;

          // If we have (near) zero acceleration, bail out.
          if (Math.abs(this.panX) < kMinKineticSpeed &&
              Math.abs(this.panY) < kMinKineticSpeed)
            break;

          // Check if this element can scroll - if not, let's bail out.
          if (!this.panElement || !this._elementCanScroll(
                 this.panElement, -this.panX, -this.panY))
            break;

          // Fire off the first kinetic panning event
          this._scrollElementBy(this.panElement, -this.panX, -this.panY);

          // TODO: Use the kinetic scrolling prefs
          this.panAccumulatedDeltaX = 0;
          this.panAccumulatedDeltaY = 0;

          let self = this;
          let panElement = this.panElement;
          let callback = {
            onBeforePaint: function kineticPanCallback(timeStamp) {
              if (self.panning || self.panElement != panElement)
                return;

              let timeDelta = timeStamp - self.panLastTime;
              self.panLastTime = timeStamp;

              // Adjust deceleration
              self.panX *= Math.pow(kPanDeceleration, timeDelta);
              self.panY *= Math.pow(kPanDeceleration, timeDelta);

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

              self._scrollElementBy(panElement, -dx, -dy);

              if (Math.abs(self.panX) >= kMinKineticSpeed ||
                  Math.abs(self.panY) >= kMinKineticSpeed)
                window.mozRequestAnimationFrame(this);
            }
          };

          // If one axis is moving a lot slower than the other, lock it.
          if (Math.abs(this.panX) < Math.abs(this.panY) / kAxisLockRatio)
            this.panX = 0;
          else if (Math.abs(this.panY) < Math.abs(this.panX) / kAxisLockRatio)
            this.panY = 0;

          // Start the panning animation
          window.mozRequestAnimationFrame(callback);
        }
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

  _updateLastPosition: function(x, y, dx, dy) {
    this.lastX = x;
    this.lastY = y;
    this.lastTime = Date.now();

    this.motionBuffer.unshift({ dx: dx, dy: dy, time: this.lastTime });
  },

  _findScrollableElement: function(elem, checkElem) {
    // Walk the DOM tree until we find a scrollable element
    let scrollable = false;
    while (elem) {
      /* Element is scrollable if its scroll-size exceeds its client size, and:
       * - It has overflow 'auto' or 'scroll'
       * - It's a textarea
       * - It's an HTML/BODY node
       */
      if (checkElem) {
        if (((elem.scrollHeight > elem.clientHeight) ||
             (elem.scrollWidth > elem.clientWidth)) &&
            (elem.style.overflow == 'auto' ||
             elem.style.overflow == 'scroll' ||
             elem.localName == 'textarea' ||
             elem.localName == 'html' ||
             elem.localName == 'body')) {
          scrollable = true;
          break;
        }
      } else {
        checkElem = true;
      }

      // Propagate up iFrames
      if (!elem.parentNode && elem.documentElement &&
          elem.documentElement.ownerDocument)
        elem = elem.documentElement.ownerDocument.defaultView.frameElement;
      else
        elem = elem.parentNode;
    }

    if (!scrollable)
      return null;

    return elem;
  },

  _scrollElementBy: function(elem, x, y) {
    elem.scrollTop = elem.scrollTop + y;
    elem.scrollLeft = elem.scrollLeft + x;
  },

  _elementCanScroll: function(elem, x, y) {
    let scrollX = true;
    let scrollY = true;

    if (x < 0) {
      if (elem.scrollLeft <= 0) {
        scrollX = false;
      }
    } else if (elem.scrollLeft >= (elem.scrollWidth - elem.clientWidth)) {
      scrollX = false;
    }

    if (y < 0) {
      if (elem.scrollTop <= 0) {
        scrollY = false;
      }
    } else if (elem.scrollTop >= (elem.scrollHeight - elem.clientHeight)) {
      scrollY = false;
    }

    return scrollX || scrollY;
  }
};
