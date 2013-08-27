/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

#ifdef MOZ_CRASHREPORTER
XPCOMUtils.defineLazyServiceGetter(this, "CrashReporter",
  "@mozilla.org/xre/app-info;1", "nsICrashReporter");
#endif

XPCOMUtils.defineLazyModuleGetter(this, "Task", "resource://gre/modules/Task.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS", "resource://gre/modules/osfile.jsm");

function dump(a) {
  Cc["@mozilla.org/consoleservice;1"].getService(Ci.nsIConsoleService).logStringMessage(a);
}

// -----------------------------------------------------------------------
// Session Store
// -----------------------------------------------------------------------

const STATE_STOPPED = 0;
const STATE_RUNNING = 1;
const STATE_QUITTING = -1;

function SessionStore() { }

SessionStore.prototype = {
  classID: Components.ID("{8c1f07d6-cba3-4226-a315-8bd43d67d032}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsISessionStore,
                                         Ci.nsIDOMEventListener,
                                         Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference]),

  _windows: {},
  _lastSaveTime: 0,
  _interval: 10000,
  _maxTabsUndo: 1,
  _shouldRestore: false,

  init: function ss_init() {
    // Get file references
    this._sessionFile = Services.dirsvc.get("ProfD", Ci.nsILocalFile);
    this._sessionFileBackup = this._sessionFile.clone();
    this._sessionFile.append("sessionstore.js");
    this._sessionFileBackup.append("sessionstore.bak");

    this._loadState = STATE_STOPPED;

    this._interval = Services.prefs.getIntPref("browser.sessionstore.interval");
    this._maxTabsUndo = Services.prefs.getIntPref("browser.sessionstore.max_tabs_undo");

    // Do we need to restore session just this once, in case of a restart?
    if (this._sessionFileBackup.exists() && Services.prefs.getBoolPref("browser.sessionstore.resume_session_once")) {
      Services.prefs.setBoolPref("browser.sessionstore.resume_session_once", false);
      this._shouldRestore = true;
    }
  },

  _clearDisk: function ss_clearDisk() {
    OS.File.remove(this._sessionFile.path);
    OS.File.remove(this._sessionFileBackup.path);
  },

  _sendMessageToJava: function (aMsg) {
    let data = Services.androidBridge.handleGeckoMessage(JSON.stringify(aMsg));
    return JSON.parse(data);
  },

  observe: function ss_observe(aSubject, aTopic, aData) {
    let self = this;
    let observerService = Services.obs;
    switch (aTopic) {
      case "app-startup":
        observerService.addObserver(this, "final-ui-startup", true);
        observerService.addObserver(this, "domwindowopened", true);
        observerService.addObserver(this, "domwindowclosed", true);
        observerService.addObserver(this, "browser-lastwindow-close-granted", true);
        observerService.addObserver(this, "browser:purge-session-history", true);
        observerService.addObserver(this, "quit-application-requested", true);
        observerService.addObserver(this, "quit-application-granted", true);
        observerService.addObserver(this, "quit-application", true);
        observerService.addObserver(this, "Session:Restore", true);
        break;
      case "final-ui-startup":
        observerService.removeObserver(this, "final-ui-startup");
        this.init();
        break;
      case "domwindowopened": {
        let window = aSubject;
        window.addEventListener("load", function() {
          self.onWindowOpen(window);
          window.removeEventListener("load", arguments.callee, false);
        }, false);
        break;
      }
      case "domwindowclosed": // catch closed windows
        this.onWindowClose(aSubject);
        break;
      case "browser-lastwindow-close-granted":
        // If a save has been queued, kill the timer and save state now
        if (this._saveTimer) {
          this._saveTimer.cancel();
          this._saveTimer = null;
          this.saveState();
        }

        // Freeze the data at what we've got (ignoring closing windows)
        this._loadState = STATE_QUITTING;
        break;
      case "quit-application-requested":
        // Get a current snapshot of all windows
        this._forEachBrowserWindow(function(aWindow) {
          self._collectWindowData(aWindow);
        });
        break;
      case "quit-application-granted":
        // Get a current snapshot of all windows
        this._forEachBrowserWindow(function(aWindow) {
          self._collectWindowData(aWindow);
        });

        // Freeze the data at what we've got (ignoring closing windows)
        this._loadState = STATE_QUITTING;
        break;
      case "quit-application":
        // If we are restarting, lets restore the tabs
        if (aData == "restart") {
          Services.prefs.setBoolPref("browser.sessionstore.resume_session_once", true);

          // Ignore purges when restarting. The notification is fired after "quit-application".
          Services.obs.removeObserver(this, "browser:purge-session-history");
        }

        // Freeze the data at what we've got (ignoring closing windows)
        this._loadState = STATE_QUITTING;

        // Move this session to sessionstore.bak so that:
        //   1) we can get "tabs from last time" from sessionstore.bak
        //   2) if sessionstore.js exists on next start, we know we crashed
        OS.File.move(this._sessionFile.path, this._sessionFileBackup.path).then(null, function onError(reason) {
          if (!(reason instanceof OS.File.Error && reason.becauseNoSuchFile)) {
            Cu.reportError("Error moving sessionstore files: " + reason);
          }
        });

        observerService.removeObserver(this, "domwindowopened");
        observerService.removeObserver(this, "domwindowclosed");
        observerService.removeObserver(this, "browser-lastwindow-close-granted");
        observerService.removeObserver(this, "quit-application-requested");
        observerService.removeObserver(this, "quit-application-granted");
        observerService.removeObserver(this, "quit-application");
        observerService.removeObserver(this, "Session:Restore");

        // If a save has been queued, kill the timer and save state now
        if (this._saveTimer) {
          this._saveTimer.cancel();
          this._saveTimer = null;
          this.saveState();
        }
        break;
      case "browser:purge-session-history": // catch sanitization 
        this._clearDisk();

        // If the browser is shutting down, simply return after clearing the
        // session data on disk as this notification fires after the
        // quit-application notification so the browser is about to exit.
        if (this._loadState == STATE_QUITTING)
          return;

        // Clear all data about closed tabs
        for (let [ssid, win] in Iterator(this._windows))
          win.closedTabs = [];

        if (this._loadState == STATE_RUNNING) {
          // Save the purged state immediately
          this.saveStateNow();
        }

        Services.obs.notifyObservers(null, "sessionstore-state-purge-complete", "");
        break;
      case "timer-callback":
        // Timer call back for delayed saving
        this._saveTimer = null;
        this.saveState();
        break;
      case "Session:Restore": {
        if (aData) {
          // Be ready to handle any restore failures by making sure we have a valid tab opened
          let window = Services.wm.getMostRecentWindow("navigator:browser");
          let restoreCleanup = {
            observe: function (aSubject, aTopic, aData) {
              Services.obs.removeObserver(restoreCleanup, "sessionstore-windows-restored");

              if (window.BrowserApp.tabs.length == 0) {
                window.BrowserApp.addTab("about:home", {
                  showProgress: false,
                  selected: true
                });
              }

              // Let Java know we're done restoring tabs so tabs added after this can be animated
              this._sendMessageToJava({
                type: "Session:RestoreEnd"
              });
            }.bind(this)
          };
          Services.obs.addObserver(restoreCleanup, "sessionstore-windows-restored", false);

          // Do a restore, triggered by Java
          let data = JSON.parse(aData);
          this.restoreLastSession(data.normalRestore, data.sessionString);
        } else if (this._shouldRestore) {
          // Do a restore triggered by Gecko (e.g., if
          // browser.sessionstore.resume_session_once is true). In these cases,
          // our Java front-end doesn't know we're doing a restore, so it has
          // already opened an about:home tab.
          this.restoreLastSession(false, null);
        } else {
          // Not doing a restore; just send restore message
          Services.obs.notifyObservers(null, "sessionstore-windows-restored", "");
        }
        break;
      }
    }
  },

  handleEvent: function ss_handleEvent(aEvent) {
    let window = aEvent.currentTarget.ownerDocument.defaultView;
    switch (aEvent.type) {
      case "TabOpen": {
        let browser = aEvent.target;
        this.onTabAdd(window, browser);
        break;
      }
      case "TabClose": {
        let browser = aEvent.target;
        this.onTabClose(window, browser);
        this.onTabRemove(window, browser);
        break;
      }
      case "TabSelect": {
        let browser = aEvent.target;
        this.onTabSelect(window, browser);
        break;
      }
      case "pageshow": {
        let browser = aEvent.currentTarget;
        this.onTabLoad(window, browser, aEvent.persisted);
        break;
      }
    }
  },

  onWindowOpen: function ss_onWindowOpen(aWindow) {
    // Return if window has already been initialized
    if (aWindow && aWindow.__SSID && this._windows[aWindow.__SSID])
      return;

    // Ignore non-browser windows and windows opened while shutting down
    if (aWindow.document.documentElement.getAttribute("windowtype") != "navigator:browser" || this._loadState == STATE_QUITTING)
      return;

    // Assign it a unique identifier (timestamp) and create its data object
    aWindow.__SSID = "window" + Date.now();
    this._windows[aWindow.__SSID] = { tabs: [], selected: 0, closedTabs: [] };

    // Perform additional initialization when the first window is loading
    if (this._loadState == STATE_STOPPED) {
      this._loadState = STATE_RUNNING;
      this._lastSaveTime = Date.now();
    }

    // Add tab change listeners to all already existing tabs
    let tabs = aWindow.BrowserApp.tabs;
    for (let i = 0; i < tabs.length; i++)
      this.onTabAdd(aWindow, tabs[i].browser, true);

    // Notification of tab add/remove/selection
    let browsers = aWindow.document.getElementById("browsers");
    browsers.addEventListener("TabOpen", this, true);
    browsers.addEventListener("TabClose", this, true);
    browsers.addEventListener("TabSelect", this, true);
  },

  onWindowClose: function ss_onWindowClose(aWindow) {
    // Ignore windows not tracked by SessionStore
    if (!aWindow.__SSID || !this._windows[aWindow.__SSID])
      return;

    let browsers = aWindow.document.getElementById("browsers");
    browsers.removeEventListener("TabOpen", this, true);
    browsers.removeEventListener("TabClose", this, true);
    browsers.removeEventListener("TabSelect", this, true);

    if (this._loadState == STATE_RUNNING) {
      // Update all window data for a last time
      this._collectWindowData(aWindow);

      // Clear this window from the list
      delete this._windows[aWindow.__SSID];

      // Save the state without this window to disk
      this.saveStateDelayed();
    }

    let tabs = aWindow.BrowserApp.tabs;
    for (let i = 0; i < tabs.length; i++)
      this.onTabRemove(aWindow, tabs[i].browser, true);

    delete aWindow.__SSID;
  },

  onTabAdd: function ss_onTabAdd(aWindow, aBrowser, aNoNotification) {
    aBrowser.addEventListener("pageshow", this, true);
    if (!aNoNotification)
      this.saveStateDelayed();
    this._updateCrashReportURL(aWindow);
  },

  onTabRemove: function ss_onTabRemove(aWindow, aBrowser, aNoNotification) {
    aBrowser.removeEventListener("pageshow", this, true);

    // If this browser is being restored, skip any session save activity
    if (aBrowser.__SS_restore)
      return;

    delete aBrowser.__SS_data;

    if (!aNoNotification)
      this.saveStateDelayed();
  },

  onTabClose: function ss_onTabClose(aWindow, aBrowser) {
    if (this._maxTabsUndo == 0)
      return;

    if (aWindow.BrowserApp.tabs.length > 0) {
      // Bundle this browser's data and extra data and save in the closedTabs
      // window property
      let data = aBrowser.__SS_data;
      data.extData = aBrowser.__SS_extdata;

      this._windows[aWindow.__SSID].closedTabs.unshift(data);
      let length = this._windows[aWindow.__SSID].closedTabs.length;
      if (length > this._maxTabsUndo)
        this._windows[aWindow.__SSID].closedTabs.splice(this._maxTabsUndo, length - this._maxTabsUndo);
    }
  },

  onTabLoad: function ss_onTabLoad(aWindow, aBrowser, aPersisted) {
    // If this browser is being restored, skip any session save activity
    if (aBrowser.__SS_restore)
      return;

    // Ignore a transient "about:blank"
    if (!aBrowser.canGoBack && aBrowser.currentURI.spec == "about:blank")
      return;

    let history = aBrowser.sessionHistory;

    if (aPersisted && aBrowser.__SS_data) {
      // Loading from the cache; just update the index
      aBrowser.__SS_data.index = history.index + 1;
      this.saveStateDelayed();
    } else {
      // Serialize the tab data
      let entries = [];
      let index = history.index + 1;
      for (let i = 0; i < history.count; i++) {
        let historyEntry = history.getEntryAtIndex(i, false);
        // Don't try to restore wyciwyg URLs
        if (historyEntry.URI.schemeIs("wyciwyg")) {
          // Adjust the index to account for skipped history entries
          if (i <= history.index)
            index--;
          continue;
        }
        let entry = this._serializeHistoryEntry(historyEntry);
        entries.push(entry);
      }
      let data = { entries: entries, index: index };

      delete aBrowser.__SS_data;
      this._collectTabData(aWindow, aBrowser, data);
      this.saveStateNow();
    }

    this._updateCrashReportURL(aWindow);
  },

  onTabSelect: function ss_onTabSelect(aWindow, aBrowser) {
    if (this._loadState != STATE_RUNNING)
      return;

    let browsers = aWindow.document.getElementById("browsers");
    let index = browsers.selectedIndex;
    this._windows[aWindow.__SSID].selected = parseInt(index) + 1; // 1-based

    // Restore the resurrected browser
    if (aBrowser.__SS_restore) {
      let data = aBrowser.__SS_data;
      if (data.entries.length > 0)
        this._restoreHistory(data, aBrowser.sessionHistory);

      delete aBrowser.__SS_restore;
      aBrowser.removeAttribute("pending");
    }

    this.saveStateDelayed();
    this._updateCrashReportURL(aWindow);
  },

  saveStateDelayed: function ss_saveStateDelayed() {
    if (!this._saveTimer) {
      // Interval until the next disk operation is allowed
      let minimalDelay = this._lastSaveTime + this._interval - Date.now();

      // If we have to wait, set a timer, otherwise saveState directly
      let delay = Math.max(minimalDelay, 2000);
      if (delay > 0) {
        this._saveTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
        this._saveTimer.init(this, delay, Ci.nsITimer.TYPE_ONE_SHOT);
      } else {
        this.saveState();
      }
    }
  },

  saveStateNow: function ss_saveStateNow() {
    // Kill any queued timer and save immediately
    if (this._saveTimer) {
      this._saveTimer.cancel();
      this._saveTimer = null;
    }
    this.saveState();
  },

  saveState: function ss_saveState() {
    let data = this._getCurrentState();
    let normalData = { windows: [] };
    let privateData = { windows: [] };

    for (let winIndex = 0; winIndex < data.windows.length; ++winIndex) {
      let win = data.windows[winIndex];
      let normalWin = {};
      for (let prop in win) {
        normalWin[prop] = data[prop];
      }
      normalWin.tabs = [];
      normalData.windows.push(normalWin);
      privateData.windows.push({ tabs: [] });

      // Split the session data into private and non-private data objects.
      // Non-private session data will be saved to disk, and private session
      // data will be sent to Java for Android to hold it in memory.
      for (let i = 0; i < win.tabs.length; ++i) {
        let tab = win.tabs[i];
        let savedWin = tab.isPrivate ? privateData.windows[winIndex] : normalData.windows[winIndex];
        savedWin.tabs.push(tab);
        if (win.selected == i + 1) {
          savedWin.selected = savedWin.tabs.length;
        }
      }
    }

    // Write only non-private data to disk
    this._writeFile(this._sessionFile, JSON.stringify(normalData));

    // If we have private data, send it to Java; otherwise, send null to
    // indicate that there is no private data
    this._sendMessageToJava({
      type: "PrivateBrowsing:Data",
      session: (privateData.windows[0].tabs.length > 0) ? JSON.stringify(privateData) : null
    });

    this._lastSaveTime = Date.now();
  },

  _getCurrentState: function ss_getCurrentState() {
    let self = this;
    this._forEachBrowserWindow(function(aWindow) {
      self._collectWindowData(aWindow);
    });

    let data = { windows: [] };
    for (let index in this._windows) {
      data.windows.push(this._windows[index]);
    }

    return data;
  },

  _collectTabData: function ss__collectTabData(aWindow, aBrowser, aHistory) {
    // If this browser is being restored, skip any session save activity
    if (aBrowser.__SS_restore)
      return;

    aHistory = aHistory || { entries: [{ url: aBrowser.currentURI.spec, title: aBrowser.contentTitle }], index: 1 };

    let tabData = {};
    tabData.entries = aHistory.entries;
    tabData.index = aHistory.index;
    tabData.attributes = { image: aBrowser.mIconURL };
    tabData.desktopMode = aWindow.BrowserApp.getTabForBrowser(aBrowser).desktopMode;
    tabData.isPrivate = aBrowser.docShell.QueryInterface(Ci.nsILoadContext).usePrivateBrowsing;

    aBrowser.__SS_data = tabData;
  },

  _collectWindowData: function ss__collectWindowData(aWindow) {
    // Ignore windows not tracked by SessionStore
    if (!aWindow.__SSID || !this._windows[aWindow.__SSID])
      return;

    let winData = this._windows[aWindow.__SSID];
    winData.tabs = [];

    let browsers = aWindow.document.getElementById("browsers");
    let index = browsers.selectedIndex;
    winData.selected = parseInt(index) + 1; // 1-based

    let tabs = aWindow.BrowserApp.tabs;
    for (let i = 0; i < tabs.length; i++) {
      let browser = tabs[i].browser;
      if (browser.__SS_data) {
        let tabData = browser.__SS_data;
        if (browser.__SS_extdata)
          tabData.extData = browser.__SS_extdata;
        winData.tabs.push(tabData);
      }
    }
  },

  _forEachBrowserWindow: function ss_forEachBrowserWindow(aFunc) {
    let windowsEnum = Services.wm.getEnumerator("navigator:browser");
    while (windowsEnum.hasMoreElements()) {
      let window = windowsEnum.getNext();
      if (window.__SSID && !window.closed)
        aFunc.call(this, window);
    }
  },

  _writeFile: function ss_writeFile(aFile, aData) {
    let stateString = Cc["@mozilla.org/supports-string;1"].createInstance(Ci.nsISupportsString);
    stateString.data = aData;
    Services.obs.notifyObservers(stateString, "sessionstore-state-write", "");

    // Don't touch the file if an observer has deleted all state data
    if (!stateString.data)
      return;

    // Asynchronously copy the data to the file.
    let array = new TextEncoder().encode(aData);
    OS.File.writeAtomic(aFile.path, array, { tmpPath: aFile.path + ".tmp" }).then(function onSuccess() {
      Services.obs.notifyObservers(null, "sessionstore-state-write-complete", "");
    });
  },

  _updateCrashReportURL: function ss_updateCrashReportURL(aWindow) {
#ifdef MOZ_CRASHREPORTER
    if (!aWindow.BrowserApp.selectedBrowser)
      return;

    try {
      let currentURI = aWindow.BrowserApp.selectedBrowser.currentURI.clone();
      // if the current URI contains a username/password, remove it
      try {
        currentURI.userPass = "";
      }
      catch (ex) { } // ignore failures on about: URIs

      CrashReporter.annotateCrashReport("URL", currentURI.spec);
    }
    catch (ex) {
      // don't make noise when crashreporter is built but not enabled
      if (ex.result != Components.results.NS_ERROR_NOT_INITIALIZED)
        Components.utils.reportError("SessionStore:" + ex);
    }
#endif
  },

  _serializeHistoryEntry: function _serializeHistoryEntry(aEntry) {
    let entry = { url: aEntry.URI.spec };

    if (aEntry.title && aEntry.title != entry.url)
      entry.title = aEntry.title;

    if (!(aEntry instanceof Ci.nsISHEntry))
      return entry;

    let cacheKey = aEntry.cacheKey;
    if (cacheKey && cacheKey instanceof Ci.nsISupportsPRUint32 && cacheKey.data != 0)
      entry.cacheKey = cacheKey.data;

    entry.ID = aEntry.ID;
    entry.docshellID = aEntry.docshellID;

    if (aEntry.referrerURI)
      entry.referrer = aEntry.referrerURI.spec;

    if (aEntry.contentType)
      entry.contentType = aEntry.contentType;

    let x = {}, y = {};
    aEntry.getScrollPosition(x, y);
    if (x.value != 0 || y.value != 0)
      entry.scroll = x.value + "," + y.value;

    if (aEntry.owner) {
      try {
        let binaryStream = Cc["@mozilla.org/binaryoutputstream;1"].createInstance(Ci.nsIObjectOutputStream);
        let pipe = Cc["@mozilla.org/pipe;1"].createInstance(Ci.nsIPipe);
        pipe.init(false, false, 0, 0xffffffff, null);
        binaryStream.setOutputStream(pipe.outputStream);
        binaryStream.writeCompoundObject(aEntry.owner, Ci.nsISupports, true);
        binaryStream.close();

        // Now we want to read the data from the pipe's input end and encode it.
        let scriptableStream = Cc["@mozilla.org/binaryinputstream;1"].createInstance(Ci.nsIBinaryInputStream);
        scriptableStream.setInputStream(pipe.inputStream);
        let ownerBytes = scriptableStream.readByteArray(scriptableStream.available());
        // We can stop doing base64 encoding once our serialization into JSON
        // is guaranteed to handle all chars in strings, including embedded
        // nulls.
        entry.owner_b64 = btoa(String.fromCharCode.apply(null, ownerBytes));
      } catch (e) { dump(e); }
    }

    entry.docIdentifier = aEntry.BFCacheEntry.ID;

    if (aEntry.stateData != null) {
      entry.structuredCloneState = aEntry.stateData.getDataAsBase64();
      entry.structuredCloneVersion = aEntry.stateData.formatVersion;
    }

    if (!(aEntry instanceof Ci.nsISHContainer))
      return entry;

    if (aEntry.childCount > 0) {
      let children = [];
      for (let i = 0; i < aEntry.childCount; i++) {
        let child = aEntry.GetChildAt(i);

        if (child) {
          // don't try to restore framesets containing wyciwyg URLs (cf. bug 424689 and bug 450595)
          if (child.URI.schemeIs("wyciwyg")) {
            children = [];
            break;
          }
          children.push(this._serializeHistoryEntry(child));
        }

        if (children.length)
          entry.children = children;
      }
    }

    return entry;
  },

  _deserializeHistoryEntry: function _deserializeHistoryEntry(aEntry, aIdMap, aDocIdentMap) {
    let shEntry = Cc["@mozilla.org/browser/session-history-entry;1"].createInstance(Ci.nsISHEntry);

    shEntry.setURI(Services.io.newURI(aEntry.url, null, null));
    shEntry.setTitle(aEntry.title || aEntry.url);
    if (aEntry.subframe)
      shEntry.setIsSubFrame(aEntry.subframe || false);
    shEntry.loadType = Ci.nsIDocShellLoadInfo.loadHistory;
    if (aEntry.contentType)
      shEntry.contentType = aEntry.contentType;
    if (aEntry.referrer)
      shEntry.referrerURI = Services.io.newURI(aEntry.referrer, null, null);

    if (aEntry.cacheKey) {
      let cacheKey = Cc["@mozilla.org/supports-PRUint32;1"].createInstance(Ci.nsISupportsPRUint32);
      cacheKey.data = aEntry.cacheKey;
      shEntry.cacheKey = cacheKey;
    }

    if (aEntry.ID) {
      // get a new unique ID for this frame (since the one from the last
      // start might already be in use)
      let id = aIdMap[aEntry.ID] || 0;
      if (!id) {
        for (id = Date.now(); id in aIdMap.used; id++);
        aIdMap[aEntry.ID] = id;
        aIdMap.used[id] = true;
      }
      shEntry.ID = id;
    }

    if (aEntry.docshellID)
      shEntry.docshellID = aEntry.docshellID;

    if (aEntry.structuredCloneState && aEntry.structuredCloneVersion) {
      shEntry.stateData =
        Cc["@mozilla.org/docshell/structured-clone-container;1"].
        createInstance(Ci.nsIStructuredCloneContainer);

      shEntry.stateData.initFromBase64(aEntry.structuredCloneState, aEntry.structuredCloneVersion);
    }

    if (aEntry.scroll) {
      let scrollPos = aEntry.scroll.split(",");
      scrollPos = [parseInt(scrollPos[0]) || 0, parseInt(scrollPos[1]) || 0];
      shEntry.setScrollPosition(scrollPos[0], scrollPos[1]);
    }

    let childDocIdents = {};
    if (aEntry.docIdentifier) {
      // If we have a serialized document identifier, try to find an SHEntry
      // which matches that doc identifier and adopt that SHEntry's
      // BFCacheEntry.  If we don't find a match, insert shEntry as the match
      // for the document identifier.
      let matchingEntry = aDocIdentMap[aEntry.docIdentifier];
      if (!matchingEntry) {
        matchingEntry = {shEntry: shEntry, childDocIdents: childDocIdents};
        aDocIdentMap[aEntry.docIdentifier] = matchingEntry;
      } else {
        shEntry.adoptBFCacheEntry(matchingEntry.shEntry);
        childDocIdents = matchingEntry.childDocIdents;
      }
    }

    if (aEntry.owner_b64) {
      let ownerInput = Cc["@mozilla.org/io/string-input-stream;1"].createInstance(Ci.nsIStringInputStream);
      let binaryData = atob(aEntry.owner_b64);
      ownerInput.setData(binaryData, binaryData.length);
      let binaryStream = Cc["@mozilla.org/binaryinputstream;1"].createInstance(Ci.nsIObjectInputStream);
      binaryStream.setInputStream(ownerInput);
      try { // Catch possible deserialization exceptions
        shEntry.owner = binaryStream.readObject(true);
      } catch (ex) { dump(ex); }
    }

    if (aEntry.children && shEntry instanceof Ci.nsISHContainer) {
      for (let i = 0; i < aEntry.children.length; i++) {
        if (!aEntry.children[i].url)
          continue;

        // We're getting sessionrestore.js files with a cycle in the
        // doc-identifier graph, likely due to bug 698656.  (That is, we have
        // an entry where doc identifier A is an ancestor of doc identifier B,
        // and another entry where doc identifier B is an ancestor of A.)
        //
        // If we were to respect these doc identifiers, we'd create a cycle in
        // the SHEntries themselves, which causes the docshell to loop forever
        // when it looks for the root SHEntry.
        //
        // So as a hack to fix this, we restrict the scope of a doc identifier
        // to be a node's siblings and cousins, and pass childDocIdents, not
        // aDocIdents, to _deserializeHistoryEntry.  That is, we say that two
        // SHEntries with the same doc identifier have the same document iff
        // they have the same parent or their parents have the same document.

        shEntry.AddChild(this._deserializeHistoryEntry(aEntry.children[i], aIdMap, childDocIdents), i);
      }
    }

    return shEntry;
  },

  _restoreHistory: function _restoreHistory(aTabData, aHistory) {
    if (aHistory.count > 0)
      aHistory.PurgeHistory(aHistory.count);
    aHistory.QueryInterface(Ci.nsISHistoryInternal);

    // helper hashes for ensuring unique frame IDs and unique document
    // identifiers.
    let idMap = { used: {} };
    let docIdentMap = {};

    for (let i = 0; i < aTabData.entries.length; i++) {
      if (!aTabData.entries[i].url)
        continue;
      aHistory.addEntry(this._deserializeHistoryEntry(aTabData.entries[i], idMap, docIdentMap), true);
    }

    // We need to force set the active history item and cause it to reload since
    // we stop the load above
    let activeIndex = (aTabData.index || aTabData.entries.length) - 1;
    aHistory.getEntryAtIndex(activeIndex, true);
    aHistory.QueryInterface(Ci.nsISHistory).reloadCurrentEntry();
  },

  getBrowserState: function ss_getBrowserState() {
    return this._getCurrentState();
  },

  _restoreWindow: function ss_restoreWindow(aData) {
    let state;
    try {
      state = JSON.parse(aData);
    } catch (e) {
      Cu.reportError("SessionStore: invalid session JSON");
      return false;
    }

    // To do a restore, we must have at least one window with one tab
    if (!state || state.windows.length == 0 || !state.windows[0].tabs || state.windows[0].tabs.length == 0) {
      Cu.reportError("SessionStore: no tabs to restore");
      return false;
    }

    let window = Services.wm.getMostRecentWindow("navigator:browser");

    let tabs = state.windows[0].tabs;
    let selected = state.windows[0].selected;
    if (selected == null || selected > tabs.length) // Clamp the selected index if it's bogus
      selected = 1;

    for (let i = 0; i < tabs.length; i++) {
      let tabData = tabs[i];
      let entry = tabData.entries[tabData.index - 1];

      // Use stubbed tab if we've already created it; otherwise, make a new tab
      let tab;
      if (tabData.tabId == null) {
        let params = {
          selected: (selected == i+1),
          delayLoad: true,
          title: entry.title,
          desktopMode: (tabData.desktopMode == true),
          isPrivate: (tabData.isPrivate == true)
        };
        tab = window.BrowserApp.addTab(entry.url, params);
      } else {
        tab = window.BrowserApp.getTabForId(tabData.tabId);
        delete tabData.tabId;

        // Don't restore tab if user has closed it
        if (tab == null) {
          continue;
        }
      }

      if (window.BrowserApp.selectedTab == tab) {
        this._restoreHistory(tabData, tab.browser.sessionHistory);
        delete tab.browser.__SS_restore;
        tab.browser.removeAttribute("pending");
      } else {
        // Make sure the browser has its session data for the delay reload
        tab.browser.__SS_data = tabData;
        tab.browser.__SS_restore = true;
        tab.browser.setAttribute("pending", "true");
      }

      tab.browser.__SS_extdata = tabData.extData;
    }

    return true;
  },

  getClosedTabCount: function ss_getClosedTabCount(aWindow) {
    if (!aWindow || !aWindow.__SSID || !this._windows[aWindow.__SSID])
      return 0; // not a browser window, or not otherwise tracked by SS.

    return this._windows[aWindow.__SSID].closedTabs.length;
  },

  getClosedTabData: function ss_getClosedTabData(aWindow) {
    if (!aWindow.__SSID)
      throw (Components.returnCode = Cr.NS_ERROR_INVALID_ARG);

    return JSON.stringify(this._windows[aWindow.__SSID].closedTabs);
  },

  undoCloseTab: function ss_undoCloseTab(aWindow, aIndex) {
    if (!aWindow.__SSID)
      throw (Components.returnCode = Cr.NS_ERROR_INVALID_ARG);

    let closedTabs = this._windows[aWindow.__SSID].closedTabs;
    if (!closedTabs)
      return null;

    // default to the most-recently closed tab
    aIndex = aIndex || 0;
    if (!(aIndex in closedTabs))
      throw (Components.returnCode = Cr.NS_ERROR_INVALID_ARG);

    // fetch the data of closed tab, while removing it from the array
    let closedTab = closedTabs.splice(aIndex, 1).shift();

    // create a new tab and bring to front
    let params = { selected: true };
    let tab = aWindow.BrowserApp.addTab(closedTab.entries[closedTab.index - 1].url, params);
    this._restoreHistory(closedTab, tab.browser.sessionHistory);

    // Put back the extra data
    tab.browser.__SS_extdata = closedTab.extData;

    return tab.browser;
  },

  forgetClosedTab: function ss_forgetClosedTab(aWindow, aIndex) {
    if (!aWindow.__SSID)
      throw (Components.returnCode = Cr.NS_ERROR_INVALID_ARG);

    let closedTabs = this._windows[aWindow.__SSID].closedTabs;

    // default to the most-recently closed tab
    aIndex = aIndex || 0;
    if (!(aIndex in closedTabs))
      throw (Components.returnCode = Cr.NS_ERROR_INVALID_ARG);

    // remove closed tab from the array
    closedTabs.splice(aIndex, 1);
  },

  getTabValue: function ss_getTabValue(aTab, aKey) {
    let browser = aTab.browser;
    let data = browser.__SS_extdata || {};
    return data[aKey] || "";
  },

  setTabValue: function ss_setTabValue(aTab, aKey, aStringValue) {
    let browser = aTab.browser;

    if (!browser.__SS_extdata)
      browser.__SS_extdata = {};
    browser.__SS_extdata[aKey] = aStringValue;
    this.saveStateDelayed();
  },

  deleteTabValue: function ss_deleteTabValue(aTab, aKey) {
    let browser = aTab.browser;
    if (browser.__SS_extdata && browser.__SS_extdata[aKey])
      delete browser.__SS_extdata[aKey];
    else
      throw (Components.returnCode = Cr.NS_ERROR_INVALID_ARG);
  },

  shouldRestore: function ss_shouldRestore() {
    return this._shouldRestore;
  },

  restoreLastSession: function ss_restoreLastSession(aNormalRestore, aSessionString) {
    let self = this;

    function restoreWindow(data) {
      if (!self._restoreWindow(data)) {
        throw "Could not restore window";
      }

      notifyObservers();
    }

    function notifyObservers(aMessage) {
      Services.obs.notifyObservers(null, "sessionstore-windows-restored", aMessage || "");
    }

    try {
      if (!aNormalRestore && !this._shouldRestore) {
        // If we're here, it means we're restoring from a crash. Check prefs
        // and other conditions to make sure we want to continue with the
        // restore.
        // TODO: Since the tabs have already been created as stubs after
        // crashing, it's too late to try to abort the restore here. This logic
        // should be moved to Java; see bug 889722.

        // Disable crash recovery if it has been turned off.
        if (!Services.prefs.getBoolPref("browser.sessionstore.resume_from_crash")) {
          throw "Restore is disabled via prefs";
        }

        // Check to see if we've exceeded the maximum number of crashes to
        // avoid a crash loop
        let maxCrashes = Services.prefs.getIntPref("browser.sessionstore.max_resumed_crashes");
        let recentCrashes = Services.prefs.getIntPref("browser.sessionstore.recent_crashes") + 1;
        Services.prefs.setIntPref("browser.sessionstore.recent_crashes", recentCrashes);
        Services.prefs.savePrefFile(null);

        if (recentCrashes > maxCrashes) {
          throw "Exceeded maximum number of allowed restores";
        }
      }

      // Normally, we'll receive the session string from Java, but there are
      // cases where we may want to restore that Java cannot detect (e.g., if
      // browser.sessionstore.resume_session_once is true). In these cases, the
      // session will be read from sessionstore.bak (which is also used for
      // "tabs from last time").
      if (aSessionString == null) {
        Task.spawn(function() {
          let bytes = yield OS.File.read(this._sessionFileBackup.path);
          let data = JSON.parse(new TextDecoder().decode(bytes) || "");
          restoreWindow(data);
        }.bind(this)).then(null, function onError(reason) {
          if (reason instanceof OS.File.Error && reason.becauseNoSuchFile) {
            Cu.reportError("Session file doesn't exist");
          } else {
            Cu.reportError("SessionStore: " + reason.message);
          }
          notifyObservers("fail");
        });
      } else {
        restoreWindow(aSessionString);
      }
    } catch (e) {
      Cu.reportError("SessionStore: " + e);
      notifyObservers("fail");
    }
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([SessionStore]);
