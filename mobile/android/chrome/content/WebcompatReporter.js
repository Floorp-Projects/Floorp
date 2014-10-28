/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/PrivateBrowsingUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

var WebcompatReporter = {
  menuItem: null,
  menuItemEnabled: null,
  init: function() {
    Services.obs.addObserver(this, "DesktopMode:Change", false);
    Services.obs.addObserver(this, "content-page-shown", false);
    this.addMenuItem();
  },

  uninit: function() {
    Services.obs.removeObserver(this, "DesktopMode:Change");

    if (this.menuItem) {
      NativeWindow.menu.remove(this.menuItem);
      this.menuItem = null;
    }
  },

  observe: function(subject, topic, data) {
    if (topic === "content-page-shown") {
      let currentURI = subject.documentURI;
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
      if (args.desktopMode && tab !== null) {
        this.reportDesktopModePrompt();
      }
    }
  },

  addMenuItem: function() {
    this.menuItem = NativeWindow.menu.add({
      name: this.strings.GetStringFromName("webcompat.menu.name"),
      callback: () => {
        let currentURI = BrowserApp.selectedTab.browser.currentURI.spec;
        this.reportIssue(currentURI);
      },
      enabled: false,
    });
  },

  isReportableUrl: function(url) {
    return url !== null && !(url.startsWith("about") ||
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
    let webcompatURL = new URL("http://webcompat.com/");
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
