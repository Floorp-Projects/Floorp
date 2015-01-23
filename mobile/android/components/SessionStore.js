/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Task", "resource://gre/modules/Task.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS", "resource://gre/modules/osfile.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Messaging", "resource://gre/modules/Messaging.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils", "resource://gre/modules/PrivateBrowsingUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FormData", "resource://gre/modules/FormData.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "TelemetryStopwatch", "resource://gre/modules/TelemetryStopwatch.jsm");

function dump(a) {
  Services.console.logStringMessage(a);
}

// -----------------------------------------------------------------------
// Session Store
// -----------------------------------------------------------------------

const STATE_STOPPED = 0;
const STATE_RUNNING = 1;

const PRIVACY_NONE = 0;
const PRIVACY_ENCRYPTED = 1;
const PRIVACY_FULL = 2;

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
  _maxTabsUndo: 5,
  _pendingWrite: 0,

  // The index where the most recently closed tab was in the tabs array
  // when it was closed.
  _lastClosedTabIndex: -1,

  // Whether or not to send notifications for changes to the closed tabs.
  _notifyClosedTabs: false,

  init: function ss_init() {
    // Get file references
    this._sessionFile = Services.dirsvc.get("ProfD", Ci.nsILocalFile);
    this._sessionFileBackup = this._sessionFile.clone();
    this._sessionFile.append("sessionstore.js");
    this._sessionFileBackup.append("sessionstore.bak");

    this._loadState = STATE_STOPPED;

    this._interval = Services.prefs.getIntPref("browser.sessionstore.interval");
    this._maxTabsUndo = Services.prefs.getIntPref("browser.sessionstore.max_tabs_undo");
  },

  _clearDisk: function ss_clearDisk() {
    OS.File.remove(this._sessionFile.path);
    OS.File.remove(this._sessionFileBackup.path);
  },

  observe: function ss_observe(aSubject, aTopic, aData) {
    let self = this;
    let observerService = Services.obs;
    switch (aTopic) {
      case "app-startup":
        observerService.addObserver(this, "final-ui-startup", true);
        observerService.addObserver(this, "domwindowopened", true);
        observerService.addObserver(this, "domwindowclosed", true);
        observerService.addObserver(this, "browser:purge-session-history", true);
        observerService.addObserver(this, "Session:Restore", true);
        observerService.addObserver(this, "application-background", true);
        observerService.addObserver(this, "ClosedTabs:StartNotifications", true);
        observerService.addObserver(this, "ClosedTabs:StopNotifications", true);
        observerService.addObserver(this, "last-pb-context-exited", true);
        observerService.addObserver(this, "Session:RestoreRecentTabs", true);
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
      case "browser:purge-session-history": // catch sanitization 
        this._clearDisk();

        // Clear all data about closed tabs
        for (let [ssid, win] in Iterator(this._windows))
          win.closedTabs = [];

        this._lastClosedTabIndex = -1;

        if (this._loadState == STATE_RUNNING) {
          // Save the purged state immediately
          this.saveState();
        }

        Services.obs.notifyObservers(null, "sessionstore-state-purge-complete", "");
        if (this._notifyClosedTabs) {
          this._sendClosedTabsToJava(Services.wm.getMostRecentWindow("navigator:browser"));
        }
        break;
      case "timer-callback":
        // Timer call back for delayed saving
        this._saveTimer = null;
        if (this._pendingWrite) {
          this.saveState();
        }
        break;
      case "Session:Restore": {
        Services.obs.removeObserver(this, "Session:Restore");
        if (aData) {
          // Be ready to handle any restore failures by making sure we have a valid tab opened
          let window = Services.wm.getMostRecentWindow("navigator:browser");
          let restoreCleanup = {
            observe: function (aSubject, aTopic, aData) {
              Services.obs.removeObserver(restoreCleanup, "sessionstore-windows-restored");

              if (window.BrowserApp.tabs.length == 0) {
                window.BrowserApp.addTab("about:home", {
                  selected: true
                });
              }
            }.bind(this)
          };
          Services.obs.addObserver(restoreCleanup, "sessionstore-windows-restored", false);

          // Do a restore, triggered by Java
          let data = JSON.parse(aData);
          this.restoreLastSession(data.sessionString);
        } else {
          // Not doing a restore; just send restore message
          Services.obs.notifyObservers(null, "sessionstore-windows-restored", "");
        }
        break;
      }
      case "application-background":
        // We receive this notification when Android's onPause callback is
        // executed. After onPause, the application may be terminated at any
        // point without notice; therefore, we must synchronously write out any
        // pending save state to ensure that this data does not get lost.
        this.flushPendingState();
        break;
      case "ClosedTabs:StartNotifications":
        this._notifyClosedTabs = true;
        this._sendClosedTabsToJava(Services.wm.getMostRecentWindow("navigator:browser"));
        break;
      case "ClosedTabs:StopNotifications":
        this._notifyClosedTabs = false;
        break;
      case "last-pb-context-exited":
        // Clear private closed tab data when we leave private browsing.
        for (let [, window] in Iterator(this._windows)) {
          window.closedTabs = window.closedTabs.filter(tab => !tab.isPrivate);
        }
        this._lastClosedTabIndex = -1;
        break;
      case "Session:RestoreRecentTabs": {
        let data = JSON.parse(aData);
        this._restoreTabs(data);
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
        this.onTabClose(window, browser, aEvent.detail);
        this.onTabRemove(window, browser);
        break;
      }
      case "TabSelect": {
        let browser = aEvent.target;
        this.onTabSelect(window, browser);
        break;
      }
      case "DOMTitleChanged": {
        let browser = aEvent.currentTarget;

        // Handle only top-level DOMTitleChanged event
        if (browser.contentDocument !== aEvent.originalTarget) {
          return;
        }

        // Use DOMTitleChanged to detect page loads over alternatives.
        // onLocationChange happens too early, so we don't have the page title
        // yet; pageshow happens too late, so we could lose session data if the
        // browser were killed.
        this.onTabLoad(window, browser);
        break;
      }
      case "load": {
        // Handle restoring the text data into the content and frames. We wait
        // until the main content and all frames are loaded before trying to
        // restore the text data.
        let browser = aEvent.currentTarget;
        if (browser.__SS_restore_data) {
          this._restoreTextData(browser.__SS_restore_data, browser);
        }
        break;
      }
      case "change":
      case "input":
      case "DOMAutoComplete": {
        let browser = aEvent.currentTarget;
        this.onTabInput(window, browser);
        break;
      }
    }
  },

  onWindowOpen: function ss_onWindowOpen(aWindow) {
    // Return if window has already been initialized
    if (aWindow && aWindow.__SSID && this._windows[aWindow.__SSID])
      return;

    // Ignore non-browser windows and windows opened while shutting down
    if (aWindow.document.documentElement.getAttribute("windowtype") != "navigator:browser")
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
    // Use DOMTitleChange to catch the initial load and restore history
    aBrowser.addEventListener("DOMTitleChanged", this, true);

    // Use load to restore text data
    aBrowser.addEventListener("load", this, true);

    // Use a combination of events to watch for text data changes
    aBrowser.addEventListener("change", this, true);
    aBrowser.addEventListener("input", this, true);
    aBrowser.addEventListener("DOMAutoComplete", this, true);

    if (!aNoNotification)
      this.saveStateDelayed();
    this._updateCrashReportURL(aWindow);
  },

  onTabRemove: function ss_onTabRemove(aWindow, aBrowser, aNoNotification) {
    // Cleanup event listeners
    aBrowser.removeEventListener("DOMTitleChanged", this, true);
    aBrowser.removeEventListener("load", this, true);
    aBrowser.removeEventListener("change", this, true);
    aBrowser.removeEventListener("input", this, true);
    aBrowser.removeEventListener("DOMAutoComplete", this, true);

    // If this browser is being restored, skip any session save activity
    if (aBrowser.__SS_restore)
      return;

    delete aBrowser.__SS_data;

    if (!aNoNotification)
      this.saveStateDelayed();
  },

  onTabClose: function ss_onTabClose(aWindow, aBrowser, aTabIndex) {
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

      this._lastClosedTabIndex = aTabIndex;

      if (this._notifyClosedTabs) {
        this._sendClosedTabsToJava(aWindow);
      }
    }
  },

  onTabLoad: function ss_onTabLoad(aWindow, aBrowser) {
    // If this browser is being restored, skip any session save activity
    if (aBrowser.__SS_restore)
      return;

    // Ignore a transient "about:blank"
    if (!aBrowser.canGoBack && aBrowser.currentURI.spec == "about:blank")
      return;

    let history = aBrowser.sessionHistory;

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
    this.saveStateDelayed();

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
      this._restoreTab(data, aBrowser);

      delete aBrowser.__SS_restore;
      aBrowser.removeAttribute("pending");
    }

    this.saveStateDelayed();
    this._updateCrashReportURL(aWindow);

    // If the selected tab has changed while listening for closed tab
    // notifications, we may have switched between different private browsing
    // modes.
    if (this._notifyClosedTabs) {
      this._sendClosedTabsToJava(aWindow);
    }
  },

  onTabInput: function ss_onTabInput(aWindow, aBrowser) {
    // If this browser is being restored, skip any session save activity
    if (aBrowser.__SS_restore) {
      return;
    }

    // Don't bother trying to save text data if we don't have history yet
    let data = aBrowser.__SS_data;
    if (!data || data.entries.length == 0) {
      return;
    }

    // Start with storing the main content
    let content = aBrowser.contentWindow;

    // If the main content document has an associated URL that we are not
    // allowed to store data for, bail out. We explicitly discard data for any
    // children as well even if storing data for those frames would be allowed.
    if (!this.checkPrivacyLevel(content.document.documentURI)) {
      return;
    }

    // Store the main content
    let formdata = FormData.collect(content) || {};

    // Loop over direct child frames, and store the text data
    let children = [];
    for (let i = 0; i < content.frames.length; i++) {
      let frame = content.frames[i];
      if (!this.checkPrivacyLevel(frame.document.documentURI)) {
        continue;
      }

      let result = FormData.collect(frame);
      if (result && Object.keys(result).length) {
        children[i] = result;
      }
    }

    // If any frame had text data, add it to the main form data
    if (children.length) {
      formdata.children = children;
    }

    // If we found any form data, main content or frames, let's save it
    if (Object.keys(formdata).length) {
      data.formdata = formdata;
      this.saveStateDelayed();
    }
  },

  saveStateDelayed: function ss_saveStateDelayed() {
    if (!this._saveTimer) {
      // Interval until the next disk operation is allowed
      let minimalDelay = this._lastSaveTime + this._interval - Date.now();

      // If we have to wait, set a timer, otherwise saveState directly
      let delay = Math.max(minimalDelay, 2000);
      if (delay > 0) {
        this._pendingWrite++;
        this._saveTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
        this._saveTimer.init(this, delay, Ci.nsITimer.TYPE_ONE_SHOT);
      } else {
        this.saveState();
      }
    }
  },

  saveState: function ss_saveState() {
    this._pendingWrite++;
    this._saveState(true);
  },

  // Immediately and synchronously writes any pending state to disk.
  flushPendingState: function ss_flushPendingState() {
    if (this._pendingWrite) {
      this._saveState(false);
    }
  },

  _saveState: function ss_saveState(aAsync) {
    // Kill any queued timer and save immediately
    if (this._saveTimer) {
      this._saveTimer.cancel();
      this._saveTimer = null;
    }

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

      // Save normal closed tabs. Forget about private closed tabs.
      normalWin.closedTabs = win.closedTabs.filter(tab => !tab.isPrivate);

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
    this._writeFile(this._sessionFile, normalData, aAsync);

    // If we have private data, send it to Java; otherwise, send null to
    // indicate that there is no private data
    Messaging.sendRequest({
      type: "PrivateBrowsing:Data",
      session: (privateData.windows.length > 0 && privateData.windows[0].tabs.length > 0) ? JSON.stringify(privateData) : null
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

  /**
   * Writes the session state to a disk file, while doing some telemetry and notification
   * bookkeeping.
   * @param aFile nsIFile used for saving the session
   * @param aData JSON session state
   * @param aAsync boolelan used to determine the method of saving the state
   */
  _writeFile: function ss_writeFile(aFile, aData, aAsync) {
    TelemetryStopwatch.start("FX_SESSION_RESTORE_SERIALIZE_DATA_MS");
    let state = JSON.stringify(aData);
    TelemetryStopwatch.finish("FX_SESSION_RESTORE_SERIALIZE_DATA_MS");

    // Convert data string to a utf-8 encoded array buffer
    let buffer = new TextEncoder().encode(state);
    Services.telemetry.getHistogramById("FX_SESSION_RESTORE_FILE_SIZE_BYTES").add(buffer.byteLength);

    Services.obs.notifyObservers(null, "sessionstore-state-write", "");
    let startWriteMs = Cu.now();
 
    let pendingWrite = this._pendingWrite;
    this._write(aFile, buffer, aAsync).then(() => {
      let stopWriteMs = Cu.now();

      // Make sure this._pendingWrite is the same value it was before we
      // fired off the async write. If the count is different, another write
      // is pending, so we shouldn't reset this._pendingWrite yet.
      if (pendingWrite === this._pendingWrite) {
        this._pendingWrite = 0;
      }

      // We don't use a stopwatch here since the calls are async and stopwatches can only manage
      // a single timer per histogram.
      Services.telemetry.getHistogramById("FX_SESSION_RESTORE_WRITE_FILE_MS").add(Math.round(stopWriteMs - startWriteMs));
      Services.obs.notifyObservers(null, "sessionstore-state-write-complete", "");
    });
  },

  /**
   * Writes the session state to a disk file, using async or sync methods
   * @param aFile nsIFile used for saving the session
   * @param aBuffer UTF-8 encoded ArrayBuffer of the session state
   * @param aAsync boolelan used to determine the method of saving the state
   * @return Promise that resolves when the file has been written
   */
  _write: function ss_write(aFile, aBuffer, aAsync) {
    // Use async file writer and just return it's promise
    if (aAsync) {
      return OS.File.writeAtomic(aFile.path, aBuffer, { tmpPath: aFile.path + ".tmp" });
    }

    // Convert buffer to an encoded string and sync write to disk
    let bytes = String.fromCharCode.apply(null, new Uint16Array(aBuffer));
    let stream = Cc["@mozilla.org/network/file-output-stream;1"].createInstance(Ci.nsIFileOutputStream);
    stream.init(aFile, 0x02 | 0x08 | 0x20, 0666, 0);
    stream.write(bytes, bytes.length);
    stream.close();

    // Return a resolved promise to make the caller happy
    return Promise.resolve();
  },

  _updateCrashReportURL: function ss_updateCrashReportURL(aWindow) {
    let crashReporterBuilt = "nsICrashReporter" in Ci && Services.appinfo instanceof Ci.nsICrashReporter;
    if (!crashReporterBuilt) {
      return;
    }

    if (!aWindow.BrowserApp.selectedBrowser) {
      return;
    }

    try {
      let currentURI = aWindow.BrowserApp.selectedBrowser.currentURI.clone();
      // if the current URI contains a username/password, remove it
      try {
        currentURI.userPass = "";
      } catch (ex) { } // ignore failures on about: URIs

      Services.appinfo.annotateCrashReport("URL", currentURI.spec);
    } catch (ex) {
      // don't make noise when crashreporter is built but not enabled
      if (ex.result != Cr.NS_ERROR_NOT_INITIALIZED) {
        Cu.reportError("SessionStore:" + ex);
      }
    }
  },

  /**
   * Determines whether a given session history entry has been added dynamically.
   */
  isDynamic: function(aEntry) {
    // aEntry.isDynamicallyAdded() is true for dynamically added
    // <iframe> and <frameset>, but also for <html> (the root of the
    // document) so we use aEntry.parent to ensure that we're not looking
    // at the root of the document
    return aEntry.parent && aEntry.isDynamicallyAdded();
  },

  /**
  * Get an object that is a serialized representation of a History entry.
  */
  _serializeHistoryEntry: function _serializeHistoryEntry(aEntry) {
    let entry = { url: aEntry.URI.spec };

    if (aEntry.title && aEntry.title != entry.url) {
      entry.title = aEntry.title;
    }

    if (!(aEntry instanceof Ci.nsISHEntry)) {
      return entry;
    }

    let cacheKey = aEntry.cacheKey;
    if (cacheKey && cacheKey instanceof Ci.nsISupportsPRUint32 && cacheKey.data != 0) {
      entry.cacheKey = cacheKey.data;
    }

    entry.ID = aEntry.ID;
    entry.docshellID = aEntry.docshellID;

    if (aEntry.referrerURI) {
      entry.referrer = aEntry.referrerURI.spec;
    }

    if (aEntry.contentType) {
      entry.contentType = aEntry.contentType;
    }

    let x = {}, y = {};
    aEntry.getScrollPosition(x, y);
    if (x.value != 0 || y.value != 0) {
      entry.scroll = x.value + "," + y.value;
    }

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

    if (!(aEntry instanceof Ci.nsISHContainer)) {
      return entry;
    }

    if (aEntry.childCount > 0) {
      let children = [];
      for (let i = 0; i < aEntry.childCount; i++) {
        let child = aEntry.GetChildAt(i);

        if (child && !this.isDynamic(child)) {
          // don't try to restore framesets containing wyciwyg URLs (cf. bug 424689 and bug 450595)
          if (child.URI.schemeIs("wyciwyg")) {
            children = [];
            break;
          }
          children.push(this._serializeHistoryEntry(child));
        }
      }

      if (children.length) {
        entry.children = children;
      }
    }

    return entry;
  },

  _deserializeHistoryEntry: function _deserializeHistoryEntry(aEntry, aIdMap, aDocIdentMap) {
    let shEntry = Cc["@mozilla.org/browser/session-history-entry;1"].createInstance(Ci.nsISHEntry);

    shEntry.setURI(Services.io.newURI(aEntry.url, null, null));
    shEntry.setTitle(aEntry.title || aEntry.url);
    if (aEntry.subframe) {
      shEntry.setIsSubFrame(aEntry.subframe || false);
    }
    shEntry.loadType = Ci.nsIDocShellLoadInfo.loadHistory;
    if (aEntry.contentType) {
      shEntry.contentType = aEntry.contentType;
    }
    if (aEntry.referrer) {
      shEntry.referrerURI = Services.io.newURI(aEntry.referrer, null, null);
    }

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

    if (aEntry.docshellID) {
      shEntry.docshellID = aEntry.docshellID;
    }

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
        if (!aEntry.children[i].url) {
          continue;
        }

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

  // This function iterates through a list of tab data restoring session for each of them.
  _restoreTabs: function ss_restoreTabs(aData) {
    let window = Services.wm.getMostRecentWindow("navigator:browser");
    for (let i = 0; i < aData.tabs.length; i++) {
      let tabData = JSON.parse(aData.tabs[i]);
      let params = {
        selected: (i == aData.tabs.length - 1),
        isPrivate: tabData.isPrivate,
        desktopMode: tabData.desktopMode,
      };

      let tab = window.BrowserApp.addTab(tabData.entries[tabData.index - 1].url, params);
      this._restoreTab(tabData, tab.browser);
    }
  },

  /**
   * Don't save sensitive data if the user doesn't want to
   * (distinguishes between encrypted and non-encrypted sites)
   */
  checkPrivacyLevel: function ss_checkPrivacyLevel(aURL) {
    let isHTTPS = aURL.startsWith("https:");
    let pref = "browser.sessionstore.privacy_level";
    return Services.prefs.getIntPref(pref) < (isHTTPS ? PRIVACY_ENCRYPTED : PRIVACY_FULL);
  },

  /**
  * Starts the restoration process for a browser. History is restored at this
  * point, but text data must be delayed until the content loads.
  */
  _restoreTab: function ss_restoreTab(aTabData, aBrowser) {
    this._restoreHistory(aTabData, aBrowser.sessionHistory);

    // Restoring the text data requires waiting for the content to load. So
    // we set a flag and delay this until the "load" event.
    //this._restoreTextData(aTabData, aBrowser);
    aBrowser.__SS_restore_data = aTabData || {};
  },

  /**
  * Takes serialized history data and create news entries into the given
  * nsISessionHistory object.
  */
  _restoreHistory: function ss_restoreHistory(aTabData, aHistory) {
    if (aHistory.count > 0) {
      aHistory.PurgeHistory(aHistory.count);
    }
    aHistory.QueryInterface(Ci.nsISHistoryInternal);

    // Helper hashes for ensuring unique frame IDs and unique document
    // identifiers.
    let idMap = { used: {} };
    let docIdentMap = {};

    for (let i = 0; i < aTabData.entries.length; i++) {
      if (!aTabData.entries[i].url) {
        continue;
      }
      aHistory.addEntry(this._deserializeHistoryEntry(aTabData.entries[i], idMap, docIdentMap), true);
    }

    // We need to force set the active history item and cause it to reload since
    // we stop the load above
    let activeIndex = (aTabData.index || aTabData.entries.length) - 1;
    aHistory.getEntryAtIndex(activeIndex, true);
    aHistory.QueryInterface(Ci.nsISHistory).reloadCurrentEntry();
  },

  /**
  * Takes serialized form text data and restores it into the given browser.
  */
  _restoreTextData: function ss_restoreTextData(aTabData, aBrowser) {
    let formdata = aTabData.formdata;
    if (formdata) {
      FormData.restoreTree(aBrowser.contentWindow, formdata);
    }
    delete aBrowser.__SS_restore_data;
  },

  getBrowserState: function ss_getBrowserState() {
    return this._getCurrentState();
  },

  _restoreWindow: function ss_restoreWindow(aData) {
    let state;
    try {
      state = JSON.parse(aData);
    } catch (e) {
      throw "Invalid session JSON: " + aData;
    }

    // To do a restore, we must have at least one window with one tab
    if (!state || state.windows.length == 0 || !state.windows[0].tabs || state.windows[0].tabs.length == 0) {
      throw "Invalid session JSON: " + aData;
    }

    let window = Services.wm.getMostRecentWindow("navigator:browser");

    let tabs = state.windows[0].tabs;
    let selected = state.windows[0].selected;
    if (selected == null || selected > tabs.length) { // Clamp the selected index if it's bogus
      selected = 1;
    }

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
        this._restoreTab(tabData, tab.browser);

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

    // Restore the closed tabs array on the current window.
    if (state.windows[0].closedTabs) {
      this._windows[window.__SSID].closedTabs = state.windows[0].closedTabs;
    }
  },

  getClosedTabCount: function ss_getClosedTabCount(aWindow) {
    if (!aWindow || !aWindow.__SSID || !this._windows[aWindow.__SSID])
      return 0; // not a browser window, or not otherwise tracked by SS.

    return this._windows[aWindow.__SSID].closedTabs.length;
  },

  getClosedTabs: function ss_getClosedTabs(aWindow) {
    if (!aWindow.__SSID)
      throw (Components.returnCode = Cr.NS_ERROR_INVALID_ARG);

    return this._windows[aWindow.__SSID].closedTabs;
  },

  undoCloseTab: function ss_undoCloseTab(aWindow, aCloseTabData) {
    if (!aWindow.__SSID)
      throw (Components.returnCode = Cr.NS_ERROR_INVALID_ARG);

    let closedTabs = this._windows[aWindow.__SSID].closedTabs;
    if (!closedTabs)
      return null;

    // If the tab data is in the closedTabs array, remove it.
    closedTabs.find(function (tabData, i) {
      if (tabData == aCloseTabData) {
        closedTabs.splice(i, 1);
        return true;
      }
    });

    // create a new tab and bring to front
    let params = {
      selected: true,
      isPrivate: aCloseTabData.isPrivate,
      desktopMode: aCloseTabData.desktopMode,
      tabIndex: this._lastClosedTabIndex
    };
    let tab = aWindow.BrowserApp.addTab(aCloseTabData.entries[aCloseTabData.index - 1].url, params);
    this._restoreTab(aCloseTabData, tab.browser);

    this._lastClosedTabIndex = -1;

    // Put back the extra data
    tab.browser.__SS_extdata = aCloseTabData.extData;

    if (this._notifyClosedTabs) {
      this._sendClosedTabsToJava(aWindow);
    }

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

    // Forget the last closed tab index if we're forgetting the last closed tab.
    if (aIndex == 0) {
      this._lastClosedTabIndex = -1;
    }
    if (this._notifyClosedTabs) {
      this._sendClosedTabsToJava(aWindow);
    }
  },

  _sendClosedTabsToJava: function ss_sendClosedTabsToJava(aWindow) {
    if (!aWindow.__SSID)
      throw (Components.returnCode = Cr.NS_ERROR_INVALID_ARG);

    let closedTabs = this._windows[aWindow.__SSID].closedTabs;
    let isPrivate = PrivateBrowsingUtils.isBrowserPrivate(aWindow.BrowserApp.selectedBrowser);

    let tabs = closedTabs
      .filter(tab => tab.isPrivate == isPrivate)
      .map(function (tab) {
        // Get the url and title for the last entry in the session history.
        let lastEntry = tab.entries[tab.entries.length - 1];
        return {
          url: lastEntry.url,
          title: lastEntry.title || "",
          data: tab
        };
      });

    Messaging.sendRequest({
      type: "ClosedTabs:Data",
      tabs: tabs
    });
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

  restoreLastSession: Task.async(function* (aSessionString) {
    let notifyMessage = "";

    try {
      this._restoreWindow(aSessionString);
    } catch (e) {
      Cu.reportError("SessionStore: " + e);
      notifyMessage = "fail";
    }

    Services.obs.notifyObservers(null, "sessionstore-windows-restored", notifyMessage);
  }),

  removeWindow: function ss_removeWindow(aWindow) {
    if (!aWindow || !aWindow.__SSID || !this._windows[aWindow.__SSID])
      return;

    delete this._windows[aWindow.__SSID];
    delete aWindow.__SSID;

    this.saveState();
  }

};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([SessionStore]);
