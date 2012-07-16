/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["Aitc"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Webapps.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

Cu.import("resource://services-aitc/manager.js");
Cu.import("resource://services-common/utils.js");
Cu.import("resource://services-common/log4moz.js");
Cu.import("resource://services-common/preferences.js");

function Aitc() {
  this._log = Log4Moz.repository.getLogger("Service.AITC");
  this._log.level = Log4Moz.Level[Preferences.get(
    "services.aitc.service.log.level"
  )];
  this._log.info("Loading AitC");

  this.DASHBOARD_ORIGIN = CommonUtils.makeURI(
    Preferences.get("services.aitc.dashboard.url")
  ).prePath;

  let self = this;
  this._manager = new AitcManager(function managerDone() {
    CommonUtils.nextTick(self._init, self);
  });
}
Aitc.prototype = {
  // The goal of the init function is to be ready to activate the AITC
  // client whenever the user is looking at the dashboard. It also calls
  // the initialSchedule function on the manager.
  _init: function _init() {
    let self = this;

    // Do an initial upload.
    this._manager.initialSchedule(function queueDone(num) {
      if (num == -1) {
        self._log.debug("No initial upload was required");
        return;
      }
      self._log.debug(num + " initial apps queued successfully");
    });

    // This is called iff the user is currently looking the dashboard.
    function dashboardLoaded(browser) {
      let win = browser.contentWindow;
      self._log.info("Dashboard was accessed " + win);

      // If page is ready to go, fire immediately.
      if (win.document && win.document.readyState == "complete") {
        self._manager.userActive(win);
        return;
      }

      // Only fire event after the page fully loads.
      browser.contentWindow.addEventListener(
        "DOMContentLoaded",
        function _contentLoaded(event) {
          self._manager.userActive(win);
        },
        false
      );
    }

    // This is called when the user's attention is elsewhere.
    function dashboardUnloaded() {
      self._log.info("Dashboard closed or in background");
      self._manager.userIdle();
    }

    // Called when a URI is loaded in any tab. We have to listen for this
    // because tabSelected is not called if I open a new tab which loads
    // about:home and then navigate to the dashboard, or navigation via
    // links on the currently open tab.
    let listener = {
      onLocationChange: function onLocationChange(browser, pr, req, loc, flag) {
        let win = Services.wm.getMostRecentWindow("navigator:browser");
        if (win.gBrowser.selectedBrowser == browser) {
          if (loc.prePath == self.DASHBOARD_ORIGIN) {
            dashboardLoaded(browser);
          }
        }
      }
    };
    // Called when the current tab selection changes.
    function tabSelected(event) {
      let browser = event.target.linkedBrowser;
      if (browser.currentURI.prePath == self.DASHBOARD_ORIGIN) {
        dashboardLoaded(browser);
      } else {
        dashboardUnloaded();
      }
    }

    // Add listeners for all windows opened in the future.
    function winWatcher(subject, topic) {
      if (topic != "domwindowopened") {
        return;
      }
      subject.addEventListener("load", function winWatcherLoad() {
        subject.removeEventListener("load", winWatcherLoad, false);
        let doc = subject.document.documentElement;
        if (doc.getAttribute("windowtype") == "navigator:browser") {
          let browser = subject.gBrowser;
          browser.addTabsProgressListener(listener);
          browser.tabContainer.addEventListener("TabSelect", tabSelected);
        }
      }, false);
    }
    Services.ww.registerNotification(winWatcher);

    // Add listeners for all current open windows.
    let enumerator = Services.wm.getEnumerator("navigator:browser");
    while (enumerator.hasMoreElements()) {
      let browser = enumerator.getNext().gBrowser;
      browser.addTabsProgressListener(listener);
      browser.tabContainer.addEventListener("TabSelect", tabSelected);

      // Also check the currently open URI.
      if (browser.currentURI.prePath == this.DASHBOARD_ORIGIN) {
        dashboardLoaded(browser);
      }
    }

    // Add listeners for app installs/uninstall.
    Services.obs.addObserver(this, "webapps-sync-install", false);
    Services.obs.addObserver(this, "webapps-sync-uninstall", false);

    // Add listener for idle service.
    let idleSvc = Cc["@mozilla.org/widget/idleservice;1"].
                  getService(Ci.nsIIdleService);
    idleSvc.addIdleObserver(this,
                            Preferences.get("services.aitc.main.idleTime"));
  },

  observe: function(subject, topic, data) {
    let app;
    switch (topic) {
      case "webapps-sync-install":
        app = JSON.parse(data);
        this._log.info(app.origin + " was installed, initiating PUT");
        this._manager.appEvent("install", app);
        break;
      case "webapps-sync-uninstall":
        app = JSON.parse(data);
        this._log.info(app.origin + " was uninstalled, initiating PUT");
        this._manager.appEvent("uninstall", app);
        break;
      case "idle":
        this._log.info("User went idle");
        if (this._manager) {
          this._manager.userIdle();
        }
        break;
      case "back":
        this._log.info("User is no longer idle");
        let win = Services.wm.getMostRecentWindow("navigator:browser");
        if (win && win.gBrowser.currentURI.prePath == this.DASHBOARD_ORIGIN &&
            this._manager) {
          this._manager.userActive();
        }
        break;
    }
  },

};
