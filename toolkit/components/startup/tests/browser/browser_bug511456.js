/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const PROMPT_URL = "chrome://global/content/commonDialog.xul";
const TEST_URL = "http://example.com/browser/toolkit/components/startup/tests/browser/beforeunload.html";

var Watcher = {
  seen: false,
  allowClose: false,

  // Window open handling
  windowLoad: function(win) {
    // Allow any other load handlers to execute
    var self = this;
    executeSoon(function() { self.windowReady(win); } );
  },

  windowReady: function(win) {
    if (win.document.location.href != PROMPT_URL)
      return;
    this.seen = true;
    if (this.allowClose)
      win.document.documentElement.acceptDialog();
    else
      win.document.documentElement.cancelDialog();
  },

  // nsIWindowMediatorListener

  onWindowTitleChange: function(win, title) {
  },

  onOpenWindow: function(win) {
    var domwindow = win.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                       .getInterface(Components.interfaces.nsIDOMWindow);
    var self = this;
    domwindow.addEventListener("load", function() {
      domwindow.removeEventListener("load", arguments.callee, false);
      self.windowLoad(domwindow);
    }, false);
  },

  onCloseWindow: function(win) {
  },

  QueryInterface: function(iid) {
    if (iid.equals(Components.interfaces.nsIWindowMediatorListener) ||
        iid.equals(Components.interfaces.nsISupports))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  }
}

function test() {
  waitForExplicitFinish();
  ignoreAllUncaughtExceptions();

  Services.wm.addListener(Watcher);

  var win2 = window.openDialog(location, "", "chrome,all,dialog=no", "about:blank");
  win2.addEventListener("load", function() {
    win2.removeEventListener("load", arguments.callee, false);

    gBrowser.selectedTab = gBrowser.addTab(TEST_URL);
    gBrowser.selectedBrowser.addEventListener("DOMContentLoaded", function() {
      if (window.content.location.href != TEST_URL)
        return;
      gBrowser.selectedBrowser.removeEventListener("DOMContentLoaded", arguments.callee, false);
      Watcher.seen = false;
      var appStartup = Cc['@mozilla.org/toolkit/app-startup;1'].
                       getService(Ci.nsIAppStartup);
      appStartup.quit(Ci.nsIAppStartup.eAttemptQuit);
      Watcher.allowClose = true;
      ok(Watcher.seen, "Should have seen a prompt dialog");
      ok(!win2.closed, "Shouldn't have closed the additional window");
      win2.close();
      gBrowser.removeTab(gBrowser.selectedTab);
      executeSoon(finish_test);
    }, false);
  }, false);
}

function finish_test() {
  Services.wm.removeListener(Watcher);
  finish();
}
