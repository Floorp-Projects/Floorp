/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils",
                                  "resource://gre/modules/PrivateBrowsingUtils.jsm");

var WebcompatReporter = {
  menuItem: null,
  menuItemEnabled: null,
  init: function() {
    Services.obs.addObserver(this, "DesktopMode:Change", false);
    Services.obs.addObserver(this, "chrome-document-global-created", false);
    Services.obs.addObserver(this, "content-document-global-created", false);

    let visible = true;
    if ("@mozilla.org/parental-controls-service;1" in Cc) {
      let pc = Cc["@mozilla.org/parental-controls-service;1"].createInstance(Ci.nsIParentalControlsService);
      visible = !pc.parentalControlsEnabled;
    }

    this.addMenuItem(visible);
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
    } else if (topic === "DesktopMode:Change") {
      let args = JSON.parse(data);
      let tab = BrowserApp.getTabForId(args.tabId);
      let currentURI = tab.browser.currentURI.spec;
      if (args.desktopMode && this.isReportableUrl(currentURI)) {
        this.reportDesktopModePrompt();
      }
    }
  },

  addMenuItem: function(visible) {
    this.menuItem = NativeWindow.menu.add({
      name: this.strings.GetStringFromName("webcompat.menu.name"),
      callback: () => {
        let currentURI = BrowserApp.selectedTab.browser.currentURI.spec;
        this.reportIssue(currentURI);
      },
      enabled: false,
      visible: visible,
    });
  },

  isReportableUrl: function(url) {
    return url && !(url.startsWith("about") ||
                    url.startsWith("chrome") ||
                    url.startsWith("file") ||
                    url.startsWith("resource"));
  },

  reportDesktopModePrompt: function() {
    let currentURI = BrowserApp.selectedTab.browser.currentURI.spec;
    let message = this.strings.GetStringFromName("webcompat.reportDesktopMode.message");
    let options = {
      button: {
        label: this.strings.GetStringFromName("webcompat.reportDesktopModeYes.label"),
        callback: () => this.reportIssue(currentURI)
      }
    };
    NativeWindow.toast.show(message, "long", options);
  },

  reportIssue: function(url) {
    let webcompatURL = new URL("https://webcompat.com/");
    webcompatURL.searchParams.append("open", "1");
    webcompatURL.searchParams.append("url", url);
    if (PrivateBrowsingUtils.isBrowserPrivate(BrowserApp.selectedTab.browser)) {
      BrowserApp.addTab(webcompatURL.href, {parentId: BrowserApp.selectedTab.id, isPrivate: true});
    } else {
      BrowserApp.addTab(webcompatURL.href);
    }
  }
};

XPCOMUtils.defineLazyGetter(WebcompatReporter, "strings", function() {
  return Services.strings.createBundle("chrome://browser/locale/webcompatReporter.properties");
});
