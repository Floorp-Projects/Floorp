/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Session Store.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mark Finkle <mfinkle@mozilla.com>
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

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "gObserverService",
  "@mozilla.org/observer-service;1", "nsIObserverService");

XPCOMUtils.defineLazyServiceGetter(this, "gWindowMediator",
  "@mozilla.org/appshell/window-mediator;1", "nsIWindowMediator");

XPCOMUtils.defineLazyServiceGetter(this, "gPrefService",
  "@mozilla.org/preferences-service;1", "nsIPrefBranch2");

#ifdef MOZ_CRASH_REPORTER
XPCOMUtils.defineLazyServiceGetter(this, "CrashReporter",
  "@mozilla.org/xre/app-info;1", "nsICrashReporter");
#endif

XPCOMUtils.defineLazyGetter(this, "NetUtil", function() {
  Cu.import("resource://gre/modules/NetUtil.jsm");
  return NetUtil;
});

// -----------------------------------------------------------------------
// Session Store
// -----------------------------------------------------------------------

const STATE_STOPPED = 0;
const STATE_RUNNING = 1;
const STATE_QUITTING = -1;

function SessionStore() { }

SessionStore.prototype = {
  classID: Components.ID("{90c3dfaf-4245-46d3-9bc1-1d8251ff8c01}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDOMEventListener, Ci.nsIObserver, Ci.nsISupportsWeakReference]),

  _windows: {},
  _lastSaveTime: 0,
  _interval: 15000,
  
  init: function ss_init() {
    // Get file references
    let dirService = Cc["@mozilla.org/file/directory_service;1"].getService(Ci.nsIProperties);
    this._sessionFile = dirService.get("ProfD", Ci.nsILocalFile);
    this._sessionFileBackup = this._sessionFile.clone();
    this._sessionFile.append("sessionstore.js");
    this._sessionFileBackup.append("sessionstore.bak");

    this._loadState = STATE_STOPPED;

    try {
      if (this._sessionFileBackup.exists())
        this._sessionFileBackup.remove(false);
      if (this._sessionFile.exists())
        this._sessionFile.copyTo(null, this._sessionFileBackup.leafName);
    } catch (ex) {
      Cu.reportError(ex);  // file was write-locked?
    }

    try {
      this._interval = gPrefService.getIntPref("sessionstore.interval");
    } catch (e) {}
  },
  
  observe: function ss_observe(aSubject, aTopic, aData) {
    let self = this;
    switch (aTopic) {
      case "app-startup": 
        gObserverService.addObserver(this, "final-ui-startup", true);
        gObserverService.addObserver(this, "domwindowopened", true);
        gObserverService.addObserver(this, "domwindowclosed", true);
        gObserverService.addObserver(this, "browser-lastwindow-close-granted", true);
        gObserverService.addObserver(this, "quit-application-requested", true);
        gObserverService.addObserver(this, "quit-application-granted", true);
        gObserverService.addObserver(this, "quit-application", true);
        break;
      case "final-ui-startup": 
        gObserverService.removeObserver(this, "final-ui-startup");
        this.init();
        break;
      case "domwindowopened":
        let window = aSubject;
        window.addEventListener("load", function() {
          self.onWindowOpen(window);
          window.removeEventListener("load", arguments.callee, false);
        }, false);
        break;
      case "domwindowclosed": // catch closed windows
        this.onWindowClose(aSubject);
        break;
      case "browser-lastwindow-close-granted":
        // Force and open timer to save state
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
      case "quit-application":
        // Freeze the data at what we've got (ignoring closing windows)
        this._loadState = STATE_QUITTING;

        gObserverService.removeObserver(this, "domwindowopened");
        gObserverService.removeObserver(this, "domwindowclosed");
        gObserverService.removeObserver(this, "browser-lastwindow-close-granted");
        gObserverService.removeObserver(this, "quit-application-requested");
        gObserverService.removeObserver(this, "quit-application-granted");
        gObserverService.removeObserver(this, "quit-application");

        // Make sure to break our cycle with the save timer
        if (this._saveTimer) {
          this._saveTimer.cancel();
          this._saveTimer = null;
        }
        break;
      case "timer-callback":
        // Timer call back for delayed saving
        this._saveTimer = null;
        this.saveState();
        break;
    }
  },

  handleEvent: function ss_handleEvent(aEvent) {
    let window = aEvent.currentTarget.ownerDocument.defaultView;
    switch (aEvent.type) {
      case "load":
      case "pageshow":
        this.onTabLoad(window, aEvent.currentTarget, aEvent);
        break;
      case "TabOpen":
      case "TabClose":
        let browser = window.Browser.getTabFromChrome(aEvent.originalTarget).browser;
        if (aEvent.type == "TabOpen") {
          this.onTabAdd(window, browser);
        }
        else {
          this.onTabClose(window, aEvent.originalTarget);
          this.onTabRemove(window, browser);
        }
        break;
      case "TabSelect":
        this.onTabSelect(window);
        break;
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
    this._windows[aWindow.__SSID] = { tabs: [], selected: 0 };

    // Perform additional initialization when the first window is loading
    if (this._loadState == STATE_STOPPED) {
      this._loadState = STATE_RUNNING;
      this._lastSaveTime = Date.now();
    }

    // Add tab change listeners to all already existing tabs
    let tabs = aWindow.Browser.tabs;
    for (let i = 0; i < tabs.length; i++)
      this.onTabAdd(aWindow, tabs[i].browser, true);
    
    // Notification of tab add/remove/selection
    let tabContainer = aWindow.document.getElementById("tabs");
    tabContainer.addEventListener("TabOpen", this, true);
    tabContainer.addEventListener("TabClose", this, true);
    tabContainer.addEventListener("TabSelect", this, true);
  },
  
  onWindowClose: function ss_onWindowClose(aWindow) {
    // Ignore windows not tracked by SessionStore
    if (!aWindow.__SSID || !this._windows[aWindow.__SSID])
      return;
    
    let tabContainer = aWindow.document.getElementById("tabs");
    tabContainer.removeEventListener("TabOpen", this, true);
    tabContainer.removeEventListener("TabClose", this, true);
    tabContainer.removeEventListener("TabSelect", this, true);
    
    if (this._loadState == STATE_RUNNING) {
      // Update all window data for a last time
      this._collectWindowData(aWindow);
      
      // Clear this window from the list
      delete this._windows[aWindow.__SSID];
      
      // Save the state without this window to disk
      this.saveStateDelayed();
    }

    let tabs = aWindow.Browser.tabs;
    for (let i = 0; i < tabs.length; i++)
      this.onTabRemove(aWindow, tabs[i].browser, true);
    
    delete aWindow.__SSID;
  },
  
  onTabAdd: function ss_onTabAdd(aWindow, aBrowser, aNoNotification) {
    aBrowser.addEventListener("load", this, true);
    aBrowser.addEventListener("pageshow", this, true);
    
    if (!aNoNotification) {
      this.saveStateDelayed(aWindow);
    }

    this._updateCrashReportURL(aWindow);
  },

  onTabRemove: function ss_onTabRemove(aWindow, aBrowser, aNoNotification) {
    aBrowser.removeEventListener("load", this, true);
    aBrowser.removeEventListener("pageshow", this, true);
    
    delete aBrowser.__SS_data;
    
    if (!aNoNotification) {
      this.saveStateDelayed(aWindow);
    }
  },

  onTabClose: function ss_onTabClose(aWindow, aTab) {
  },

  onTabLoad: function ss_onTabLoad(aWindow, aBrowser, aEvent) { 
    // react on "load" and solitary "pageshow" events (the first "pageshow"
    // following "load" is too late for deleting the data caches)
    if (aEvent.type != "load" && !aEvent.persisted) {
      return;
    }
    
    delete aBrowser.__SS_data;
    this._collectTabData(aBrowser);

    this.saveStateDelayed(aWindow);

    this._updateCrashReportURL(aWindow);
  },

  onTabSelect: function ss_onTabSelect(aWindow) {
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
      }
      else {
        this.saveState();
      }
    }
  },

  saveState: function ss_saveState() {
    let self = this;
    this._forEachBrowserWindow(function(aWindow) {
      self._collectWindowData(aWindow);
    });

    let data = { windows: [] };
    let index;
    for (index in this._windows)
      data.windows.push(this._windows[index]);
    
    this._writeFile(this._sessionFile, JSON.stringify(data));

    this._lastSaveTime = Date.now();
  },
  
  _collectTabData: function ss__collectTabData(aBrowser) {
    let tabData = { url: aBrowser.currentURI.spec, title: aBrowser.contentTitle };
    aBrowser.__SS_data = tabData;
  },
  
  _collectWindowData: function ss__collectWindowData(aWindow) {
    // Ignore windows not tracked by SessionStore
    if (!aWindow.__SSID || !this._windows[aWindow.__SSID])
      return;

    let winData = this._windows[aWindow.__SSID];
    winData.tabs = [];

    let tabs = aWindow.Browser.tabs;
    for (let i = 0; i < tabs.length; i++) {
      if (tabs[i].browser.__SS_data)
        winData.tabs.push(tabs[i].browser.__SS_data);
    }
  },

  _forEachBrowserWindow: function ss_forEachBrowserWindow(aFunc) {
    let windowsEnum = gWindowMediator.getEnumerator("navigator:browser");
    while (windowsEnum.hasMoreElements()) {
      let window = windowsEnum.getNext();
      if (window.__SSID && !window.closed)
        aFunc.call(this, window);
    }
  },

  _writeFile: function ss_writeFile(aFile, aData) {
    let stateString = Cc["@mozilla.org/supports-string;1"].createInstance(Ci.nsISupportsString);
    stateString.data = aData;
    gObserverService.notifyObservers(stateString, "sessionstore-state-write", "");

    // Don't touch the file if an observer has deleted all state data
    if (!stateString.data)
      return;

    // Initialize the file output stream.
    let ostream = Cc["@mozilla.org/network/safe-file-output-stream;1"].createInstance(Ci.nsIFileOutputStream);
    ostream.init(aFile, 0x02 | 0x08 | 0x20, 0600, 0);

    // Obtain a converter to convert our data to a UTF-8 encoded input stream.
    let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"].createInstance(Ci.nsIScriptableUnicodeConverter);
    converter.charset = "UTF-8";

    // Asynchronously copy the data to the file.
    let istream = converter.convertToInputStream(aData);
    NetUtil.asyncCopy(istream, ostream, function(rc) {
      if (Components.isSuccessCode(rc)) {
        gObserverService.notifyObservers(null, "sessionstore-state-write-complete", "");
      }
    });
  },

  _updateCrashReportURL: function ss_updateCrashReportURL(aWindow) {
#ifdef MOZ_CRASH_REPORTER
    try {
      let currentURI = aWindow.Browser.selectedBrowser.currentURI.clone();
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
  }
};

const NSGetFactory = XPCOMUtils.generateNSGetFactory([SessionStore]);
