/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import('resource://tps/mozmill/sync.jsm');

var setupModule = function(module) {
  controller = mozmill.getBrowserController();
  assert.ok(true, "SetupModule passes");
}

var setupTest = function(module) {
  assert.ok(true, "SetupTest passes");
}

var testTestStep = function() {
  assert.ok(true, "test Passes");
  controller.open("http://www.mozilla.org");
  TPS.SetupSyncAccount();
  assert.equal(TPS.Sync(SYNC_WIPE_SERVER), 0, "sync succeeded");
}

var teardownTest = function () {
  assert.ok(true, "teardownTest passes");
}

var teardownModule = function() {
  assert.ok(true, "teardownModule passes");
}
