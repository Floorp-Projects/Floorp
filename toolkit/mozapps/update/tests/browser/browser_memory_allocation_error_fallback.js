/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * When the updater fails with a memory allocation error, we should fall back to
 * updating without staging.
 */

const READ_STRINGS_MEM_ERROR = 10;
const ARCHIVE_READER_MEM_ERROR = 11;
const BSPATCH_MEM_ERROR = 12;
const UPDATER_MEM_ERROR = 13;
const UPDATER_QUOTED_PATH_MEM_ERROR = 14;

const EXPECTED_STATUS =
  AppConstants.platform == "win" ? STATE_PENDING_SVC : STATE_PENDING;

add_setup(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [PREF_APP_UPDATE_STAGING_ENABLED, true],
      [PREF_APP_UPDATE_SERVICE_ENABLED, true],
    ],
  });

  registerCleanupFunction(() => {
    Services.env.set("MOZ_FORCE_ERROR_CODE", "");
  });
});

async function memAllocErrorFallback(errorCode) {
  Services.env.set("MOZ_FORCE_ERROR_CODE", errorCode.toString());

  // Since the partial should be successful specify an invalid size for the
  // complete update.
  let params = {
    queryString: "&invalidCompleteSize=1",
    backgroundUpdate: true,
    continueFile: CONTINUE_STAGING,
    waitForUpdateState: EXPECTED_STATUS,
  };
  await runAboutPrefsUpdateTest(params, [
    {
      panelId: "apply",
      checkActiveUpdate: { state: EXPECTED_STATUS },
      continueFile: null,
    },
  ]);
}

function cleanup() {
  reloadUpdateManagerData(true);
  removeUpdateFiles(true);
}

add_task(async function memAllocErrorFallback_READ_STRINGS_MEM_ERROR() {
  await memAllocErrorFallback(READ_STRINGS_MEM_ERROR);
  cleanup();
});

add_task(async function memAllocErrorFallback_ARCHIVE_READER_MEM_ERROR() {
  await memAllocErrorFallback(ARCHIVE_READER_MEM_ERROR);
  cleanup();
});

add_task(async function memAllocErrorFallback_BSPATCH_MEM_ERROR() {
  await memAllocErrorFallback(BSPATCH_MEM_ERROR);
  cleanup();
});

add_task(async function memAllocErrorFallback_UPDATER_MEM_ERROR() {
  await memAllocErrorFallback(UPDATER_MEM_ERROR);
  cleanup();
});

add_task(async function memAllocErrorFallback_UPDATER_QUOTED_PATH_MEM_ERROR() {
  await memAllocErrorFallback(UPDATER_QUOTED_PATH_MEM_ERROR);
  cleanup();
});
