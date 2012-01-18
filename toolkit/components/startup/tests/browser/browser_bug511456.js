/* ***** BEGIN LICENSE BLOCK *****
 *   Version: MPL 1.1/GPL 2.0/LGPL 2.1
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dave Townsend <dtownsend@oxymoronical.com> (Original Author)
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
