/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { RemoteSettings } = ChromeUtils.import(
  "resource://services-settings/remote-settings.js"
);

let { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");

// Name of the RemoteSettings collection containing the rules.
const COLLECTION_NAME = "cookie-banner-rules-list";

let rulesInserted = [];
let rulesRemoved = [];
let insertCallback = null;

const RULE_A_ORIGINAL = {
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

// Testing with RemoteSettings requires a profile.
do_get_profile();

add_setup(async () => {
  // Stub some nsICookieBannerService methods for easy testing.
  let removeRuleForDomain = sinon.stub().callsFake(domain => {
    rulesRemoved.push(domain);
  });

  let insertRule = sinon.stub().callsFake(rule => {
    rulesInserted.push(rule);
    insertCallback?.();
  });

  let oldCookieBanners = Services.cookieBanners;
  Services.cookieBanners = { insertRule, removeRuleForDomain };

  // Remove stubs on test end.
  registerCleanupFunction(() => {
    Services.cookieBanners = oldCookieBanners;
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
  cookieBannerListService.init();

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
  db.clear();
  await db.importChanges({}, Date.now());
  rulesInserted = [];
  rulesRemoved = [];
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
  cookieBannerListService.init();

  const payload = {
    current: [RULE_A_ORIGINAL, RULE_C],
    created: [RULE_B],
    updated: [{ old: RULE_A_ORIGINAL, new: RULE_A_UPDATED }],
    deleted: [RULE_C],
  };

  Assert.equal(rulesInserted.length, 0, "No inserted rules initially.");
  Assert.equal(rulesRemoved.length, 0, "No removed rules initially.");

  info("Dispatching artificial RemoteSettings sync event");
  await RemoteSettings(COLLECTION_NAME).emit("sync", { data: payload });

  Assert.equal(rulesInserted.length, 2, "Two inserted rules after sync.");
  Assert.equal(rulesRemoved.length, 1, "One removed rules after sync.");

  let ruleA = rulesInserted.find(rule => rule.domain == RULE_A_UPDATED.domain);
  let ruleB = rulesInserted.find(rule => rule.domain == RULE_B.domain);
  let ruleCDomain = rulesRemoved[0];

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

  Assert.equal(ruleB.domain, RULE_B.domain, "Should have inserted RULE_B");
  Assert.equal(ruleCDomain, RULE_C.domain, "Should have removed RULE_C");

  // Cleanup
  cookieBannerListService.shutdown();
  rulesInserted = [];
  rulesRemoved = [];
});
