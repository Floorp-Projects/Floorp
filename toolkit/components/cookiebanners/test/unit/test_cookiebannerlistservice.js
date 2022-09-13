/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { RemoteSettings } = ChromeUtils.import(
  "resource://services-settings/remote-settings.js"
);

let { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");

// Name of the RemoteSettings collection containing the rules.
const COLLECTION_NAME = "cookie-banner-rules-list";

// Name of pref used to import test rules.
const PREF_TEST_RULES = "cookiebanners.listService.testRules";

let rulesInserted = [];
let rulesRemoved = [];
let insertCallback = null;

function genUUID() {
  return Services.uuid.generateUUID().number.slice(1, -1);
}

const RULE_A_ORIGINAL = {
  id: genUUID(),
  click: {
    optOut: "#fooOut",
    presence: "#foobar",
  },
  domain: "example.com",
  cookies: {
    optOut: [
      {
        name: "doOptOut",
        value: "true",
        isSecure: true,
        isSession: false,
      },
    ],
  },
};

const RULE_B = {
  id: genUUID(),
  click: {
    optOut: "#fooOutB",
    presence: "#foobarB",
  },
  domain: "example.org",
  cookies: {
    optOut: [
      {
        name: "doOptOutB",
        value: "true",
        isSecure: true,
      },
    ],
  },
};

const RULE_C = {
  id: genUUID(),
  click: {
    optOut: "#fooOutC",
    presence: "#foobarC",
  },
  domain: "example.net",
  cookies: {
    optIn: [
      {
        name: "gdpr",
        value: "1",
        path: "/myPath",
        host: "foo.example.net",
      },
    ],
  },
};

const RULE_A_UPDATED = {
  id: RULE_A_ORIGINAL,
  click: {
    optOut: "#fooOut",
    optIn: "#barIn",
    presence: "#foobar",
  },
  domain: "example.com",
  cookies: {
    optOut: [
      {
        name: "doOptOutUpdated",
        value: "true",
        isSecure: true,
      },
      {
        name: "hideBanner",
        value: "1",
      },
    ],
  },
};

const INVALID_RULE_CLICK = {
  id: genUUID(),
  domain: "foobar.com",
  click: {
    presence: 1,
    optIn: "#foo",
  },
};

const INVALID_RULE_EMPTY = {};

const RULE_D_GLOBAL = {
  id: genUUID(),
  click: {
    optOut: "#globalOptOutD",
    presence: "#globalBannerD",
  },
  domain: "*",
  cookies: {},
};

const RULE_E_GLOBAL = {
  id: genUUID(),
  click: {
    optOut: "#globalOptOutE",
    presence: "#globalBannerE",
  },
  domain: "*",
  cookies: {},
};

// Testing with RemoteSettings requires a profile.
do_get_profile();

add_setup(async () => {
  // Enable debug logging.
  Services.prefs.setStringPref("cookiebanners.listService.logLevel", "Debug");

  // Stub some nsICookieBannerService methods for easy testing.
  let removeRule = sinon.stub().callsFake(rule => {
    rulesRemoved.push(rule);
  });

  let insertRule = sinon.stub().callsFake(rule => {
    rulesInserted.push(rule);
    insertCallback?.();
  });

  let oldCookieBanners = Services.cookieBanners;
  Services.cookieBanners = { insertRule, removeRule, resetRules() {} };

  // Remove stubs on test end.
  registerCleanupFunction(() => {
    Services.cookieBanners = oldCookieBanners;
    Services.prefs.clearUserPref("cookiebanners.listService.logLevel");
  });
});

/**
 * Promise wrapper to listen for Services.cookieBanners.insertRule calls from
 * the CookieBannerListService.
 * @param {function} checkFn - Function which returns true or false to indicate
 * if this is the insert the caller is looking for.
 * @returns {Promise} - Promise which resolves when checkFn matches after
 * insertRule call.
 */
function waitForInsert(checkFn) {
  return new Promise(resolve => {
    insertCallback = () => {
      if (checkFn()) {
        insertCallback = null;
        resolve();
      }
    };
  });
}

/**
 * Tests that the cookie banner list service imports all rules on init.
 */
add_task(async function test_initial_import() {
  info("Initializing RemoteSettings collection " + COLLECTION_NAME);

  let db = RemoteSettings(COLLECTION_NAME).db;
  db.clear();
  await db.create(RULE_A_ORIGINAL);
  await db.create(RULE_C);
  await db.importChanges({}, Date.now());

  Assert.equal(rulesInserted.length, 0, "No inserted rules initially.");
  Assert.equal(rulesRemoved.length, 0, "No removed rules initially.");

  let insertPromise = waitForInsert(() => rulesInserted.length >= 2);

  info(
    "Initializing the cookie banner list service which triggers a collection get call"
  );
  let cookieBannerListService = Cc[
    "@mozilla.org/cookie-banner-list-service;1"
  ].getService(Ci.nsICookieBannerListService);
  await cookieBannerListService.initForTest();

  await insertPromise;

  Assert.equal(rulesInserted.length, 2, "Two inserted rules after init.");
  Assert.equal(rulesRemoved.length, 0, "No removed rules after init.");

  let ruleA = rulesInserted.find(rule => rule.domain == RULE_A_UPDATED.domain);
  let cookieRuleA = ruleA.cookiesOptOut[0].cookie;
  let ruleC = rulesInserted.find(rule => rule.domain == RULE_C.domain);
  let cookieRuleC = ruleC.cookiesOptIn[0].cookie;

  Assert.ok(ruleA, "Has rule A.");
  // Test the defaults which CookieBannerListService sets when the rule does
  // not.
  Assert.equal(
    cookieRuleA.isSession,
    false,
    "Cookie for rule A should not be a session cookie."
  );
  Assert.equal(
    cookieRuleA.host,
    ".example.com",
    "Cookie host for rule A should be default."
  );
  Assert.equal(
    cookieRuleA.path,
    "/",
    "Cookie path for rule A should be default."
  );

  Assert.ok(ruleC, "Has rule C.");
  Assert.equal(
    ruleC.cookiesOptIn[0].cookie.isSession,
    true,
    "Cookie for rule C should should be a session cookie."
  );
  Assert.equal(
    cookieRuleC.host,
    "foo.example.net",
    "Cookie host for rule C should be custom."
  );
  Assert.equal(
    cookieRuleC.path,
    "/myPath",
    "Cookie path for rule C should be custom."
  );

  // Cleanup
  cookieBannerListService.shutdown();
  rulesInserted = [];
  rulesRemoved = [];

  db.clear();
  await db.importChanges({}, Date.now());
});

/**
 * Tests that the cookie banner list service updates rules on sync.
 */
add_task(async function test_remotesettings_sync() {
  // Initialize the cookie banner list service so it subscribes to
  // RemoteSettings updates.
  let cookieBannerListService = Cc[
    "@mozilla.org/cookie-banner-list-service;1"
  ].getService(Ci.nsICookieBannerListService);
  await cookieBannerListService.initForTest();

  const payload = {
    current: [RULE_A_ORIGINAL, RULE_C, RULE_D_GLOBAL],
    created: [RULE_B, RULE_E_GLOBAL],
    updated: [{ old: RULE_A_ORIGINAL, new: RULE_A_UPDATED }],
    deleted: [RULE_C, RULE_D_GLOBAL],
  };

  Assert.equal(rulesInserted.length, 0, "No inserted rules initially.");
  Assert.equal(rulesRemoved.length, 0, "No removed rules initially.");

  info("Dispatching artificial RemoteSettings sync event");
  await RemoteSettings(COLLECTION_NAME).emit("sync", { data: payload });

  Assert.equal(rulesInserted.length, 3, "Three inserted rules after sync.");
  Assert.equal(rulesRemoved.length, 2, "Two removed rules after sync.");

  let ruleA = rulesInserted.find(rule => rule.id == RULE_A_UPDATED.id);
  let ruleB = rulesInserted.find(rule => rule.id == RULE_B.id);
  let ruleE = rulesInserted.find(rule => rule.id == RULE_E_GLOBAL.id);
  let ruleC = rulesRemoved[0];

  info("Testing that service inserted updated version of RULE_A.");
  Assert.equal(
    ruleA.domain,
    RULE_A_UPDATED.domain,
    "Domain should match RULE_A."
  );
  Assert.equal(
    ruleA.cookiesOptOut.length,
    RULE_A_UPDATED.cookies.optOut.length,
    "cookiesOptOut length should match RULE_A."
  );

  info("Testing opt-out cookies of RULE_A");
  for (let i = 0; i < RULE_A_UPDATED.cookies.optOut.length; i += 1) {
    Assert.equal(
      ruleA.cookiesOptOut[i].cookie.name,
      RULE_A_UPDATED.cookies.optOut[i].name,
      "cookiesOptOut cookie name should match RULE_A."
    );
    Assert.equal(
      ruleA.cookiesOptOut[i].cookie.value,
      RULE_A_UPDATED.cookies.optOut[i].value,
      "cookiesOptOut cookie value should match RULE_A."
    );
  }

  Assert.equal(ruleB.id, RULE_B.id, "Should have inserted RULE_B");
  Assert.equal(ruleC.id, RULE_C.id, "Should have removed RULE_C");
  Assert.equal(
    ruleE.id,
    RULE_E_GLOBAL.id,
    "Should have inserted RULE_E_GLOBAL"
  );

  // Cleanup
  cookieBannerListService.shutdown();
  rulesInserted = [];
  rulesRemoved = [];

  let { db } = RemoteSettings(COLLECTION_NAME);
  db.clear();
  await db.importChanges({}, Date.now());
});

/**
 * Tests the cookie banner rule test pref.
 */
add_task(async function test_rule_test_pref() {
  Services.prefs.setStringPref(
    PREF_TEST_RULES,
    JSON.stringify([RULE_A_ORIGINAL, RULE_B])
  );

  Assert.equal(rulesInserted.length, 0, "No inserted rules initially.");
  Assert.equal(rulesRemoved.length, 0, "No removed rules initially.");

  let insertPromise = waitForInsert(() => rulesInserted.length >= 2);

  // Initialize the cookie banner list service so it imports test rules and listens for pref changes.
  let cookieBannerListService = Cc[
    "@mozilla.org/cookie-banner-list-service;1"
  ].getService(Ci.nsICookieBannerListService);
  await cookieBannerListService.initForTest();

  info("Wait for rules to be inserted");
  await insertPromise;

  Assert.equal(rulesInserted.length, 2, "Should have inserted two rules.");
  Assert.equal(rulesRemoved.length, 0, "Should not have removed any rules.");

  Assert.ok(
    rulesInserted.some(rule => rule.domain == RULE_A_ORIGINAL.domain),
    "Should have inserted RULE_A"
  );
  Assert.ok(
    rulesInserted.some(rule => rule.domain == RULE_B.domain),
    "Should have inserted RULE_B"
  );

  rulesInserted = [];
  rulesRemoved = [];

  let insertPromise2 = waitForInsert(() => rulesInserted.length >= 3);

  info(
    "Updating test rules via pref. The list service should detect the pref change."
  );
  // This includes some invalid rules, they should be skipped.
  Services.prefs.setStringPref(
    PREF_TEST_RULES,
    JSON.stringify([
      RULE_A_ORIGINAL,
      RULE_B,
      INVALID_RULE_EMPTY,
      RULE_C,
      INVALID_RULE_CLICK,
    ])
  );

  info("Wait for rules to be inserted");
  await insertPromise2;

  Assert.equal(rulesInserted.length, 3, "Should have inserted three rules.");
  Assert.equal(rulesRemoved.length, 0, "Should not have removed any rules.");

  Assert.ok(
    rulesInserted.some(rule => rule.domain == RULE_A_ORIGINAL.domain),
    "Should have inserted RULE_A"
  );
  Assert.ok(
    rulesInserted.some(rule => rule.domain == RULE_B.domain),
    "Should have inserted RULE_B"
  );
  Assert.ok(
    rulesInserted.some(rule => rule.domain == RULE_C.domain),
    "Should have inserted RULE_C"
  );

  // Cleanup
  cookieBannerListService.shutdown();
  rulesInserted = [];
  rulesRemoved = [];

  Services.prefs.clearUserPref(PREF_TEST_RULES);
});
