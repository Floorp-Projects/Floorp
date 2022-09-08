/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=4 ts=4 sts=4 et
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { BackgroundUpdate } = ChromeUtils.import(
  "resource://gre/modules/BackgroundUpdate.jsm"
);
let reasons = () => BackgroundUpdate._reasonsToNotUpdateInstallation();
let REASON = BackgroundUpdate.REASON;
const { EnterprisePolicyTesting } = ChromeUtils.importESModule(
  "resource://testing-common/EnterprisePolicyTesting.sys.mjs"
);
const { UpdateService } = ChromeUtils.import(
  "resource://gre/modules/UpdateService.jsm"
);

const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");

// We can't reasonably check NO_MOZ_BACKGROUNDTASKS, nor NO_OMNIJAR.

// These tests use per-installation prefs, and those are a shared resource, so
// they require some non-trivial setup.
setupTestCommon(null);
standardInit();

function setup_enterprise_policy_testing() {
  // This initializes the policy engine for xpcshell tests
  let policies = Cc["@mozilla.org/enterprisepolicies;1"].getService(
    Ci.nsIObserver
  );
  policies.observe(null, "policies-startup", null);
}
setup_enterprise_policy_testing();

async function setupPolicyEngineWithJson(json, customSchema) {
  if (typeof json != "object") {
    let filePath = do_get_file(json ? json : "non-existing-file.json").path;
    return EnterprisePolicyTesting.setupPolicyEngineWithJson(
      filePath,
      customSchema
    );
  }
  return EnterprisePolicyTesting.setupPolicyEngineWithJson(json, customSchema);
}

add_task(async function test_reasons_update_no_app_update_auto() {
  let prev = await UpdateUtils.getAppUpdateAutoEnabled();
  try {
    await UpdateUtils.setAppUpdateAutoEnabled(false);
    let result = await reasons();
    Assert.ok(result.includes(REASON.NO_APP_UPDATE_AUTO));

    await UpdateUtils.setAppUpdateAutoEnabled(true);
    result = await reasons();
    Assert.ok(!result.includes(REASON.NO_APP_UPDATE_AUTO));
  } finally {
    await UpdateUtils.setAppUpdateAutoEnabled(prev);
  }
});

add_task(async function test_reasons_update_no_app_update_background_enabled() {
  let prev = await UpdateUtils.readUpdateConfigSetting(
    "app.update.background.enabled"
  );
  try {
    await UpdateUtils.writeUpdateConfigSetting(
      "app.update.background.enabled",
      false
    );
    let result = await reasons();
    Assert.ok(result.includes(REASON.NO_APP_UPDATE_BACKGROUND_ENABLED));

    await UpdateUtils.writeUpdateConfigSetting(
      "app.update.background.enabled",
      true
    );
    result = await reasons();
    Assert.ok(!result.includes(REASON.NO_APP_UPDATE_BACKGROUND_ENABLED));
  } finally {
    await UpdateUtils.writeUpdateConfigSetting(
      "app.update.background.enabled",
      prev
    );
  }
});

add_task(async function test_reasons_update_cannot_usually_check() {
  // It's difficult to arrange the conditions in a testing environment, so
  // we'll use mocks to get a little assurance.
  let result = await reasons();
  Assert.ok(!result.includes(REASON.CANNOT_USUALLY_CHECK));

  let sandbox = sinon.createSandbox();
  try {
    sandbox
      .stub(UpdateService.prototype, "canUsuallyCheckForUpdates")
      .get(() => false);
    result = await reasons();
    Assert.ok(result.includes(REASON.CANNOT_USUALLY_CHECK));
  } finally {
    sandbox.restore();
  }
});

add_task(async function test_reasons_update_can_usually_stage_or_appl() {
  // It's difficult to arrange the conditions in a testing environment, so
  // we'll use mocks to get a little assurance.
  let sandbox = sinon.createSandbox();
  try {
    sandbox
      .stub(UpdateService.prototype, "canUsuallyStageUpdates")
      .get(() => true);
    sandbox
      .stub(UpdateService.prototype, "canUsuallyApplyUpdates")
      .get(() => true);
    let result = await reasons();
    Assert.ok(
      !result.includes(REASON.CANNOT_USUALLY_STAGE_AND_CANNOT_USUALLY_APPLY)
    );

    sandbox
      .stub(UpdateService.prototype, "canUsuallyStageUpdates")
      .get(() => false);
    sandbox
      .stub(UpdateService.prototype, "canUsuallyApplyUpdates")
      .get(() => false);
    result = await reasons();
    Assert.ok(
      result.includes(REASON.CANNOT_USUALLY_STAGE_AND_CANNOT_USUALLY_APPLY)
    );
  } finally {
    sandbox.restore();
  }
});

add_task(
  {
    skip_if: () =>
      !AppConstants.MOZ_BITS_DOWNLOAD || AppConstants.platform != "win",
  },
  async function test_reasons_update_can_usually_use_bits() {
    let prev = Services.prefs.getBoolPref("app.update.BITS.enabled");

    // Here we use mocks to "get by" preconditions that are not
    // satisfied in the testing environment.
    let sandbox = sinon.createSandbox();
    try {
      sandbox
        .stub(UpdateService.prototype, "canUsuallyStageUpdates")
        .get(() => true);
      sandbox
        .stub(UpdateService.prototype, "canUsuallyApplyUpdates")
        .get(() => true);

      Services.prefs.setBoolPref("app.update.BITS.enabled", false);
      let result = await reasons();
      Assert.ok(result.includes(REASON.WINDOWS_CANNOT_USUALLY_USE_BITS));

      Services.prefs.setBoolPref("app.update.BITS.enabled", true);
      result = await reasons();
      Assert.ok(!result.includes(REASON.WINDOWS_CANNOT_USUALLY_USE_BITS));
    } finally {
      sandbox.restore();
      Services.prefs.setBoolPref("app.update.BITS.enabled", prev);
    }
  }
);

add_task(async function test_reasons_update_manual_update_only() {
  await setupPolicyEngineWithJson({
    policies: {
      ManualAppUpdateOnly: true,
    },
  });
  Assert.equal(
    Services.policies.status,
    Ci.nsIEnterprisePolicies.ACTIVE,
    "Engine is active"
  );

  let result = await reasons();
  Assert.ok(result.includes(REASON.MANUAL_UPDATE_ONLY));

  await setupPolicyEngineWithJson({});

  result = await reasons();
  Assert.ok(!result.includes(REASON.MANUAL_UPDATE_ONLY));
});

add_task(() => {
  // `setupTestCommon()` calls `do_test_pending()`; this calls
  // `do_test_finish()`.  The `add_task` schedules this to run after all the
  // other tests have completed.
  doTestFinish();
});
