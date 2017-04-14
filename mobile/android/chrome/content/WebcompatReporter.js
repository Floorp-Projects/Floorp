/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils",
                                  "resource://gre/modules/PrivateBrowsingUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Snackbars", "resource://gre/modules/Snackbars.jsm");

var WebcompatReporter = {
  menuItem: null,
  menuItemEnabled: null,
  init: function() {
    GlobalEventDispatcher.registerListener(this, "DesktopMode:Change");
    Services.obs.addObserver(this, "chrome-document-global-created");
    Services.obs.addObserver(this, "content-document-global-created");

    let visible = true;
    if ("@mozilla.org/parental-controls-service;1" in Cc) {
      let pc = Cc["@mozilla.org/parental-controls-service;1"].createInstance(Ci.nsIParentalControlsService);
      visible = !pc.parentalControlsEnabled;
    }

    this.addMenuItem(visible);
  },

  onEvent: function(event, data, callback) {
    if (event === "DesktopMode:Change") {
      let tab = BrowserApp.getTabForId(data.tabId);
      let currentURI = tab.browser.currentURI.spec;
      if (data.desktopMode && this.isReportableUrl(currentURI)) {
        this.reportDesktopModePrompt(tab);
      }
    }
  },

  observe: function(subject, topic, data) {
    if (topic == "content-document-global-created" || topic == "chrome-document-global-created") {
      let win = subject;
      let currentURI = win.document.documentURI;

      // Ignore non top-level documents
      if (currentURI !== win.top.location.href) {
        return;
      }

      if (!this.menuItemEnabled && this.isReportableUrl(currentURI)) {
        NativeWindow.menu.update(this.menuItem, {enabled: true});
        this.menuItemEnabled = true;
      } else if (this.menuItemEnabled && !this.isReportableUrl(currentURI)) {
        NativeWindow.menu.update(this.menuItem, {enabled: false});
        this.menuItemEnabled = false;
      }
    }
  },

  addMenuItem: function(visible) {
    this.menuItem = NativeWindow.menu.add({
      name: this.strings.GetStringFromName("webcompat.menu.name"),
      callback: () => {
        Promise.resolve(BrowserApp.selectedTab).then(this.getScreenshot)
                                               .then(this.reportIssue)
                                               .catch(Cu.reportError);
      },
      enabled: false,
      visible: visible,
    });
  },

  getScreenshot: (tab) => {
    return new Promise((resolve) => {
      try {
        let win = tab.window;
        let dpr = win.devicePixelRatio;
        let canvas = win.document.createElement("canvas");
        let ctx = canvas.getContext("2d");
        // Grab the visible viewport coordinates
        let x = win.document.documentElement.scrollLeft;
        let y = win.document.documentElement.scrollTop;
        let w = win.innerWidth;
        let h = win.innerHeight;
        // Scale according to devicePixelRatio and coordinates
        canvas.width = dpr * w;
        canvas.height = dpr * h;
        ctx.scale(dpr, dpr);
        ctx.drawWindow(win, x, y, w, h, '#ffffff');
        let screenshot = canvas.toDataURL();
        resolve({tab: tab, data: screenshot});
      } catch (e) {
        // drawWindow can fail depending on memory or surface size. Rather than reject here,
        // we resolve the URL so the user can continue to file an issue without a screenshot.
        Cu.reportError("WebCompatReporter: getting a screenshot failed: " + e);
        resolve({tab: tab});
      }
    });
  },

  isReportableUrl: function(url) {
    return url && !(url.startsWith("about") ||
                    url.startsWith("chrome") ||
                    url.startsWith("file") ||
                    url.startsWith("resource"));
  },

  reportDesktopModePrompt: function(tab) {
    let message = this.strings.GetStringFromName("webcompat.reportDesktopMode.message");
    let options = {
      action: {
        label: this.strings.GetStringFromName("webcompat.reportDesktopModeYes.label"),
        callback: () => this.reportIssue({tab: tab})
      }
    };
    Snackbars.show(message, Snackbars.LENGTH_LONG, options);
  },

  reportIssue: (tabData) => {
    return new Promise((resolve) => {
      const WEBCOMPAT_ORIGIN = "https://webcompat.com";
      let url = tabData.tab.browser.currentURI.spec
      let webcompatURL = `${WEBCOMPAT_ORIGIN}/issues/new?url=${url}&src=mobile-reporter`;

      if (tabData.data && typeof tabData.data === "string") {
        BrowserApp.deck.addEventListener("DOMContentLoaded", function(event) {
          if (event.target.defaultView.location.origin === WEBCOMPAT_ORIGIN) {
            // Waive Xray vision so event.origin is not chrome://browser on the other side.
            let win = Cu.waiveXrays(event.target.defaultView);
            win.postMessage(tabData.data, WEBCOMPAT_ORIGIN);
          }
        }, {once: true});
      }

      let isPrivateTab = PrivateBrowsingUtils.isBrowserPrivate(tabData.tab.browser);
      BrowserApp.addTab(webcompatURL, {parentId: tabData.tab.id, isPrivate: isPrivateTab});
      resolve();
    });
  }
};

XPCOMUtils.defineLazyGetter(WebcompatReporter, "strings", function() {
  return Services.strings.createBundle("chrome://browser/locale/webcompatReporter.properties");
});
