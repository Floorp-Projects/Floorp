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

const endl = '\n';

Cu.import("resource://gre/modules/SpatialNavigation.js");
Cu.import("resource://gre/modules/PluralForm.jsm");

function getBrowser() {
  return Browser.selectedBrowser;
}

const kDefaultBrowserWidth = 800;

function debug() {
  let bv = Browser._browserView;
  let tc = bv._tileManager._tileCache;
  let scrollbox = document.getElementById("content-scrollbox")
                .boxObject.QueryInterface(Ci.nsIScrollBoxObject);

  let x = {};
  let y = {};
  let w = {};
  let h = {};
  scrollbox.getPosition(x, y);
  scrollbox.getScrolledSize(w, h);
  let container = document.getElementById("tile-container");
  let [x, y] = [x.value, y.value];
  let [w, h] = [w.value, h.value];
  if (bv) {
    dump('----------------------DEBUG!-------------------------\n');
    dump(bv._browserViewportState.toString() + endl);

    dump(endl);

    dump('location from Browser: ' + Browser.selectedBrowser.contentWindow.location + endl);
    dump('location from BV     : ' + bv.getBrowser().contentWindow.location + endl);

    dump(endl + endl);

    let cr = bv._tileManager._criticalRect;
    dump('criticalRect from BV: ' + (cr ? cr.toString() : null) + endl);
    dump('visibleRect from BV : ' + bv.getVisibleRect().toString() + endl);
    dump('visibleRect from foo: ' + Browser.getVisibleRect().toString() + endl);

    dump('bv batchops depth:    ' + bv._batchOps.length + endl);
    dump('renderpause depth:    ' + bv._renderMode + endl);

    dump(endl);

    dump('window.innerWidth : ' + window.innerWidth  + endl);
    dump('window.innerHeight: ' + window.innerHeight + endl);

    dump(endl);

    dump('container width,height from BV: ' + bv._container.style.width + ', '
                                            + bv._container.style.height + endl);
    dump('container width,height via DOM: ' + container.style.width + ', '
                                            + container.style.height + endl);

    dump(endl);

    dump('scrollbox position    : ' + x + ', ' + y + endl);
    dump('scrollbox scrolledsize: ' + w + ', ' + h + endl);


    let sb = document.getElementById("content-scrollbox");
    dump('container location:     ' + Math.round(container.getBoundingClientRect().left) + " " +
                                      Math.round(container.getBoundingClientRect().top) + endl);

    dump(endl);

    let mouseModule = ih._modules[0];
    dump('ih grabber  : ' + ih._grabber           + endl);
    dump('ih grabdepth: ' + ih._grabDepth         + endl);
    dump('ih listening: ' + !ih._ignoreEvents     + endl);
    dump('ih suppress : ' + ih._suppressNextClick + endl);
    dump('mouseModule : ' + mouseModule           + endl);

    dump(endl);

    dump('tilecache capacity: ' + bv._tileManager._tileCache.getCapacity() + endl);
    dump('tilecache size    : ' + bv._tileManager._tileCache.size          + endl);
    dump('tilecache iBound  : ' + bv._tileManager._tileCache.iBound        + endl);
    dump('tilecache jBound  : ' + bv._tileManager._tileCache.jBound        + endl);

    dump('-----------------------------------------------------\n');
  }
}


function debugTile(i, j) {
  let bv = Browser._browserView;
  let tc = bv._tileManager._tileCache;
  let t  = tc.getTile(i, j);

  dump('------ DEBUGGING TILE (' + i + ',' + j + ') --------\n');

  dump('in bounds: ' + tc.inBounds(i, j) + endl);
  dump('occupied : ' + tc.isOccupied(i, j) + endl);
  if (t)
  {
  dump('toString : ' + t.toString(true) + endl);
  dump('free     : ' + t.free + endl);
  dump('dirtyRect: ' + t._dirtyTileCanvasRect + endl);

  let len = tc._tilePool.length;
  for (let k = 0; k < len; ++k)
    if (tc._tilePool[k] === t)
      dump('found in tilePool at index ' + k + endl);
  }

  dump('------------------------------------\n');
}

function onDebugKeyPress(ev) {
  let bv = Browser._browserView;

  if (!ev.ctrlKey)
    return;

  // use capitals so we require SHIFT here too

  const a = 65;   // debug all critical tiles
  const b = 66;   // dump an ASCII graphic of the tile map
  const c = 67;   // set tilecache capacity
  const d = 68;  // debug dump
  const e = 69;
  const f = 70;  // free memory by clearing a tab.
  const g = 71;
  const h = 72;
  const i = 73;  // toggle info click mode
  const j = 74;
  const k = 75;
  const l = 76;  // restart lazy crawl
  const m = 77;  // fix mouseout
  const n = 78;
  const o = 79;
  const p = 80;  // debug tiles in pool order
  const q = 81;
  const r = 82;  // reset visible rect
  const s = 83;
  const t = 84;  // debug given list of tiles separated by space
  const u = 85;
  const v = 86;
  const w = 87;
  const x = 88;
  const y = 89;
  const z = 90;  // set zoom level to 1

  if (window.tileMapMode) {
    function putChar(ev, col, row) {
      let tile = tc.getTile(col, row);
      switch (ev.charCode) {
      case h: // held tiles
        dump(tile ? (tile.free ? '*' : 'h') : ' ');
        break;
      case d: // dirty tiles
        dump(tile ? (tile.isDirty() ? 'd' : '*') : ' ');
        break;
      case o: // occupied tileholders
        dump(tc.isOccupied(col, row) ? 'o' : ' ');
        break;
      }
    }

    let tc = Browser._browserView._tileManager._tileCache;
    let col, row;

    dump(endl);

    dump('  ');
    for (col = 0; col < tc.iBound; ++col)
      dump(col % 10);

    dump(endl);

    for (row = 0; row < tc.jBound; ++row) {

      dump((row % 10) + ' ');

      for (col = 0; col < tc.iBound; ++col) {
        putChar(ev, col, row);
      }

      dump(endl);
    }
    dump(endl + endl);

    for (let ii = 0; ii < tc._tilePool.length; ++ii) {
      let tile = tc._tilePool[ii];
      putChar(ev, tile.i, tile.j);
    }

    dump(endl + endl);

    window.tileMapMode = false;
    return;
  }

  switch (ev.charCode) {
  case f:
    var result = Browser.sacrificeTab();
    if (result)
      dump("Freed a tab\n");
    else
      dump("There are no tabs left to free\n");
    break;

  case r:
    bv.onAfterVisibleMove();
    //bv.setVisibleRect(Browser.getVisibleRect());

  case d:
    debug();

    break;
  case l:
    bv._tileManager.restartLazyCrawl(bv._tileManager._criticalRect);

    break;
  case c:
    let cap = parseInt(window.prompt('new capacity'));
    bv._tileManager._tileCache.setCapacity(cap);

    break;
  case b:
    window.tileMapMode = true;
    break;
  case t:
    let ijstrs = window.prompt('row,col plz').split(' ');
    for each (let ijstr in ijstrs) {
      let [i, j] = ijstr.split(',').map(function (x) { return parseInt(x); });
      debugTile(i, j);
    }

    break;
  case a:
    let cr = bv._tileManager._criticalRect;
    dump('>>>>>> critical rect is ' + (cr ? cr.toString() : cr) + '\n');
    if (cr) {
      let starti = cr.left  >> kTileExponentWidth;
      let endi   = cr.right >> kTileExponentWidth;

      let startj = cr.top    >> kTileExponentHeight;
      let endj   = cr.bottom >> kTileExponentHeight;

      for (var jj = startj; jj <= endj; ++jj)
        for (var ii = starti; ii <= endi; ++ii)
          debugTile(ii, jj);
    }

    break;
  case i:
    window.infoMode = !window.infoMode;
    break;
  case m:
    bv.resumeRendering();
    break;
  case p:
    let tc = bv._tileManager._tileCache;
    dump('************* TILE POOL ****************\n');
    for (let ii = 0, len = tc._tilePool.length; ii < len; ++ii) {
      if (window.infoMode)
        debugTile(tc._tilePool[ii].i, tc._tilePool[ii].j);
      else
        dump(tc._tilePool[ii].i + ',' + tc._tilePool[ii].j + '\n');
    }
    dump('****************************************\n');
    break;
  case z:
    bv.setZoomLevel(1.0);
    break;
  default:
    break;
  }
}
window.infoMode = false;
window.tileMapMode = false;

var ih = null;

var Browser = {
  _tabs : [],
  _selectedTab : null,
  windowUtils: window.QueryInterface(Ci.nsIInterfaceRequestor)
                     .getInterface(Ci.nsIDOMWindowUtils),
  contentScrollbox: null,
  contentScrollboxScroller: null,
  controlsScrollbox: null,
  controlsScrollboxScroller: null,
  pageScrollbox: null,
  pageScrollboxScroller: null,
  styles: {},

  startup: function() {
    var self = this;

    let container = document.getElementById("tile-container");
    let bv = this._browserView = new BrowserView(container, Browser.getVisibleRect);

    /* handles dispatching clicks on tiles into clicks in content or zooms */
    container.customClicker = new ContentCustomClicker(bv);

    /* scrolling box that contains tiles */
    let contentScrollbox = this.contentScrollbox = document.getElementById("content-scrollbox");
    this.contentScrollboxScroller = contentScrollbox.boxObject.QueryInterface(Ci.nsIScrollBoxObject);
    contentScrollbox.customDragger = new Browser.MainDragger(bv);

    /* horizontally scrolling box that holds the sidebars as well as the contentScrollbox */
    let controlsScrollbox = this.controlsScrollbox = document.getElementById("controls-scrollbox");
    this.controlsScrollboxScroller = controlsScrollbox.boxObject.QueryInterface(Ci.nsIScrollBoxObject);
    controlsScrollbox.customDragger = {
      isDraggable: function isDraggable(target, content) { return false; },
      dragStart: function dragStart(cx, cy, target, scroller) {},
      dragStop: function dragStop(dx, dy, scroller) { return false; },
      dragMove: function dragMove(dx, dy, scroller) { return false; }
    };

    /* vertically scrolling box that contains the url bar, notifications, and content */
    let pageScrollbox = this.pageScrollbox = document.getElementById("page-scrollbox");
    this.pageScrollboxScroller = pageScrollbox.boxObject.QueryInterface(Ci.nsIScrollBoxObject);
    pageScrollbox.customDragger = controlsScrollbox.customDragger;

    // during startup a lot of viewportHandler calls happen due to content and window resizes
    bv.beginBatchOperation();

    let stylesheet = document.styleSheets[0];
    for each (let style in ["window-width", "window-height", "toolbar-height", "browser", "browser-handheld", "browser-viewport"]) {
      let index = stylesheet.insertRule("." + style + " {}", stylesheet.cssRules.length);
      this.styles[style] = stylesheet.cssRules[index].style;
    }

    function resizeHandler(e) {

      if (e.target != window)
        return;

      // XXX is this code right here actually needed?
      let w = window.innerWidth;
      let h = window.innerHeight;
      let maximize = (document.documentElement.getAttribute("sizemode") == "maximized");
      if (maximize && w > screen.width)
        return;

      bv.beginBatchOperation();

      let toolbarHeight = Math.round(document.getElementById("toolbar-main").getBoundingClientRect().height);
      let scaledDefaultH = (kDefaultBrowserWidth * (h / w));
      let scaledScreenH = (window.screen.width * (h / w));

      Browser.styles["window-width"].width = w + "px";
      Browser.styles["window-height"].height = h + "px";
      Browser.styles["toolbar-height"].height = toolbarHeight + "px";
      Browser.styles["browser"].width = kDefaultBrowserWidth + "px";
      Browser.styles["browser"].height = scaledDefaultH + "px";
      Browser.styles["browser-handheld"].width = window.screen.width + "px";
      Browser.styles["browser-handheld"].height = scaledScreenH + "px";

      // Tell the UI to resize the browser controls before calling  updateSize
      BrowserUI.sizeControls(w, h);
      
      bv.updateDefaultZoom();
      if (bv.isDefaultZoom())
        // XXX this should really only happen on browser startup, not every resize
        Browser.hideSidebars();
      bv.onAfterVisibleMove();

      bv.commitBatchOperation();
    }
    window.addEventListener("resize", resizeHandler, false);

    function fullscreenHandler() {
      if (!window.fullScreen)
        document.getElementById("toolbar-main").setAttribute("fullscreen", "true");
      else
        document.getElementById("toolbar-main").removeAttribute("fullscreen");
    }
    window.addEventListener("fullscreen", fullscreenHandler, false);

    function notificationHandler() {
      // Let the view know that the layout might have changed
      Browser.forceChromeReflow();
      bv.onAfterVisibleMove();
    }
    let notifications = document.getElementById("notifications");
    notifications.addEventListener("AlertActive", notificationHandler, false);
    notifications.addEventListener("AlertClose", notificationHandler, false);

    // initialize input handling
    ih = new InputHandler(container);

    BrowserUI.init();

    window.controllers.appendController(this);
    window.controllers.appendController(BrowserUI);

    var styleSheets = Cc["@mozilla.org/content/style-sheet-service;1"].getService(Ci.nsIStyleSheetService);

    // Should we hide the cursors
    var hideCursor = gPrefService.getBoolPref("browser.ui.cursor") == false;
    if (hideCursor) {
      window.QueryInterface(Ci.nsIDOMChromeWindow).setCursor("none");

      var styleURI = gIOService.newURI("chrome://browser/content/cursor.css", null, null);
      styleSheets.loadAndRegisterSheet(styleURI, styleSheets.AGENT_SHEET);
    }

    // load styles for scrollbars
    var styleURI = gIOService.newURI("chrome://browser/content/content.css", null, null);
    styleSheets.loadAndRegisterSheet(styleURI, styleSheets.AGENT_SHEET);

    var os = Cc["@mozilla.org/observer-service;1"].getService(Ci.nsIObserverService);
    os.addObserver(gXPInstallObserver, "xpinstall-install-blocked", false);
    os.addObserver(gSessionHistoryObserver, "browser:purge-session-history", false);
#ifdef WINCE
    os.addObserver(SoftKeyboardObserver, "softkb-change", false);
#endif

    // clear out tabs the user hasn't touched lately on memory crunch
    os.addObserver(MemoryObserver, "memory-pressure", false);

    // search engine changes
    os.addObserver(BrowserSearch, "browser-search-engine-modified", false);

    window.QueryInterface(Ci.nsIDOMChromeWindow).browserDOMWindow = new nsBrowserAccess();

    let browsers = document.getElementById("browsers");
    browsers.addEventListener("command", this._handleContentCommand, true);
    browsers.addEventListener("MozApplicationManifest", OfflineApps, false);
    browsers.addEventListener("DOMUpdatePageReport", gPopupBlockerObserver.onUpdatePageReport, false);

    // Initialize Spatial Navigation
    function panCallback(aElement) {
      if (!aElement)
        return;

      // XXX We need to add this back
      //canvasBrowser.ensureElementIsVisible(aElement);
    }
    // Init it with the "browsers" element, which will receive keypress events
    // for all of our <browser>s
    SpatialNavigation.init(browsers, panCallback);

    // Login Manager
    Cc["@mozilla.org/login-manager;1"].getService(Ci.nsILoginManager);

    // Make sure we're online before attempting to load
    Util.forceOnline();

    // Command line arguments/initial homepage
    let whereURI = "about:blank";
    switch (Util.needHomepageOverride()) {
      case "new profile":
        whereURI = "about:firstrun";
        break;
      case "new version":
      case "none":
        whereURI = "about:blank";
        break;
    }

    // If this is an intial window launch (was a nsICommandLine passed via window params)
    // we execute some logic to load the initial launch page
    if (window.arguments && window.arguments[0] &&
        window.arguments[0] instanceof Ci.nsICommandLine) {
      try {
        var cmdLine = window.arguments[0];

        // Check for and use a single commandline parameter
        if (cmdLine.length == 1) {
          // Assume the first arg is a URI if it is not a flag
          var uri = cmdLine.getArgument(0);
          if (uri != "" && uri[0] != '-') {
            whereURI = cmdLine.resolveURI(uri);
            if (whereURI)
              whereURI = whereURI.spec;
          }
        }

        // Check for the "url" flag
        var uriFlag = cmdLine.handleFlagWithParam("url", false);
        if (uriFlag) {
          whereURI = cmdLine.resolveURI(uriFlag);
          if (whereURI)
            whereURI = whereURI.spec;
        }
      } catch (e) {}
    }

    this.addTab(whereURI, true);
    if (whereURI == "about:blank")
      BrowserUI.showAutoComplete();

    // JavaScript Error Console
    if (gPrefService.getBoolPref("browser.console.showInPanel")){
      let tool_console = document.getElementById("tool-console");
      tool_console.hidden = false;
    }

    bv.commitBatchOperation();

    // If some add-ons were disabled during during an application update, alert user
    if (gPrefService.prefHasUserValue("extensions.disabledAddons")) {
      let addons = gPrefService.getCharPref("extensions.disabledAddons").split(",");
      if (addons.length > 0) {
        let disabledStrings = Elements.browserBundle.getString("alertAddonsDisabled");
        let label = PluralForm.get(addons.length, disabledStrings).replace("#1", addons.length);
  
        let alerts = Cc["@mozilla.org/alerts-service;1"].getService(Ci.nsIAlertsService);
        alerts.showAlertNotification(URI_GENERIC_ICON_XPINSTALL, strings.getString("alertAddons"),
                                     label, false, "", null);
      }
      gPrefService.clearUserPref("extensions.disabledAddons");
    }

    // some day (maybe fennec 1.0), we can remove both of these
    // preference reseting checks.  they are here because we wanted to
    // disable plugins and flash explictly, then we fixed things and
    // needed to re-enable support.  as time goes on, fewer people
    // will have ever had these temporary.* preference set.

    // Re-enable plugins if we had previously disabled them. We should get rid of
    // this code eventually...
    if (gPrefService.prefHasUserValue("temporary.disablePlugins")) {
      gPrefService.clearUserPref("temporary.disablePlugins");
      this.setPluginState(true);
    }

    // Re-enable flash
    if (gPrefService.prefHasUserValue("temporary.disabledFlash")) {
      this.setPluginState(true, /flash/i);
      gPrefService.clearUserPref("temporary.disabledFlash");
    }

    // Force commonly used border-images into the image cache
    ImagePreloader.cache();

    this._pluginObserver = new PluginObserver(bv);
    new PreferenceToggle("plugins.enabled", this._pluginObserver);
  },

  shutdown: function() {
    this._browserView.uninit();
    BrowserUI.uninit();
    this._pluginObserver.stop();

    var os = Cc["@mozilla.org/observer-service;1"].getService(Ci.nsIObserverService);
    os.removeObserver(gXPInstallObserver, "xpinstall-install-blocked");
    os.removeObserver(gSessionHistoryObserver, "browser:purge-session-history");
    os.removeObserver(MemoryObserver, "memory-pressure");
#ifdef WINCE
    os.removeObserver(SoftKeyboardObserver, "softkb-change");
#endif
    os.removeObserver(BrowserSearch, "browser-search-engine-modified");

    window.controllers.removeController(this);
    window.controllers.removeController(BrowserUI);
  },

  setPluginState: function(enabled, nameMatch) {
    // XXX clear this out so that we always disable flash on startup, even
    // after the user has disabled/re-enabled plugins
    try {
      gPrefService.clearUserPref("temporary.disabledFlash");
    } catch (ex) {}

    var phs = Cc["@mozilla.org/plugin/host;1"].getService(Ci.nsIPluginHost);
    var plugins = phs.getPluginTags({ });
    for (var i = 0; i < plugins.length; ++i) {
      if (nameMatch && !nameMatch.test(plugins[i].name))
        continue;
      plugins[i].disabled = !enabled;
    }
  },

  get browsers() {
    return this._tabs.map(function(tab) { return tab.browser; });
  },

  scrollContentToTop: function scrollContentToTop() {
    this.contentScrollboxScroller.scrollTo(0, 0);
    this.pageScrollboxScroller.scrollTo(0, 0);
    this._browserView.onAfterVisibleMove();
  },

  hideSidebars: function scrollSidebarsOffscreen() {
    let container = this.contentScrollbox;
    let rect = container.getBoundingClientRect();
    this.controlsScrollboxScroller.scrollBy(Math.round(rect.left), 0);
    this._browserView.onAfterVisibleMove();
  },

  hideTitlebar: function hideTitlebar() {
    let container = this.contentScrollbox;
    let rect = container.getBoundingClientRect();
    this.pageScrollboxScroller.scrollBy(0, Math.round(rect.top));
    this.tryUnfloatToolbar();
    this._browserView.onAfterVisibleMove();
  },

  /**
   * Return the currently active <browser> object
   */
  get selectedBrowser() {
    return this._selectedTab.browser;
  },

  getTabForDocument: function(aDocument) {
    let tabs = this._tabs;
    for (let i = 0; i < tabs.length; i++) {
      if (tabs[i].browser.contentDocument === aDocument)
        return tabs[i];
    }
    return null;
  },

  getTabAtIndex: function(index) {
    if (index > this._tabs.length || index < 0)
      return null;
    return this._tabs[index];
  },

  getTabFromChrome: function(chromeTab) {
    for (var t = 0; t < this._tabs.length; t++) {
      if (this._tabs[t].chromeTab == chromeTab)
        return this._tabs[t];
    }
    return null;
  },

  addTab: function(uri, bringFront) {
    let newTab = new Tab();
    this._tabs.push(newTab);

    if (bringFront)
      this.selectedTab = newTab;

    newTab.load(uri);

    let event = document.createEvent("Events");
    event.initEvent("TabOpen", true, false);
    newTab.chromeTab.dispatchEvent(event);

    return newTab;
  },

  closeTab: function(tab) {
    if (tab instanceof XULElement)
      tab = this.getTabFromChrome(tab);

    if (!tab)
      return;

    let tabIndex = this._tabs.indexOf(tab);

    let nextTab = this._selectedTab;
    if (this._selectedTab == tab) {
      nextTab = this.getTabAtIndex(tabIndex + 1) || this.getTabAtIndex(tabIndex - 1);
      if (!nextTab)
        return;
    }

    let event = document.createEvent("Events");
    event.initEvent("TabClose", true, false);
    tab.chromeTab.dispatchEvent(event);

    this.selectedTab = nextTab;

    tab.destroy();
    this._tabs.splice(tabIndex, 1);
  },

  get selectedTab() {
    return this._selectedTab;
  },

  set selectedTab(tab) {
    let bv = this._browserView;

    if (tab instanceof XULElement)
      tab = this.getTabFromChrome(tab);

    if (!tab || this._selectedTab == tab)
      return;

    if (this._selectedTab) {
      this._selectedTab.scrollOffset = this.getScrollboxPosition(this.contentScrollboxScroller);
    }

    let isFirstTab = this._selectedTab == null;
    let lastTab = this._selectedTab;
    this._selectedTab = tab;

    tab.ensureBrowserExists();

    bv.beginBatchOperation();

    bv.setBrowser(tab.browser, tab.browserViewportState);
    bv.forceContainerResize();

    document.getElementById("tabs").selectedTab = tab.chromeTab;

    if (!isFirstTab) {
      // Update all of our UI to reflect the new tab's location
      BrowserUI.updateURI();
      getIdentityHandler().checkIdentity();

      let event = document.createEvent("Events");
      event.initEvent("TabSelect", true, false);
      event.lastTab = lastTab;
      tab.chromeTab.dispatchEvent(event);
    }

    tab.lastSelected = Date.now();

    if (tab.scrollOffset) {
      // XXX incorrect behavior if page was scrolled by tab in the background.
      let { x: scrollX, y: scrollY } = tab.scrollOffset;
      Browser.contentScrollboxScroller.scrollTo(scrollX, scrollY);
    }

    bv.commitBatchOperation();
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

  getNotificationBox: function() {
    return document.getElementById("notifications");
  },

  translatePhoneNumbers: function() {
    let doc = getBrowser().contentDocument;
    // jonas black magic (only match text nodes that contain a sequence of 4 numbers)
    let textnodes = doc.evaluate('//text()[contains(translate(., "0123456789", "^^^^^^^^^^"), "^^^^")]',
                                 doc,
                                 null,
                                 XPathResult.UNORDERED_NODE_SNAPSHOT_TYPE,
                                 null);
    let s, node, lastLastIndex;
    let re = /(\+?1? ?-?\(?\d{3}\)?[ +-\.]\d{3}[ +-\.]\d{4})/;
    for (var i = 0; i < textnodes.snapshotLength; i++) {
      node = textnodes.snapshotItem(i);
      s = node.data;
      if (s.match(re)) {
        s = s.replace(re, "<a href='tel:$1'> $1 </a>");
        try {
          let replacement = doc.createElement("span");
          replacement.innerHTML = s;
          node.parentNode.insertBefore(replacement, node);
          node.parentNode.removeChild(node);
        } catch(e) {
          //do nothing, but continue
        }
      }
    }
  },

  /** Returns true iff a tab's browser has been destroyed to free up memory. */
  sacrificeTab: function sacrificeTab() {
    let tabToClear = this._tabs.reduce(function(prevTab, currentTab) {
      if (currentTab == Browser.selectedTab || !currentTab.browser) {
        return prevTab;
      } else {
        return (prevTab && prevTab.lastSelected <= currentTab.lastSelected) ? prevTab : currentTab;
      }
    }, null);

    if (tabToClear) {
      tabToClear.saveState();
      tabToClear._destroyBrowser();
      return true;
    } else {
      return false;
    }
  },

  /**
   * Handle command event bubbling up from content.  This allows us to do chrome-
   * privileged things based on buttons in, e.g., unprivileged error pages.
   * Obviously, care should be taken not to trust events that web pages could have
   * synthesized.
   */
  _handleContentCommand: function _handleContentCommand(aEvent) {
    // Don't trust synthetic events
    if (!aEvent.isTrusted)
      return;

    var ot = aEvent.originalTarget;
    var errorDoc = ot.ownerDocument;

    // If the event came from an ssl error page, it is probably either the "Add
    // Exception…" or "Get me out of here!" button
    if (/^about:certerror\?e=nssBadCert/.test(errorDoc.documentURI)) {
      if (ot == errorDoc.getElementById("temporaryExceptionButton") ||
          ot == errorDoc.getElementById("permanentExceptionButton")) {
        try {
          // add a new SSL exception for this URL
          let uri = gIOService.newURI(errorDoc.location.href, null, null);
          let sslExceptions = new SSLExceptions();

          if (ot == errorDoc.getElementById("permanentExceptionButton")) {
            sslExceptions.addPermanentException(uri);
          } else {
            sslExceptions.addTemporaryException(uri);
          }
        } catch (e) {
          dump("EXCEPTION handle content command: " + e + "\n" );
        }

        // automatically reload after the exception was added  
        errorDoc.location.reload();
      }
      else if (ot == errorDoc.getElementById('getMeOutOfHereButton')) {
        // Get the start page from the *default* pref branch, not the user's
        var defaultPrefs = Cc["@mozilla.org/preferences-service;1"]
                          .getService(Ci.nsIPrefService).getDefaultBranch(null);
        var url = "about:blank";
        try {
          url = defaultPrefs.getCharPref("browser.startup.homepage");
          // If url is a pipe-delimited set of pages, just take the first one.
          if (url.indexOf("|") != -1)
            url = url.split("|")[0];
        } catch (e) { /* Fall back on about blank */ }

        Browser.selectedBrowser.loadURI(url, null, null, false);
      }
    } 
    else if (/^about:neterror\?e=netOffline/.test(errorDoc.documentURI)) {
      if (ot == errorDoc.getElementById("errorTryAgain")) {
        // Make sure we're online before attempting to load
        Util.forceOnline();
      }
    }
  },

  /**
   * Compute the sidebar percentage visibility.
   *
   * @param [optional] dx
   * @param [optional] dy an offset distance at which to perform the visibility
   * computation
   * @return [leftVisibility, rightVisiblity, leftTotalWidth, rightTotalWidth]
   */
  computeSidebarVisibility: function computeSidebarVisibility(dx, dy) {
    function visibility(bar, visrect) {
      let w = bar.width;
      bar.restrictTo(visrect);
      return bar.width / w;
    }

    if (!dx) dx = 0;
    if (!dy) dy = 0;

    let leftbarCBR = document.getElementById('tabs-container').getBoundingClientRect();
    let ritebarCBR = document.getElementById('browser-controls').getBoundingClientRect();

    let leftbar = new Rect(Math.round(leftbarCBR.left) - dx, 0, Math.round(leftbarCBR.width), 1);
    let ritebar = new Rect(Math.round(ritebarCBR.left) - dx, 0, Math.round(ritebarCBR.width), 1);
    let leftw = leftbar.width;
    let ritew = ritebar.width;

    let visrect = new Rect(0, 0, window.innerWidth, 1);

    let leftvis = visibility(leftbar, visrect);
    let ritevis = visibility(ritebar, visrect);

    return [leftvis, ritevis, leftw, ritew];
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

    if (leftvis != 0 && leftvis != 1) {
      if (leftvis >= 0.6666) {
        snappedX = -((1 - leftvis) * leftw);
      } else {
        snappedX = leftvis * leftw;
      }
    }
    else if (ritevis != 0 && ritevis != 1) {
      if (ritevis >= 0.6666) {
        snappedX = (1 - ritevis) * ritew;
      } else {
        snappedX = -ritevis * ritew;
      }
    }

    return Math.round(snappedX);
  },

  tryFloatToolbar: function tryFloatToolbar(dx, dy) {
    if (this.floatedWhileDragging)
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

  zoom: function zoom(aDirection) {
    Browser._browserView.zoom(aDirection);
    //Browser.forceChromeReflow();  // Zoom causes a width/height change to the
                                    // BrowserView's containing element, but
                                    // sometimes doesn't cause the reflow that
                                    // resizes the parent right away, so we
                                    // can preempt it here.  Not needed anywhere
                                    // currently but if this becomes API or is
                                    // needed then uncomment this line.
  },

  /**
   * Find the needed zoom level for zooming on an element
   */
  _getZoomLevelForElement: function _getZoomLevelForElement(element) {
    const margin = 15;

    let bv = this._browserView;
    let elRect = bv.browserToViewportRect(Browser.getBoundingContentRect(element));

    let vis = bv.getVisibleRect();
    return BrowserView.Util.clampZoomLevel(bv.getZoomLevel() * vis.width / (elRect.width + margin * 2));
  },

  /**
   * Find a good zoom rectangle for point specified in browser coordinates.
   * @return Point in viewport coordinates
   */
  _getZoomRectForPoint: function _getZoomRectForPoint(x, y, zoomLevel) {
    let bv = Browser._browserView;
    let vis = bv.getVisibleRect();
    x = bv.browserToViewport(x);
    y = bv.browserToViewport(y);

    zoomLevel = Math.min(kBrowserViewZoomLevelMax, zoomLevel);
    let zoomRatio = zoomLevel / bv.getZoomLevel();
    let newVisW = vis.width / zoomRatio, newVisH = vis.height / zoomRatio;
    let result = new Rect(x - newVisW / 2, y - newVisH / 2, newVisW, newVisH);

    // Make sure rectangle doesn't poke out of viewport
    return result.translateInside(bv._browserViewportState.viewportRect);
  },

  setVisibleRect: function setVisibleRect(rect) {
    let bv = Browser._browserView;
    let vis = bv.getVisibleRect();
    let zoomRatio = vis.width / rect.width;
    let zoomLevel = bv.getZoomLevel() * zoomRatio;
    let scrollX = rect.left * zoomRatio;
    let scrollY = rect.top * zoomRatio;

    // The order of operations below is important for artifacting and for performance. Important
    // side effects of functions are noted below.

    // Hardware scrolling happens immediately when scrollTo is called.  Hide to prevent artifacts.
    bv.beginOffscreenOperation();

    // We must scroll to the correct area before TileManager is informed of the change
    // so that only one render is done. Ensures setZoomLevel puts it off.
    bv.beginBatchOperation();

    // Critical rect changes when controls are hidden. Must hide before tilemanager viewport.
    Browser.hideSidebars();
    Browser.hideTitlebar();
    bv.setZoomLevel(zoomLevel);

    // Ensure container is big enough for scroll values.
    bv.forceContainerResize();
    Browser.forceChromeReflow();
    Browser.contentScrollboxScroller.scrollTo(scrollX, scrollY);
    bv.onAfterVisibleMove();

    // Inform tile manager, which happens to render new tiles too. Must call in case a batch
    // operation was in progress before zoom.
    bv.forceViewportChange();

    bv.commitBatchOperation();
    bv.commitOffscreenOperation();
  },

  zoomToPoint: function zoomToPoint(cX, cY) {
    let [elementX, elementY] = Browser.transformClientToBrowser(cX, cY);

    let element = Browser.elementFromPoint(elementX, elementY);
    if (!element)
      return false;

    let bv = this._browserView;
    let oldZoomLevel = bv.getZoomLevel();
    let zoomLevel = this._getZoomLevelForElement(element);
    let zoomRatio = oldZoomLevel / zoomLevel;

    // Don't zoom in a marginal amount, but be more lenient for the first zoom.
    // > 2/3 means operation increases the zoom level by less than 1.5
    // > 9/10 means operation increases the zoom level by less than 1.1
    let zoomTolerance = (bv.isDefaultZoom()) ? .9 : .6666;
    if (zoomRatio >= zoomTolerance)
       return false;

    let elRect = Browser.getBoundingContentRect(element);
    let zoomRect = this._getZoomRectForPoint(elRect.center().x, elementY, zoomLevel);

    this.setVisibleRect(zoomRect);
    return true;
  },

  zoomFromPoint: function zoomFromPoint(cX, cY) {
    let bv = this._browserView;
    
    let zoomLevel = bv.getZoomForPage();
    if (!bv.isDefaultZoom()) {
      let [elementX, elementY] = this.transformClientToBrowser(cX, cY);
      let zoomRect = this._getZoomRectForPoint(elementX, elementY, zoomLevel);
      this.setVisibleRect(zoomRect);
    }
  },

  getBoundingContentRect: function getBoundingContentRect(contentElem) {
    let tab = Browser.getTabForDocument(contentElem.ownerDocument);
    if (!tab)
      return null;
    let browser = tab.browser;
    if (!browser)
      return null;

    let offset = BrowserView.Util.getContentScrollOffset(browser);

    let r = contentElem.getBoundingClientRect();

    // step out of iframes and frames, offsetting scroll values
    for (let frame = contentElem.ownerDocument.defaultView; frame != browser.contentWindow; frame = frame.parent) {
      // adjust client coordinates' origin to be top left of iframe viewport
      let rect = frame.frameElement.getBoundingClientRect();
      offset.add(rect.left, rect.top);
    }

    return new Rect(r.left + offset.x, r.top + offset.y, r.width, r.height);
  },

  /**
   * Transform x and y from client coordinates to BrowserView coordinates.
   */
  clientToBrowserView: function clientToBrowserView(x, y) {
    let container = document.getElementById("tile-container");
    let containerBCR = container.getBoundingClientRect();

    let x0 = Math.round(containerBCR.left);
    let y0;
    if (arguments.length > 1)
      y0 = Math.round(containerBCR.top);

    return (arguments.length > 1) ? [x - x0, y - y0] : (x - x0);
  },

  browserViewToClient: function browserViewToClient(x, y) {
    let container = document.getElementById("tile-container");
    let containerBCR = container.getBoundingClientRect();

    let x0 = Math.round(-containerBCR.left);
    let y0;
    if (arguments.length > 1)
      y0 = Math.round(-containerBCR.top);

    return (arguments.length > 1) ? [x - x0, y - y0] : (x - x0);
  },

  browserViewToClientRect: function browserViewToClientRect(rect) {
    let container = document.getElementById("tile-container");
    let containerBCR = container.getBoundingClientRect();
    return rect.clone().translate(Math.round(containerBCR.left), Math.round(containerBCR.top));
  },

  /**
   * turn client coordinates into page-relative ones (adjusted for
   * zoom and page position)
   */
  transformClientToBrowser: function transformClientToBrowser(cX, cY) {
    return this.clientToBrowserView(cX, cY).map(this._browserView.viewportToBrowser);
  },

  /**
   * @param x,y Browser coordinates
   * @return Element at position, null if no active browser or no element found
   */
  elementFromPoint: function elementFromPoint(x, y) {
    //Util.dumpLn("*** elementFromPoint: page ", x, ",", y);

    let browser = this._browserView.getBrowser();
    if (!browser) return null;

    // browser's elementFromPoint expect browser-relative client coordinates.
    // subtract browser's scroll values to adjust
    let cwu = BrowserView.Util.getBrowserDOMWindowUtils(browser);
    let scrollX = {}, scrollY = {};
    cwu.getScrollXY(false, scrollX, scrollY);
    x = x - scrollX.value;
    y = y - scrollY.value;
    let elem = cwu.elementFromPoint(x, y,
                                    true,   /* ignore root scroll frame*/
                                    false); /* don't flush layout */

    // step through layers of IFRAMEs and FRAMES to find innermost element
    while (elem && (elem instanceof HTMLIFrameElement || elem instanceof HTMLFrameElement)) {
      // adjust client coordinates' origin to be top left of iframe viewport
      let rect = elem.getBoundingClientRect();
      x = x - rect.left;
      y = y - rect.top;
      elem = elem.contentDocument.elementFromPoint(x, y);
    }

    return elem;
  },

  /**
   * Return the visible rect in coordinates with origin at the (left, top) of
   * the tile container, i.e. BrowserView coordinates.
   */
  getVisibleRect: function getVisibleRect() {
    let stack = document.getElementById("tile-stack");
    let container = document.getElementById("tile-container");
    let containerBCR = container.getBoundingClientRect();

    let x = Math.round(-containerBCR.left);
    let y = Math.round(-containerBCR.top);
    let w = window.innerWidth;
    let h = stack.getBoundingClientRect().height;

    return new Rect(x, y, w, h);
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
  }

};

Browser.MainDragger = function MainDragger(browserView) {
  this.bv = browserView;
  this.draggedFrame = null;
};

Browser.MainDragger.prototype = {

  isDraggable: function isDraggable(target, scroller) { return true; },

  dragStart: function dragStart(clientX, clientY, target, scroller) {
    let [x, y] = Browser.transformClientToBrowser(clientX, clientY);
    let element = Browser.elementFromPoint(x, y);

    this.draggedFrame = null;
    if (element)
      this.draggedFrame = element.ownerDocument.defaultView;

    this.bv.pauseRendering();
  },

  dragStop: function dragStop(dx, dy, scroller) {
    this.draggedFrame = null;
    this.dragMove(Browser.snapSidebars(), 0, scroller);

    Browser.tryUnfloatToolbar();

    this.bv.resumeRendering();
  },

  dragMove: function dragMove(dx, dy, scroller) {
    let elem = this.draggedFrame;
    let doffset = new Point(dx, dy);
    let render = false;

    this.bv.onBeforeVisibleMove(dx, dy);

    // First calculate any panning to take sidebars out of view
    let panOffset = this._panControlsAwayOffset(doffset);

    // Do all iframe panning
    if (elem) {
      while (elem.frameElement && !doffset.isZero()) {
        this._panFrame(elem, doffset);
        elem = elem.frameElement;
        render = true;
      }
    }

    // Do content panning
    this._panScroller(Browser.contentScrollboxScroller, doffset);

    // Any leftover panning in doffset would bring controls into view. Add to sidebar
    // away panning for the total scroll offset.
    doffset.add(panOffset);
    Browser.tryFloatToolbar(doffset.x, 0);
    this._panScroller(Browser.controlsScrollboxScroller, doffset);
    this._panScroller(Browser.pageScrollboxScroller, doffset);

    this.bv.onAfterVisibleMove();

    if (render)
      this.bv.renderNow();

    return !doffset.equals(dx, dy);
  },

  /** Return offset that pans controls away from screen. Updates doffset with leftovers. */
  _panControlsAwayOffset: function(doffset) {
    let x = 0, y = 0, rect;

    rect = Rect.fromRect(Browser.pageScrollbox.getBoundingClientRect()).map(Math.round);
    if (doffset.x < 0 && rect.right < window.innerWidth)
      x = Math.max(doffset.x, rect.right - window.innerWidth);
    if (doffset.x > 0 && rect.left > 0)
      x = Math.min(doffset.x, rect.left);

    let height = document.getElementById("tile-stack").getBoundingClientRect().height;
    rect = Rect.fromRect(Browser.contentScrollbox.getBoundingClientRect()).map(Math.round);
    if (doffset.y < 0 && rect.bottom < height)
      y = Math.max(doffset.y, rect.bottom - height);
    if (doffset.y > 0 && rect.top > 0)
      y = Math.min(doffset.y, rect.top);

    doffset.subtract(x, y);
    return new Point(x, y);
  },

  /** Pan scroller by the given amount. Updates doffset with leftovers. */
  _panScroller: function _panScroller(scroller, doffset) {
    let { x: x0, y: y0 } = Browser.getScrollboxPosition(scroller);
    scroller.scrollBy(doffset.x, doffset.y);
    let { x: x1, y: y1 } = Browser.getScrollboxPosition(scroller);
    doffset.subtract(x1 - x0, y1 - y0);
  },

  /** Pan frame by the given amount. Updates doffset with leftovers. */
  _panFrame: function _panFrame(frame, doffset) {
    let origX = {}, origY = {}, newX = {}, newY = {};
    let windowUtils = frame.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);

    windowUtils.getScrollXY(false, origX, origY);
    frame.scrollBy(doffset.x, doffset.y);
    windowUtils.getScrollXY(false, newX, newY);

    doffset.subtract(newX.value - origX.value, newY.value - origY.value);
  }
  
};

function nsBrowserAccess()
{
}

nsBrowserAccess.prototype = {
  QueryInterface: function(aIID) {
    if (aIID.equals(Ci.nsIBrowserDOMWindow) || aIID.equals(Ci.nsISupports))
      return this;
    throw Components.results.NS_NOINTERFACE;
  },

  openURI: function(aURI, aOpener, aWhere, aContext) {
    var isExternal = (aContext == Ci.nsIBrowserDOMWindow.OPEN_EXTERNAL);
    if (isExternal && aURI && aURI.schemeIs("chrome")) {
      //dump("use -chrome command-line option to load external chrome urls\n");
      return null;
    }

    var loadflags = isExternal ?
                       Ci.nsIWebNavigation.LOAD_FLAGS_FROM_EXTERNAL :
                       Ci.nsIWebNavigation.LOAD_FLAGS_NONE;
    var location;
    if (aWhere == Ci.nsIBrowserDOMWindow.OPEN_DEFAULTWINDOW) {
      switch (aContext) {
        case Ci.nsIBrowserDOMWindow.OPEN_EXTERNAL :
          aWhere = gPrefService.getIntPref("browser.link.open_external");
          break;
        default : // OPEN_NEW or an illegal value
          aWhere = gPrefService.getIntPref("browser.link.open_newwindow");
      }
    }

    var newWindow;
    if (aWhere == Ci.nsIBrowserDOMWindow.OPEN_NEWWINDOW) {
      var url = aURI ? aURI.spec : "about:blank";
      newWindow = openDialog("chrome://browser/content/browser.xul", "_blank",
                             "all,dialog=no", url, null, null, null);
    } else {
      if (aWhere == Ci.nsIBrowserDOMWindow.OPEN_NEWTAB)
        newWindow = Browser.addTab("about:blank", true).browser.contentWindow;
      else
        newWindow = aOpener ? aOpener.top : browser.contentWindow;
    }

    try {
      var referrer;
      if (aURI) {
        if (aOpener) {
          location = aOpener.location;
          referrer = gIOService.newURI(location, null, null);
        }
        newWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                 .getInterface(Ci.nsIWebNavigation)
                 .loadURI(aURI.spec, loadflags, referrer, null, null);
      }
      newWindow.focus();
    } catch(e) { }

    return newWindow;
  },

  isTabContentWindow: function(aWindow) {
    return Browser.browsers.some(function (browser) browser.contentWindow == aWindow);
  }
};

const BrowserSearch = {
  engines: null,
  _allEngines: [],

  observe: function (aSubject, aTopic, aData) {
    if (aTopic != "browser-search-engine-modified")
      return;

    switch (aData) {
      case "engine-added":
      case "engine-removed":
        // force a rebuild of the prefs list, if needed
        // XXX this is inefficient, shouldn't have to rebuild the entire list
        if (ExtensionsView._list)
          ExtensionsView.getAddonsFromLocal();

        // fall through
      case "engine-changed":
        // XXX we should probably also update the ExtensionsView list here once
        // that's efficient, since the icon can change (happen during an async
        // installs from the web)

        // blow away our cache
        this._engines = null;
        break;
      case "engine-current":
        // Not relevant
        break;
    }
  },

  get _currentEngines() {
    let doc = getBrowser().contentDocument;
    return this._allEngines.filter(function(element) element.doc === doc, this);
  },

  get searchService() {
    delete this.searchService;
    return this.searchService = Cc["@mozilla.org/browser/search-service;1"].getService(Ci.nsIBrowserSearchService);
  },

  get engines() {
    if (this._engines)
      return this._engines;
    return this._engines = this.searchService.getVisibleEngines({ });    
  },

  addPageSearchEngine: function (aEngine, aDocument) {
    // Clean the engine referenced for document that didn't exist anymore
    let browsers = Browser.browsers;
    this._allEngines = this._allEngines.filter(function(element) {
       return browsers.some(function (browser) browser.contentDocument == element.doc);
    }, this);

    // Prevent duplicate
    if (!this._allEngines.some(function (e) {
        return (e.engine.title == aEngine.title) && (e.doc == aDocument);
    })) this._allEngines.push( {engine:aEngine, doc:aDocument});
  },

  updatePageSearchEngines: function() {
    // Check to see whether we've already added an engine with this title in
    // the search list
    let newEngines = this._currentEngines.filter(function(element) {
      return !this.engines.some(function (e) e.name == element.engine.title);
    }, this);

    let container = document.getElementById('search-container');
    let buttons = container.getElementsByAttribute("class", "search-engine-button button-dark");
    for (let i=0; i<buttons.length; i++)
      container.removeChild(buttons[i]);

    if (newEngines.length == 0) {
      container.collapsed = true;
      return;
    }

    // XXX limit to the first search engine for now
    for (let i = 0; i<1; i++) {
      let button = document.createElement("button");
      button.className = "search-engine-button button-dark";
      button.setAttribute("oncommand", "BrowserSearch.addPermanentSearchEngine(this.engine);this.parentNode.collapsed=true;");
      
      let engine = newEngines[i];
      button.engine = engine.engine;
      button.setAttribute("label", engine.engine.title);
      button.setAttribute("image", BrowserUI._favicon.src);

      container.appendChild(button);
    }

    container.collapsed = false;
  },

  addPermanentSearchEngine: function (aEngine) {
    let iconURL = BrowserUI._favicon.src;
    this.searchService.addEngine(aEngine.href, Ci.nsISearchEngine.DATA_XML, iconURL, false);

    this._engines = null;
  },

  updateSearchButtons: function() {
    if (this._engines)
      return;

    // Clean the previous search engines button
    var container = document.getElementById("search-buttons");
    while (container.hasChildNodes())
      container.removeChild(container.lastChild);

    let engines = this.engines;
    for (var e = 0; e < engines.length; e++) {
      var button = document.createElement("radio");
      var engine = engines[e];
      button.id = engine.name;
      button.setAttribute("label", engine.name);
      button.className = "searchengine";
      if (engine.iconURI)
        button.setAttribute("src", engine.iconURI.spec);
      container.appendChild(button);
      button.engine = engine;
    }
  }
}

/** Watches for mouse events in chrome and sends them to content. */
function ContentCustomClicker(browserView) {
  this._browserView = browserView;
}

ContentCustomClicker.prototype = {
    /** Dispatch a mouse event with chrome client coordinates. */
    _dispatchMouseEvent: function _dispatchMouseEvent(name, cX, cY) {
      let browser = this._browserView.getBrowser();
      if (browser) {
        let [x, y] = Browser.transformClientToBrowser(cX, cY);
        let cwu = BrowserView.Util.getBrowserDOMWindowUtils(browser);
        let scrollX = {}, scrollY = {};
        cwu.getScrollXY(false, scrollX, scrollY);
        cwu.sendMouseEvent(name, x - scrollX.value, y - scrollY.value, 0, 1, 0, true);
      }
    },

    mouseDown: function mouseDown(cX, cY) {
    },

    mouseUp: function mouseUp(cX, cY) {
    },

    singleClick: function singleClick(cX, cY, modifiers) {
      let [elementX, elementY] = Browser.transformClientToBrowser(cX, cY);
      let element = Browser.elementFromPoint(elementX, elementY);
      if (modifiers == 0) {
        if (element instanceof HTMLOptionElement)
          element = element.parentNode;

        if (FormHelper.canShowUIFor(element)) {
          FormHelper.open(element);
          return;
        }

        this._dispatchMouseEvent("mousedown", cX, cY);
        this._dispatchMouseEvent("mouseup", cX, cY);
      }
      else if (modifiers == Ci.nsIDOMNSEvent.CONTROL_MASK) {
        let uri = Util.getHrefForElement(element);
        if (uri)
          Browser.addTab(uri, false);
      }
    },

    doubleClick: function doubleClick(cX1, cY1, cX2, cY2) {
      const kDoubleClickRadius = 32;
      
      let maxRadius = kDoubleClickRadius * Browser._browserView.getZoomLevel();
      let isClickInRadius = (Math.abs(cX1 - cX2) < maxRadius && Math.abs(cY1 - cY2) < maxRadius);
      if (isClickInRadius && !Browser.zoomToPoint(cX1, cY1))
        Browser.zoomFromPoint(cX1, cY1);
    },

    toString: function toString() {
      return "[ContentCustomClicker] { }";
    }
};

/**
 * Utility class to handle manipulations of the identity indicators in the UI
 */
function IdentityHandler() {
  this._staticStrings = {};
  this._staticStrings[this.IDENTITY_MODE_DOMAIN_VERIFIED] = {
    encryption_label: Elements.browserBundle.getString("identity.encrypted2")
  };
  this._staticStrings[this.IDENTITY_MODE_IDENTIFIED] = {
    encryption_label: Elements.browserBundle.getString("identity.encrypted2")
  };
  this._staticStrings[this.IDENTITY_MODE_UNKNOWN] = {
    encryption_label: Elements.browserBundle.getString("identity.unencrypted2")
  };

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

  /**
   * Helper to parse out the important parts of _lastStatus (of the SSL cert in
   * particular) for use in constructing identity UI strings
   */
  getIdentityData: function() {
    var result = {};
    var status = this._lastStatus.QueryInterface(Ci.nsISSLStatus);
    var cert = status.serverCert;

    // Human readable name of Subject
    result.subjectOrg = cert.organization;

    // SubjectName fields, broken up for individual access
    if (cert.subjectName) {
      result.subjectNameFields = {};
      cert.subjectName.split(",").forEach(function(v) {
        var field = v.split("=");
        this[field[0]] = field[1];
      }, result.subjectNameFields);

      // Call out city, state, and country specifically
      result.city = result.subjectNameFields.L;
      result.state = result.subjectNameFields.ST;
      result.country = result.subjectNameFields.C;
    }

    // Human readable name of Certificate Authority
    result.caOrg =  cert.issuerOrganization || cert.issuerCommonName;
    result.cert = cert;

    return result;
  },

  /**
   * Determine the identity of the page being displayed by examining its SSL cert
   * (if available) and, if necessary, update the UI to reflect this.
   */
  checkIdentity: function() {
    let state = Browser.selectedTab.getIdentityState();
    let location = getBrowser().contentWindow.location;
    let currentStatus = getBrowser().securityUI.QueryInterface(Ci.nsISSLStatusProvider).SSLStatus;
    
    this._lastStatus = currentStatus;
    this._lastLocation = {};
    try {
      // make a copy of the passed in location to avoid cycles
      this._lastLocation = { host: location.host, hostname: location.hostname, port: location.port };
    } catch (ex) { }

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
    let strings = Elements.browserBundle;

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
      var tooltip = strings.getFormattedString("identity.identified.verifier",
                                               [iData.caOrg]);

      // Check whether this site is a security exception. XPConnect does the right
      // thing here in terms of converting _lastLocation.port from string to int, but
      // the overrideService doesn't like undefined ports, so make sure we have
      // something in the default case (bug 432241).
      if (this._overrideService.hasMatchingOverride(this._lastLocation.hostname,
                                                    (this._lastLocation.port || 443),
                                                    iData.cert, {}, {}))
        tooltip = strings.getString("identity.identified.verified_by_you");
    }
    else if (newMode == this.IDENTITY_MODE_IDENTIFIED) {
      // If it's identified, then we can populate the dialog with credentials
      iData = this.getIdentityData();
      tooltip = strings.getFormattedString("identity.identified.verifier",
                                           [iData.caOrg]);
    }
    else {
      tooltip = strings.getString("identity.unknown.tooltip");
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

    let strings = Elements.browserBundle;

    if (newMode == this.IDENTITY_MODE_DOMAIN_VERIFIED) {
      var iData = this.getIdentityData();
      var host = this.getEffectiveHost();
      var owner = strings.getString("identity.ownerUnknown2");
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
        supplemental += iData.city + " ";
      if (iData.state && iData.country)
        supplemental += strings.getFormattedString("identity.identified.state_and_country",
                                                   [iData.state, iData.country]);
      else if (iData.state) // State only
        supplemental += iData.state;
      else if (iData.country) // Country only
        supplemental += iData.country;
    }
    else {
      // These strings will be hidden in CSS anyhow
      host = "";
      owner = "";
    }

    // Push the appropriate strings out to the UI
    this._identityPopupContentHost.textContent = host;
    this._identityPopupContentOwner.textContent = owner;
    this._identityPopupContentSupp.textContent = supplemental;
    this._identityPopupContentVerif.textContent = verifier;

    // Update the search engines results
    BrowserSearch.updatePageSearchEngines();
  },

  show: function ih_show() {
    // dismiss any dialog which hide the identity popup
    while (BrowserUI.activeDialog)
      BrowserUI.activeDialog.close();

    this._identityPopup.hidden = false;
    this._identityPopup.top = BrowserUI.toolbarH;
    this._identityPopup.focus();

    this._identityBox.setAttribute("open", "true");

    // Update the popup strings
    this.setPopupMessages(this._identityBox.getAttribute("mode") || this.IDENTITY_MODE_UNKNOWN);

    BrowserUI.pushPopup(this, [this._identityPopup, this._identityBox]);
    BrowserUI.lockToolbar();
  },

  hide: function ih_hide() {
    this._identityPopup.hidden = true;
    this._identityBox.removeAttribute("open");
    
    BrowserUI.popPopup();
    BrowserUI.unlockToolbar();
  },

  /**
   * Click handler for the identity-box element in primary chrome.
   */
  handleIdentityButtonEvent: function(event) {
    event.stopPropagation();

    if ((event.type == "click" && event.button != 0) ||
        (event.type == "keypress" && event.charCode != KeyEvent.DOM_VK_SPACE &&
         event.keyCode != KeyEvent.DOM_VK_RETURN))
      return; // Left click, space or enter only

    if (this._identityPopup.hidden)
      this.show();
    else
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
const gPopupBlockerObserver = {
  _kIPM: Ci.nsIPermissionManager,

  onUpdatePageReport: function onUpdatePageReport(aEvent)
  {
    var cBrowser = Browser.selectedBrowser;
    if (aEvent.originalTarget != cBrowser)
      return;

    if (!cBrowser.pageReport)
      return;

    let pm = Cc["@mozilla.org/permissionmanager;1"].getService(Ci.nsIPermissionManager);
    let result = pm.testExactPermission(Browser.selectedBrowser.currentURI, "popup");
    if (result == Ci.nsIPermissionManager.DENY_ACTION)
      return;

    // Only show the notification again if we've not already shown it. Since
    // notifications are per-browser, we don't need to worry about re-adding
    // it.
    if (!cBrowser.pageReport.reported) {
      if(gPrefService.getBoolPref("privacy.popups.showBrowserMessage")) {
        var brandBundle = document.getElementById("bundle_brand");
        var brandShortName = brandBundle.getString("brandShortName");
        var message;
        var popupCount = cBrowser.pageReport.length;

        let strings = Elements.browserBundle;
        if (popupCount > 1)
          message = strings.getFormattedString("popupWarningMultiple", [brandShortName, popupCount]);
        else
          message = strings.getFormattedString("popupWarning", [brandShortName]);

        var notificationBox = Browser.getNotificationBox();
        var notification = notificationBox.getNotificationWithValue("popup-blocked");
        if (notification) {
          notification.label = message;
        }
        else {
          var buttons = [
            {
              label: strings.getString("popupButtonAllowOnce"),
              accessKey: null,
              callback: function() { gPopupBlockerObserver.showPopupsForSite(); }
            },
            {
              label: strings.getString("popupButtonAlwaysAllow2"),
              accessKey: null,
              callback: function() { gPopupBlockerObserver.allowPopupsForSite(); }
            },
            {
              label: strings.getString("popupButtonNeverWarn2"),
              accessKey: null,
              callback: function() { gPopupBlockerObserver.denyPopupsForSite(); }
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

  allowPopupsForSite: function allowPopupsForSite(aEvent) {
    var currentURI = Browser.selectedBrowser.currentURI;
    var pm = Cc["@mozilla.org/permissionmanager;1"].getService(this._kIPM);
    pm.add(currentURI, "popup", this._kIPM.ALLOW_ACTION);

    Browser.getNotificationBox().removeCurrentNotification();
  },

  denyPopupsForSite: function denyPopupsForSite(aEvent) {
    var currentURI = Browser.selectedBrowser.currentURI;
    var pm = Cc["@mozilla.org/permissionmanager;1"].getService(this._kIPM);
    pm.add(currentURI, "popup", this._kIPM.DENY_ACTION);

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
        if (popupURIspec == "" || popupURIspec == "about:blank" ||
            popupURIspec == uri.spec)
          continue;

        let popupFeatures = pageReport[i].popupWindowFeatures;
        let popupName = pageReport[i].popupWindowName;

        Browser.addTab(popupURIspec, false);
      }
    }
  }
};

const gXPInstallObserver = {
  observe: function xpi_observer(aSubject, aTopic, aData)
  {
    var brandBundle = document.getElementById("bundle_brand");
    switch (aTopic) {
      case "xpinstall-install-blocked":
        var installInfo = aSubject.QueryInterface(Ci.nsIXPIInstallInfo);
        var host = installInfo.originatingURI.host;
        var brandShortName = brandBundle.getString("brandShortName");
        var notificationName, messageString, buttons;
        var strings = Elements.browserBundle;
        if (!gPrefService.getBoolPref("xpinstall.enabled")) {
          notificationName = "xpinstall-disabled";
          if (gPrefService.prefIsLocked("xpinstall.enabled")) {
            messageString = strings.getString("xpinstallDisabledMessageLocked");
            buttons = [];
          }
          else {
            messageString = strings.getFormattedString("xpinstallDisabledMessage",
                                                             [brandShortName, host]);
            buttons = [{
              label: strings.getString("xpinstallDisabledButton"),
              accessKey: null,
              popup: null,
              callback: function editPrefs() {
                gPrefService.setBoolPref("xpinstall.enabled", true);
                return false;
              }
            }];
          }
        }
        else {
          notificationName = "xpinstall";
          messageString = strings.getFormattedString("xpinstallPromptWarning",
                                                           [brandShortName, host]);

          buttons = [{
            label: strings.getString("xpinstallPromptAllowButton"),
            accessKey: null,
            popup: null,
            callback: function() {
              // Kick off the xpinstall
              var mgr = Cc["@mozilla.org/xpinstall/install-manager;1"].createInstance(Ci.nsIXPInstallManager);
              mgr.initManagerWithInstallInfo(installInfo);
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

const gSessionHistoryObserver = {
  observe: function sho_observe(subject, topic, data) {
    if (topic != "browser:purge-session-history")
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

var MemoryObserver = {
  observe: function() {
    let memory = Cc["@mozilla.org/xpcom/memory-service;1"].getService(Ci.nsIMemory);
    do {
      Browser.windowUtils.garbageCollect();      
    } while (memory.isLowMemory() && Browser.sacrificeTab());
  }
};

#ifdef WINCE
// Windows Mobile does not resize the window automatically when the soft
// keyboard is displayed. Maemo does resize the window.
var SoftKeyboardObserver = {
  observe: function sko_observe(subject, topic, data) {
    if (topic === "softkb-change") {
      // The rect passed to us is the space available to our window, so
      // let's use it to resize the main window
      let rect = JSON.parse(data);
      if (rect) {
        let height = rect.bottom - rect.top;
        let width = rect.right - rect.left;
        let popup = document.getElementById("popup_autocomplete");
        popup.height = height - BrowserUI.toolbarH;
        popup.width = width;
      }
    }
  }
};
#endif

function getNotificationBox(aWindow) {
  return Browser.getNotificationBox();
}

function importDialog(parent, src, arguments) {
  // load the dialog with a synchronous XHR
  let xhr = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].createInstance();
  xhr.open("GET", src, false);
  xhr.overrideMimeType("text/xml");
  xhr.send(null);
  if (!xhr.responseXML)
    return null;
  
  let currentNode;
  let nodeIterator = xhr.responseXML.createNodeIterator(xhr.responseXML,
                                                    NodeFilter.SHOW_TEXT,
                                                    null,
                                                    false);
  while (currentNode = nodeIterator.nextNode()) {
    let trimmed = currentNode.nodeValue.replace(/^\s\s*/, "").replace(/\s\s*$/, "");
    if (!trimmed.length)
      currentNode.parentNode.removeChild(currentNode);
  }
            
  let doc = xhr.responseXML.documentElement;
 
  var dialog  = null;
  
  // we need to insert before select-container if we want it to show correctly
  let selectContainer = document.getElementById("select-container");
  let parent = selectContainer.parentNode;
  
  // emit DOMWillOpenModalDialog event
  let event = document.createEvent("Events");
  event.initEvent("DOMWillOpenModalDialog", true, false);
  let dispatcher = parent || getBrowser();
  dispatcher.dispatchEvent(event);

  // create a full-screen semi-opaque box as a background 
  let back = document.createElement("box");
  back.setAttribute("class", "modal-block");
  dialog = back.appendChild(document.importNode(doc, true));
  parent.insertBefore(back, selectContainer);
  
  dialog.arguments = arguments;
  dialog.parent = parent;
  return dialog;
}

function showDownloadManager(aWindowContext, aID, aReason) {
  BrowserUI.showPanel("downloads-container");
  // TODO: select the download with aID
}

var AlertsHelper = {
  _timeoutID: -1,
  _listener: null,
  _cookie: "",
  _clickable: false,

  showAlertNotification: function ah_show(aImageURL, aTitle, aText, aTextClickable, aCookie, aListener) {
    this._clickable = aTextClickable || false;
    this._listener = aListener || null;
    this._cookie = aCookie || "";

    document.getElementById("alerts-image").setAttribute("src", aImageURL);
    document.getElementById("alerts-title").value = aTitle;
    document.getElementById("alerts-text").textContent = aText;

    let container = document.getElementById("alerts-container");
    container.hidden = false;

    let rect = container.getBoundingClientRect();
    container.top = window.innerHeight - (rect.height + 20);
    container.left = window.innerWidth - (rect.width + 20);

    let timeout = gPrefService.getIntPref("alerts.totalOpenTime");
    let self = this;
    this._timeoutID = setTimeout(function() { self._timeoutAlert(); }, timeout);
  },

  _timeoutAlert: function ah__timeoutAlert() {
    this._timeoutID = -1;
    let container = document.getElementById("alerts-container");
    container.hidden = true;

    if (this._listener)
      this._listener.observe(null, "alertfinished", this._cookie);

    // TODO: add slide to UI
  },

  click: function ah_click(aEvent) {
    if (this._clickable && this._listener)
      this._listener.observe(null, "alertclickcallback", this._cookie);

    if (this._timeoutID != -1) {
      clearTimeout(this._timeoutID);
      this._timeoutAlert();
    }
  }
};

var HelperAppDialog = {
  _launcher: null,

  show: function had_show(aLauncher) {
    this._launcher = aLauncher;
    document.getElementById("helperapp-target").value = this._launcher.suggestedFileName;

    if (!this._launcher.MIMEInfo.hasDefaultHandler)
      document.getElementById("helperapp-open").disabled = true;

    let container = document.getElementById("helperapp-container");
    container.hidden = false;

    let rect = container.getBoundingClientRect();
    container.top = (window.innerHeight - rect.height) / 2;
    container.left = (window.innerWidth - rect.width) / 2;
  },

  save: function had_save() {
    this._launcher.saveToDisk(null, false);
    this.close();
  },

  open: function had_open() {
    this._launcher.launchWithApplication(null, false);
    this.close();
  },

  close: function had_close() {
    document.getElementById("helperapp-target").value = "";
    let container = document.getElementById("helperapp-container");
    container.hidden = true;
  }
};

function ProgressController(tab) {
  this._tab = tab;

  // Properties used to cache security state used to update the UI
  this.state = null;
  this._hostChanged = false; // onLocationChange will flip this bit
}

ProgressController.prototype = {
  get browser() {
    return this._tab.browser;
  },

  onStateChange: function onStateChange(aWebProgress, aRequest, aStateFlags, aStatus) {
    if (aStateFlags & Ci.nsIWebProgressListener.STATE_IS_NETWORK) {
      if (aStateFlags & Ci.nsIWebProgressListener.STATE_START)
        this._networkStart();
      else if (aStateFlags & Ci.nsIWebProgressListener.STATE_STOP)
        this._networkStop();
    } else if (aStateFlags & Ci.nsIWebProgressListener.STATE_IS_DOCUMENT) {
      if (aStateFlags & Ci.nsIWebProgressListener.STATE_STOP)
        this._documentStop();
    }
  },

  /** This method is called to indicate progress changes for the currently loading page. */
  onProgressChange: function(aWebProgress, aRequest, aCurSelf, aMaxSelf, aCurTotal, aMaxTotal) {
  },

  /** This method is called to indicate a change to the current location. */
  onLocationChange: function(aWebProgress, aRequest, aLocationURI) {
    let location = aLocationURI ? aLocationURI.spec : "";
    let selectedBrowser = Browser.selectedBrowser;

    this._hostChanged = true;

    if (this._tab == Browser.selectedTab) {
      BrowserUI.updateURI();

      // We're about to have new page content, to scroll the content area
      // to the top so the new paints will draw correctly.
      Browser.scrollContentToTop();
    }
  },

  /**
   * This method is called to indicate a status changes for the currently
   * loading page.  The message is already formatted for display.
   */
  onStatusChange: function(aWebProgress, aRequest, aStatus, aMessage) {
  },

  /** This method is called when the security state of the browser changes. */
  onSecurityChange: function(aWebProgress, aRequest, aState) {
    // Don't need to do anything if the data we use to update the UI hasn't changed
    if (this.state == aState && !this._hostChanged)
      return;

    this._hostChanged = false;
    this.state = aState;

    if (this._tab == Browser.selectedTab) {
      getIdentityHandler().checkIdentity();
    }
  },

  QueryInterface: function(aIID) {
    if (aIID.equals(Ci.nsIWebProgressListener) ||
        aIID.equals(Ci.nsISupportsWeakReference) ||
        aIID.equals(Ci.nsISupports))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  _networkStart: function _networkStart() {
    this._tab.startLoading();

    if (this._tab == Browser.selectedTab)
      BrowserUI.update(TOOLBARSTATE_LOADING);

    // broadcast a URLChanged message for consumption by InputHandler
    let event = document.createEvent("Events");
    event.initEvent("URLChanged", true, false);
    this.browser.dispatchEvent(event);
  },

  _networkStop: function _networkStop() {
    this._tab.endLoading();

    if (this._tab == Browser.selectedTab) {
      BrowserUI.update(TOOLBARSTATE_LOADED);
      this.browser.docShell.isOffScreenBrowser = true;
    }

    // if we are idle at this point, be sure to kick start the prefetcher
    if (Browser._browserView._idleServiceObserver.isIdle())
      Browser._browserView._tileManager.restartPrefetchCrawl();
    
    if (this.browser.currentURI.spec != "about:blank")
      this._tab.updateThumbnail();
  },

  _documentStop: function() {
    // translate any phone numbers
    Browser.translatePhoneNumbers();
  }
};

var OfflineApps = {
  get _pm() {
    delete this._pm;
    return this._pm = Cc["@mozilla.org/permissionmanager;1"].getService(Ci.nsIPermissionManager);
  },

  offlineAppRequested: function(aDocument) {
    if (!gPrefService.getBoolPref("browser.offline-apps.notify"))
      return;

    let currentURI = aDocument.documentURIObject;

    // don't bother showing UI if the user has already made a decision
    if (this._pm.testExactPermission(currentURI, "offline-app") != Ci.nsIPermissionManager.UNKNOWN_ACTION)
      return;

    try {
      if (gPrefService.getBoolPref("offline-apps.allow_by_default")) {
        // all pages can use offline capabilities, no need to ask the user
        return;
      }
    } catch(e) {
      // this pref isn't set by default, ignore failures
    }

    let host = currentURI.asciiHost;
    let notificationID = "offline-app-requested-" + host;
    let notificationBox = Browser.getNotificationBox();

    let notification = notificationBox.getNotificationWithValue(notificationID);
    let strings = Elements.browserBundle;
    if (notification) {
      notification.documents.push(aDocument);
    } else {
      let buttons = [{
        label: strings.getString("offlineApps.allow"),
        accessKey: null,
        callback: function() {
          for (let i = 0; i < notification.documents.length; i++)
            OfflineApps.allowSite(notification.documents[i]);
        }
      },{
        label: strings.getString("offlineApps.never"),
        accessKey: null,
        callback: function() {
          for (let i = 0; i < notification.documents.length; i++)
            OfflineApps.disallowSite(notification.documents[i]);
        }
      },{
        label: strings.getString("offlineApps.notNow"),
        accessKey: null,
        callback: function() { /* noop */ }
      }];

      const priority = notificationBox.PRIORITY_INFO_LOW;
      let message = strings.getFormattedString("offlineApps.available", [host]);
      notification = notificationBox.appendNotification(message, notificationID,
                                                        "", priority, buttons);
      notification.documents = [aDocument];
    }
  },

  allowSite: function(aDocument) {
    this._pm.add(aDocument.documentURIObject, "offline-app", Ci.nsIPermissionManager.ALLOW_ACTION);

    // When a site is enabled while loading, manifest resources will start
    // fetching immediately.  This one time we need to do it ourselves.
    this._startFetching(aDocument);
  },

  disallowSite: function(aDocument) {
    this._pm.add(aDocument.documentURIObject, "offline-app", Ci.nsIPermissionManager.DENY_ACTION);
  },

  _startFetching: function(aDocument) {
    if (!aDocument.documentElement)
      return;

    let manifest = aDocument.documentElement.getAttribute("manifest");
    if (!manifest)
      return;

    let manifestURI = gIOService.newURI(manifest, aDocument.characterSet, aDocument.documentURIObject);

    let updateService = Cc["@mozilla.org/offlinecacheupdate-service;1"].getService(Ci.nsIOfflineCacheUpdateService);
    updateService.scheduleUpdate(manifestURI, aDocument.documentURIObject);
  },

  handleEvent: function(aEvent) {
    if (aEvent.type == "MozApplicationManifest")
      this.offlineAppRequested(aEvent.originalTarget.defaultView.document);
  }
};

function Tab() {
  this._id = null;
  this._browser = null;
  this._browserViewportState = null;
  this._state = null;
  this._listener = null;
  this._loading = false;
  this._chromeTab = null;

  // Set to 0 since new tabs that have not been viewed yet are good tabs to
  // toss if app needs more memory.
  this.lastSelected = 0;

  this.create();
}

Tab.prototype = {
  get browser() {
    return this._browser;
  },

  get browserViewportState() {
    return this._browserViewportState;
  },

  get chromeTab() {
    return this._chromeTab;
  },

  /**
   * Throttles redraws to once every 2 seconds while loading the page, zooming to fit page if
   * user hasn't started zooming.
   */
  _resizeAndPaint: function() {
    let bv = Browser._browserView;
    bv.commitBatchOperation();
    if (this._loading) {
      // kick ourselves off 2s later while we're still loading
      bv.beginBatchOperation();
      this._loadingTimeout = setTimeout(Util.bind(this._resizeAndPaint, this), 2000);
    } else {
      delete this._loadingTimeout;
    }
  },

  /** Returns tab's identity state for updating security UI. */
  getIdentityState: function() {
    return this._listener.state;
  },

  startLoading: function() {
    this._loading = true;
    let bvs = this._browserViewportState;
    bvs.defaultZoomLevel = bvs.zoomLevel; // ensures zoom level is reset on new pages

    if (!this._loadingTimeout) {
      Browser._browserView.beginBatchOperation();
      Browser._browserView.invalidateEntireView();
      this._loadingTimeout = setTimeout(Util.bind(this._resizeAndPaint, this), 2000);
    }
  },

  endLoading: function() {
    // Determine at what resolution the browser is rendered based on meta tag
    let browser = this._browser;
    let metaData = Util.contentIsHandheld(browser);

    if (metaData.reason == "handheld" || metaData.reason == "doctype") {
      browser.className = "browser-handheld";
    } else if (metaData.reason == "viewport") {
      let screenW = window.innerWidth;
      let screenH = window.innerHeight;
      let viewportW = metaData.width; 
      let viewportH = metaData.height;
      let validW = viewportW > 0;
      let validH = viewportH > 0;

      if (validW && !validH) {
        viewportH = viewportW * (screenH / screenW);
      } else if (!validW && validH) {
        viewportW = viewportH * (screenW / screenH);
      } else {
        viewportW = kDefaultBrowserWidth;
        viewportH = kDefaultBrowserWidth * (screenH / screenW);
      }
      browser.className = "browser-viewport";
      browser.style.width = viewportW + "px";
      browser.style.height = viewportH + "px";
    } else {
      browser.className = "browser";
    }

    this.setIcon(browser.mIconURL);

    this._loading = false;
    clearTimeout(this._loadingTimeout);

    // in order to ensure we commit our current batch,
    // we need to run this function here
    this._resizeAndPaint();

    // if this tab was sacrificed previously, restore its state
    this.restoreState();
  },

  isLoading: function() {
    return this._loading;
  },

  load: function(uri) {
    this._browser.setAttribute("src", uri);
  },

  create: function() {
    // Initialize a viewport state for BrowserView
    this._browserViewportState = BrowserView.Util.createBrowserViewportState();

    this._chromeTab = document.getElementById("tabs").addTab();
    this._createBrowser();
  },

  destroy: function() {
    this._destroyBrowser();
    document.getElementById("tabs").removeTab(this._chromeTab);
    this._chromeTab = null;
  },

  /** Create browser if it doesn't already exist. */
  ensureBrowserExists: function() {
    if (!this._browser) {
      this._createBrowser();
      this.browser.contentDocument.location = this._state._url;
    }
  },

  _createBrowser: function() {
    if (this._browser)
      throw "Browser already exists";

    // Create the browser using the current width the dynamically size the height
    let browser = this._browser = document.createElement("browser");

    browser.className = "browser";
    browser.setAttribute("style", "overflow: -moz-hidden-unscrollable; visibility: hidden;");
    browser.setAttribute("type", "content");

    // Append the browser to the document, which should start the page load
    document.getElementById("browsers").appendChild(browser);

    // stop about:blank from loading
    browser.stop();

    // Attach a separate progress listener to the browser
    this._listener = new ProgressController(this);
    browser.addProgressListener(this._listener);
  },

  _destroyBrowser: function() {
    if (this._browser) {
      document.getElementById("browsers").removeChild(this._browser);
      this._browser = null;

      if (this._loading) {
        Browser._browserView.commitBatchOperation();
        clearTimeout(this._loadingTimeout);
        delete this._loadingTimeout;
      }
    }
  },

  /** Serializes as much state as possible of the current content.  */
  saveState: function() {
    let state = { };

    var browser = this._browser;
    var doc = browser.contentDocument;
    state._url = doc.location.href;
    state._scroll = BrowserView.Util.getContentScrollOffset(this.browser);
    if (doc instanceof HTMLDocument) {
      var tags = ["input", "textarea", "select"];

      for (var t = 0; t < tags.length; t++) {
        var elements = doc.getElementsByTagName(tags[t]);
        for (var e = 0; e < elements.length; e++) {
          var element = elements[e];
          var id;
          if (element.id)
            id = "#" + element.id;
          else if (element.name)
            id = "$" + element.name;

          if (id)
            state[id] = element.value;
        }
      }
    }

    this._state = state;
  },

  /** Restores serialized content from saveState.  */
  restoreState: function() {
    let state = this._state;
    if (!state)
      return;

    let doc = this._browser.contentDocument;

    for (var item in state) {
      var elem = null;
      if (item.charAt(0) == "#") {
        elem = doc.getElementById(item.substring(1));
      } else if (item.charAt(0) == "$") {
        var list = doc.getElementsByName(item.substring(1));
        if (list.length)
          elem = list[0];
      }

      if (elem)
        elem.value = state[item];
    }

    this.browser.contentWindow.scrollX = state._scroll.x;
    this.browser.contentWindow.scrollY = state._scroll.y;

    this._state = null;
  },

  updateThumbnail: function() {
    if (!this._browser)
      return;

    let browserView = (Browser.selectedBrowser == this._browser) ? Browser._browserView : null;
    this._chromeTab.updateThumbnail(this._browser, browserView);
  },

  setIcon: function(aURI) {
    let faviconURI = null;
    if (aURI) {
      try {
        faviconURI = gIOService.newURI(aURI, null, null);
      }
      catch (e) {
        faviconURI = null;
      }
    }

    if (!faviconURI || faviconURI.schemeIs("javascript") || gFaviconService.isFailedFavicon(faviconURI)) {
      try {
        // Use documentURIObject in the favicon construction so that we
        // do the right thing with about:-style error pages.  Bug 515188
        faviconURI = gIOService.newURI(this._browser.contentDocument.documentURIObject.prePath + "/favicon.ico", null, null);
        gFaviconService.setAndLoadFaviconForPage(this._browser.currentURI, faviconURI, true);
      }
      catch (e) {
        faviconURI = null;
      }
      if (faviconURI && gFaviconService.isFailedFavicon(faviconURI))
        faviconURI = null;
    }

    this._browser.mIconURL = faviconURI ? faviconURI.spec : "";
  },


  toString: function() {
    return "[Tab " + (this._browser ? this._browser.contentDocument.location.toString() : "(no browser)") + "]";
  }
};

var ImagePreloader = {
  cache: function ip_cache() {
    // Preload images used in border-image CSS
    let images = ["button-active", "button-default",
                  "buttondark-active", "buttondark-default",
                  "toggleon-active", "toggleon-inactive",
                  "toggleoff-active", "toggleoff-inactive",
                  "toggleleft-active", "toggleleft-inactive",
                  "togglemiddle-active", "togglemiddle-inactive",
                  "toggleright-active", "toggleright-inactive",
                  "toggleboth-active", "toggleboth-inactive",
                  "toggledarkleft-active", "toggledarkleft-inactive",
                  "toggledarkmiddle-active", "toggledarkmiddle-inactive",
                  "toggledarkright-active", "toggledarkright-inactive",
                  "toggledarkboth-active", "toggledarkboth-inactive",
                  "toolbarbutton-active", "toolbarbutton-default",
                  "addons-active", "addons-default",
                  "downloads-active", "downloads-default",
                  "preferences-active", "preferences-default",
                  "settings-active", "settings-open"];

    let size = screen.width > 400 ? "-64" : "-36";
    for (let i = 0; i < images.length; i++) {
      let image = new Image();
      image.src = "chrome://browser/skin/images/" + images[i] + size + ".png";
    }
  }
}

/** Call obj.start() or stop() based on boolean pref. */
function PreferenceToggle(pref, obj) {
  // Make weak reference just to be sure there are no cycles
  gPrefService.addObserver(pref, this, true);
  this._obj = obj;
  this._pref = pref;
  this._check();
}

PreferenceToggle.prototype = {
  QueryInterface: function QueryInterface(iid) {
   if (iid.equals(Ci.nsIObserver) ||
       iid.equals(Ci.nsISupportsWeakReference) ||
       iid.equals(Ci.nsISupports))
     return this;
   throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  observe: function observe(subject, topic, data) {
    if (topic == "nsPref:changed" && data == this._pref)
      this._check();
  },

  _check: function _check() {
    try {
      let value = gPrefService.getBoolPref(this._pref);
      value ? this._obj.start() : this._obj.stop();
    } catch(e) {
      this._obj.stop();
    }
  }
};

const nsIObjectLoadingContent = Ci.nsIObjectLoadingContent_MOZILLA_1_9_2_BRANCH || Ci.nsIObjectLoadingContent;

/**
 * Allows fast-path embed rendering by letting objects know where to absolutely
 * render on the screen.
 */
function PluginObserver(bv) {
  this._emptyRect = new Rect(0, 0, 0, 0);
  this._contentShowing = document.getElementById("observe_contentShowing");
  this._bv = bv;
  this._started = false;
}

PluginObserver.prototype = {
  /** Starts flash objects fast path. */
  start: function() {
    if (this._started)
      return;
    this._started = true;

    document.getElementById("tabs-container").addEventListener("TabSelect", this, false);
    this._contentShowing.addEventListener("broadcast", this, false);
    let browsers = document.getElementById("browsers");
    browsers.addEventListener("RenderStateChanged", this, false);
    gObserverService.addObserver(this, "plugin-changed-event", false);

    let browser = Browser.selectedBrowser;
    if (browser) {
      browser.addEventListener("ZoomChanged", this, false);
      browser.addEventListener("MozAfterPaint", this, false);
    }
  },

  /** Stops listening for events. */
  stop: function() {
    if (!this._started)
      return;
    this._started = false;

    document.getElementById("tabs-container").removeEventListener("TabSelect", this, false);
    this._contentShowing.removeEventListener("broadcast", this, false);
    let browsers = document.getElementById("browsers");
    browsers.removeEventListener("RenderStateChanged", this, false);
    gObserverService.removeObserver(this, "plugin-changed-event");

    let browser = Browser.selectedBrowser;
    if (browser) {
      browser.removeEventListener("ZoomChanged", this, false);
      browser.removeEventListener("MozAfterPaint", this, false);
    }
  },

  /** Observe listens for plugin change events and maintains an embed cache. */
  observe: function observe(subject, topic, data) {
    let doc = subject.ownerDocument;
    if (data == "init") {
      if (doc.pluginCache === undefined)
        doc.pluginCache = [];
      doc.pluginCache.push(subject);
    } else if (data == "reflow") {
      this.updateCurrentBrowser();
    } else if (data == "destroy") {
      if (doc.pluginCache) {
        let index = doc.pluginCache.indexOf(subject);
        if (index != -1)
          doc.pluginCache.splice(index, 1);
      }
    }
  },

  /** Update flash objects */
  handleEvent: function handleEvent(ev) {
    if (ev.type == "TabSelect") {
      if (ev.lastTab) {
        let browser = ev.lastTab.browser;
        let oldDoc = browser.contentDocument;
        browser.removeEventListener("ZoomChanged", this, false);
        browser.removeEventListener("MozAfterPaint", this, false);
        if (oldDoc.pluginCache)
          this.updateEmbedRegions(oldDoc.pluginCache, this._emptyRect);
      }

      let browser = Browser.selectedBrowser;
      browser.addEventListener("ZoomChanged", this, false);
      browser.addEventListener("MozAfterPaint", this, false);
    }

    this.updateCurrentBrowser();
  },

  /** Update the current browser's flash objects. */
  updateCurrentBrowser: function updateCurrentBrowser() {
    let doc = Browser.selectedTab.browser.contentDocument;
    if (!doc.pluginCache)
      return;

    let rect = this.getCriticalRect();
    if (rect == this._emptyRect) {
      // Always stop rendering immediately.
      this.updateEmbedRegions(doc.pluginCache, rect);
    } else {
      // Wait a moment so that any chrome redraws occur first.
      let self = this;
      setTimeout(function() {
        // Recalculate critical rect so we don't render when we ought not to.
        self.updateEmbedRegions(doc.pluginCache, self.getCriticalRect());
      }, 0);
    }
  },

  /** More accurate version of finding the current visible region. */
  // XXX should visible rect call take some of these things into account?
  getCriticalRect: function getCriticalRect() {
    let bv = this._bv;
    if (!bv.isRendering())
      return this._emptyRect;
    if (Elements.contentShowing.hasAttribute("disabled"))
      return this._emptyRect;

    let vs = bv._browserViewportState;
    let vr = bv.getVisibleRect();
    let crit = BrowserView.Util.visibleRectToCriticalRect(vr, vs);

    if (BrowserUI.isToolbarLocked()) {
      let urlbar = document.getElementById("toolbar-moveable-container");
      let height = urlbar.getBoundingClientRect().height;
      crit.top += height;
    }
    return crit;
  },

  /** Tell embedded objects where to absolutely render, using crit for clipping. */
  updateEmbedRegions: function updateEmbedRegions(objects, crit) {
    let bv = this._bv;
    let oprivate, r, dest, clip;
    for (let i = objects.length - 1; i >= 0; i--) {
      r = bv.browserToViewportRect(Browser.getBoundingContentRect(objects[i]));
      dest = Browser.browserViewToClientRect(r);
      clip = Browser.browserViewToClientRect(r.intersect(crit)).translate(-dest.left, -dest.top);
      oprivate = objects[i].QueryInterface(nsIObjectLoadingContent);
      oprivate.setAbsoluteScreenPosition(Browser.contentScrollbox, dest, clip);
    }
  },
};
