/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=4 ts=4 sts=4 et
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { BackgroundUpdate } = ChromeUtils.import(
  "resource://gre/modules/BackgroundUpdate.jsm"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "UpdateService",
  "@mozilla.org/updates/update-service;1",
  "nsIApplicationUpdateService"
);

Cu.importGlobalProperties(["Glean"]);

add_task(function test_setup() {
  // FOG needs a profile directory to put its data in.
  do_get_profile();

  // We need to initialize it once, otherwise operations will be stuck in the pre-init queue.
  let FOG = Cc["@mozilla.org/toolkit/glean;1"].createInstance(Ci.nsIFOG);
  FOG.initializeFOG();
});

add_task(async function test_record_update_environment() {
  await BackgroundUpdate.recordUpdateEnvironment();

  Assert.equal(
    Services.prefs.getBoolPref("app.update.service.enabled", false),
    Glean.update.serviceEnabled.testGetValue()
  );

  Assert.equal(
    await UpdateUtils.getAppUpdateAutoEnabled(),
    Glean.update.autoDownload.testGetValue()
  );

  Assert.equal(
    await UpdateUtils.readUpdateConfigSetting("app.update.background.enabled"),
    Glean.update.backgroundUpdate.testGetValue()
  );

  Assert.equal(UpdateUtils.UpdateChannel, Glean.update.channel.testGetValue());
  Assert.equal(
    !Services.policies || Services.policies.isAllowed("appUpdate"),
    Glean.update.enabled.testGetValue()
  );

  Assert.equal(
    UpdateService.canUsuallyApplyUpdates,
    Glean.update.canUsuallyApplyUpdates.testGetValue()
  );
  Assert.equal(
    UpdateService.canUsuallyCheckForUpdates,
    Glean.update.canUsuallyCheckForUpdates.testGetValue()
  );
  Assert.equal(
    UpdateService.canUsuallyStageUpdates,
    Glean.update.canUsuallyStageUpdates.testGetValue()
  );
  Assert.equal(
    UpdateService.canUsuallyUseBits,
    Glean.update.canUsuallyUseBits.testGetValue()
  );
});
