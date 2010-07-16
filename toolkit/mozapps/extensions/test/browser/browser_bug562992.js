/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
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
 * The Original Code is the Extension Manager UI.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Drew Willcoxon <adw@mozilla.com> (Original Author)
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

  open_manager("addons://list/extension", function (aWindow) {
    gManagerWindow = aWindow;
    run_next_test();
  });
}

function end_test() {
  close_manager(gManagerWindow, function () {
    finish();
  });
}

// Create a MockInstall with a MockAddon payload and add it to the provider,
// causing the onNewInstall event to fire, which in turn will cause a new
// "installing" item to appear in the list of extensions.
add_test(function () {
  let addon = new MockAddon(undefined, EXTENSION_NAME, "extension", true);
  gInstall = new MockInstall(undefined, undefined, addon);
  gInstall.addTestListener({
    onNewInstall: function () {
      run_next_test();
    }
  });
  gProvider.addInstall(gInstall);
});

// Finish the install, which will cause the "installing" item to be converted
// to an "installed" item, which should have the correct add-on name.
add_test(function () {
  gInstall.addTestListener({
    onInstallEnded: function () {
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
