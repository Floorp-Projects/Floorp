/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=4 ts=4 sts=4 et
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { BackgroundUpdate } = ChromeUtils.import(
  "resource://gre/modules/BackgroundUpdate.jsm"
);

const { maybeSubmitBackgroundUpdatePing } = ChromeUtils.import(
  "resource://gre/modules/backgroundtasks/BackgroundTask_backgroundupdate.jsm"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "UpdateService",
  "@mozilla.org/updates/update-service;1",
  "nsIApplicationUpdateService"
);

add_task(function test_setup() {
  // FOG needs a profile directory to put its data in.
  do_get_profile();

  // We need to initialize it once, otherwise operations will be stuck in the pre-init queue.
  Services.fog.initializeFOG();
});

add_task(async function test_record_update_environment() {
  await BackgroundUpdate.recordUpdateEnvironment();

  let pingSubmitted = false;
  let appUpdateAutoEnabled = await UpdateUtils.getAppUpdateAutoEnabled();
  let backgroundUpdateEnabled = await UpdateUtils.readUpdateConfigSetting(
    "app.update.background.enabled"
  );
  GleanPings.backgroundUpdate.testBeforeNextSubmit(reason => {
    pingSubmitted = true;
    Assert.equal(
      Services.prefs.getBoolPref("app.update.service.enabled", false),
      Glean.update.serviceEnabled.testGetValue()
    );

    Assert.equal(
      appUpdateAutoEnabled,
      Glean.update.autoDownload.testGetValue()
    );

    Assert.equal(
      backgroundUpdateEnabled,
      Glean.update.backgroundUpdate.testGetValue()
    );

    Assert.equal(
      UpdateUtils.UpdateChannel,
      Glean.update.channel.testGetValue()
    );
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

  // There's nothing async in this function atm, but it's annotated async, so..
  await maybeSubmitBackgroundUpdatePing();

  ok(pingSubmitted, "'background-update' ping was submitted");
});
