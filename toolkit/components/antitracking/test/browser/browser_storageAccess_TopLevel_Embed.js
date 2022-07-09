Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/components/antitracking/test/browser/storage_access_head.js",
  this
);
/* import-globals-from storage_access_head.js */

async function requestStorageAccessUnderSiteAndExpectSuccess() {
  SpecialPowers.wrap(document).notifyUserGestureActivation();
  var p = document.requestStorageAccessUnderSite("http://example.org");
  try {
    await p;
    ok(true, "Must resolve.");
  } catch {
    ok(false, "Must not reject.");
  }
}

async function requestStorageAccessUnderSiteAndExpectFailure() {
  SpecialPowers.wrap(document).notifyUserGestureActivation();
  var p = document.requestStorageAccessUnderSite("http://example.org");
  try {
    await p;
    ok(false, "Must not resolve.");
  } catch {
    ok(true, "Must reject.");
  }
}

async function completeStorageAccessRequestFromSiteAndExpectSuccess() {
  SpecialPowers.wrap(document).notifyUserGestureActivation();
  var p = document.completeStorageAccessRequestFromSite("http://example.org");
  try {
    await p;
    ok(true, "Must resolve.");
  } catch {
    ok(false, "Must not reject.");
  }
}

async function completeStorageAccessRequestFromSiteAndExpectFailure() {
  SpecialPowers.wrap(document).notifyUserGestureActivation();
  var p = document.completeStorageAccessRequestFromSite("http://example.org");
  try {
    await p;
    ok(false, "Must not resolve.");
  } catch {
    ok(true, "Must reject.");
  }
}

async function setIntermediatePreference() {
  await SpecialPowers.pushPermissions([
    {
      type: "AllowStorageAccessRequest^http://example.org",
      allow: 1,
      context: "http://example.com/",
    },
  ]);
}

async function configurePrefs() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.storage_access.enabled", true],
      ["dom.storage_access.forward_declared.enabled", true],
      [
        "network.cookie.cookieBehavior",
        BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
      ],
      ["dom.storage_access.auto_grants", false],
      ["dom.storage_access.max_concurrent_auto_grants", 1],
    ],
  });
}

add_task(async function rSAUS_sameOriginIframe() {
  await configurePrefs();
  await openPageAndRunCode(
    TEST_TOP_PAGE_7,
    () => {},
    TEST_DOMAIN_7 + TEST_PATH + "3rdParty.html",
    requestStorageAccessUnderSiteAndExpectSuccess
  );
  await cleanUpData();
  await SpecialPowers.flushPrefEnv();
});

add_task(async function rSAUS_sameSiteIframe() {
  await configurePrefs();
  await openPageAndRunCode(
    TEST_TOP_PAGE_7,
    () => {},
    TEST_DOMAIN_8 + TEST_PATH + "3rdParty.html",
    requestStorageAccessUnderSiteAndExpectSuccess
  );
  await cleanUpData();
  await SpecialPowers.flushPrefEnv();
});

add_task(async function rSAUS_crossSiteIframe() {
  await configurePrefs();
  await openPageAndRunCode(
    TEST_TOP_PAGE_7,
    () => {},
    TEST_DOMAIN + TEST_PATH + "3rdParty.html",
    requestStorageAccessUnderSiteAndExpectFailure
  );
  await cleanUpData();
  await SpecialPowers.flushPrefEnv();
});

add_task(async function cSAR_sameOriginIframe() {
  await configurePrefs();
  await openPageAndRunCode(
    TEST_TOP_PAGE_7,
    setIntermediatePreference,
    TEST_DOMAIN_7 + TEST_PATH + "3rdParty.html",
    completeStorageAccessRequestFromSiteAndExpectSuccess
  );
  await cleanUpData();
  await SpecialPowers.flushPrefEnv();
});

add_task(async function cSAR_sameSiteIframe() {
  await configurePrefs();
  await setIntermediatePreference();
  await openPageAndRunCode(
    TEST_TOP_PAGE_7,
    () => {},
    TEST_DOMAIN_8 + TEST_PATH + "3rdParty.html",
    completeStorageAccessRequestFromSiteAndExpectSuccess
  );
  await cleanUpData();
  await SpecialPowers.flushPrefEnv();
});

add_task(async function cSAR_crossSiteIframe() {
  await configurePrefs();
  await openPageAndRunCode(
    TEST_TOP_PAGE_7,
    setIntermediatePreference,
    TEST_DOMAIN + TEST_PATH + "3rdParty.html",
    completeStorageAccessRequestFromSiteAndExpectFailure
  );
  await cleanUpData();
  await SpecialPowers.flushPrefEnv();
});

add_task(async () => {
  Services.perms.removeAll();
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
      resolve()
    );
  });
});
