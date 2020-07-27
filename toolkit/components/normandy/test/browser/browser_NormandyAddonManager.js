"use strict";

ChromeUtils.import("resource://gre/modules/PromiseUtils.jsm", this);
ChromeUtils.import("resource://normandy/lib/NormandyAddonManager.jsm", this);

decorate_task(ensureAddonCleanup, async function download_and_install() {
  const applyDeferred = PromiseUtils.defer();

  const [addonId, addonVersion] = await NormandyAddonManager.downloadAndInstall(
    {
      extensionDetails: {
        extension_id: FIXTURE_ADDON_ID,
        hash: FIXTURE_ADDON_DETAILS["normandydriver-a-1.0"].hash,
        hash_algorithm: "sha256",
        version: "1.0",
        xpi: FIXTURE_ADDON_DETAILS["normandydriver-a-1.0"].url,
      },
      applyNormandyChanges: () => {
        applyDeferred.resolve();
      },
      createError: () => {},
      reportError: () => {},
      undoNormandyChanges: () => {},
    }
  );

  // Ensure applyNormandyChanges was called
  await applyDeferred;

  const addon = await AddonManager.getAddonByID(FIXTURE_ADDON_ID);
  is(addon.id, addonId, "add-on is installed");
  is(addon.version, addonVersion, "add-on version is correct");

  // Cleanup
  await addon.uninstall();
});

decorate_task(ensureAddonCleanup, async function id_mismatch() {
  const applyDeferred = PromiseUtils.defer();
  const undoDeferred = PromiseUtils.defer();

  let error;

  try {
    await NormandyAddonManager.downloadAndInstall({
      extensionDetails: {
        extension_id: FIXTURE_ADDON_ID,
        hash: FIXTURE_ADDON_DETAILS["normandydriver-b-1.0"].hash,
        hash_algorithm: "sha256",
        version: "1.0",
        xpi: FIXTURE_ADDON_DETAILS["normandydriver-b-1.0"].url,
      },
      applyNormandyChanges: () => {
        applyDeferred.resolve();
      },
      createError: (reason, extra) => {
        return [reason, extra];
      },
      reportError: err => {
        return err;
      },
      undoNormandyChanges: () => {
        undoDeferred.resolve();
      },
    });
  } catch ([reason, extra]) {
    error = true;
    is(reason, "metadata-mismatch", "the expected reason is provided");
    Assert.deepEqual(
      extra,
      undefined,
      "the expected extra details are provided"
    );
  }

  ok(error, "an error occured");

  // Ensure applyNormandyChanges was called
  await applyDeferred;

  // Ensure undoNormandyChanges was called
  await undoDeferred;

  const addon = await AddonManager.getAddonByID(FIXTURE_ADDON_ID);
  ok(!addon, "add-on is not installed");
});

decorate_task(ensureAddonCleanup, async function version_mismatch() {
  const applyDeferred = PromiseUtils.defer();
  const undoDeferred = PromiseUtils.defer();

  let error;

  try {
    await NormandyAddonManager.downloadAndInstall({
      extensionDetails: {
        extension_id: FIXTURE_ADDON_ID,
        hash: FIXTURE_ADDON_DETAILS["normandydriver-a-1.0"].hash,
        hash_algorithm: "sha256",
        version: "2.0",
        xpi: FIXTURE_ADDON_DETAILS["normandydriver-a-1.0"].url,
      },
      applyNormandyChanges: () => {
        applyDeferred.resolve();
      },
      createError: (reason, extra) => {
        return [reason, extra];
      },
      reportError: err => {
        return err;
      },
      undoNormandyChanges: () => {
        undoDeferred.resolve();
      },
    });
  } catch ([reason, extra]) {
    error = true;
    is(reason, "metadata-mismatch", "the expected reason is provided");
    Assert.deepEqual(
      extra,
      undefined,
      "the expected extra details are provided"
    );
  }

  ok(error, "should throw an error");

  // Ensure applyNormandyChanges was called
  await applyDeferred;

  // Ensure undoNormandyChanges was called
  await undoDeferred;

  const addon = await AddonManager.getAddonByID(FIXTURE_ADDON_ID);
  ok(!addon, "add-on is not installed");
});

decorate_task(ensureAddonCleanup, async function download_failure() {
  const applyDeferred = PromiseUtils.defer();
  const undoDeferred = PromiseUtils.defer();

  let error;

  try {
    await NormandyAddonManager.downloadAndInstall({
      extensionDetails: {
        extension_id: FIXTURE_ADDON_ID,
        hash: FIXTURE_ADDON_DETAILS["normandydriver-b-1.0"].hash,
        hash_algorithm: "sha256",
        version: "1.0",
        xpi: FIXTURE_ADDON_DETAILS["normandydriver-a-1.0"].url,
      },
      applyNormandyChanges: () => {
        applyDeferred.resolve();
      },
      createError: (reason, extra) => {
        return [reason, extra];
      },
      reportError: err => {
        return err;
      },
      undoNormandyChanges: () => {
        undoDeferred.resolve();
      },
    });
  } catch ([reason, extra]) {
    error = true;
    is(reason, "download-failure", "the expected reason is provided");
    Assert.deepEqual(
      extra,
      {
        detail: "ERROR_INCORRECT_HASH",
      },
      "the expected extra details are provided"
    );
  }

  ok(error, "an error occured");

  // Ensure applyNormandyChanges was called
  await applyDeferred;

  // Ensure undoNormandyChanges was called
  await undoDeferred;

  const addon = await AddonManager.getAddonByID(FIXTURE_ADDON_ID);
  ok(!addon, "add-on is not installed");
});
