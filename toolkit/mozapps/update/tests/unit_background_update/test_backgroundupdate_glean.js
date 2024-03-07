/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=4 ts=4 sts=4 et
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { ASRouterTargeting } = ChromeUtils.importESModule(
  "resource:///modules/asrouter/ASRouterTargeting.sys.mjs"
);
const { BackgroundUpdate } = ChromeUtils.importESModule(
  "resource://gre/modules/BackgroundUpdate.sys.mjs"
);
const { ExperimentFakes } = ChromeUtils.importESModule(
  "resource://testing-common/NimbusTestUtils.sys.mjs"
);

const { maybeSubmitBackgroundUpdatePing } = ChromeUtils.importESModule(
  "resource://gre/modules/backgroundtasks/BackgroundTask_backgroundupdate.sys.mjs"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "UpdateService",
  "@mozilla.org/updates/update-service;1",
  "nsIApplicationUpdateService"
);

add_setup(function test_setup() {
  // FOG needs a profile directory to put its data in.
  do_get_profile();

  // We need to initialize it once, otherwise operations will be stuck in the pre-init queue.
  Services.fog.initializeFOG();

  setupProfileService();
});

add_task(async function test_record_update_environment() {
  await BackgroundUpdate.recordUpdateEnvironment();

  let pingSubmitted = false;
  let appUpdateAutoEnabled = await UpdateUtils.getAppUpdateAutoEnabled();
  let backgroundUpdateEnabled = await UpdateUtils.readUpdateConfigSetting(
    "app.update.background.enabled"
  );
  GleanPings.backgroundUpdate.testBeforeNextSubmit(reason => {
    Assert.equal(reason, "backgroundupdate_task");

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

async function do_readTargeting(content, beforeNextSubmitCallback) {
  let profileService = Cc["@mozilla.org/toolkit/profile-service;1"].getService(
    Ci.nsIToolkitProfileService
  );

  let file = do_get_profile();
  file.append("profile_cannot_be_locked");

  let profile = profileService.createUniqueProfile(
    file,
    "test_default_profile"
  );

  let targetingSnapshot = profile.rootDir.clone();
  targetingSnapshot.append("targeting.snapshot.json");

  if (content) {
    await IOUtils.writeUTF8(targetingSnapshot.path, content);
  }

  let lock = profile.lock({});

  Services.fog.testResetFOG();
  try {
    await BackgroundUpdate.readFirefoxMessagingSystemTargetingSnapshot(lock);
  } finally {
    lock.unlock();
  }

  let pingSubmitted = false;
  GleanPings.backgroundUpdate.testBeforeNextSubmit(reason => {
    pingSubmitted = true;
    return beforeNextSubmitCallback(reason);
  });

  // There's nothing async in this function atm, but it's annotated async, so..
  await maybeSubmitBackgroundUpdatePing();

  ok(pingSubmitted, "'background-update' ping was submitted");
}

// Missing targeting is anticipated.
add_task(async function test_targeting_missing() {
  await do_readTargeting(null, _reason => {
    Assert.equal(false, Glean.backgroundUpdate.targetingExists.testGetValue());

    Assert.equal(
      false,
      Glean.backgroundUpdate.targetingException.testGetValue()
    );
  });
});

// Malformed JSON yields an exception.
add_task(async function test_targeting_exception() {
  await do_readTargeting("{", _reason => {
    Assert.equal(false, Glean.backgroundUpdate.targetingExists.testGetValue());

    Assert.equal(
      true,
      Glean.backgroundUpdate.targetingException.testGetValue()
    );
  });
});

// Well formed targeting values are reflected into the Glean telemetry.
add_task(async function test_targeting_exists() {
  // We can't take a full environment snapshot under `xpcshell`; these are just
  // the items we need.
  let target = {
    currentDate: ASRouterTargeting.Environment.currentDate,
    profileAgeCreated: ASRouterTargeting.Environment.profileAgeCreated,
    firefoxVersion: ASRouterTargeting.Environment.firefoxVersion,
  };

  // Arrange fake experiment enrollment details.
  const manager = ExperimentFakes.manager();

  await manager.onStartup();
  await manager.store.addEnrollment(ExperimentFakes.experiment("foo"));
  manager.unenroll("foo", "some-reason");
  await manager.store.addEnrollment(
    ExperimentFakes.experiment("bar", { active: false })
  );
  await manager.store.addEnrollment(
    ExperimentFakes.experiment("baz", { active: true })
  );

  manager.store.addEnrollment(ExperimentFakes.rollout("rol1"));
  manager.unenroll("rol1", "some-reason");
  manager.store.addEnrollment(ExperimentFakes.rollout("rol2"));

  let targetSnapshot = await ASRouterTargeting.getEnvironmentSnapshot({
    targets: [manager.createTargetingContext(), target],
  });

  await do_readTargeting(JSON.stringify(targetSnapshot), _reason => {
    Assert.equal(true, Glean.backgroundUpdate.targetingExists.testGetValue());

    Assert.equal(
      false,
      Glean.backgroundUpdate.targetingException.testGetValue()
    );

    // `environment.firefoxVersion` is a positive integer.
    Assert.ok(
      Glean.backgroundUpdate.targetingEnvFirefoxVersion.testGetValue() > 0
    );

    Assert.equal(
      targetSnapshot.environment.firefoxVersion,
      Glean.backgroundUpdate.targetingEnvFirefoxVersion.testGetValue()
    );

    let profileAge =
      Glean.backgroundUpdate.targetingEnvProfileAge.testGetValue();

    Assert.ok(profileAge instanceof Date);
    Assert.ok(0 < profileAge.getTime());
    Assert.ok(profileAge.getTime() < Date.now());

    // `environment.profileAgeCreated` is an integer, milliseconds since the
    // Unix epoch.
    let targetProfileAge = new Date(
      targetSnapshot.environment.profileAgeCreated
    );
    // Our `time_unit: day` has Glean round to the nearest day *in the local
    // timezone*, so we must do the same.
    targetProfileAge.setHours(0, 0, 0, 0);

    Assert.equal(targetProfileAge.toISOString(), profileAge.toISOString());

    let currentDate =
      Glean.backgroundUpdate.targetingEnvCurrentDate.testGetValue();

    Assert.ok(0 < currentDate.getTime());
    Assert.ok(currentDate.getTime() < Date.now());

    // `environment.currentDate` is in ISO string format.
    let targetCurrentDate = new Date(targetSnapshot.environment.currentDate);
    // Our `time_unit: day` has Glean round to the nearest day *in the local
    // timezone*, so we must do the same.
    targetCurrentDate.setHours(0, 0, 0, 0);

    Assert.equal(targetCurrentDate.toISOString(), currentDate.toISOString());

    // Verify active experiments.
    Assert.deepEqual(
      {
        branch: "treatment",
        extra: { source: "defaultProfile", type: "nimbus-nimbus" },
      },
      Services.fog.testGetExperimentData("baz"),
      "experiment data for active experiment 'baz' is correct"
    );

    Assert.deepEqual(
      {
        branch: "treatment",
        extra: { source: "defaultProfile", type: "nimbus-rollout" },
      },
      Services.fog.testGetExperimentData("rol2"),
      "experiment data for active experiment 'rol2' is correct"
    );

    // Bug 1879247: there is currently no API (even test-only) to get experiment
    // data for inactive experiments.
    for (let inactive of ["bar", "foo", "rol1"]) {
      Assert.equal(
        null,
        Services.fog.testGetExperimentData(inactive),
        `no experiment data for inactive experiment '${inactive}`
      );
    }
  });
});
