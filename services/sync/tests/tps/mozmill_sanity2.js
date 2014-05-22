/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var setupModule = function(module) {
  module.controller = mozmill.getBrowserController();
};

var testGetNode = function() {
  controller.open("about:support");
  controller.waitForPageLoad();

  var appbox = findElement.ID(controller.tabs.activeTab, "application-box");
  assert.waitFor(() => appbox.getNode().textContent == 'Firefox', 'correct app name');
};
