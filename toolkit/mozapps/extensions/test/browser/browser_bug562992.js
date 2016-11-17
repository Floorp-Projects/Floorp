/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This test ensures that when the extension manager UI is open and a
 * restartless extension is installed from the web, its correct name appears
 * when the download and installation complete.  See bug 562992.
 */

var gManagerWindow;
var gProvider;
var gInstall;

const EXTENSION_NAME = "Wunderbar";

function test() {
  waitForExplicitFinish();

  gProvider = new MockProvider();

  open_manager("addons://list/extension", function(aWindow) {
    gManagerWindow = aWindow;
    run_next_test();
  });
}

function end_test() {
  close_manager(gManagerWindow, function() {
    finish();
  });
}

// Create a MockInstall with a MockAddon payload and add it to the provider,
// causing the onNewInstall event to fire, which in turn will cause a new
// "installing" item to appear in the list of extensions.
add_test(function() {
  let addon = new MockAddon(undefined, EXTENSION_NAME, "extension", true);
  gInstall = new MockInstall(undefined, undefined, addon);
  gInstall.addTestListener({
    onNewInstall: run_next_test
  });
  gProvider.addInstall(gInstall);
});

// Finish the install, which will cause the "installing" item to be converted
// to an "installed" item, which should have the correct add-on name.
add_test(function() {
  gInstall.addTestListener({
    onInstallEnded: function() {
      let list = gManagerWindow.document.getElementById("addon-list");

      // To help prevent future breakage, don't assume the item is the only one
      // in the list, or that it's first in the list.  Find it by name.
      for (let i = 0; i < list.itemCount; i++) {
        let item = list.getItemAtIndex(i);
        if (item.getAttribute("name") === EXTENSION_NAME) {
          ok(true, "Item with correct name found");
          run_next_test();
          return;
        }
      }
      ok(false, "Item with correct name was not found");
      run_next_test();
    }
  });
  gInstall.install();
});
