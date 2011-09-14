// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/*
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Mobile Browser.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brad Lassey <blassey@mozilla.com>
 *   Mark Finkle <mfinkle@mozilla.com>
 *   Aleks Totic <a@totic.org>
 *   Johnathan Nightingale <johnath@mozilla.com>
 *   Stuart Parmenter <stuart@mozilla.com>
 *   Taras Glek <tglek@mozilla.com>
 *   Roy Frostig <rfrostig@mozilla.com>
 *   Ben Combee <bcombee@mozilla.com>
 *   Matt Brubeck <mbrubeck@mozilla.com>
 *   Benjamin Stover <bstover@mozilla.com>
 *   Miika Jarvinen <mjarvin@gmail.com>
 *   Jaakko Kiviluoto <jaakko.kiviluoto@digia.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

let Cc = Components.classes;
let Ci = Components.interfaces;
let Cu = Components.utils;
let Cr = Components.results;

function getBrowser() {
  return Browser.selectedBrowser;
}

const kBrowserViewZoomLevelPrecision = 10000;

// allow panning after this timeout on pages with registered touch listeners
const kTouchTimeout = 300;

const kDefaultMetadata = { autoSize: false, allowZoom: true, autoScale: true };

// Override sizeToContent in the main window. It breaks things (bug 565887)
window.sizeToContent = function() {
  Cu.reportError("window.sizeToContent is not allowed in this window");
}

#ifdef MOZ_CRASH_REPORTER
XPCOMUtils.defineLazyServiceGetter(this, "CrashReporter",
  "@mozilla.org/xre/app-info;1", "nsICrashReporter");
#endif

function onDebugKeyPress(aEvent) {
  if (!aEvent.ctrlKey)
    return;

  // prevent the keypress from being triggered twice when page is local - bug 655501
  if (aEvent.originalTarget.nodeName == "html")
    return;

  function doSwipe(aDirection) {
    let evt = document.createEvent("SimpleGestureEvent");
    evt.initSimpleGestureEvent("MozSwipeGesture", true, true, window, null,
                               0, 0, 0, 0, false, false, false, false, 0, null,
                               aDirection, 0);
    Browser.selectedTab.inputHandler.dispatchEvent(evt);
  }

  let nsIDOMKeyEvent  = Ci.nsIDOMKeyEvent;
  switch (aEvent.charCode) {
    case nsIDOMKeyEvent.DOM_VK_A: // Swipe Left
      doSwipe(Ci.nsIDOMSimpleGestureEvent.DIRECTION_LEFT);
      break;
    case nsIDOMKeyEvent.DOM_VK_D: // Swipe Right
      doSwipe(Ci.nsIDOMSimpleGestureEvent.DIRECTION_RIGHT);
      break;
    case nsIDOMKeyEvent.DOM_VK_F: // Forge GC
      MemoryObserver.observe();
      dump("Forced a GC\n");
      break;
    case nsIDOMKeyEvent.DOM_VK_M: // Android Menu
      CommandUpdater.doCommand("cmd_menu");
      break;
#ifndef MOZ_PLATFORM_MAEMO
    case nsIDOMKeyEvent.DOM_VK_P: // Fake pinch zoom
      function dispatchMagnifyEvent(aName, aDelta) {
        let evt = document.createEvent("SimpleGestureEvent");
        evt.initSimpleGestureEvent("MozMagnifyGesture" + aName, true, true, window, null,
                                   0, 0, 0, 0, false, false, false, false, 0, null, 0, aDelta);
        Browser.selectedTab.inputHandler.dispatchEvent(evt);
      }
      dispatchMagnifyEvent("Start", 0);

      let frame = 0;
      let timer = new Util.Timeout();
      timer.interval(100, function() {
        dispatchMagnifyEvent("Update", 20);
        if (++frame > 10) {
          timer.clear();
          dispatchMagnifyEvent("", frame*20);
        }
      });
      break;
    case nsIDOMKeyEvent.DOM_VK_Q: // Toggle Orientation
      if (Util.isPortrait())
        window.top.resizeTo(800,480);
      else
        window.top.resizeTo(480,800);
      break;
#endif
    case nsIDOMKeyEvent.DOM_VK_S: // Swipe down
      doSwipe(Ci.nsIDOMSimpleGestureEvent.DIRECTION_DOWN);
      break;
    case nsIDOMKeyEvent.DOM_VK_W: // Swipe up
      doSwipe(Ci.nsIDOMSimpleGestureEvent.DIRECTION_UP);
      break;
    default:
      break;
  }
}

var Browser = {
  _tabs: [],
  _selectedTab: null,
  windowUtils: window.QueryInterface(Ci.nsIInterfaceRequestor)
                     .getInterface(Ci.nsIDOMWindowUtils),
  controlsScrollbox: null,
  controlsScrollboxScroller: null,
  controlsPosition: null,
  pageScrollbox: null,
  pageScrollboxScroller: null,
  styles: {},

  get defaultBrowserWidth() {
    delete this.defaultBrowserWidth;
    var width = Services.prefs.getIntPref("browser.viewport.desktopWidth");
    this.defaultBrowserWidth = width;
    return width;
  },

  startup: function startup() {
    var self = this;

    try {
      messageManager.loadFrameScript("chrome://browser/content/Util.js", true);
      messageManager.loadFrameScript("chrome://browser/content/forms.js", true);
      messageManager.loadFrameScript("chrome://browser/content/content.js", true);
    } catch (e) {
      // XXX whatever is calling startup needs to dump errors!
      dump("###########" + e + "\n");
    }

    // XXX change

    /* handles dispatching clicks on browser into clicks in content or zooms */
    Elements.browsers.customDragger = new Browser.MainDragger();

    /* handles web progress management for open browsers */
    Elements.browsers.webProgress = new Browser.WebProgress();

    this.keySender = new ContentCustomKeySender(Elements.browsers);
    let mouseModule = new MouseModule();
    let gestureModule = new GestureModule(Elements.browsers);
    let scrollWheelModule = new ScrollwheelModule(Elements.browsers);

    ContentTouchHandler.init();

    // Warning, total hack ahead. All of the real-browser related scrolling code
    // lies in a pretend scrollbox here. Let's not land this as-is. Maybe it's time
    // to redo all the dragging code.
    this.contentScrollbox = Elements.browsers;
    this.contentScrollboxScroller = {
      scrollBy: function(aDx, aDy) {
        let view = getBrowser().getRootView();
        view.scrollBy(aDx, aDy);
      },

      scrollTo: function(aX, aY) {
        let view = getBrowser().getRootView();
        view.scrollTo(aX, aY);
      },

      getPosition: function(aScrollX, aScrollY) {
        let view = getBrowser().getRootView();
        let scroll = view.getPosition();
        aScrollX.value = scroll.x;
        aScrollY.value = scroll.y;
      }
    };

    /* horizontally scrolling box that holds the sidebars as well as the contentScrollbox */
    let controlsScrollbox = this.controlsScrollbox = document.getElementById("controls-scrollbox");
    this.controlsScrollboxScroller = controlsScrollbox.boxObject.QueryInterface(Ci.nsIScrollBoxObject);
    controlsScrollbox.customDragger = {
      isDraggable: function isDraggable(target, content) { return {}; },
      dragStart: function dragStart(cx, cy, target, scroller) {},
      dragStop: function dragStop(dx, dy, scroller) { return false; },
      dragMove: function dragMove(dx, dy, scroller) { return false; }
    };

    /* vertically scrolling box that contains the url bar, notifications, and content */
    let pageScrollbox = this.pageScrollbox = document.getElementById("page-scrollbox");
    this.pageScrollboxScroller = pageScrollbox.boxObject.QueryInterface(Ci.nsIScrollBoxObject);
    pageScrollbox.customDragger = controlsScrollbox.customDragger;

    let stylesheet = document.styleSheets[0];
    for each (let style in ["window-width", "window-height", "viewable-height", "viewable-width", "toolbar-height"]) {
      let index = stylesheet.insertRule("." + style + " {}", stylesheet.cssRules.length);
      this.styles[style] = stylesheet.cssRules[index].style;
    }

    // Saved the scrolls values before the resizing of the window, to restore
    // the scrollbox position once the resize has finished.
    // The last parameter of addEventListener is true to be sure we performed
    // the computation before something else could happened (bug 622121)
    window.addEventListener("MozBeforeResize", function(aEvent) {
      if (aEvent.target != window)
        return;

      let { x: x1, y: y1 } = Browser.getScrollboxPosition(Browser.controlsScrollboxScroller);
      let { x: x2, y: y2 } = Browser.getScrollboxPosition(Browser.pageScrollboxScroller);
      let [,, leftWidth, rightWidth] = Browser.computeSidebarVisibility();

      let shouldHideSidebars = Browser.controlsPosition ? Browser.controlsPosition.hideSidebars : true;
      Browser.controlsPosition = { x: x1, y: y2, hideSidebars: shouldHideSidebars,
                                   leftSidebar: leftWidth, rightSidebar: rightWidth };
    }, true);

    function resizeHandler(e) {
      if (e.target != window)
        return;

      let w = window.innerWidth;
      let h = window.innerHeight;

      // Don't bother doing unuseful work during intermediate resized during
      // startup if the goal is to be fullscreen
      let fullscreen = (document.documentElement.getAttribute("sizemode") == "fullscreen");
      if (fullscreen && w != screen.width)
        return;

      BrowserUI.updateTabletLayout();

      let toolbarHeight = Math.round(document.getElementById("toolbar-main").getBoundingClientRect().height);

      Browser.styles["window-width"].width = w + "px";
      Browser.styles["window-height"].height = h + "px";
      Browser.styles["toolbar-height"].height = toolbarHeight + "px";

      // Tell the UI to resize the browser controls
      BrowserUI.sizeControls(w, h);
      ViewableAreaObserver.update();

      // Restore the previous scroll position
      let restorePosition = Browser.controlsPosition || { hideSidebars: true };
      if (restorePosition.hideSidebars) {
        restorePosition.hideSidebars = false;
        Browser.hideSidebars();
      } else {
        // Handle Width transformation of the tabs sidebar
        if (restorePosition.x) {
          let [,, leftWidth, rightWidth] = Browser.computeSidebarVisibility();
          let delta = ((restorePosition.leftSidebar - leftWidth) || (restorePosition.rightSidebar - rightWidth));
          restorePosition.x += (restorePosition.x == leftWidth) ? delta : -delta;
        }

        Browser.controlsScrollboxScroller.scrollTo(restorePosition.x, 0);
        Browser.pageScrollboxScroller.scrollTo(0, restorePosition.y);
        Browser.tryFloatToolbar(0, 0);
      }

      // We want to keep the current focused element into view if possible
      let currentElement = document.activeElement;
      let [scrollbox, scrollInterface] = ScrollUtils.getScrollboxFromElement(currentElement);
      if (scrollbox && scrollInterface && currentElement && currentElement != scrollbox) {
        // retrieve the direct child of the scrollbox
        while (currentElement.parentNode != scrollbox)
          currentElement = currentElement.parentNode;

        setTimeout(function() { scrollInterface.ensureElementIsVisible(currentElement) }, 0);
      }
    }
    window.addEventListener("resize", resizeHandler, false);
    window.addEventListener("AlertActive", this._alertShown.bind(this), false);

    function fullscreenHandler() {
      if (!window.fullScreen)
        document.getElementById("toolbar-main").setAttribute("fullscreen", "true");
      else
        document.getElementById("toolbar-main").removeAttribute("fullscreen");
    }
    window.addEventListener("fullscreen", fullscreenHandler, false);

    BrowserUI.init();

    window.controllers.appendController(this);
    window.controllers.appendController(BrowserUI);

    var os = Services.obs;
    os.addObserver(XPInstallObserver, "addon-install-blocked", false);
    os.addObserver(XPInstallObserver, "addon-install-started", false);
    os.addObserver(SessionHistoryObserver, "browser:purge-session-history", false);
    os.addObserver(ContentCrashObserver, "ipc:content-shutdown", false);
    os.addObserver(MemoryObserver, "memory-pressure", false);
    os.addObserver(ActivityObserver, "application-background", false);
    os.addObserver(ActivityObserver, "application-foreground", false);
    os.addObserver(ActivityObserver, "system-active", false);
    os.addObserver(ActivityObserver, "system-idle", false);
    os.addObserver(ActivityObserver, "system-display-on", false);
    os.addObserver(ActivityObserver, "system-display-off", false);

    // Listens for change in the viewable area
#if MOZ_PLATFORM_MAEMO == 6
    os.addObserver(ViewableAreaObserver, "softkb-change", false);
#endif
   messageManager.addMessageListener("Content:IsKeyboardOpened", ViewableAreaObserver);

    window.QueryInterface(Ci.nsIDOMChromeWindow).browserDOMWindow = new nsBrowserAccess();

    Elements.browsers.addEventListener("DOMUpdatePageReport", PopupBlockerObserver.onUpdatePageReport, false);

    // Make sure we're online before attempting to load
    Util.forceOnline();

    // If this is an intial window launch the commandline handler passes us the default
    // page as an argument. commandURL _should_ never be empty, but we protect against it
    // below. However, we delay trying to get the fallback homepage until we really need it.
    let commandURL = null;
    if (window.arguments && window.arguments[0])
      commandURL = window.arguments[0];

    // Should we restore the previous session (crash or some other event)
    let ss = Cc["@mozilla.org/browser/sessionstore;1"].getService(Ci.nsISessionStore);
    if (ss.shouldRestore()) {
      let bringFront = false;
      // First open any commandline URLs, except the homepage
      if (commandURL && commandURL != this.getHomePage()) {
        this.addTab(commandURL, true);
      } else {
        bringFront = true;
        // Initial window resizes call functions that assume a tab is in the tab list
        // and restored tabs are added too late. We add a dummy to to satisfy the resize
        // code and then remove the dummy after the session has been restored.
        let dummy = this.addTab("about:blank", true);
        let dummyCleanup = {
          observe: function(aSubject, aTopic, aData) {
            Services.obs.removeObserver(dummyCleanup, "sessionstore-windows-restored");
            if (aData == "fail")
              Browser.addTab(commandURL || Browser.getHomePage(), true);
            dummy.chromeTab.ignoreUndo = true;
            Browser.closeTab(dummy, { forceClose: true });
          }
        };
        Services.obs.addObserver(dummyCleanup, "sessionstore-windows-restored", false);
      }
      ss.restoreLastSession(bringFront);
    } else {
      this.addTab(commandURL || this.getHomePage(), true);
    }

    messageManager.addMessageListener("MozScrolledAreaChanged", this);
    messageManager.addMessageListener("Browser:ViewportMetadata", this);
    messageManager.addMessageListener("Browser:CanCaptureMouse:Return", this);
    messageManager.addMessageListener("Browser:FormSubmit", this);
    messageManager.addMessageListener("Browser:KeyPress", this);
    messageManager.addMessageListener("Browser:ZoomToPoint:Return", this);
    messageManager.addMessageListener("Browser:CanUnload:Return", this);
    messageManager.addMessageListener("scroll", this);
    messageManager.addMessageListener("Browser:CertException", this);
    messageManager.addMessageListener("Browser:BlockedSite", this);
    messageManager.addMessageListener("Browser:ErrorPage", this);

    // Broadcast a UIReady message so add-ons know we are finished with startup
    let event = document.createEvent("Events");
    event.initEvent("UIReady", true, false);
    window.dispatchEvent(event);
  },

  _alertShown: function _alertShown() {
    // ensure that the full notification still visible, even if the urlbar is floating
    if (BrowserUI.isToolbarLocked())
      Browser.pageScrollboxScroller.scrollTo(0, 0);
  },

  quit: function quit() {
    // NOTE: onclose seems to be called only when using OS chrome to close a window,
    // so we need to handle the Browser.closing check ourselves.
    if (this.closing()) {
      window.QueryInterface(Ci.nsIDOMChromeWindow).minimize();
      window.close();
    }
  },

  _waitingToClose: false,
  closing: function closing() {
    // If we are already waiting for the close prompt, don't show another
    if (this._waitingToClose)
      return false;

    // Prompt if we have multiple tabs before closing window
    let numTabs = this._tabs.length;
    if (numTabs > 1) {
      let shouldPrompt = Services.prefs.getBoolPref("browser.tabs.warnOnClose");
      if (shouldPrompt) {
        let prompt = Services.prompt;

        // Default to true: if it were false, we wouldn't get this far
        let warnOnClose = { value: true };

        let messageBase = Strings.browser.GetStringFromName("tabs.closeWarning");
        let message = PluralForm.get(numTabs, messageBase).replace("#1", numTabs);

        let title = Strings.browser.GetStringFromName("tabs.closeWarningTitle");
        let closeText = Strings.browser.GetStringFromName("tabs.closeButton");
        let checkText = Strings.browser.GetStringFromName("tabs.closeWarningPromptMe");
        let buttons = (prompt.BUTTON_TITLE_IS_STRING * prompt.BUTTON_POS_0) +
                      (prompt.BUTTON_TITLE_CANCEL * prompt.BUTTON_POS_1);

        this._waitingToClose = true;
#ifdef MOZ_PLATFORM_MAEMO
        window.QueryInterface(Ci.nsIDOMChromeWindow).restore();
#endif
        let pressed = prompt.confirmEx(window, title, message, buttons, closeText, null, null, checkText, warnOnClose);
        this._waitingToClose = false;

        // Don't set the pref unless they press OK and it's false
        let reallyClose = (pressed == 0);
        if (reallyClose && !warnOnClose.value)
          Services.prefs.setBoolPref("browser.tabs.warnOnClose", false);

        // If we don't want to close, return now. If we are closing, continue with other housekeeping.
        if (!reallyClose)
          return false;
      }
    }

    // Figure out if there's at least one other browser window around.
    let lastBrowser = true;
    let e = Services.wm.getEnumerator("navigator:browser");
    while (e.hasMoreElements() && lastBrowser) {
      let win = e.getNext();
      if (win != window)
        lastBrowser = false;
    }
    if (!lastBrowser)
      return true;

    // Let everyone know we are closing the last browser window
    let closingCancelled = Cc["@mozilla.org/supports-PRBool;1"].createInstance(Ci.nsISupportsPRBool);
    Services.obs.notifyObservers(closingCancelled, "browser-lastwindow-close-requested", null);
    if (closingCancelled.data)
      return false;

    Services.obs.notifyObservers(null, "browser-lastwindow-close-granted", null);
    return true;
  },

  shutdown: function shutdown() {
    BrowserUI.uninit();

    messageManager.removeMessageListener("MozScrolledAreaChanged", this);
    messageManager.removeMessageListener("Browser:ViewportMetadata", this);
    messageManager.removeMessageListener("Browser:FormSubmit", this);
    messageManager.removeMessageListener("Browser:KeyPress", this);
    messageManager.removeMessageListener("Browser:ZoomToPoint:Return", this);
    messageManager.removeMessageListener("scroll", this);
    messageManager.removeMessageListener("Browser:CertException", this);
    messageManager.removeMessageListener("Browser:BlockedSite", this);
    messageManager.removeMessageListener("Browser:ErrorPage", this);

    var os = Services.obs;
    os.removeObserver(XPInstallObserver, "addon-install-blocked");
    os.removeObserver(XPInstallObserver, "addon-install-started");
    os.removeObserver(SessionHistoryObserver, "browser:purge-session-history");
    os.removeObserver(ContentCrashObserver, "ipc:content-shutdown");
    os.removeObserver(MemoryObserver, "memory-pressure");
    os.removeObserver(ActivityObserver, "application-background", false);
    os.removeObserver(ActivityObserver, "application-foreground", false);
    os.removeObserver(ActivityObserver, "system-active", false);
    os.removeObserver(ActivityObserver, "system-idle", false);
    os.removeObserver(ActivityObserver, "system-display-on", false);
    os.removeObserver(ActivityObserver, "system-display-off", false);

    window.controllers.removeController(this);
    window.controllers.removeController(BrowserUI);
  },

  getHomePage: function getHomePage(aOptions) {
    aOptions = aOptions || { useDefault: false };

    let url = "about:home";
    try {
      let prefs = aOptions.useDefault ? Services.prefs.getDefaultBranch(null) : Services.prefs;
      url = prefs.getComplexValue("browser.startup.homepage", Ci.nsIPrefLocalizedString).data;
    }
    catch(e) { }

    return url;
  },

  get browsers() {
    return this._tabs.map(function(tab) { return tab.browser; });
  },

  scrollContentToTop: function scrollContentToTop(aOptions) {
    let x = {}, y = {};
    this.contentScrollboxScroller.getPosition(x, y);
    if (aOptions)
      x.value = ("x" in aOptions ? aOptions.x : x.value);

    this.contentScrollboxScroller.scrollTo(x.value, 0);
    this.pageScrollboxScroller.scrollTo(0, 0);
  },

  // cmd_scrollBottom does not work in Fennec (Bug 590535).
  scrollContentToBottom: function scrollContentToBottom(aOptions) {
    let x = {}, y = {};
    this.contentScrollboxScroller.getPosition(x, y);
    if (aOptions)
      x.value = ("x" in aOptions ? aOptions.x : x.value);

    this.contentScrollboxScroller.scrollTo(x.value, Number.MAX_VALUE);
    this.pageScrollboxScroller.scrollTo(0, Number.MAX_VALUE);
    this.hideTitlebar();
  },

  hideSidebars: function scrollSidebarsOffscreen() {
    let rect = Elements.browsers.getBoundingClientRect();
    this.controlsScrollboxScroller.scrollBy(Math.round(rect.left), 0);
    this.tryUnfloatToolbar();
  },

  /** Workaround to hide the tabstrip if it is partially visible (bug 524469 and bug 626660) */
  hidePartialTabSidebar: function hidePartialSidebars() {
    let [tabsVisibility,,,] = this.computeSidebarVisibility();
    if (tabsVisibility > 0.0 && tabsVisibility < 1.0)
      this.hideSidebars();
  },

  hideTitlebar: function hideTitlebar() {
    let rect = Elements.browsers.getBoundingClientRect();
    this.pageScrollboxScroller.scrollBy(0, Math.round(rect.top));
    this.tryUnfloatToolbar();
  },

  /**
   * Load a URI in the current tab, or a new tab if necessary.
   * @param aURI String
   * @param aParams Object with optional properties that will be passed to loadURIWithFlags:
   *    flags, referrerURI, charset, postData.
   */
  loadURI: function loadURI(aURI, aParams) {
    let browser = this.selectedBrowser;

    // We need to keep about: pages opening in new "local" tabs. We also want to spawn
    // new "remote" tabs if opening web pages from a "local" about: page.
    let currentURI = browser.currentURI.spec;
    let useLocal = Util.isLocalScheme(aURI);
    let hasLocal = Util.isLocalScheme(currentURI);

    if (hasLocal != useLocal) {
      let oldTab = this.selectedTab;

      // Add new tab before closing the old one, in case there is only one.
      Browser.addTab(aURI, true, oldTab, aParams);
      if (/^about:(blank|empty)$/.test(currentURI) && !browser.canGoBack && !browser.canGoForward) {
        oldTab.chromeTab.ignoreUndo = true;
        this.closeTab(oldTab, { forceClose: true });
        oldTab = null;
      }
    }
    else {
      let params = aParams || {};
      let flags = params.flags || Ci.nsIWebNavigation.LOAD_FLAGS_NONE;
      browser.loadURIWithFlags(aURI, flags, params.referrerURI, params.charset, params.postData);
    }
  },

  /**
   * Determine if the given URL is a shortcut/keyword and, if so, expand it
   * @param aURL String
   * @param aPostDataRef Out param contains any required post data for a search
   * @returns the expanded shortcut, or the original URL if not a shortcut
   */
  getShortcutOrURI: function getShortcutOrURI(aURL, aPostDataRef) {
    let shortcutURL = null;
    let keyword = aURL;
    let param = "";

    let offset = aURL.indexOf(" ");
    if (offset > 0) {
      keyword = aURL.substr(0, offset);
      param = aURL.substr(offset + 1);
    }

    if (!aPostDataRef)
      aPostDataRef = {};

    let engine = Services.search.getEngineByAlias(keyword);
    if (engine) {
      let submission = engine.getSubmission(param);
      aPostDataRef.value = submission.postData;
      return submission.uri.spec;
    }

    try {
      [shortcutURL, aPostDataRef.value] = PlacesUtils.getURLAndPostDataForKeyword(keyword);
    } catch (e) {}

    if (!shortcutURL)
      return aURL;

    let postData = "";
    if (aPostDataRef.value)
      postData = unescape(aPostDataRef.value);

    if (/%s/i.test(shortcutURL) || /%s/i.test(postData)) {
      let charset = "";
      const re = /^(.*)\&mozcharset=([a-zA-Z][_\-a-zA-Z0-9]+)\s*$/;
      let matches = shortcutURL.match(re);
      if (matches)
        [, shortcutURL, charset] = matches;
      else {
        // Try to get the saved character-set.
        try {
          // makeURI throws if URI is invalid.
          // Will return an empty string if character-set is not found.
          charset = PlacesUtils.history.getCharsetForURI(Util.makeURI(shortcutURL));
        } catch (e) { dump("--- error " + e + "\n"); }
      }

      let encodedParam = "";
      if (charset)
        encodedParam = escape(convertFromUnicode(charset, param));
      else // Default charset is UTF-8
        encodedParam = encodeURIComponent(param);

      shortcutURL = shortcutURL.replace(/%s/g, encodedParam).replace(/%S/g, param);

      if (/%s/i.test(postData)) // POST keyword
        aPostDataRef.value = getPostDataStream(postData, param, encodedParam, "application/x-www-form-urlencoded");
    } else if (param) {
      // This keyword doesn't take a parameter, but one was provided. Just return
      // the original URL.
      aPostDataRef.value = null;

      return aURL;
    }

    return shortcutURL;
  },

  /**
   * Return the currently active <browser> object
   */
  get selectedBrowser() {
    return this._selectedTab.browser;
  },

  get tabs() {
    return this._tabs;
  },

  getTabForBrowser: function getTabForBrowser(aBrowser) {
    let tabs = this._tabs;
    for (let i = 0; i < tabs.length; i++) {
      if (tabs[i].browser == aBrowser)
        return tabs[i];
    }
    return null;
  },

  getBrowserForWindowId: function getBrowserForWindowId(aWindowId) {
    for (let i = 0; i < this.browsers.length; i++) {
      if (this.browsers[i].contentWindowId == aWindowId)
        return this.browsers[i];
    }
    return null;
  },

  getTabAtIndex: function getTabAtIndex(index) {
    if (index > this._tabs.length || index < 0)
      return null;
    return this._tabs[index];
  },

  getTabFromChrome: function getTabFromChrome(chromeTab) {
    for (var t = 0; t < this._tabs.length; t++) {
      if (this._tabs[t].chromeTab == chromeTab)
        return this._tabs[t];
    }
    return null;
  },

  addTab: function browser_addTab(aURI, aBringFront, aOwner, aParams) {
    let params = aParams || {};
    let newTab = new Tab(aURI, params);
    newTab.owner = aOwner || null;
    this._tabs.push(newTab);

    if (aBringFront)
      this.selectedTab = newTab;

    let getAttention = ("getAttention" in params ? params.getAttention : !aBringFront);
    let event = document.createEvent("UIEvents");
    event.initUIEvent("TabOpen", true, false, window, getAttention);
    newTab.chromeTab.dispatchEvent(event);
    newTab.browser.messageManager.sendAsyncMessage("Browser:TabOpen");

    let cmd = document.getElementById("cmd_showTabs");
    cmd.setAttribute("label", this._tabs.length - 1);

    return newTab;
  },

  closeTab: function closeTab(aTab, aOptions) {
    let tab = aTab instanceof XULElement ? this.getTabFromChrome(aTab) : aTab;
    if (!tab || !this._getNextTab(tab))
      return;

    if (aOptions && "forceClose" in aOptions && aOptions.forceClose) {
      this._doCloseTab(aTab);
      return;
    }

    tab.browser.messageManager.sendAsyncMessage("Browser:CanUnload", {});

    let cmd = document.getElementById("cmd_showTabs");
    cmd.setAttribute("label", this._tabs.length - 1);
  },

  _doCloseTab: function _doCloseTab(aTab) {
    let nextTab = this._getNextTab(aTab);
    if (!nextTab)
       return;

    // Make sure we leave the toolbar in an unlocked state
    if (aTab == this._selectedTab && aTab.isLoading())
      BrowserUI.unlockToolbar();

    // Tabs owned by the closed tab are now orphaned.
    this._tabs.forEach(function(item, index, array) {
      if (item.owner == aTab)
        item.owner = null;
    });

    let event = document.createEvent("Events");
    event.initEvent("TabClose", true, false);
    aTab.chromeTab.dispatchEvent(event);
    aTab.browser.messageManager.sendAsyncMessage("Browser:TabClose");

    let container = aTab.chromeTab.parentNode;
    aTab.destroy();
    this._tabs.splice(this._tabs.indexOf(aTab), 1);

    this.selectedTab = nextTab;

    event = document.createEvent("Events");
    event.initEvent("TabRemove", true, false);
    container.dispatchEvent(event);
  },

  _getNextTab: function _getNextTab(aTab) {
    let tabIndex = this._tabs.indexOf(aTab);
    if (tabIndex == -1)
      return null;

    let nextTab = this._selectedTab;
    if (nextTab == aTab) {
      nextTab = this.getTabAtIndex(tabIndex + 1) || this.getTabAtIndex(tabIndex - 1);

      // If the next tab is not a sibling, switch back to the parent.
      if (aTab.owner && nextTab.owner != aTab.owner)
        nextTab = aTab.owner;

      if (!nextTab)
        return null;
    }

    return nextTab;
  },

  get selectedTab() {
    return this._selectedTab;
  },

  set selectedTab(tab) {
    if (tab instanceof XULElement)
      tab = this.getTabFromChrome(tab);

    if (!tab)
      return;

    if (this._selectedTab == tab) {
      // Deck does not update its selectedIndex when children
      // are removed. See bug 602708
      Elements.browsers.selectedPanel = tab.notification;
      return;
    }

    if (this._selectedTab) {
      this._selectedTab.pageScrollOffset = this.getScrollboxPosition(this.pageScrollboxScroller);

      // Make sure we leave the toolbar in an unlocked state
      if (this._selectedTab.isLoading())
        BrowserUI.unlockToolbar();
    }

    let isFirstTab = this._selectedTab == null;
    let lastTab = this._selectedTab;
    let oldBrowser = lastTab ? lastTab._browser : null;
    let browser = tab.browser;

    this._selectedTab = tab;

    // Lock the toolbar if the new tab is still loading
    if (this._selectedTab.isLoading())
      BrowserUI.lockToolbar();

    if (lastTab)
      lastTab.active = false;

    if (tab)
      tab.active = true;

    if (isFirstTab) {
      // Don't waste time at startup updating the whole UI; just display the URL.
      BrowserUI._titleChanged(browser);
    } else {
      // Update all of our UI to reflect the new tab's location
      BrowserUI.updateURI();
      getIdentityHandler().checkIdentity();

      let event = document.createEvent("Events");
      event.initEvent("TabSelect", true, false);
      event.lastTab = lastTab;
      tab.chromeTab.dispatchEvent(event);
    }

    tab.lastSelected = Date.now();

    if (tab.pageScrollOffset) {
      let pageScroll = tab.pageScrollOffset;
      Browser.pageScrollboxScroller.scrollTo(pageScroll.x, pageScroll.y);
    }
  },

  supportsCommand: function(cmd) {
    var isSupported = false;
    switch (cmd) {
      case "cmd_fullscreen":
        isSupported = true;
        break;
      default:
        isSupported = false;
        break;
    }
    return isSupported;
  },

  isCommandEnabled: function(cmd) {
    return true;
  },

  doCommand: function(cmd) {
    switch (cmd) {
      case "cmd_fullscreen":
        window.fullScreen = !window.fullScreen;
        break;
    }
  },

  getNotificationBox: function getNotificationBox(aBrowser) {
    let browser = aBrowser || this.selectedBrowser;
    return browser.parentNode;
  },

  /**
   * Handle cert exception message from content.
   */
  _handleCertException: function _handleCertException(aMessage) {
    let json = aMessage.json;
    if (json.action == "leave") {
      // Get the start page from the *default* pref branch, not the user's
      let url = Browser.getHomePage({ useDefault: true });
      this.loadURI(url);
    } else {
      // Handle setting an cert exception and reloading the page
      try {
        // Add a new SSL exception for this URL
        let uri = Services.io.newURI(json.url, null, null);
        let sslExceptions = new SSLExceptions();

        if (json.action == "permanent")
          sslExceptions.addPermanentException(uri);
        else
          sslExceptions.addTemporaryException(uri);
      } catch (e) {
        dump("EXCEPTION handle content command: " + e + "\n" );
      }

      // Automatically reload after the exception was added
      aMessage.target.reload();
    }
  },

  /**
   * Handle blocked site message from content.
   */
  _handleBlockedSite: function _handleBlockedSite(aMessage) {
    let formatter = Cc["@mozilla.org/toolkit/URLFormatterService;1"].getService(Ci.nsIURLFormatter);
    let json = aMessage.json;
    switch (json.action) {
      case "leave": {
        // Get the start page from the *default* pref branch, not the user's
        let url = Browser.getHomePage({ useDefault: true });
        this.loadURI(url);
        break;
      }
      case "report-malware": {
        // Get the stop badware "why is this blocked" report url, append the current url, and go there.
        try {
          let reportURL = formatter.formatURLPref("browser.safebrowsing.malware.reportURL");
          reportURL += json.url;
          this.loadURI(reportURL);
        } catch (e) {
          Cu.reportError("Couldn't get malware report URL: " + e);
        }
        break;
      }
      case "report-phishing": {
        // It's a phishing site, not malware
        try {
          let reportURL = formatter.formatURLPref("browser.safebrowsing.warning.infoURL");
          this.loadURI(reportURL);
        } catch (e) {
          Cu.reportError("Couldn't get phishing info URL: " + e);
        }
        break;
      }
    }
  },

  /**
   * Handle error page message from the content.
   */
  _handleErrorPage: function _handleErrorPage(aMessage) {
    let tab = this.getTabForBrowser(aMessage.target);
    tab.updateThumbnail({ force: true });
  },

  /**
   * Compute the sidebar percentage visibility.
   *
   * @param [optional] dx
   * @param [optional] dy an offset distance at which to perform the visibility
   * computation
   */
  computeSidebarVisibility: function computeSidebarVisibility(dx, dy) {
    function visibility(aSidebarRect, aVisibleRect) {
      let width = aSidebarRect.width;
      aSidebarRect.restrictTo(aVisibleRect);
      return (width ? aSidebarRect.width / width : 0);
    }

    if (!dx) dx = 0;
    if (!dy) dy = 0;

    let [leftSidebar, rightSidebar] = [Elements.tabs.getBoundingClientRect(), Elements.controls.getBoundingClientRect()];

    let visibleRect = new Rect(0, 0, window.innerWidth, 1);
    let leftRect = new Rect(Math.round(leftSidebar.left) - Math.round(dx), 0, Math.round(leftSidebar.width), 1);
    let rightRect = new Rect(Math.round(rightSidebar.left) - Math.round(dx), 0, Math.round(rightSidebar.width), 1);

    let leftTotalWidth = leftRect.width;
    let leftVisibility = visibility(leftRect, visibleRect);

    let rightTotalWidth = rightRect.width;
    let rightVisibility = visibility(rightRect, visibleRect);

    return [leftVisibility, rightVisibility, leftTotalWidth, rightTotalWidth];
  },

  /**
   * Compute the horizontal distance needed to scroll in order to snap the
   * sidebars into place.
   *
   * Visibility is computed by creating dummy rectangles for the sidebar and the
   * visible rect.  Sidebar rectangles come from getBoundingClientRect(), so
   * they are in absolute client coordinates (and since we're in a scrollbox,
   * this means they are positioned relative to the window, which is anchored at
   * (0, 0) regardless of the scrollbox's scroll position.  The rectangles are
   * made to have a top of 0 and a height of 1, since these do not affect how we
   * compute visibility (we care only about width), and using rectangles allows
   * us to use restrictTo(), which comes in handy.
   *
   * @return scrollBy dx needed to make snap happen
   */
  snapSidebars: function snapSidebars() {
    let [leftvis, ritevis, leftw, ritew] = Browser.computeSidebarVisibility();

    let snappedX = 0;

    let dirVal = Util.localeDir;
    if (leftvis != 0 && leftvis != 1) {
      if (leftvis >= 0.6666) {
        snappedX = -((1 - leftvis) * leftw) * dirVal;
      } else {
        snappedX = leftvis * leftw * dirVal;
      }
    }
    else if (ritevis != 0 && ritevis != 1) {
      if (ritevis >= 0.6666) {
        snappedX = (1 - ritevis) * ritew * dirVal;
      } else {
        snappedX = -ritevis * ritew * dirVal;
      }
    }

    return Math.round(snappedX);
  },

  tryFloatToolbar: function tryFloatToolbar(dx, dy) {
    if (this.floatedWhileDragging || Util.isTablet())
      return;

    let [leftvis, ritevis, leftw, ritew] = Browser.computeSidebarVisibility(dx, dy);
    if (leftvis > 0 || ritevis > 0) {
      BrowserUI.lockToolbar();
      this.floatedWhileDragging = true;
    }
  },

  tryUnfloatToolbar: function tryUnfloatToolbar(dx, dy) {
    if (!this.floatedWhileDragging)
      return true;

    let [leftvis, ritevis, leftw, ritew] = Browser.computeSidebarVisibility(dx, dy);
    if (leftvis == 0 && ritevis == 0) {
      BrowserUI.unlockToolbar();
      this.floatedWhileDragging = false;
      return true;
    }
    return false;
  },

  /** Zoom one step in (negative) or out (positive). */
  zoom: function zoom(aDirection) {
    let tab = this.selectedTab;
    if (!tab.allowZoom)
      return;

    let browser = tab.browser;
    let oldZoomLevel = browser.scale;
    let zoomLevel = oldZoomLevel;

    let zoomValues = ZoomManager.zoomValues;
    let i = zoomValues.indexOf(ZoomManager.snap(zoomLevel)) + (aDirection < 0 ? 1 : -1);
    if (i >= 0 && i < zoomValues.length)
      zoomLevel = zoomValues[i];

    zoomLevel = tab.clampZoomLevel(zoomLevel);

    let browserRect = browser.getBoundingClientRect();
    let center = browser.transformClientToBrowser(browserRect.width / 2,
                                                  browserRect.height / 2);
    let rect = this._getZoomRectForPoint(center.x, center.y, zoomLevel);
    AnimatedZoom.animateTo(rect);
  },

  /** Rect should be in browser coordinates. */
  _getZoomLevelForRect: function _getZoomLevelForRect(rect) {
    const margin = 15;
    return this.selectedTab.clampZoomLevel(ViewableAreaObserver.width / (rect.width + margin * 2));
  },

  /**
   * Find an appropriate zoom rect for an element bounding rect, if it exists.
   * @return Rect in viewport coordinates, or null
   */
  _getZoomRectForRect: function _getZoomRectForRect(rect, y) {
    let zoomLevel = this._getZoomLevelForRect(rect);
    return this._getZoomRectForPoint(rect.center().x, y, zoomLevel);
  },

  /**
   * Find a good zoom rectangle for point that is specified in browser coordinates.
   * @return Rect in viewport coordinates
   */
  _getZoomRectForPoint: function _getZoomRectForPoint(x, y, zoomLevel) {
    let browser = getBrowser();
    x = x * browser.scale;
    y = y * browser.scale;

    zoomLevel = Math.min(ZoomManager.MAX, zoomLevel);
    let oldScale = browser.scale;
    let zoomRatio = zoomLevel / oldScale;
    let browserRect = browser.getBoundingClientRect();
    let newVisW = browserRect.width / zoomRatio, newVisH = browserRect.height / zoomRatio;
    let result = new Rect(x - newVisW / 2, y - newVisH / 2, newVisW, newVisH);

    // Make sure rectangle doesn't poke out of viewport
    return result.translateInside(new Rect(0, 0, browser.contentDocumentWidth * oldScale,
                                                 browser.contentDocumentHeight * oldScale));
  },

  zoomToPoint: function zoomToPoint(cX, cY, aRect) {
    let tab = this.selectedTab;
    if (!tab.allowZoom)
      return null;

    let zoomRect = null;
    if (aRect)
      zoomRect = this._getZoomRectForRect(aRect, cY);

    if (!zoomRect && tab.isDefaultZoomLevel()) {
      let scale = tab.clampZoomLevel(tab.browser.scale * 2);
      zoomRect = this._getZoomRectForPoint(cX, cY, scale);
    }

    if (zoomRect)
      AnimatedZoom.animateTo(zoomRect);

    return zoomRect;
  },

  zoomFromPoint: function zoomFromPoint(cX, cY) {
    let tab = this.selectedTab;
    if (tab.allowZoom && !tab.isDefaultZoomLevel()) {
      let zoomLevel = tab.getDefaultZoomLevel();
      let zoomRect = this._getZoomRectForPoint(cX, cY, zoomLevel);
      AnimatedZoom.animateTo(zoomRect);
    }
  },

  // The device-pixel-to-CSS-px ratio used to adjust meta viewport values.
  // This is higher on higher-dpi displays, so pages stay about the same physical size.
  getScaleRatio: function getScaleRatio() {
    let prefValue = Services.prefs.getIntPref("browser.viewport.scaleRatio");
    if (prefValue > 0)
      return prefValue / 100;

    let dpi = Util.displayDPI;
    if (dpi < 200) // Includes desktop displays, and LDPI and MDPI Android devices
      return 1;
    else if (dpi < 300) // Includes Nokia N900, and HDPI Android devices
      return 1.5;

    // For very high-density displays like the iPhone 4, calculate an integer ratio.
    return Math.floor(dpi / 150);
  },

  /**
   * Convenience function for getting the scrollbox position off of a
   * scrollBoxObject interface.  Returns the actual values instead of the
   * wrapping objects.
   *
   * @param scroller a scrollBoxObject on which to call scroller.getPosition()
   */
  getScrollboxPosition: function getScrollboxPosition(scroller) {
    let x = {};
    let y = {};
    scroller.getPosition(x, y);
    return new Point(x.value, y.value);
  },

  forceChromeReflow: function forceChromeReflow() {
    let dummy = getComputedStyle(document.documentElement, "").width;
  },

  receiveMessage: function receiveMessage(aMessage) {
    let json = aMessage.json;
    let browser = aMessage.target;

    switch (aMessage.name) {
      case "MozScrolledAreaChanged": {
        let tab = this.getTabForBrowser(browser);
        if (tab)
          tab.scrolledAreaChanged();
        break;
      }
      case "Browser:ViewportMetadata": {
        let tab = this.getTabForBrowser(browser);
        // Some browser such as iframes loaded dynamically into the chrome UI
        // does not have any assigned tab
        if (tab)
          tab.updateViewportMetadata(json);
        break;
      }
      case "Browser:CanCaptureMouse:Return": {
        let tab = this.getTabForBrowser(browser);
        tab.contentMightCaptureMouse = json.contentMightCaptureMouse;
        break;
      }
      case "Browser:FormSubmit":
        browser.lastLocation = null;
        break;

      case "Browser:CanUnload:Return": {
        if (!json.permit)
          return;

        // Allow a little delay to not close the target tab while processing
        // a message for this particular tab
        setTimeout(function(self) {
          let tab = self.getTabForBrowser(browser);
          self._doCloseTab(tab);
        }, 0, this);
        break;
      }

      case "Browser:KeyPress":
        let event = document.createEvent("KeyEvents");
        event.initKeyEvent("keypress", true, true, null,
                           json.ctrlKey, json.altKey, json.shiftKey, json.metaKey,
                           json.keyCode, json.charCode);
        document.getElementById("mainKeyset").dispatchEvent(event);
        break;

      case "Browser:ZoomToPoint:Return":
        if (json.zoomTo) {
          let rect = Rect.fromRect(json.zoomTo);
          this.zoomToPoint(json.x, json.y, rect);
        } else {
          this.zoomFromPoint(json.x, json.y);
        }
        break;

      case "scroll":
        if (browser == this.selectedBrowser) {
          if (json.x != 0)
            this.hideSidebars();

          if (json.y != 0)
            this.hideTitlebar();
        }
        break;
      case "Browser:CertException":
        this._handleCertException(aMessage);
        break;
      case "Browser:BlockedSite":
        this._handleBlockedSite(aMessage);
        break;
      case "Browser:ErrorPage":
        this._handleErrorPage(aMessage);
        break;
    }
  }
};


Browser.MainDragger = function MainDragger() {
  this._horizontalScrollbar = document.getElementById("horizontal-scroller");
  this._verticalScrollbar = document.getElementById("vertical-scroller");
  this._scrollScales = { x: 0, y: 0 };

  Elements.browsers.addEventListener("PanBegin", this, false);
  Elements.browsers.addEventListener("PanFinished", this, false);

  // allow pages to to override panning, but should
  // still allow the sidebars to be panned out of view
  this.contentMouseCapture = false;
};

Browser.MainDragger.prototype = {
  isDraggable: function isDraggable(target, scroller) {
    return { x: true, y: true };
  },

  dragStart: function dragStart(clientX, clientY, target, scroller) {
    let browser = getBrowser();
    let bcr = browser.getBoundingClientRect();
    this._contentView = browser.getViewAt(clientX - bcr.left, clientY - bcr.top);
    this._stopAtSidebar = 0;
    this._panToolbars = !Util.isTablet();
    if (this._sidebarTimeout) {
      clearTimeout(this._sidebarTimeout);
      this._sidebarTimeout = null;
    }
  },

  dragStop: function dragStop(dx, dy, scroller) {
    if (this._contentView && this._contentView._updateCacheViewport)
      this._contentView._updateCacheViewport();
    this._contentView = null;
    this.dragMove(Browser.snapSidebars(), 0, scroller);
    Browser.tryUnfloatToolbar();
  },

  dragMove: function dragMove(dx, dy, scroller, aIsKinetic) {
    let doffset = new Point(dx, dy);
    let sidebarOffset = null;

    if (this._panToolbars) {
      // If the sidebars are showing, we pan them out of the way before panning the content.
      // The panning distance that should be used for the sidebars in is stored in sidebarOffset,
      // and subtracted from doffset.
      sidebarOffset = this._getSidebarOffset(doffset);

      // If we started with one sidebar open, stop when we get to the other.
      if (sidebarOffset.x != 0)
        this._blockSidebars(sidebarOffset);
    }

    if (!this.contentMouseCapture)
      this._panContent(doffset);

    if (this._panToolbars) {
      if (aIsKinetic && doffset.x != 0)
        return false;

      this._panChrome(doffset, sidebarOffset);
    }

    this._updateScrollbars();

    return !doffset.equals(dx, dy);
  },

  _blockSidebars: function md_blockSidebars(aSidebarOffset) {
    // only call this code once
    if (!this._stopAtSidebar) {
      this._stopAtSidebar = aSidebarOffset.x; // negative: stop at left; positive: stop at right

      // after a timeout, we allow showing the sidebar, to give the appearance of some "friction" at the edge
      this._sidebarTimeout = setTimeout(function(self) {
        self._stopAtSidebar = 0;
        self._sidebarTimeout = null;
      }, 350, this);
    }
  },

  handleEvent: function handleEvent(aEvent) {
    let browser = getBrowser();
    switch (aEvent.type) {
      case "PanBegin": {
        let width = ViewableAreaObserver.width, height = ViewableAreaObserver.height;
        let contentWidth = browser.contentDocumentWidth * browser.scale;
        let contentHeight = browser.contentDocumentHeight * browser.scale;

        // Allow a small margin on both sides to prevent adding scrollbars
        // on small viewport approximation
        const ALLOWED_MARGIN = 5;
        const SCROLL_CORNER_SIZE = 8;
        this._scrollScales = {
          x: (width + ALLOWED_MARGIN) < contentWidth ? (width - SCROLL_CORNER_SIZE) / contentWidth : 0,
          y: (height + ALLOWED_MARGIN) < contentHeight ? (height - SCROLL_CORNER_SIZE) / contentHeight : 0
        }
        this._showScrollbars();
        break;
      }
      case "PanFinished":
        this._hideScrollbars();

        // Update the scroll position of the content
        browser._updateCSSViewport();
        break;
    }
  },

  _panContent: function md_panContent(aOffset) {
    if (this._contentView && !this._contentView.isRoot()) {
      this._panContentView(this._contentView, aOffset);
      // XXX we may need to have "escape borders" for iframe panning
      // XXX does not deal with scrollables within scrollables
    }
    // Do content panning
    this._panContentView(getBrowser().getRootView(), aOffset);
  },

  _panChrome: function md_panChrome(aOffset, aSidebarOffset) {
    // In order to prevent users from hiding one sidebar and followed by immediately bringing
    // out the other one, we absorb sidebar pans here for a fixed time.
    //
    // Also, if users are panning a website then we allow them to pan away sidebars, but
    // nothing more.
    //
    let offsetX = aOffset.x;
    if (this.contentMouseCapture)
      aOffset.set(aSidebarOffset);
    else if ((this._stopAtSidebar > 0 && offsetX > 0) ||
             (this._stopAtSidebar < 0 && offsetX < 0))
      aOffset.x = aSidebarOffset.x;
    else
      aOffset.add(aSidebarOffset);

    Browser.tryFloatToolbar(aOffset.x, 0);

    // pan the sidebars
    this._panScroller(Browser.controlsScrollboxScroller, aOffset);
    // pan the urlbar
    this._panScroller(Browser.pageScrollboxScroller, aOffset);
  },

  /** Return offset that pans controls away from screen. Updates doffset with leftovers. */
  _getSidebarOffset: function(doffset) {
    let x = 0, y = 0, rect;

    rect = Rect.fromRect(Browser.pageScrollbox.getBoundingClientRect()).map(Math.round);
    if (doffset.x < 0 && rect.right < window.innerWidth)
      x = Math.max(doffset.x, rect.right - window.innerWidth);
    if (doffset.x > 0 && rect.left > 0)
      x = Math.min(doffset.x, rect.left);

    // XXX could we use getBrowser().getBoundingClientRect().height here?
    let height = Elements.contentViewport.getBoundingClientRect().height;
    height -= Elements.contentNavigator.getBoundingClientRect().height;

    rect = Rect.fromRect(Browser.contentScrollbox.getBoundingClientRect()).map(Math.round);
    if (doffset.y < 0 && rect.bottom < height)
      y = Math.max(doffset.y, rect.bottom - height);
    if (doffset.y > 0 && rect.top > 0)
      y = Math.min(doffset.y, rect.top);

    doffset.subtract(x, y);
    return new Point(x, y);
  },

  /** Pan scroller by the given amount. Updates doffset with leftovers. */
  _panContentView: function _panContentView(contentView, doffset) {
    let pos0 = contentView.getPosition();
    contentView.scrollBy(doffset.x, doffset.y);
    let pos1 = contentView.getPosition();
    doffset.subtract(pos1.x - pos0.x, pos1.y - pos0.y);
  },

  /** Pan scroller by the given amount. Updates doffset with leftovers. */
  _panScroller: function _panScroller(scroller, doffset) {
    let scroll = Browser.getScrollboxPosition(scroller);
    scroller.scrollBy(doffset.x, doffset.y);
    let scroll1 = Browser.getScrollboxPosition(scroller);
    doffset.subtract(scroll1.x - scroll.x, scroll1.y - scroll.y);
  },

  _updateScrollbars: function _updateScrollbars() {
    let scaleX = this._scrollScales.x, scaleY = this._scrollScales.y;
    let contentScroll = Browser.getScrollboxPosition(Browser.contentScrollboxScroller);
    if (scaleX)
      this._horizontalScrollbar.style.MozTransform = "translateX(" + Math.round(contentScroll.x * scaleX) + "px)";

    if (scaleY) {
      let y = Math.round(contentScroll.y * scaleY);

      // Vertical scrollbar is out of view when showing the tabs sidebar,
      // the 'solution' for now is to reposition it if needed
      let x = 0;
      if (Browser.floatedWhileDragging) {
        let [tabsVis, controlsVis, tabsW, controlsW] = Browser.computeSidebarVisibility();
        let [tabsSidebar, controlsSidebar] = [Elements.tabs.getBoundingClientRect(), Elements.controls.getBoundingClientRect()];

        // Check if the sidebars are inverted (rtl)
        let direction = -1 * Util.localeDir;
        x = Math.round(tabsW * tabsVis) * direction
      }

      this._verticalScrollbar.style.MozTransform = "translate(" + x + "px," + y + "px)";
    }
  },

  _showScrollbars: function _showScrollbars() {
    this._updateScrollbars();
    let scaleX = this._scrollScales.x, scaleY = this._scrollScales.y;
    if (scaleX) {
      this._horizontalScrollbar.width = ViewableAreaObserver.width * scaleX;
      this._horizontalScrollbar.setAttribute("panning", "true");
    }

    if (scaleY) {
      this._verticalScrollbar.height = ViewableAreaObserver.height * scaleY;
      this._verticalScrollbar.setAttribute("panning", "true");
    }
  },

  _hideScrollbars: function _hideScrollbars() {
    this._scrollScales.x = 0, this._scrollScales.y = 0;
    this._horizontalScrollbar.removeAttribute("panning");
    this._verticalScrollbar.removeAttribute("panning");
    this._horizontalScrollbar.style.MozTransform = "";
    this._verticalScrollbar.style.MozTransform = "";
  }
};


Browser.WebProgress = function WebProgress() {
  messageManager.addMessageListener("Content:StateChange", this);
  messageManager.addMessageListener("Content:LocationChange", this);
  messageManager.addMessageListener("Content:SecurityChange", this);
};

Browser.WebProgress.prototype = {
  receiveMessage: function receiveMessage(aMessage) {
    let json = aMessage.json;
    let tab = Browser.getTabForBrowser(aMessage.target);

    switch (aMessage.name) {
      case "Content:StateChange": {
        if (json.stateFlags & Ci.nsIWebProgressListener.STATE_IS_NETWORK) {
          if (json.stateFlags & Ci.nsIWebProgressListener.STATE_START)
            this._networkStart(tab);
          else if (json.stateFlags & Ci.nsIWebProgressListener.STATE_STOP)
            this._networkStop(tab);
        }

        if (json.stateFlags & Ci.nsIWebProgressListener.STATE_IS_DOCUMENT) {
          if (json.stateFlags & Ci.nsIWebProgressListener.STATE_STOP)
            this._documentStop(tab);
        }
        break;
      }

      case "Content:LocationChange": {
        let spec = json.location;
        let location = spec.split("#")[0]; // Ignore fragment identifier changes.

        if (tab == Browser.selectedTab)
          BrowserUI.updateURI();

        let locationHasChanged = (location != tab.browser.lastLocation);
        if (locationHasChanged) {
          Browser.getNotificationBox(tab.browser).removeTransientNotifications();
          tab.resetZoomLevel();
          tab.hostChanged = true;
          tab.browser.lastLocation = location;
          tab.browser.userTypedValue = "";
          tab.browser.appIcon = { href: null, size:-1 };

#ifdef MOZ_CRASH_REPORTER
          if (CrashReporter.enabled)
            CrashReporter.annotateCrashReport("URL", spec);
#endif
          this._waitForLoad(tab);
        }

        let event = document.createEvent("UIEvents");
        event.initUIEvent("URLChanged", true, false, window, locationHasChanged);
        tab.browser.dispatchEvent(event);
        break;
      }

      case "Content:SecurityChange": {
        // Don't need to do anything if the data we use to update the UI hasn't changed
        if (tab.state == json.state && !tab.hostChanged)
          return;

        tab.hostChanged = false;
        tab.state = json.state;

        if (tab == Browser.selectedTab)
          getIdentityHandler().checkIdentity();
        break;
      }
    }
  },

  _networkStart: function _networkStart(aTab) {
    aTab.startLoading();

    if (aTab == Browser.selectedTab) {
      BrowserUI.update(TOOLBARSTATE_LOADING);

      // We should at least show something in the URLBar until
      // the load has progressed further along
      if (aTab.browser.currentURI.spec == "about:blank")
        BrowserUI.updateURI({ captionOnly: true });
    }
  },

  _networkStop: function _networkStop(aTab) {
    aTab.endLoading();

    if (aTab == Browser.selectedTab)
      BrowserUI.update(TOOLBARSTATE_LOADED);
  },

  _documentStop: function _documentStop(aTab) {
    // Make sure the URLbar is in view. If this were the selected tab,
    // _waitForLoad would scroll to top.
    aTab.pageScrollOffset = { x: 0, y: 0 };
  },

  _waitForLoad: function _waitForLoad(aTab) {
    let browser = aTab.browser;

    aTab._firstPaint = false;

    browser.messageManager.addMessageListener("Browser:FirstPaint", function firstPaintListener(aMessage) {
      browser.messageManager.removeMessageListener(aMessage.name, arguments.callee);
      aTab._firstPaint = true;

      // We're about to have new page content, so scroll the content area
      // so the new paints will draw correctly.
      // Background tabs are delayed scrolled to top in _documentStop
      if (getBrowser() == browser) {
        let json = aMessage.json;
        browser.getRootView().scrollTo(Math.floor(json.x * browser.scale),
                                       Math.floor(json.y * browser.scale));
        if (json.x == 0 && json.y == 0)
          Browser.pageScrollboxScroller.scrollTo(0, 0);
        else
          Browser.pageScrollboxScroller.scrollTo(0, Number.MAX_VALUE);
      }

      aTab.scrolledAreaChanged(true);
      aTab.updateThumbnail();

      aTab.updateContentCapture();
    });
  }
};


const OPEN_APPTAB = 100; // Hack until we get a real API

function nsBrowserAccess() { }

nsBrowserAccess.prototype = {
  QueryInterface: function(aIID) {
    if (aIID.equals(Ci.nsIBrowserDOMWindow) || aIID.equals(Ci.nsISupports))
      return this;
    throw Cr.NS_NOINTERFACE;
  },

  _getBrowser: function _getBrowser(aURI, aOpener, aWhere, aContext) {
    let isExternal = (aContext == Ci.nsIBrowserDOMWindow.OPEN_EXTERNAL);
    if (isExternal && aURI && aURI.schemeIs("chrome"))
      return null;

    let loadflags = isExternal ?
                      Ci.nsIWebNavigation.LOAD_FLAGS_FROM_EXTERNAL :
                      Ci.nsIWebNavigation.LOAD_FLAGS_NONE;
    let location;
    if (aWhere == Ci.nsIBrowserDOMWindow.OPEN_DEFAULTWINDOW) {
      switch (aContext) {
        case Ci.nsIBrowserDOMWindow.OPEN_EXTERNAL :
          aWhere = Services.prefs.getIntPref("browser.link.open_external");
          break;
        default : // OPEN_NEW or an illegal value
          aWhere = Services.prefs.getIntPref("browser.link.open_newwindow");
      }
    }

    let browser;
    if (aWhere == Ci.nsIBrowserDOMWindow.OPEN_NEWWINDOW) {
      let url = aURI ? aURI.spec : "about:blank";
      let newWindow = openDialog("chrome://browser/content/browser.xul", "_blank",
                                 "all,dialog=no", url, null, null, null);
      // since newWindow.Browser doesn't exist yet, just return null
      return null;
    } else if (aWhere == Ci.nsIBrowserDOMWindow.OPEN_NEWTAB) {
      let owner = isExternal ? null : Browser.selectedTab;
      let tab = Browser.addTab("about:blank", true, owner, { getAttention: true });
      if (isExternal)
        tab.closeOnExit = true;
      browser = tab.browser;
    } else if (aWhere == OPEN_APPTAB) {
      Browser.tabs.forEach(function(aTab) {
        if ("appURI" in aTab.browser && aTab.browser.appURI.spec == aURI.spec) {
          Browser.selectedTab = aTab;
          browser = aTab.browser;
        }
      });

      if (!browser) {
        // Make a new tab to hold the app
        let tab = Browser.addTab("about:blank", true, null, { getAttention: true });
        browser = tab.browser;
        browser.appURI = aURI;
      } else {
        // Just use the existing browser, but return null to keep the system from trying to load the URI again
        browser = null;
      }
    } else { // OPEN_CURRENTWINDOW and illegal values
      browser = Browser.selectedBrowser;
    }

    try {
      let referrer;
      if (aURI && browser) {
        if (aOpener) {
          location = aOpener.location;
          referrer = Services.io.newURI(location, null, null);
        }
        browser.loadURIWithFlags(aURI.spec, loadflags, referrer, null, null);
      }
      browser.focus();
    } catch(e) { }

    // We are loading web content into this window, so make sure content is visible
    BrowserUI.hidePanel();
    BrowserUI.closeAutoComplete();
    Browser.hideSidebars();
    return browser;
  },

  openURI: function browser_openURI(aURI, aOpener, aWhere, aContext) {
    let browser = this._getBrowser(aURI, aOpener, aWhere, aContext);
    return browser ? browser.contentWindow : null;
  },

  openURIInFrame: function browser_openURIInFrame(aURI, aOpener, aWhere, aContext) {
    let browser = this._getBrowser(aURI, aOpener, aWhere, aContext);
    return browser ? browser.QueryInterface(Ci.nsIFrameLoaderOwner) : null;
  },

  isTabContentWindow: function(aWindow) {
    return Browser.browsers.some(function (browser) browser.contentWindow == aWindow);
  }
};


/** Watches for mouse events in chrome and sends them to content. */
const ContentTouchHandler = {
  // Use lightweight transactions so that old context menus and tap
  // highlights don't ever see the light of day.
  _messageId: 0,

  init: function init() {
    document.addEventListener("TapDown", this, true);
    document.addEventListener("TapOver", this, false);
    document.addEventListener("TapUp", this, false);
    document.addEventListener("TapSingle", this, false);
    document.addEventListener("TapDouble", this, false);
    document.addEventListener("TapLong", this, false);
    document.addEventListener("TapMove", this, false);

    document.addEventListener("PanBegin", this, false);
    document.addEventListener("PopupChanged", this, false);
    document.addEventListener("CancelTouchSequence", this, false);

    // Context menus have the following flow:
    //   [parent] mousedown -> TapLong -> Browser:MouseLong
    //   [child]  Browser:MouseLong -> contextmenu -> Browser:ContextMenu
    //   [parent] Browser:ContextMenu -> ...*
    //
    // * = Here some time will elapse. Although we get the context menu we need
    //     ASAP, we do not act on the context menu until we receive a LongTap.
    //     This is so we can show the context menu as soon as we know it is
    //     a long tap, without waiting for child process.
    //
    messageManager.addMessageListener("Browser:ContextMenu", this);
    messageManager.addMessageListener("Browser:Highlight", this);
    messageManager.addMessageListener("Browser:CaptureEvents", this);
  },

  handleEvent: function handleEvent(aEvent) {
    // ignore content events we generate
    if (aEvent.target.localName == "browser")
      return;

    switch (aEvent.type) {
      case "PanBegin":
        getBrowser().messageManager.sendAsyncMessage("Browser:MouseCancel", {});
        break;
      case "PopupChanged":
      case "CancelTouchSequence":
        this._clearPendingMessages();
        break;

      default: {
        if (ContextHelper.popupState) {
          // Don't send content events when there's a popup
          aEvent.preventDefault();
          break;
        }

        if (!this._targetIsContent(aEvent)) {
          // Received tap event on something that isn't for content.
          this._clearPendingMessages();
          break;
        }

        switch (aEvent.type) {
          case "TapDown":
            this.tapDown(aEvent.clientX, aEvent.clientY);
            break;
          case "TapOver":
            this.tapOver(aEvent.clientX, aEvent.clientY);
            break;
          case "TapUp":
            if (aEvent.isClick) {
              if (!Browser.selectedTab.allowZoom) {
                this.tapSingle(aEvent.clientX, aEvent.clientY, Util.modifierMaskFromEvent(aEvent));
                aEvent.preventDefault();
              }
            }
            this._dispatchMouseEvent("Browser:MouseUp", aEvent.clientX, aEvent.clientY);
            break;
          case "TapSingle":
            this.tapSingle(aEvent.clientX, aEvent.clientY, aEvent.modifiers);
            this._dispatchMouseEvent("Browser:MouseUp", aEvent.clientX, aEvent.clientY);
            break;
          case "TapDouble":
            this.tapDouble(aEvent.clientX, aEvent.clientY, aEvent.modifiers);
            break;
          case "TapLong":
            this.tapLong(aEvent.clientX, aEvent.clientY);
            break;
          case "TapMove":
            this.tapMove(aEvent.clientX, aEvent.clientY);
            break;
        }
      }
    }
  },

  receiveMessage: function receiveMessage(aMessage) {
    let json = aMessage.json;
    if (json.messageId != this._messageId)
      return;

    switch (aMessage.name) {
      case "Browser:ContextMenu":
        // Long tap
        let contextMenu = { name: aMessage.name, json: json, target: aMessage.target };
        if (ContextHelper.showPopup(contextMenu)) {
          // Stop all input sequences
          let event = document.createEvent("Events");
          event.initEvent("CancelTouchSequence", true, false);
          document.dispatchEvent(event);
        } else {
          SelectionHelper.showPopup(contextMenu);
        }
        break;
      case "Browser:CaptureEvents": {
        let tab = Browser.getTabForBrowser(aMessage.target);
        tab.contentMightCaptureMouse = json.contentMightCaptureMouse;
        if (this.touchTimeout) {
          clearTimeout(this.touchTimeout);
          this.touchTimeout = null;
        }

        if (json.click)
          this.clickPrevented = true;
        if (json.panning)
          this.panningPrevented = true;

        // We don't know if panning is allowed until the first touchmove event is processed.
        if (this.canCancelPan && json.type == "touchmove")
          Elements.browsers.customDragger.contentMouseCapture = this.panningPrevented;
        break;
      }
    }
  },

  /** Invalidates any messages received from content that are sensitive to time. */
  _clearPendingMessages: function _clearPendingMessages() {
    this._messageId++;
    let browser = getBrowser();
    browser.messageManager.sendAsyncMessage("Browser:MouseCancel", {});
  },

  /**
   * Check if the event concern the browser content
   */
  _targetIsContent: function _targetIsContent(aEvent) {
    // TapUp event with XULDocument as a target occurs on desktop when the
    // mouse is released outside of the Fennec window, and because XULDocument
    // does not have a classList properties, just check it exists first to
    // prevent a warning
    let target = aEvent.target;
    return target && ("classList" in target && target.classList.contains("inputHandler"));
  },

  _dispatchMouseEvent: function _dispatchMouseEvent(aName, aX, aY, aOptions) {
    let browser = getBrowser();
    let pos = browser.transformClientToBrowser(aX || 0, aY || 0);

    let json = aOptions || {};
    json.x = pos.x;
    json.y = pos.y;
    json.messageId = this._messageId;
    browser.messageManager.sendAsyncMessage(aName, json);
  },

  touchTimeout: null,
  canCancelPan: false,
  clickPrevented: false,
  panningPrevented: false,

  updateCanCancel: function(aX, aY) {
    let dpi = Util.displayDPI;

    const kSafetyX = Services.prefs.getIntPref("dom.w3c_touch_events.safetyX") / 240 * dpi;
    const kSafetyY = Services.prefs.getIntPref("dom.w3c_touch_events.safetyY") / 240 * dpi;
    let browser = getBrowser();
    let bcr = browser.getBoundingClientRect();
    let rect = new Rect(0, 0, window.innerWidth, window.innerHeight);
    rect.restrictTo(Rect.fromRect(bcr));

    // Check if the user touched near to one of the edges of the browser area
    // or if the urlbar is showing
    this.canCancelPan = (aX >= rect.left + kSafetyX) && (aX <= rect.right - kSafetyX) &&
                        (aY >= rect.top  + kSafetyY) &&
                        (bcr.top == 0 || Util.isTablet());
  },

  tapDown: function tapDown(aX, aY) {
    let browser = getBrowser();
    browser.focus();

    // if the page might capture touch events, we give it the option
    this.updateCanCancel(aX, aY);
    this.clickPrevented = false;
    this.panningPrevented = false;

    let dragger = Elements.browsers.customDragger;
    dragger.contentMouseCapture = this.canCancelPan && Browser.selectedTab.contentMightCaptureMouse;
    if (this.touchTimeout) {
      clearTimeout(this.touchTimeout);
      this.touchTimeout = null;
    }

    if (dragger.contentMouseCapture)
      this.touchTimeout = setTimeout(function() dragger.contentMouseCapture = false, kTouchTimeout);

    this._dispatchMouseEvent("Browser:MouseDown", aX, aY);
  },

  tapOver: function tapOver(aX, aY) {
    this._dispatchMouseEvent("Browser:MouseOver", aX, aY);
  },

  tapUp: function tapUp(aX, aY) {
    let browser = getBrowser();
    browser.messageManager.sendAsyncMessage("Browser:MouseCancel", {});
  },

  tapSingle: function tapSingle(aX, aY, aModifiers) {
    // Cancel the mouse click if we are showing a context menu
    if (!ContextHelper.popupState && !this.clickPrevented)
      this._dispatchMouseEvent("Browser:MouseClick", aX, aY, { modifiers: aModifiers });
  },

  tapMove: function tapMove(aX, aY) {
    if (Browser.selectedTab.contentMightCaptureMouse)
      this._dispatchMouseEvent("Browser:MouseMove", aX, aY);
  },

  tapDouble: function tapDouble(aX, aY, aModifiers) {
    this._clearPendingMessages();

    let tab = Browser.selectedTab;
    if (!tab.allowZoom)
      return;

    let width = ViewableAreaObserver.width / Browser.getScaleRatio();
    this._dispatchMouseEvent("Browser:ZoomToPoint", aX, aY, { width: width });
  },

  tapLong: function tapLong(aX, aY) {
    this._dispatchMouseEvent("Browser:MouseLong", aX, aY);
  },

  toString: function toString() {
    return "[ContentTouchHandler] { }";
  }
};


/** Watches for mouse events in chrome and sends them to content. */
function ContentCustomKeySender(container) {
  container.addEventListener("keypress", this, false);
  container.addEventListener("keyup", this, false);
  container.addEventListener("keydown", this, false);
}

ContentCustomKeySender.prototype = {
  handleEvent: function handleEvent(aEvent) {
    if (Elements.contentShowing.getAttribute("disabled") == "true")
      return;

    let browser = getBrowser();
    if (browser && browser.active && browser.getAttribute("remote") == "true") {
      aEvent.stopPropagation();
      aEvent.preventDefault();

      let fl = browser.QueryInterface(Ci.nsIFrameLoaderOwner).frameLoader;
      fl.sendCrossProcessKeyEvent(aEvent.type,
                                  aEvent.keyCode,
                                  (aEvent.type != "keydown") ? aEvent.charCode : null,
                                  this._parseModifiers(aEvent));
    }
  },

  _parseModifiers: function _parseModifiers(aEvent) {
    const masks = Ci.nsIDOMNSEvent;
    let mval = 0;
    if (aEvent.shiftKey)
      mval |= masks.SHIFT_MASK;
    if (aEvent.ctrlKey)
      mval |= masks.CONTROL_MASK;
    if (aEvent.altKey)
      mval |= masks.ALT_MASK;
    if (aEvent.metaKey)
      mval |= masks.META_MASK;
    return mval;
  },

  toString: function toString() {
    return "[ContentCustomKeySender] { }";
  }
};


/**
 * Utility class to handle manipulations of the identity indicators in the UI
 */
function IdentityHandler() {
  this._staticStrings = {};
  this._staticStrings[this.IDENTITY_MODE_DOMAIN_VERIFIED] = {
    encryption_label: Strings.browser.GetStringFromName("identity.encrypted2")
  };
  this._staticStrings[this.IDENTITY_MODE_IDENTIFIED] = {
    encryption_label: Strings.browser.GetStringFromName("identity.encrypted2")
  };
  this._staticStrings[this.IDENTITY_MODE_UNKNOWN] = {
    encryption_label: Strings.browser.GetStringFromName("identity.unencrypted2")
  };

  // Close the popup when reloading the page
  Elements.browsers.addEventListener("URLChanged", this, true);

  this._cacheElements();
}

IdentityHandler.prototype = {
  // Mode strings used to control CSS display
  IDENTITY_MODE_IDENTIFIED       : "verifiedIdentity", // High-quality identity information
  IDENTITY_MODE_DOMAIN_VERIFIED  : "verifiedDomain",   // Minimal SSL CA-signed domain verification
  IDENTITY_MODE_UNKNOWN          : "unknownIdentity",  // No trusted identity information

  // Cache the most recent SSLStatus and Location seen in checkIdentity
  _lastStatus : null,
  _lastLocation : null,

  /**
   * Build out a cache of the elements that we need frequently.
   */
  _cacheElements: function() {
    this._identityBox = document.getElementById("identity-box");
    this._identityPopup = document.getElementById("identity-container");
    this._identityPopupContentBox = document.getElementById("identity-popup-content-box");
    this._identityPopupContentHost = document.getElementById("identity-popup-content-host");
    this._identityPopupContentOwner = document.getElementById("identity-popup-content-owner");
    this._identityPopupContentSupp = document.getElementById("identity-popup-content-supplemental");
    this._identityPopupContentVerif = document.getElementById("identity-popup-content-verifier");
    this._identityPopupEncLabel = document.getElementById("identity-popup-encryption-label");
  },

  getIdentityData: function() {
    return this._lastStatus.serverCert;
  },

  /**
   * Determine the identity of the page being displayed by examining its SSL cert
   * (if available) and, if necessary, update the UI to reflect this.
   */
  checkIdentity: function() {
    let browser = getBrowser();
    let state = browser.securityUI.state;
    let location = browser.currentURI;
    let currentStatus = browser.securityUI.SSLStatus;

    this._lastStatus = currentStatus;
    this._lastLocation = {};

    try {
      // make a copy of the passed in location to avoid cycles
      this._lastLocation = { host: location.hostPort, hostname: location.host, port: location.port == -1 ? "" : location.port};
    } catch(e) { }

    if (state & Ci.nsIWebProgressListener.STATE_IDENTITY_EV_TOPLEVEL)
      this.setMode(this.IDENTITY_MODE_IDENTIFIED);
    else if (state & Ci.nsIWebProgressListener.STATE_SECURE_HIGH)
      this.setMode(this.IDENTITY_MODE_DOMAIN_VERIFIED);
    else
      this.setMode(this.IDENTITY_MODE_UNKNOWN);
  },

  /**
   * Return the eTLD+1 version of the current hostname
   */
  getEffectiveHost: function() {
    // Cache the eTLDService if this is our first time through
    if (!this._eTLDService)
      this._eTLDService = Cc["@mozilla.org/network/effective-tld-service;1"]
                         .getService(Ci.nsIEffectiveTLDService);
    try {
      return this._eTLDService.getBaseDomainFromHost(this._lastLocation.hostname);
    } catch (e) {
      // If something goes wrong (e.g. hostname is an IP address) just fail back
      // to the full domain.
      return this._lastLocation.hostname;
    }
  },

  /**
   * Update the UI to reflect the specified mode, which should be one of the
   * IDENTITY_MODE_* constants.
   */
  setMode: function(newMode) {
    this._identityBox.setAttribute("mode", newMode);
    this.setIdentityMessages(newMode);

    // Update the popup too, if it's open
    if (!this._identityPopup.hidden)
      this.setPopupMessages(newMode);
  },

  /**
   * Set up the messages for the primary identity UI based on the specified mode,
   * and the details of the SSL cert, where applicable
   *
   * @param newMode The newly set identity mode.  Should be one of the IDENTITY_MODE_* constants.
   */
  setIdentityMessages: function(newMode) {
    let strings = Strings.browser;

    if (newMode == this.IDENTITY_MODE_DOMAIN_VERIFIED) {
      var iData = this.getIdentityData();

      // We need a port number for all lookups.  If one hasn't been specified, use
      // the https default
      var lookupHost = this._lastLocation.host;
      if (lookupHost.indexOf(':') < 0)
        lookupHost += ":443";

      // Cache the override service the first time we need to check it
      if (!this._overrideService)
        this._overrideService = Cc["@mozilla.org/security/certoverride;1"].getService(Ci.nsICertOverrideService);

      // Verifier is either the CA Org, for a normal cert, or a special string
      // for certs that are trusted because of a security exception.
      var tooltip = strings.formatStringFromName("identity.identified.verifier", [iData.caOrg], 1);

      // Check whether this site is a security exception.
      if (iData.isException)
        tooltip = strings.GetStringFromName("identity.identified.verified_by_you");
    }
    else if (newMode == this.IDENTITY_MODE_IDENTIFIED) {
      // If it's identified, then we can populate the dialog with credentials
      iData = this.getIdentityData();
      tooltip = strings.formatStringFromName("identity.identified.verifier", [iData.caOrg], 1);
    }
    else {
      tooltip = strings.GetStringFromName("identity.unknown.tooltip");
    }

    // Push the appropriate strings out to the UI
    this._identityBox.tooltipText = tooltip;
  },

  /**
   * Set up the title and content messages for the identity message popup,
   * based on the specified mode, and the details of the SSL cert, where
   * applicable
   *
   * @param newMode The newly set identity mode.  Should be one of the IDENTITY_MODE_* constants.
   */
  setPopupMessages: function(newMode) {
    this._identityPopup.setAttribute("mode", newMode);
    this._identityPopupContentBox.className = newMode;

    // Set the static strings up front
    this._identityPopupEncLabel.textContent = this._staticStrings[newMode].encryption_label;

    // Initialize the optional strings to empty values
    var supplemental = "";
    var verifier = "";

    if (newMode == this.IDENTITY_MODE_DOMAIN_VERIFIED) {
      var iData = this.getIdentityData();
      var host = this.getEffectiveHost();
      var owner = Strings.browser.GetStringFromName("identity.ownerUnknown2");
      verifier = this._identityBox.tooltipText;
      supplemental = "";
    }
    else if (newMode == this.IDENTITY_MODE_IDENTIFIED) {
      // If it's identified, then we can populate the dialog with credentials
      iData = this.getIdentityData();
      host = this.getEffectiveHost();
      owner = iData.subjectOrg;
      verifier = this._identityBox.tooltipText;

      // Build an appropriate supplemental block out of whatever location data we have
      if (iData.city)
        supplemental += iData.city + "\n";
      if (iData.state && iData.country)
        supplemental += Strings.browser.formatStringFromName("identity.identified.state_and_country", [iData.state, iData.country], 2);
      else if (iData.state) // State only
        supplemental += iData.state;
      else if (iData.country) // Country only
        supplemental += iData.country;
    } else {
      // These strings will be hidden in CSS anyhow
      host = "";
      owner = "";
    }

    // Push the appropriate strings out to the UI
    this._identityPopupContentHost.textContent = host;
    this._identityPopupContentOwner.textContent = owner;
    this._identityPopupContentSupp.textContent = supplemental;
    this._identityPopupContentVerif.textContent = verifier;

    PageActions.updateSiteMenu();
  },

  show: function ih_show() {
    Elements.contentShowing.setAttribute("disabled", "true");

    // dismiss any dialog which hide the identity popup
    AwesomeScreen.activePanel = null;
    while (BrowserUI.activeDialog)
      BrowserUI.activeDialog.close();

    // Update the popup strings
    this.setPopupMessages(this._identityBox.getAttribute("mode") || this.IDENTITY_MODE_UNKNOWN);

    this._identityPopup.hidden = false;
    let anchorPos = "";
    if (Util.isTablet())
      anchorPos = "after_start";
    else
      this._identityPopup.top = BrowserUI.toolbarH - this._identityPopup.offset;

    this._identityBox.setAttribute("open", "true");

    BrowserUI.pushPopup(this, [this._identityPopup, this._identityBox, Elements.toolbarContainer]);
    BrowserUI.lockToolbar();
    this._identityPopup.anchorTo(this._identityBox, anchorPos);
  },

  hide: function ih_hide() {
    Elements.contentShowing.setAttribute("disabled", "false");

    this._identityPopup.hidden = true;
    this._identityBox.removeAttribute("open");

    BrowserUI.popPopup(this);
    BrowserUI.unlockToolbar();
  },

  toggle: function ih_toggle() {
    if (this._identityPopup.hidden)
      this.show();
    else
      this.hide();
  },

  /**
   * Click handler for the identity-box element in primary chrome.
   */
  handleIdentityButtonEvent: function(aEvent) {
    let broadcaster = document.getElementById("bcast_uidiscovery");
    if (broadcaster && broadcaster.getAttribute("mode") == "discovery")
      return;

    aEvent.stopPropagation();

    if ((aEvent.type == "click" && aEvent.button != 0) ||
        (aEvent.type == "keypress" && aEvent.charCode != KeyEvent.DOM_VK_SPACE &&
         aEvent.keyCode != KeyEvent.DOM_VK_RETURN))
      return; // Left click, space or enter only

    this.toggle();
  },

  handleEvent: function(aEvent) {
    if (aEvent.type == "URLChanged" && aEvent.target == Browser.selectedBrowser && !this._identityPopup.hidden)
      this.hide();
  }
};

var gIdentityHandler;

/**
 * Returns the singleton instance of the identity handler class.  Should always be
 * used instead of referencing the global variable directly or creating new instances
 */
function getIdentityHandler() {
  if (!gIdentityHandler)
    gIdentityHandler = new IdentityHandler();
  return gIdentityHandler;
}


/**
 * Handler for blocked popups, triggered by DOMUpdatePageReport events in browser.xml
 */
var PopupBlockerObserver = {
  onUpdatePageReport: function onUpdatePageReport(aEvent)
  {
    var cBrowser = Browser.selectedBrowser;
    if (aEvent.originalTarget != cBrowser)
      return;

    if (!cBrowser.pageReport)
      return;

    let result = Services.perms.testExactPermission(Browser.selectedBrowser.currentURI, "popup");
    if (result == Ci.nsIPermissionManager.DENY_ACTION)
      return;

    // Only show the notification again if we've not already shown it. Since
    // notifications are per-browser, we don't need to worry about re-adding
    // it.
    if (!cBrowser.pageReport.reported) {
      if (Services.prefs.getBoolPref("privacy.popups.showBrowserMessage")) {
        var brandShortName = Strings.brand.GetStringFromName("brandShortName");
        var message;
        var popupCount = cBrowser.pageReport.length;

        let strings = Strings.browser;
        if (popupCount > 1)
          message = strings.formatStringFromName("popupWarningMultiple", [brandShortName, popupCount], 2);
        else
          message = strings.formatStringFromName("popupWarning", [brandShortName], 1);

        var notificationBox = Browser.getNotificationBox();
        var notification = notificationBox.getNotificationWithValue("popup-blocked");
        if (notification) {
          notification.label = message;
        }
        else {
          var buttons = [
            {
              label: strings.GetStringFromName("popupButtonAllowOnce"),
              accessKey: null,
              callback: function() { PopupBlockerObserver.showPopupsForSite(); }
            },
            {
              label: strings.GetStringFromName("popupButtonAlwaysAllow2"),
              accessKey: null,
              callback: function() { PopupBlockerObserver.allowPopupsForSite(true); }
            },
            {
              label: strings.GetStringFromName("popupButtonNeverWarn2"),
              accessKey: null,
              callback: function() { PopupBlockerObserver.allowPopupsForSite(false); }
            }
          ];

          const priority = notificationBox.PRIORITY_WARNING_MEDIUM;
          notificationBox.appendNotification(message, "popup-blocked",
                                             "",
                                             priority, buttons);
        }
      }
      // Record the fact that we've reported this blocked popup, so we don't
      // show it again.
      cBrowser.pageReport.reported = true;
    }
  },

  allowPopupsForSite: function allowPopupsForSite(aAllow) {
    var currentURI = Browser.selectedBrowser.currentURI;
    Services.perms.add(currentURI, "popup", aAllow
                       ?  Ci.nsIPermissionManager.ALLOW_ACTION
                       :  Ci.nsIPermissionManager.DENY_ACTION);

    Browser.getNotificationBox().removeCurrentNotification();
  },

  showPopupsForSite: function showPopupsForSite() {
    let uri = Browser.selectedBrowser.currentURI;
    let pageReport = Browser.selectedBrowser.pageReport;
    if (pageReport) {
      for (let i = 0; i < pageReport.length; ++i) {
        var popupURIspec = pageReport[i].popupWindowURI.spec;

        // Sometimes the popup URI that we get back from the pageReport
        // isn't useful (for instance, netscape.com's popup URI ends up
        // being "http://www.netscape.com", which isn't really the URI of
        // the popup they're trying to show).  This isn't going to be
        // useful to the user, so we won't create a menu item for it.
        if (popupURIspec == "" || popupURIspec == "about:blank" || popupURIspec == uri.spec)
          continue;

        let popupFeatures = pageReport[i].popupWindowFeatures;
        let popupName = pageReport[i].popupWindowName;

        Browser.addTab(popupURIspec, false, Browser.selectedTab);
      }
    }
  }
};

var XPInstallObserver = {
  observe: function xpi_observer(aSubject, aTopic, aData)
  {
    switch (aTopic) {
      case "addon-install-started":
        var messageString = Strings.browser.GetStringFromName("alertAddonsDownloading");
        ExtensionsView.showAlert(messageString);
        break;
      case "addon-install-blocked":
        var installInfo = aSubject.QueryInterface(Ci.amIWebInstallInfo);
        var host = installInfo.originatingURI.host;
        var brandShortName = Strings.brand.GetStringFromName("brandShortName");
        var notificationName, messageString, buttons;
        var strings = Strings.browser;
        var enabled = true;
        try {
          enabled = Services.prefs.getBoolPref("xpinstall.enabled");
        }
        catch (e) {}
        if (!enabled) {
          notificationName = "xpinstall-disabled";
          if (Services.prefs.prefIsLocked("xpinstall.enabled")) {
            messageString = strings.GetStringFromName("xpinstallDisabledMessageLocked");
            buttons = [];
          }
          else {
            messageString = strings.formatStringFromName("xpinstallDisabledMessage2", [brandShortName, host], 2);
            buttons = [{
              label: strings.GetStringFromName("xpinstallDisabledButton"),
              accessKey: null,
              popup: null,
              callback: function editPrefs() {
                Services.prefs.setBoolPref("xpinstall.enabled", true);
                return false;
              }
            }];
          }
        }
        else {
          notificationName = "xpinstall";
          messageString = strings.formatStringFromName("xpinstallPromptWarning2", [brandShortName, host], 2);

          buttons = [{
            label: strings.GetStringFromName("xpinstallPromptAllowButton"),
            accessKey: null,
            popup: null,
            callback: function() {
              // Kick off the install
              installInfo.install();
              return false;
            }
          }];
        }

        var nBox = Browser.getNotificationBox();
        if (!nBox.getNotificationWithValue(notificationName)) {
          const priority = nBox.PRIORITY_WARNING_MEDIUM;
          const iconURL = "chrome://mozapps/skin/update/update.png";
          nBox.appendNotification(messageString, notificationName, iconURL, priority, buttons);
        }
        break;
    }
  }
};

var SessionHistoryObserver = {
  observe: function sho_observe(aSubject, aTopic, aData) {
    if (aTopic != "browser:purge-session-history")
      return;

    let back = document.getElementById("cmd_back");
    back.setAttribute("disabled", "true");
    let forward = document.getElementById("cmd_forward");
    forward.setAttribute("disabled", "true");

    let urlbar = document.getElementById("urlbar-edit");
    if (urlbar) {
      // Clear undo history of the URL bar
      urlbar.editor.transactionManager.clear();
    }
  }
};

var ContentCrashObserver = {
  get CrashSubmit() {
    delete this.CrashSubmit;
    Cu.import("resource://gre/modules/CrashSubmit.jsm", this);
    return this.CrashSubmit;
  },

  observe: function cco_observe(aSubject, aTopic, aData) {
    if (aTopic != "ipc:content-shutdown") {
      Cu.reportError("ContentCrashObserver unexpected topic: " + aTopic);
      return;
    }

    if (!aSubject.QueryInterface(Ci.nsIPropertyBag2).hasKey("abnormal"))
      return;

    // See if we should hide the UI or auto close the app based on env vars
    let env = Cc["@mozilla.org/process/environment;1"].getService(Ci.nsIEnvironment);
    let shutdown = env.get("MOZ_CRASHREPORTER_SHUTDOWN");
    if (shutdown) {
      let appStartup = Cc["@mozilla.org/toolkit/app-startup;1"].getService(Ci.nsIAppStartup);
      appStartup.quit(Ci.nsIAppStartup.eForceQuit);
      return;
    }

    let hideUI = env.get("MOZ_CRASHREPORTER_NO_REPORT");
    if (hideUI)
      return;

    // Spin through the open tabs and resurrect the out-of-process tabs. Resurrection
    // does not auto-reload the content. We delay load the content as needed.
    Browser.tabs.forEach(function(aTab) {
      if (aTab.browser.getAttribute("remote") == "true")
        aTab.resurrect();
    });

    let dumpID = aSubject.hasKey("dumpID") ? aSubject.getProperty("dumpID") : null;
    let crashedURL = Browser.selectedTab.browser.__SS_data.entries[0].url;

    // Execute the UI prompt after the notification has had a chance to return and close the child process
    setTimeout(function(self) {
      // Ask the user if we should reload or close the current tab. Other tabs
      // will be reloaded when selected.
      let title = Strings.browser.GetStringFromName("tabs.crashWarningTitle");
      let message = Strings.browser.GetStringFromName("tabs.crashWarningMsg");
      let submitText = Strings.browser.GetStringFromName("tabs.crashSubmitReport");
      let reloadText = Strings.browser.GetStringFromName("tabs.crashReload");
      let closeText = Strings.browser.GetStringFromName("tabs.crashClose");
      let buttons = Ci.nsIPrompt.BUTTON_POS_1_DEFAULT +
                    (Ci.nsIPrompt.BUTTON_TITLE_IS_STRING * Ci.nsIPrompt.BUTTON_POS_0) +
                    (Ci.nsIPrompt.BUTTON_TITLE_IS_STRING * Ci.nsIPrompt.BUTTON_POS_1);

      // Only show the submit checkbox if we have a crash report we can submit
      if (!dumpID)
        submitText = null;

      let submit = { value: true };
      let reload = Services.prompt.confirmEx(window, title, message, buttons, closeText, reloadText, null, submitText, submit);
      if (reload) {
        // Fire a TabSelect event to kick start the restore process
        let event = document.createEvent("Events");
        event.initEvent("TabSelect", true, false);
        event.lastTab = null;
        Browser.selectedTab.chromeTab.dispatchEvent(event);
      } else {
        // If this is the only tab, we need to pre-fab a new tab. We should never
        // have zero open tabs
        if (Browser.tabs.length == 1) {
          // Get the start page from the *default* pref branch, not the user's
          let fallbackURL = Browser.getHomePage({ useDefault: true });
          Browser.addTab(fallbackURL, false, null, { getAttention: false });
        }

        // Close this tab, it could be the reason we crashed. The undo-close-tab
        // system will pick it up.
        Browser.closeTab(Browser.selectedTab, { forceClose: true });
      }

      // Submit the report, if we have one and the user wants to submit it
      if (submit.value && dumpID) {
        let directoryService = Cc["@mozilla.org/file/directory_service;1"].getService(Ci.nsIProperties);
        let extra = directoryService.get("UAppData", Ci.nsIFile);
        extra.append("Crash Reports");
        extra.append("pending");
        extra.append(dumpID + ".extra");
        let foStream = Cc["@mozilla.org/network/file-output-stream;1"].createInstance(Ci.nsIFileOutputStream);
        try {
          // use 0x02 | 0x10 to open file for appending.
          foStream.init(extra, 0x02 |  0x10, 0666, 0); 
          let data = "URL=" + crashedURL + "\n";
          foStream.write(data, data.length);
          foStream.close();
        } catch (x) {
          dump (x);
        }
        self.CrashSubmit.submit(dumpID);
      }
    }, 0, this);
  }
};

var MemoryObserver = {
  observe: function mo_observe(aSubject, aTopic, aData) {
    function gc() {
      window.QueryInterface(Ci.nsIInterfaceRequestor)
            .getInterface(Ci.nsIDOMWindowUtils).garbageCollect();
      Cu.forceGC();
    };

    if (aData == "heap-minimize") {
      // The JS engine would normally GC on this notification, but since we
      // disabled that in favor of this method (bug 669346), we should gc here.
      gc();
      return;
    }

    for (let i = Browser.tabs.length - 1; i >= 0; i--) {
      let tab = Browser.tabs[i];
      if (tab == Browser.selectedTab)
        continue;
      tab.resurrect();
    }

    // gc *after* throwing out the tabs so we can reclaim that memory.
    gc();

    // Bug 637582 - The low memory condition throws out some stuff that we still
    // need, re-selecting the active tab gets us back to where we need to be.
    let sTab = Browser.selectedTab;
    Browser._selectedTab = null;
    Browser.selectedTab = sTab;
  }
};

var ActivityObserver = {
  _inBackground : false,
  _notActive : false,
  _isDisplayOff : false,
  observe: function ao_observe(aSubject, aTopic, aData) {
    if (aTopic == "application-background") {
      this._inBackground = true;
    } else if (aTopic == "application-foreground") {
      this._inBackground = false;
    } else if (aTopic == "system-idle") {
      this._notActive = true;
    } else if (aTopic == "system-active") {
      this._notActive = false;
    } else if (aTopic == "system-display-on") {
      this._isDisplayOff = false;
    } else if (aTopic == "system-display-off") {
      this._isDisplayOff = true;
    }
    let activeTabState = !this._inBackground && !this._notActive && !this._isDisplayOff;
    if (Browser.selectedTab.active != activeTabState) {
      // On Maemo all backgrounded applications getting portrait orientation
      // so if browser had landscape mode then we need timeout in order
      // to finish last rotate/paint operation and have nice lookine browser in TS
      setTimeout(function() { Browser.selectedTab.active = activeTabState; }, 0);
    }
  }
};

function getNotificationBox(aBrowser) {
  return Browser.getNotificationBox(aBrowser);
}

function importDialog(aParent, aSrc, aArguments) {
  // load the dialog with a synchronous XHR
  let xhr = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].createInstance();
  xhr.open("GET", aSrc, false);
  xhr.overrideMimeType("text/xml");
  xhr.send(null);
  if (!xhr.responseXML)
    return null;

  let currentNode;
  let nodeIterator = xhr.responseXML.createNodeIterator(xhr.responseXML, NodeFilter.SHOW_TEXT, null, false);
  while (currentNode = nodeIterator.nextNode()) {
    let trimmed = currentNode.nodeValue.replace(/^\s\s*/, "").replace(/\s\s*$/, "");
    if (!trimmed.length)
      currentNode.parentNode.removeChild(currentNode);
  }

  let doc = xhr.responseXML.documentElement;

  let dialog  = null;

  // we need to insert before context-container if we want allow pasting (using
  //  the context menu) into dialogs
  let contentMenuContainer = document.getElementById("context-container");
  let parentNode = contentMenuContainer.parentNode;

  // emit DOMWillOpenModalDialog event
  let event = document.createEvent("Events");
  event.initEvent("DOMWillOpenModalDialog", true, false);
  let dispatcher = aParent || getBrowser();
  dispatcher.dispatchEvent(event);

  // create a full-screen semi-opaque box as a background
  let back = document.createElement("box");
  back.setAttribute("class", "modal-block");
  dialog = back.appendChild(document.importNode(doc, true));
  parentNode.insertBefore(back, contentMenuContainer);

  dialog.arguments = aArguments;
  dialog.parent = aParent;
  return dialog;
}

function showDownloadManager(aWindowContext, aID, aReason) {
  BrowserUI.showPanel("downloads-container");
  // TODO: select the download with aID
}

function Tab(aURI, aParams) {
  this._id = null;
  this._browser = null;
  this._notification = null;
  this._loading = false;
  this._chromeTab = null;
  this._metadata = null;

  this.contentMightCaptureMouse = false;
  this.owner = null;

  this.hostChanged = false;
  this.state = null;

  // Set to 0 since new tabs that have not been viewed yet are good tabs to
  // toss if app needs more memory.
  this.lastSelected = 0;

  // aParams is an object that contains some properties for the initial tab
  // loading like flags, a referrerURI, a charset or even a postData.
  this.create(aURI, aParams || {});

  // default tabs to inactive (i.e. no display port)
  this.active = false;
}

Tab.prototype = {
  get browser() {
    return this._browser;
  },

  get notification() {
    return this._notification;
  },

  get chromeTab() {
    return this._chromeTab;
  },

  get metadata() {
    return this._metadata || kDefaultMetadata;
  },

  get inputHandler() {
    if (!this._notification)
      return null;
    return this._notification.inputHandler;
  },

  /** Update browser styles when the viewport metadata changes. */
  updateViewportMetadata: function updateViewportMetadata(aMetadata) {
    if (aMetadata && aMetadata.autoScale) {
      let scaleRatio = aMetadata.scaleRatio = Browser.getScaleRatio();

      if ("defaultZoom" in aMetadata && aMetadata.defaultZoom > 0)
        aMetadata.defaultZoom *= scaleRatio;
      if ("minZoom" in aMetadata && aMetadata.minZoom > 0)
        aMetadata.minZoom *= scaleRatio;
      if ("maxZoom" in aMetadata && aMetadata.maxZoom > 0)
        aMetadata.maxZoom *= scaleRatio;
    }
    this._metadata = aMetadata;
    this.updateViewportSize();
  },

  /**
   * Update browser size when the metadata or the window size changes.
   */
  updateViewportSize: function updateViewportSize() {
    let browser = this._browser;
    if (!browser)
      return;

    let screenW = ViewableAreaObserver.width;
    let screenH = ViewableAreaObserver.height;
    let viewportW, viewportH;

    let metadata = this.metadata;
    if (metadata.autoSize) {
      if ("scaleRatio" in metadata) {
        viewportW = screenW / metadata.scaleRatio;
        viewportH = screenH / metadata.scaleRatio;
      } else {
        viewportW = screenW;
        viewportH = screenH;
      }
    } else {
      viewportW = metadata.width;
      viewportH = metadata.height;

      // If (scale * width) < device-width, increase the width (bug 561413).
      let maxInitialZoom = metadata.defaultZoom || metadata.maxZoom;
      if (maxInitialZoom && viewportW)
        viewportW = Math.max(viewportW, screenW / maxInitialZoom);

      let validW = viewportW > 0;
      let validH = viewportH > 0;

      if (!validW)
        viewportW = validH ? (viewportH * (screenW / screenH)) : Browser.defaultBrowserWidth;
      if (!validH)
        viewportH = viewportW * (screenH / screenW);
    }

    // Make sure the viewport height is not shorter than the window when
    // the page is zoomed out to show its full width.
    let pageZoomLevel = this.getPageZoomLevel(screenW);
    let minScale = this.clampZoomLevel(pageZoomLevel, pageZoomLevel);
    viewportH = Math.max(viewportH, screenH / minScale);

    if (browser.contentWindowWidth != viewportW || browser.contentWindowHeight != viewportH)
      browser.setWindowSize(viewportW, viewportH);
  },

  restoreViewportPosition: function restoreViewportPosition(aOldWidth, aNewWidth) {
    let browser = this._browser;

    // zoom to keep the same portion of the document visible
    let oldScale = browser.scale;
    let newScale = this.clampZoomLevel(oldScale * aNewWidth / aOldWidth);
    let scaleRatio = newScale / oldScale;

    let view = browser.getRootView();
    let pos = view.getPosition();
    browser.fuzzyZoom(newScale, pos.x * scaleRatio, pos.y * scaleRatio);
    browser.finishFuzzyZoom();
  },

  startLoading: function startLoading() {
    if (this._loading) throw "Already Loading!";
    this._loading = true;
  },

  endLoading: function endLoading() {
    if (!this._loading) throw "Not Loading!";
    this._loading = false;
    if (this._drawThumb) {
      this._drawThumb = false;
      this.updateThumbnail();
    }
  },

  isLoading: function isLoading() {
    return this._loading;
  },

  create: function create(aURI, aParams) {
    this._chromeTab = Elements.tabList.addTab();
    let browser = this._createBrowser(aURI, null);

    // Should we fully load the new browser, or wait until later
    if ("delayLoad" in aParams && aParams.delayLoad)
      return;

    try {
      let flags = aParams.flags || Ci.nsIWebNavigation.LOAD_FLAGS_NONE;
      let postData = "postData" in aParams ? aParams.postData.value : null;
      let referrerURI = "referrerURI" in aParams ? aParams.referrerURI : null;
      let charset = "charset" in aParams ? aParams.charset : null;
      browser.loadURIWithFlags(aURI, flags, referrerURI, charset, postData);
    } catch(e) {
      dump("Error: " + e + "\n");
    }
  },

  destroy: function destroy() {
    Elements.tabList.removeTab(this._chromeTab);
    this._chromeTab = null;
    this._destroyBrowser();
  },

  resurrect: function resurrect() {
    let dead = this._browser;
    let active = this.active;

    // Hold onto the session store data
    let session = { data: dead.__SS_data, extra: dead.__SS_extdata };

    // We need this data to correctly create and position the new browser
    // If this browser is already a zombie, fallback to the session data
    let currentURL = dead.__SS_restore ? session.data.entries[0].url : dead.currentURI.spec;
    let sibling = dead.nextSibling;

    // Destory and re-create the browser
    this._destroyBrowser();
    let browser = this._createBrowser(currentURL, sibling);
    if (active)
      this.active = true;

    // Reattach session store data and flag this browser so it is restored on select
    browser.__SS_data = session.data;
    browser.__SS_extdata = session.extra;
    browser.__SS_restore = true;
  },

  _createBrowser: function _createBrowser(aURI, aInsertBefore) {
    if (this._browser)
      throw "Browser already exists";

    // Create a notification box around the browser
    let notification = this._notification = document.createElement("notificationbox");
    notification.classList.add("inputHandler");

    let browser = this._browser = document.createElement("browser");
    browser.setAttribute("class", "viewable-width viewable-height");
    this._chromeTab.linkedBrowser = browser;

    browser.setAttribute("type", "content");

    let useRemote = Services.prefs.getBoolPref("browser.tabs.remote");
    let useLocal = Util.isLocalScheme(aURI);
    browser.setAttribute("remote", (!useLocal && useRemote) ? "true" : "false");

    // Append the browser to the document, which should start the page load
    notification.appendChild(browser);
    Elements.browsers.insertBefore(notification, aInsertBefore);

    // stop about:blank from loading
    browser.stop();

    let fl = browser.QueryInterface(Ci.nsIFrameLoaderOwner).frameLoader;
    fl.renderMode = Ci.nsIFrameLoader.RENDER_MODE_ASYNC_SCROLL;
    fl.eventMode = Ci.nsIFrameLoader.EVENT_MODE_DONT_FORWARD_TO_CHILD;

    return browser;
  },

  _destroyBrowser: function _destroyBrowser() {
    if (this._browser) {
      let notification = this._notification;
      let browser = this._browser;
      browser.active = false;

      this._notification = null;
      this._browser = null;
      this._loading = false;

      Elements.browsers.removeChild(notification);
    }
  },

  /**
   * Takes a scale and restricts it based on this tab's zoom limits.
   * @param aScale The original scale.
   * @param aPageZoomLevel (optional) The zoom-to-fit scale, if known.
   *   This is a performance optimization to avoid extra calls.
   */
  clampZoomLevel: function clampZoomLevel(aScale, aPageZoomLevel) {
    let md = this.metadata;
    if (!this.allowZoom) {
      return (md && md.defaultZoom)
        ? md.defaultZoom
        : (aPageZoomLevel || this.getPageZoomLevel());
    }

    let browser = this._browser;
    let bounded = Util.clamp(aScale, ZoomManager.MIN, ZoomManager.MAX);

    if (md && md.minZoom)
      bounded = Math.max(bounded, md.minZoom);
    if (md && md.maxZoom)
      bounded = Math.min(bounded, md.maxZoom);

    bounded = Math.max(bounded, this.getPageZoomLevel());

    let rounded = Math.round(bounded * kBrowserViewZoomLevelPrecision) / kBrowserViewZoomLevelPrecision;
    return rounded || 1.0;
  },

  /** Record the initial zoom level when a page first loads. */
  resetZoomLevel: function resetZoomLevel() {
    this._defaultZoomLevel = this._browser.scale;
  },

  scrolledAreaChanged: function scrolledAreaChanged(firstPaint) {
    if (!this._browser)
      return;

    if (firstPaint) {
      // You only get one shot, do not miss your chance to reflow.
      this.updateViewportSize();
    }

    this.updateDefaultZoomLevel();
  },

  /**
   * Recalculate default zoom level when page size changes, and update zoom
   * level if we are at default.
   */
  updateDefaultZoomLevel: function updateDefaultZoomLevel() {
    let browser = this._browser;
    if (!browser || !this._firstPaint)
      return;

    let isDefault = this.isDefaultZoomLevel();
    this._defaultZoomLevel = this.getDefaultZoomLevel();
    if (isDefault) {
      if (browser.scale != this._defaultZoomLevel) {
        browser.scale = this._defaultZoomLevel;
      } else {
        // If the scale level has not changed we want to be sure the content
        // render correctly since the page refresh process could have been
        // stalled during page load. In this case if the page has the exact
        // same width (like the same page, so by doing 'refresh') and the
        // page was scrolled the content is just checkerboard at this point
        // and this call ensure we render it correctly.
        browser.getRootView()._updateCacheViewport();
      }
    } else {
      // if we are reloading, the page will retain its scale. if it is zoomed
      // we need to refresh the viewport so that we do not show checkerboard
      browser.getRootView()._updateCacheViewport();
    }
  },

  isDefaultZoomLevel: function isDefaultZoomLevel() {
    return this._browser.scale == this._defaultZoomLevel;
  },

  getDefaultZoomLevel: function getDefaultZoomLevel() {
    let md = this.metadata;
    if (md && md.defaultZoom)
      return this.clampZoomLevel(md.defaultZoom);

    let pageZoom = this.getPageZoomLevel();

    // If pageZoom is "almost" 100%, zoom in to exactly 100% (bug 454456).
    let granularity = Services.prefs.getIntPref("browser.ui.zoom.pageFitGranularity");
    let threshold = 1 - 1 / granularity;
    if (threshold < pageZoom && pageZoom < 1)
      pageZoom = 1;

    return this.clampZoomLevel(pageZoom);
  },

  /**
   * @param aScreenWidth (optional) The width of the browser widget, if known.
   *   This is a performance optimization to save extra calls to getBoundingClientRect.
   * @return The scale at which the browser will be zoomed out to fit the document width.
   */
  getPageZoomLevel: function getPageZoomLevel(aScreenWidth) {
    let browserW = this._browser.contentDocumentWidth;
    if (browserW == 0)
      return 1.0;

    let screenW = aScreenWidth || this._browser.getBoundingClientRect().width;
    return screenW / browserW;
  },

  get allowZoom() {
    return this.metadata.allowZoom && !Util.isURLEmpty(this.browser.currentURI.spec);
  },

  _thumbnailWindowId: null,

  updateThumbnail: function updateThumbnail(options) {
    let options = options || {};
    let browser = this._browser;

    if (this._loading) {
      this._drawThumb = true;
      return;
    }

    let forceUpdate = ("force" in options && options.force);

    // Do not repaint thumbnail if we already painted for this load. Bad things
    // happen when we do async canvas draws in quick succession.
    if (!forceUpdate && (!browser || this._thumbnailWindowId == browser.contentWindowId))
      return;

    // Do not try to paint thumbnails if contentWindowWidth/Height have not been
    // set yet. This also blows up for async canvas draws.
    if (!browser.contentWindowWidth || !browser.contentWindowHeight)
      return;

    this._thumbnailWindowId = browser.contentWindowId;
    this._chromeTab.updateThumbnail(browser, browser.contentWindowWidth, browser.contentWindowHeight);
  },

  set active(aActive) {
    if (!this._browser)
      return;

    let notification = this._notification;
    let browser = this._browser;

    if (aActive) {
      browser.setAttribute("type", "content-primary");
      Elements.browsers.selectedPanel = notification;
      browser.active = true;
      Elements.tabList.selectedTab = this._chromeTab;
      browser.focus();
    } else {
      browser.messageManager.sendAsyncMessage("Browser:Blur", { });
      browser.setAttribute("type", "content");
      browser.active = false;
    }
  },

  get active() {
    if (!this._browser)
      return false;
    return this._browser.getAttribute("type") == "content-primary";
  },

  updateContentCapture: function() {
    this._browser.messageManager.sendAsyncMessage("Browser:CanCaptureMouse", {});
  },

  toString: function() {
    return "[Tab " + (this._browser ? this._browser.currentURI.spec : "(no browser)") + "]";
  }
};

// Helper used to hide IPC / non-IPC differences for rendering to a canvas
function rendererFactory(aBrowser, aCanvas) {
  let wrapper = {};

  if (aBrowser.contentWindow) {
    let ctx = aCanvas.getContext("2d");
    let draw = function(browser, aLeft, aTop, aWidth, aHeight, aColor, aFlags) {
      ctx.drawWindow(browser.contentWindow, aLeft, aTop, aWidth, aHeight, aColor, aFlags);
      let e = document.createEvent("HTMLEvents");
      e.initEvent("MozAsyncCanvasRender", true, true);
      aCanvas.dispatchEvent(e);
    };
    wrapper.checkBrowser = function(browser) {
      return browser.contentWindow;
    };
    wrapper.drawContent = function(callback) {
      callback(ctx, draw);
    };
  }
  else {
    let ctx = aCanvas.MozGetIPCContext("2d");
    let draw = function(browser, aLeft, aTop, aWidth, aHeight, aColor, aFlags) {
      ctx.asyncDrawXULElement(browser, aLeft, aTop, aWidth, aHeight, aColor, aFlags);
    };
    wrapper.checkBrowser = function(browser) {
      return !browser.contentWindow;
    };
    wrapper.drawContent = function(callback) {
      callback(ctx, draw);
    };
  }

  return wrapper;
};

/* ViewableAreaObserver is an helper object where width/height represents the
 * size of the currently viewable area in pixels. This is use instead of
 * window.innerHeight/innerWidth because some keyboards does not resize the
 * window but floats over it.
 */
var ViewableAreaObserver = {
  get width() {
    let width = this._width || window.innerWidth;
    if (Util.isTablet()) {
      let sidebarWidth = Math.round(Elements.tabs.getBoundingClientRect().width);
      width -= sidebarWidth;
    }
    return width;
  },

  get height() {
    let height = (this._height || window.innerHeight);
    if (Util.isTablet())
      height -= BrowserUI.toolbarH;
    return height;
  },

  _isKeyboardOpened: true,
  get isKeyboardOpened() {
    return this._isKeyboardOpened;
  },

  set isKeyboardOpened(aValue) {
    if (!this.hasVirtualKeyboard())
      return this._isKeyboardOpened;

    let oldValue = this._isKeyboardOpened;

    if (oldValue != aValue) {
      this._isKeyboardOpened = aValue;

      let event = document.createEvent("UIEvents");
      event.initUIEvent("KeyboardChanged", true, false, window, aValue);
      window.dispatchEvent(event);
    }
  },

  hasVirtualKeyboard: function va_hasVirtualKeyboard() {
#ifndef ANDROID
#ifndef MOZ_PLATFORM_MAEMO
    return false;
#endif
#endif

    return true;
  },


  observe: function va_observe(aSubject, aTopic, aData) {
#if MOZ_PLATFORM_MAEMO == 6
    let rect = Rect.fromRect(JSON.parse(aData));
    let height = rect.bottom - rect.top;
    let width = rect.right - rect.left;
    if (height == window.innerHeight && width == window.innerWidth) {
      this._height = null;
      this._width = null;
    }
    else {
      this._height = height;
      this._width = width;
    }
    this.update();
#endif
  },

  receiveMessage: function receiveMessage(aMessage) {
    return this.isKeyboardOpened;
  },

  update: function va_update() {
    let oldHeight = parseInt(Browser.styles["viewable-height"].height);
    let oldWidth = parseInt(Browser.styles["viewable-width"].width);

    let newWidth = this.width;
    let newHeight = this.height;
    if (newHeight == oldHeight && newWidth == oldWidth)
      return;

    // Guess if the window has been resize to handle a virtual keyboard
    this.isKeyboardOpened = (newHeight < oldHeight && newWidth == oldWidth);

    Browser.styles["viewable-height"].height = newHeight + "px";
    Browser.styles["viewable-height"].maxHeight = newHeight + "px";

    Browser.styles["viewable-width"].width = newWidth + "px";
    Browser.styles["viewable-width"].maxWidth = newWidth + "px";

    // Don't update the viewport if screen height is shrinking, which typically
    // means the keyboard appeared. This helps smooth out the experience of our
    // form filler.
    if (newHeight > oldHeight || newWidth != oldWidth) {
      let startup = !oldHeight && !oldWidth;
      for (let i = Browser.tabs.length - 1; i >= 0; i--) {
        let tab = Browser.tabs[i];
        let oldContentWindowWidth = tab.browser.contentWindowWidth;
        tab.updateViewportSize(); // contentWindowWidth may change here.

        // Don't bother updating the zoom level on startup
        if (!startup) {
          // If the viewport width is still the same, the page layout has not
          // changed, so we can keep keep the same content on-screen.
          if (tab.browser.contentWindowWidth == oldContentWindowWidth)
            tab.restoreViewportPosition(oldWidth, newWidth);

          tab.updateDefaultZoomLevel();
        }
      }
    }

    // setTimeout(callback, 0) to ensure the resize event handler dispatch is finished
    setTimeout(function() {
      let event = document.createEvent("Events");
      event.initEvent("SizeChanged", true, false);
      Elements.browsers.dispatchEvent(event);
    }, 0);
  }
};

