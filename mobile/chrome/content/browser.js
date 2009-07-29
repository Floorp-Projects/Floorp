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

const FINDSTATE_FIND = 0;
const FINDSTATE_FIND_AGAIN = 1;
const FINDSTATE_FIND_PREVIOUS = 2;

const endl = '\n';

Cu.import("resource://gre/modules/SpatialNavigation.js");

function getBrowser() {
  return Browser.selectedBrowser;
}

const kDefaultTextZoom = 1.2;
const kDefaultBrowserWidth = 1024;

function debug() {
  let bv = Browser._browserView;
  let tc = bv._tileManager._tileCache;
  let scrollbox = document.getElementById("scrollbox")
		.boxObject
		.QueryInterface(Components.interfaces.nsIScrollBoxObject);

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

    let cr = bv._tileManager._criticalRect;
    dump('criticalRect from BV: ' + (cr ? cr.toString() : null) + endl);
    dump('visibleRect from BV : ' + bv._visibleRect.toString() + endl);
    dump('visibleRect from foo: ' + Browser.getVisibleRect().toString() + endl);

    dump('batch depth:          ' + bv._batchOps.length + endl);
    dump('renderpause depth:    ' + bv._renderMode + endl);

    dump(endl);

    dump('window.innerWidth: ' + window.innerWidth + endl);

    dump(endl);

    dump('container width,height from BV: ' + bv._container.style.width + ', '
                                            + bv._container.style.height + endl);
    dump('container width,height via DOM: ' + container.style.width + ', '
                                            + container.style.height + endl);

    dump(endl);

    dump('scrollbox position    : ' + x + ', ' + y + endl);
    dump('scrollbox scrolledsize: ' + w + ', ' + h + endl);


    let sb = document.getElementById("scrollbox");
    dump('container location:     ' + Math.round(container.getBoundingClientRect().left) + " " +
                                      Math.round(container.getBoundingClientRect().top) + endl);



    dump(endl);

    dump('tilecache capacity: ' + bv._tileManager._tileCache.getCapacity() + endl);
    dump('tilecache size    : ' + bv._tileManager._tileCache.size          + endl);
    dump('tilecache numFree : ' + bv._tileManager._tileCache.numFree       + endl);
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
  dump('occupied : ' + tc._isOccupied(i, j) + endl);
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

function onKeyPress(e) {
  let bv = Browser._browserView;

  if (!e.ctrlKey)
    return;

  const a = 97;   // debug all critical tiles
  const c = 99;   // set tilecache capacity
  const d = 100;  // debug dump
  const f = 102;  // run noop() through forEachIntersectingRect (for timing)
  const i = 105;  // toggle info click mode
  const l = 108;  // restart lazy crawl
  const m = 109;  // fix mouseout
  const r = 114;  // reset visible rect
  const t = 116;  // debug given list of tiles separated by space

  switch (e.charCode) {
  case r:
    bv.setVisibleRect(Browser.getVisibleRect());

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
  case f:
    let noop = function noop() { for (let i = 0; i < 10; ++i); };
    bv._tileManager._tileCache.forEachIntersectingRect(bv._tileManager._criticalRect,
                                                       false, noop, window);

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
  default:
    break;
  }
}

var ih = null;

var Browser = {
  _tabs : [],
  _browsers : [],
  _selectedTab : null,
  windowUtils: window.QueryInterface(Ci.nsIInterfaceRequestor)
                     .getInterface(Ci.nsIDOMWindowUtils),

  startup: function() {
    var self = this;

    dump("begin startup\n");

    let container = document.getElementById("tile-container");
    let bv = this._browserView = new BrowserView(container, Browser.getVisibleRect());

    container.customClicker = this._createContentCustomClicker(bv);

    let scrollbox = document.getElementById("scrollbox");

    scrollbox.customDragger = {
      dragStart: function dragStart(scroller) {
        bv.pauseRendering();
      },

      dragStop: function dragStop(dx, dy, scroller) {
        let dx = this.dragMove(dx, dy, scroller, true);

        let snapdx = Browser.snapSidebars(scroller);
        bv.onAfterVisibleMove(snapdx, 0);

        bv.resumeRendering();

        return (dy != 0) || ((dx + snapdx) != 0);
      },

      dragMove: function dragMove(dx, dy, scroller, doReturnDX) {
        bv.onBeforeVisibleMove(dx, dy);

        let [x0, y0] = Browser.getScrollboxPosition(scroller);

        scroller.scrollBy(dx, dy);

        let [x1, y1] = Browser.getScrollboxPosition(scroller);

        let realdx = x1 - x0;
        let realdy = y1 - y0;

        bv.onAfterVisibleMove(realdx, realdy);

        if (realdx != dx || realdy != dy) {
          dump('--> scroll asked for ' + dx + ',' + dy + ' and got ' + realdx + ',' + realdy + '\n');
        }

        return (doReturnDX) ? realdx : !(realdx == 0 && realdy == 0);
      }
    };

    // during startup a lot of viewportHandler calls happen due to content and window resizes
    bv.beginBatchOperation();

    function panHandler(vr, dx, dy) {
      if (dx) {
        let visibleNow = ws.isWidgetVisible("tabs-container") || ws.isWidgetVisible("browser-controls");
        let isToolbarFrozen = ws.isWidgetFrozen('toolbar-main');
        if (visibleNow && !isToolbarFrozen) {
          BrowserUI.showToolbar(URLBAR_FORCE);
        }
        else if (!visibleNow && isToolbarFrozen) {
          BrowserUI.showToolbar();
        }
      }

      // this is really only necessary for maemo, where we don't
      // always repaint fast enough.
      self.windowUtils.processUpdates();
    }

    //ws.setPanHandler(panHandler);

    function resizeHandler(e) {

      if (e.target != window)
        return;

      dump(window.innerWidth + "," + window.innerHeight + "\n");
      // XXX is this code right here actually needed?
      let w = window.innerWidth;
      let maximize = (document.documentElement.getAttribute("sizemode") == "maximized");
      if (maximize && w > screen.width)
        return;

      bv.beginBatchOperation();

      let h = window.innerHeight;

      // Tell the UI to resize the browser controls before calling  updateSize
      BrowserUI.sizeControls(w, h);

      // Resize the browsers...
      let browsers = Browser.browsers;
      if (browsers) {
        let scaledH = (kDefaultBrowserWidth * (h / w));
        for (let i=0; i<browsers.length; i++) {
          let browserStyle = browsers[i].style;
          browserStyle.height = scaledH + "px";
        }
      }

      bv.setVisibleRect(Browser.getVisibleRect());
      bv.zoomToPage();
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

    // initialize input handling
    ih = new InputHandler(container);

    BrowserUI.init();

    window.controllers.appendController(this);
    window.controllers.appendController(BrowserUI);

    var ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
    var styleSheets = Cc["@mozilla.org/content/style-sheet-service;1"].getService(Ci.nsIStyleSheetService);

    // Should we hide the cursors
    var hideCursor = gPrefService.getBoolPref("browser.ui.cursor") == false;
    if (hideCursor) {
      window.QueryInterface(Ci.nsIDOMChromeWindow).setCursor("none");

      var styleURI = ios.newURI("chrome://browser/content/cursor.css", null, null);
      styleSheets.loadAndRegisterSheet(styleURI, styleSheets.AGENT_SHEET);
    }

    // load styles for scrollbars
    var styleURI = ios.newURI("chrome://browser/content/content.css", null, null);
    styleSheets.loadAndRegisterSheet(styleURI, styleSheets.AGENT_SHEET);

    var os = Cc["@mozilla.org/observer-service;1"].getService(Ci.nsIObserverService);
    os.addObserver(gXPInstallObserver, "xpinstall-install-blocked", false);
    os.addObserver(gSessionHistoryObserver, "browser:purge-session-history", false);

    // XXX hook up memory-pressure notification to clear out tab browsers
    //os.addObserver(function(subject, topic, data) self.destroyEarliestBrowser(), "memory-pressure", false);

    window.QueryInterface(Ci.nsIDOMChromeWindow).browserDOMWindow = new nsBrowserAccess();

    let browsers = document.getElementById("browsers");
    browsers.addEventListener("command", this._handleContentCommand, false);
    browsers.addEventListener("DOMUpdatePageReport", gPopupBlockerObserver.onUpdatePageReport, false);

    /* Initialize Spatial Navigation */
    function panCallback(aElement) {
      if (!aElement)
        return;

      //canvasBrowser.ensureElementIsVisible(aElement);
    }
    // Init it with the "browsers" element, which will receive keypress events
    // for all of our <browser>s
    SpatialNavigation.init(browsers, panCallback);

    /* Login Manager */
    Cc["@mozilla.org/login-manager;1"].getService(Ci.nsILoginManager);

    /* Command line arguments/initial homepage */
    // If this is an intial window launch (was a nsICommandLine passed via window params)
    // we execute some logic to load the initial launch page
    if (window.arguments && window.arguments[0]) {
      var whereURI = null;

      try {
        // Try to access the commandline
        var cmdLine = window.arguments[0].QueryInterface(Ci.nsICommandLine);

        try {
          // Check for and use a default homepage
          whereURI = gPrefService.getCharPref("browser.startup.homepage");
        } catch (e) {}

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

      if (whereURI)
        this.addTab(whereURI, true);
    }

    // JavaScript Error Console
    if (gPrefService.getBoolPref("browser.console.showInPanel")){
      let tool_console = document.getElementById("tool-console");
      tool_console.hidden = false;
    }

    // Re-enable plugins if we had previously disabled them. We should get rid of
    // this code eventually...
    if (gPrefService.prefHasUserValue("temporary.disablePlugins")) {
      gPrefService.clearUserPref("temporary.disablePlugins");
      this.setPluginState(true);
    }

    bv.commitBatchOperation();


    dump("end startup\n");
  },

  shutdown: function() {
    this._browserView.setBrowser(null, false);

    BrowserUI.uninit();

    var os = Cc["@mozilla.org/observer-service;1"].getService(Ci.nsIObserverService);
    os.removeObserver(gXPInstallObserver, "xpinstall-install-blocked");
    os.removeObserver(gSessionHistoryObserver, "browser:purge-session-history");
    window.controllers.removeController(this);
    window.controllers.removeController(BrowserUI);
  },

  setPluginState: function(enabled)
  {
    var phs = Cc["@mozilla.org/plugin/host;1"].getService(Ci.nsIPluginHost);
    var plugins = phs.getPluginTags({ });
    for (var i = 0; i < plugins.length; ++i)
      plugins[i].disabled = !enabled;
  },

  get browsers() {
    return this._browsers;
  },

  _resizeAndPaint: function() {
    // !!! --- RESIZE HACK BEGIN -----
    this._browserView.simulateMozAfterSizeChange();
    // !!! --- RESIZE HACK END -----

    this._browserView.zoomToPage();
    this._browserView.commitBatchOperation();

    if (this._pageLoading) {
      // kick ourselves off 2s later while we're still loading
      this._browserView.beginBatchOperation();
      this._loadingTimeout = setTimeout(resizeAndPaint, 2000);
    } else {
      delete this._loadingTimeout;
    }
  },

  startLoading: function() {
    if (this._pageLoading)
      throw "!@@!#!";

    this._pageLoading = true;

    if (!this._loadingTimeout) {
      this._browserView.beginBatchOperation();
      this._loadingTimeout = setTimeout(Util.bind(this, this._resizeAndPaint), 2000);
    }
  },

  endLoading: function() {
    if (!this._pageLoading)
      alert("endLoading when page is already done\n");

    this._pageLoading = false;
    clearTimeout(this._loadingTimeout);
    // in order to ensure we commit our current batch,
    // we need to run this function here
    this._resizeAndPaint();
  },


  /**
   * Return the currently active <browser> object
   */
  get selectedBrowser() {
    return this._selectedTab.browser;
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
    this._browsers.push(newTab.browser);

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
    this._browsers.splice(tabIndex, 1);

    // redraw the tabs
    for (let t = tabIndex; t < this._tabs.length; t++)
      this._tabs[t].updateThumbnail();
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

    let firstTab = this._selectedTab == null;
    this._selectedTab = tab;

    bv.beginBatchOperation();
    this._browserView.setBrowser(this.selectedBrowser, true);
    document.getElementById("tabs").selectedItem = tab.chromeTab;

    if (!firstTab) {
      let webProgress = this.selectedBrowser.webProgress;
      let securityUI = this.selectedBrowser.securityUI;

      try {
        tab._listener.onLocationChange(webProgress, null, this.selectedBrowser.currentURI);
        if (securityUI)
          tab._listener.onSecurityChange(webProgress, null, securityUI.state);
      } catch (e) {
        // don't inhibit other listeners or following code
        Components.utils.reportError(e);
      }

      let event = document.createEvent("Events");
      event.initEvent("TabSelect", true, false);
      tab.chromeTab.dispatchEvent(event);
    }
    bv.commitBatchOperation(true);
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

  findState: FINDSTATE_FIND,
  openFind: function(aState) {
    this.findState = aState;
    var findbar = document.getElementById("findbar");
    if (!findbar.browser)
      findbar.browser = this.selectedBrowser;

    var panel = document.getElementById("findbar-container");
    if (panel.hidden) {
      panel.hidden = false;
    }
    this.doFind();
  },

  doFind: function() {
    var findbar = document.getElementById("findbar");
    if (Browser.findState == FINDSTATE_FIND)
      findbar.onFindCommand();
    else
      findbar.onFindAgainCommand(Browser.findState == FINDSTATE_FIND_PREVIOUS);

    var panel = document.getElementById("findbar-container");
    panel.top = window.innerHeight - Math.floor(findbar.getBoundingClientRect().height);
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

  /**
   * Handle command event bubbling up from content.  This allows us to do chrome-
   * privileged things based on buttons in, e.g., unprivileged error pages.
   * Obviously, care should be taken not to trust events that web pages could have
   * synthesized.
   */
  _handleContentCommand: function (aEvent) {
    // Don't trust synthetic events
    if (!aEvent.isTrusted)
      return;

    var ot = aEvent.originalTarget;
    var errorDoc = ot.ownerDocument;

    // If the event came from an ssl error page, it is probably either the "Add
    // Exception…" or "Get me out of here!" button
    if (/^about:neterror\?e=nssBadCert/.test(errorDoc.documentURI)) {
      if (ot == errorDoc.getElementById('exceptionDialogButton')) {
        var params = { exceptionAdded : false };

        try {
          switch (gPrefService.getIntPref("browser.ssl_override_behavior")) {
            case 2 : // Pre-fetch & pre-populate
              params.prefetchCert = true;
            case 1 : // Pre-populate
              params.location = errorDoc.location.href;
          }
        } catch (e) {
          Components.utils.reportError("Couldn't get ssl_override pref: " + e);
        }

        window.openDialog('chrome://pippki/content/exceptionDialog.xul',
                          '','chrome,centerscreen,modal', params);

        // If the user added the exception cert, attempt to reload the page
        if (params.exceptionAdded)
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
  },

  _createContentCustomClicker: function _createContentCustomClicker(browserView) {
    // TODO don't generate this dynamically like this, but actualy make
    // it a prototype somewhere and instantiate it and such...

    function transformClientToBrowser(cX, cY) {
      return Browser.clientToBrowserView(cX, cY).map(browserView.viewportToBrowser);
    }

    function elementFromPoint(browser, x, y) {
      [x, y] = transformClientToBrowser(browserView, x, y);
      let cwu = BrowserView.Util.getBrowserDOMWindowUtils(browser);
      return cwu.elementFromPoint(x, y,
				  true,   /* ignore root scroll frame*/
				  false); /* don't flush layout */
    }

    function dispatchContentClick(browser, x, y) {
      dump('dispatching on the right browser? ' + (browser == Browser.selectedBrowser) + '\n');
      let cwu = BrowserView.Util.getBrowserDOMWindowUtils(browser);
      dump('     down on ' + cwu + '\n');
      cwu.sendMouseEvent("mousedown", x, y, 0, 1, 0, true);
      dump('     up\n');
      cwu.sendMouseEvent("mouseup",   x, y, 0, 1, 0, true);
    }

    return {
      zoomDir: 1,

      singleClick: function singleClick(cX, cY) {
        let browser = browserView.getBrowser();
        if (browser) {
	        dump('singleClick was invoked with ' + cX + ', ' + cY + '\n');
          let [x, y] = transformClientToBrowser(cX, cY);
	        dump('dispatching in browser ' + x + ', ' + y + '\n');
          dispatchContentClick(browser, x, y);
        }
      },

      doubleClick: function doubleClick(cX1, cY1, cX2, cY2) {
        let browser = browserView.getBrowser();
        if (browser) {
          let zoomElement = elementFromPoint(browser, cX2, cY2);

          if (zoomElement) {
	          // TODO actually zoom to and from element
            //browserView.zoom(this.zoomDir);
	          //this.zoomDir *= -1;
	          dump('zooming to/from element: ' + zoomElement + '\n');
          }

          //let [x, y] = transformClientToBrowser(cX1, cY1);
          //dispatchContentClick(browser, x, y);
          //[x, y] = transformClientToBrowser(cX2, cY2);
          //dispatchContentClick(browser, x, y);
        }
      }
    };
  },

  /**
   * Use the scroller to snap the sidebars in or out of view.
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
   * @param scroller A scrollBoxObject interface with which to scroll the
   * scrollbox
   * @return scrollBy dx caused by the snap
   */
  snapSidebars: function snapSidebars(scroller) {
    function visibility(bar, visrect) {
      try {
        let w = bar.width;
        let h = bar.height;
        bar.restrictTo(visrect); // throws exception if intersection of rects is empty
        return [bar.width / w, bar.height / h];
      } catch (e) {
        return [0, 0];
      }
    }

    let leftbarCBR = document.getElementById('tabs-container').getBoundingClientRect();
    let ritebarCBR = document.getElementById('browser-controls').getBoundingClientRect();

    let leftbar = new wsRect(leftbarCBR.left, 0, leftbarCBR.width, 1);
    let ritebar = new wsRect(ritebarCBR.left, 0, ritebarCBR.width, 1);
    let leftw = leftbar.width;
    let ritew = ritebar.width;

    let visrect = new wsRect(0, 0, window.innerWidth, 1);

    let [leftvis, ] = visibility(leftbar, visrect);
    let [ritevis, ] = visibility(ritebar, visrect);

    let snappedX = 0;

    if (leftvis != 0 && leftvis != 1) {
      if (leftvis >= 0.6666)
        snappedX = -((1 - leftvis) * leftw);
      else
        snappedX = leftvis * leftw;

      snappedX = Math.round(snappedX);
      scroller.scrollBy(snappedX, 0);
    }
    else if (ritevis != 0 && ritevis != 1) {
      if (ritevis >= 0.6666)
        snappedX = (1 - ritevis) * ritew;
      else
        snappedX = -ritevis * ritew;

      snappedX = Math.round(snappedX);
      scroller.scrollBy(snappedX, 0);
    }

    return snappedX;
  },

  /**
   * Transform x and y from client coordinates to BrowserView coordinates.
   */
  clientToBrowserView: function clientToBrowserView(x, y) {
    let container = document.getElementById("tile-container");
    let containerBCR = container.getBoundingClientRect();

    let dx = Math.round(-containerBCR.left);
    let dy = Math.round(-containerBCR.top);

    return [x + dx, y + dy];
  },

  /**
   * Return the visible rect in coordinates with origin at the (left, top) of
   * the tile container, i.e. BrowserView coordinates.
   */
  getVisibleRect: function getVisibleRect() {
    let container = document.getElementById("tile-container");
    let containerBCR = container.getBoundingClientRect();

    let x = Math.round(-containerBCR.left);
    let y = Math.round(-containerBCR.top);
    let w = window.innerWidth;
    let h = window.innerHeight;

    return new wsRect(x, y, w, h);
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
    return [x.value, y.value];
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
      dump("use -chrome command-line option to load external chrome urls\n");
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
    }
    else {
      if (aWhere == Ci.nsIBrowserDOMWindow.OPEN_NEWTAB)
        newWindow = BrowserUI.newTab().browser.contentWindow;
      else
        newWindow = aOpener ? aOpener.top : browser.contentWindow;
    }

    try {
      var referrer;
      if (aURI) {
        if (aOpener) {
          location = aOpener.location;
          referrer = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService)
                                                            .newURI(location, null, null);
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

/**
 * Utility class to handle manipulations of the identity indicators in the UI
 */
function IdentityHandler() {
  this._stringBundle = document.getElementById("bundle_browser");
  this._staticStrings = {};
  this._staticStrings[this.IDENTITY_MODE_DOMAIN_VERIFIED] = {
    encryption_label: this._stringBundle.getString("identity.encrypted2")
  };
  this._staticStrings[this.IDENTITY_MODE_IDENTIFIED] = {
    encryption_label: this._stringBundle.getString("identity.encrypted2")
  };
  this._staticStrings[this.IDENTITY_MODE_UNKNOWN] = {
    encryption_label: this._stringBundle.getString("identity.unencrypted2")
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
   * (if available) and, if necessary, update the UI to reflect this.  Intended to
   * be called by onSecurityChange
   *
   * @param PRUint32 state
   * @param JS Object location that mirrors an nsLocation (i.e. has .host and
   *                           .hostname and .port)
   */
  checkIdentity: function(state, location) {
    var currentStatus = getBrowser().securityUI
                                .QueryInterface(Ci.nsISSLStatusProvider)
                                .SSLStatus;
    this._lastStatus = currentStatus;
    this._lastLocation = location;

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
      var tooltip = this._stringBundle.getFormattedString("identity.identified.verifier",
                                                          [iData.caOrg]);

      // Check whether this site is a security exception. XPConnect does the right
      // thing here in terms of converting _lastLocation.port from string to int, but
      // the overrideService doesn't like undefined ports, so make sure we have
      // something in the default case (bug 432241).
      if (this._overrideService.hasMatchingOverride(this._lastLocation.hostname,
                                                    (this._lastLocation.port || 443),
                                                    iData.cert, {}, {}))
        tooltip = this._stringBundle.getString("identity.identified.verified_by_you");
    }
    else if (newMode == this.IDENTITY_MODE_IDENTIFIED) {
      // If it's identified, then we can populate the dialog with credentials
      iData = this.getIdentityData();
      tooltip = this._stringBundle.getFormattedString("identity.identified.verifier",
                                                      [iData.caOrg]);
    }
    else {
      tooltip = this._stringBundle.getString("identity.unknown.tooltip");
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
      var owner = this._stringBundle.getString("identity.ownerUnknown2");
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
        supplemental += this._stringBundle.getFormattedString("identity.identified.state_and_country",
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
  },

  show: function ih_show() {
    this._identityPopup.hidden = false;
    this._identityPopup.top = BrowserUI.toolbarH;
    this._identityPopup.focus();

    // Update the popup strings
    this.setPopupMessages(this._identityBox.getAttribute("mode") || this.IDENTITY_MODE_UNKNOWN);

    window.addEventListener("blur", this, true);
  },

  hide: function ih_hide() {
    window.removeEventListener("blur", this, true);
    this._identityPopup.hidden = true;
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
  },

  handleEvent: function(event) {
    // Watch for blur events so we can close the panel
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

  onUpdatePageReport: function (aEvent)
  {
    var cBrowser = Browser.selectedBrowser;
    if (aEvent.originalTarget != cBrowser)
      return;

    if (!cBrowser.pageReport)
      return;

    // Only show the notification again if we've not already shown it. Since
    // notifications are per-browser, we don't need to worry about re-adding
    // it.
    if (!cBrowser.pageReport.reported) {
      if(gPrefService.getBoolPref("privacy.popups.showBrowserMessage")) {
        var bundle_browser = document.getElementById("bundle_browser");
        var brandBundle = document.getElementById("bundle_brand");
        var brandShortName = brandBundle.getString("brandShortName");
        var message;
        var popupCount = cBrowser.pageReport.length;

        if (popupCount > 1)
          message = bundle_browser.getFormattedString("popupWarningMultiple", [brandShortName, popupCount]);
        else
          message = bundle_browser.getFormattedString("popupWarning", [brandShortName]);

        var notificationBox = Browser.getNotificationBox();
        var notification = notificationBox.getNotificationWithValue("popup-blocked");
        if (notification) {
          notification.label = message;
        }
        else {
          var buttons = [
            {
              label: bundle_browser.getString("popupButtonAlwaysAllow"),
              accessKey: bundle_browser.getString("popupButtonAlwaysAllow.accesskey"),
              callback: function() { gPopupBlockerObserver.toggleAllowPopupsForSite(); }
            },
            {
              label: bundle_browser.getString("popupButtonNeverWarn"),
              accessKey: bundle_browser.getString("popupButtonNeverWarn.accesskey"),
              callback: function() { gPopupBlockerObserver.dontShowMessage(); }
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

  toggleAllowPopupsForSite: function (aEvent)
  {
    var currentURI = Browser.selectedBrowser.webNavigation.currentURI;
    var pm = Cc["@mozilla.org/permissionmanager;1"].getService(this._kIPM);
    pm.add(currentURI, "popup", this._kIPM.ALLOW_ACTION);

    Browser.getNotificationBox().removeCurrentNotification();
  },

  dontShowMessage: function ()
  {
    var showMessage = gPrefService.getBoolPref("privacy.popups.showBrowserMessage");
    gPrefService.setBoolPref("privacy.popups.showBrowserMessage", !showMessage);
    Browser.getNotificationBox().removeCurrentNotification();
  }
};

const gXPInstallObserver = {
  observe: function (aSubject, aTopic, aData)
  {
    var brandBundle = document.getElementById("bundle_brand");
    var browserBundle = document.getElementById("bundle_browser");
    switch (aTopic) {
      case "xpinstall-install-blocked":
        var installInfo = aSubject.QueryInterface(Ci.nsIXPIInstallInfo);
        var host = installInfo.originatingURI.host;
        var brandShortName = brandBundle.getString("brandShortName");
        var notificationName, messageString, buttons;
        if (!gPrefService.getBoolPref("xpinstall.enabled")) {
          notificationName = "xpinstall-disabled";
          if (gPrefService.prefIsLocked("xpinstall.enabled")) {
            messageString = browserBundle.getString("xpinstallDisabledMessageLocked");
            buttons = [];
          }
          else {
            messageString = browserBundle.getFormattedString("xpinstallDisabledMessage",
                                                             [brandShortName, host]);
            buttons = [{
              label: browserBundle.getString("xpinstallDisabledButton"),
              accessKey: browserBundle.getString("xpinstallDisabledButton.accesskey"),
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
          messageString = browserBundle.getFormattedString("xpinstallPromptWarning",
                                                           [brandShortName, host]);

          buttons = [{
            label: browserBundle.getString("xpinstallPromptAllowButton"),
            accessKey: browserBundle.getString("xpinstallPromptAllowButton.accesskey"),
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

function getNotificationBox(aWindow) {
  return Browser.getNotificationBox();
}

function showDownloadsManager(aWindowContext, aID, aReason) {
  BrowserUI.show(UIMODE_PANEL);
  BrowserUI.switchPane("downloads-container");
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

    let toolbar = document.getElementById("toolbar-main");
    let top = toolbar.top + toolbar.boxObject.height;
    let container = document.getElementById("helperapp-container");
    container.hidden = false;

    let rect = container.getBoundingClientRect();
    container.top = top < 0 ? 0 : top;
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
}

ProgressController.prototype = {
  get browser() {
    return this._tab.browser;
  },

  onStateChange: function(aWebProgress, aRequest, aStateFlags, aStatus) {
    // ignore notification that aren't about the main document (iframes, etc)
    if (aWebProgress.DOMWindow != this._tab.browser.contentWindow)
      return;

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

  // This method is called to indicate progress changes for the currently
  // loading page.
  onProgressChange: function(aWebProgress, aRequest, aCurSelf, aMaxSelf, aCurTotal, aMaxTotal) {
  },

  // This method is called to indicate a change to the current location.
  onLocationChange: function(aWebProgress, aRequest, aLocationURI) {
    // XXX this code is not multiple-tab friendly.
    var location = aLocationURI ? aLocationURI.spec : "";
    let selectedBrowser = Browser.selectedBrowser;
    let lastURI = selectedBrowser.lastURI;

    //don't do anything for about:blank or about:firstrun on first display
    if (!lastURI && (location == "about:blank" || location == "about:firstrun" ))
      return;

    this._hostChanged = true;

    // This code here does not compare uris exactly when determining
    // whether or not the message(s) should be hidden since the message
    // may be prematurely hidden when an install is invoked by a click
    // on a link that looks like this:
    //
    // <a href="#" onclick="return install();">Install Foo</a>
    //
    // - which fires a onLocationChange message to uri + '#'...
    selectedBrowser.lastURI = aLocationURI;
    if (lastURI) {
      var oldSpec = lastURI.spec;
      var oldIndexOfHash = oldSpec.indexOf("#");
      if (oldIndexOfHash != -1)
        oldSpec = oldSpec.substr(0, oldIndexOfHash);
      var newSpec = location;
      var newIndexOfHash = newSpec.indexOf("#");
      if (newIndexOfHash != -1)
        newSpec = newSpec.substr(0, newSpec.indexOf("#"));
      if (newSpec != oldSpec) {
        // Remove all the notifications, except for those which want to
        // persist across the first location change.

        // XXX
        // var nBox = Browser.getNotificationBox();
        // nBox.removeTransientNotifications();
      }
    }

    if (aWebProgress.DOMWindow == selectedBrowser.contentWindow) {
      BrowserUI.setURI();
    }
  },

  // This method is called to indicate a status changes for the currently
  // loading page.  The message is already formatted for display.
  onStatusChange: function(aWebProgress, aRequest, aStatus, aMessage) {
  },

  _networkStart: function() {
    this._tab.setLoading(true);
    if (Browser.selectedBrowser == this.browser) {
      Browser.startLoading();
      BrowserUI.update(TOOLBARSTATE_LOADING);

      // We increase the default text size to make the text more readable when
      // the page is zoomed out
      if (this.browser.markupDocumentViewer.textZoom != kDefaultTextZoom)
        this.browser.markupDocumentViewer.textZoom = kDefaultTextZoom;
    }

    // broadcast a URLChanged message for consumption by InputHandler
    let event = document.createEvent("Events");
    event.initEvent("URLChanged", true, false);
    this.browser.dispatchEvent(event);
  },

  _networkStop: function _networkStop() {
    this._tab.setLoading(false);

    if (Browser.selectedBrowser == this.browser) {
      BrowserUI.update(TOOLBARSTATE_LOADED);
      this.browser.docShell.isOffScreenBrowser = true;
      Browser.endLoading();
    }

    this._tab.updateThumbnail();
  },

  _documentStop: function() {
    // translate any phone numbers
    Browser.translatePhoneNumbers();

    if (Browser.selectedBrowser == this.browser) {
      // focus the dom window
      if (this.browser.currentURI.spec != "about:blank")
        this.browser.contentWindow.focus();
    }
  },

 // Properties used to cache security state used to update the UI
  _state: null,
  _host: undefined,
  _hostChanged: false, // onLocationChange will flip this bit

  // This method is called when the security state of the browser changes.
  onSecurityChange: function(aWebProgress, aRequest, aState) {
    // Don't need to do anything if the data we use to update the UI hasn't
    // changed
    if (this._state == aState && !this._hostChanged) {
      return;
    }
    this._state = aState;

    try {
      this._host = getBrowser().contentWindow.location.host;
    }
    catch(ex) {
      this._host = null;
    }

    this._hostChanged = false;

    // Don't pass in the actual location object, since it can cause us to
    // hold on to the window object too long.  Just pass in the fields we
    // care about. (bug 424829)
    var location = getBrowser().contentWindow.location;
    var locationObj = {};
    try {
      locationObj.host = location.host;
      locationObj.hostname = location.hostname;
      locationObj.port = location.port;
    }
    catch (ex) {
      // Can sometimes throw if the URL being visited has no host/hostname,
      // e.g. about:blank. The _state for these pages means we won't need these
      // properties anyways, though.
    }
    getIdentityHandler().checkIdentity(this._state, locationObj);
  },

  QueryInterface: function(aIID) {
    if (aIID.equals(Ci.nsIWebProgressListener) ||
        aIID.equals(Ci.nsISupportsWeakReference) ||
        aIID.equals(Ci.nsISupports))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  }
};


function Tab() {
  this.create();
}

Tab.prototype = {
  _id: null,
  _browser: null,
  _state: null,
  _listener: null,
  _loading: false,
  _chromeTab: null,

  get browser() {
    return this._browser;
  },

  get chromeTab() {
    return this._chromeTab;
  },

  isLoading: function() {
    return this._loading;
  },

  setLoading: function(b) {
    this._loading = b;
  },

  load: function(uri) {
    dump('browser 3: ' + this._browser.contentWindow + '\n');
    dump("cb set src\n");
    this._browser.setAttribute("src", uri);
    dump("cb end src\n");
    dump('browser 4: ' + this._browser.contentWindow + '\n');
    try {
      dump('QIs to: ' + this._browser.contentWindow.QueryInterface(Ci.nsIDOMChromeWindow) + '\n');
    } catch (e) {
      dump('failed to QI\n');
    }
  },

  create: function() {
    this._chromeTab = document.createElement("richlistitem");
    this._chromeTab.setAttribute("type", "documenttab");
    document.getElementById("tabs").addTab(this._chromeTab);

    this._createBrowser();
  },

  destroy: function() {
    this._destroyBrowser();
    document.getElementById("tabs").removeTab(this._chromeTab);
    this._chromeTab = null;
  },

  _createBrowser: function() {
    dump('for the break\n');
    if (this._browser)
      throw "Browser already exists";

    // Create the browser using the current width the dynamically size the height
    let scaledHeight = kDefaultBrowserWidth * (window.innerHeight / window.innerWidth);
    let browser = this._browser = document.createElement("browser");

    dump('browser 1: ' + browser.contentWindow + '\n');

    browser.setAttribute("style", "overflow: -moz-hidden-unscrollable; visibility: hidden; width: " + kDefaultBrowserWidth + "px; height: " + scaledHeight + "px;");
    browser.setAttribute("type", "content");

    // Attach the popup contextmenu
    let container = document.getElementById("tile-container");
    browser.setAttribute("contextmenu", container.getAttribute("contextmenu"));
    let autocompletepopup = container.getAttribute("autocompletepopup");
    if (autocompletepopup)
      browser.setAttribute("autocompletepopup", autocompletepopup);

    // Append the browser to the document, which should start the page load
    document.getElementById("browsers").appendChild(browser);

    // stop about:blank from loading
    browser.stop();

    dump('browser 2: ' + browser.contentWindow + '\n');

    // Attach a separate progress listener to the browser
    this._listener = new ProgressController(this);
    browser.addProgressListener(this._listener);
  },

  _destroyBrowser: function() {
    document.getElementById("browsers").removeChild(this._browser);
    this._browser = null;
  },

  saveState: function() {
    let state = { };

    this._url = browser.contentWindow.location.toString();
    var browser = this.getBrowserForDisplay(display);
    var doc = browser.contentDocument;
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

    state._scrollX = browser.contentWindow.scrollX;
    state._scrollY = browser.contentWindow.scrollY;

    this._state = state;
  },

  restoreState: function() {
    let state = this._state;
    if (!state)
      return;

    let doc = this._browser.contentDocument;
    for (item in state) {
      var elem = null;
      if (item.charAt(0) == "#") {
        elem = doc.getElementById(item.substring(1));
      }
      else if (item.charAt(0) == "$") {
        var list = doc.getElementsByName(item.substring(1));
        if (list.length)
          elem = list[0];
      }

      if (elem)
        elem.value = state[item];
    }

    this._browser.contentWindow.scrollTo(state._scrollX, state._scrollY);
  },

  updateThumbnail: function() {
    if (!this._browser)
      return;

    // XXX draw from the tiles in to the source
    let srcCanvas = (Browser.selectedBrowser == this._browser) ? document.getElementById("browser-canvas") : null;
    this._chromeTab.updateThumbnail(this._browser, srcCanvas);
  }
};
