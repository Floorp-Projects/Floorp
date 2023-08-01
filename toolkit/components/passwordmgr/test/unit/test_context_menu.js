/**
 * Test the password manager context menu.
 */

"use strict";

const { LoginManagerContextMenu } = ChromeUtils.importESModule(
  "resource://gre/modules/LoginManagerContextMenu.sys.mjs"
);

const dateAndTimeFormatter = new Services.intl.DateTimeFormat(undefined, {
  dateStyle: "medium",
});

const ORIGIN_HTTP_EXAMPLE_ORG = "http://example.org";
const ORIGIN_HTTPS_EXAMPLE_ORG = "https://example.org";
const ORIGIN_HTTPS_EXAMPLE_ORG_8080 = "https://example.org:8080";

const FORM_LOGIN_HTTPS_EXAMPLE_ORG_U1_P1 = formLogin({
  formActionOrigin: ORIGIN_HTTPS_EXAMPLE_ORG,
  guid: "FORM_LOGIN_HTTPS_EXAMPLE_ORG_U1_P1",
  origin: ORIGIN_HTTPS_EXAMPLE_ORG,
});

// HTTP version of the above
const FORM_LOGIN_HTTP_EXAMPLE_ORG_U1_P1 = formLogin({
  formActionOrigin: ORIGIN_HTTP_EXAMPLE_ORG,
  guid: "FORM_LOGIN_HTTP_EXAMPLE_ORG_U1_P1",
  origin: ORIGIN_HTTP_EXAMPLE_ORG,
});

// Same as above but with a different password
const FORM_LOGIN_HTTP_EXAMPLE_ORG_U1_P2 = formLogin({
  formActionOrigin: ORIGIN_HTTP_EXAMPLE_ORG,
  guid: "FORM_LOGIN_HTTP_EXAMPLE_ORG_U1_P2",
  origin: ORIGIN_HTTP_EXAMPLE_ORG,
  password: "pass2",
});

// Non-default port

const FORM_LOGIN_HTTPS_EXAMPLE_ORG_8080_U1_P2 = formLogin({
  formActionOrigin: ORIGIN_HTTPS_EXAMPLE_ORG_8080,
  guid: "FORM_LOGIN_HTTPS_EXAMPLE_ORG_8080_U1_P2",
  origin: ORIGIN_HTTPS_EXAMPLE_ORG_8080,
  password: "pass2",
});

// HTTP Auth.

const HTTP_LOGIN_HTTPS_EXAMPLE_ORG_U1_P1 = authLogin({
  guid: "FORM_LOGIN_HTTPS_EXAMPLE_ORG_U1_P1",
  origin: ORIGIN_HTTPS_EXAMPLE_ORG,
});

ChromeUtils.defineLazyGetter(this, "_stringBundle", function () {
  return Services.strings.createBundle(
    "chrome://passwordmgr/locale/passwordmgr.properties"
  );
});

/**
 * Prepare data for the following tests.
 */
add_task(async function test_initialize() {
  Services.prefs.setBoolPref("signon.schemeUpgrades", true);
});

add_task(async function test_sameOriginBothHTTPAndHTTPSDeduped() {
  await runTestcase({
    formOrigin: FORM_LOGIN_HTTPS_EXAMPLE_ORG_U1_P1.origin,
    savedLogins: [FORM_LOGIN_HTTPS_EXAMPLE_ORG_U1_P1],
    expectedItems: [
      {
        login: FORM_LOGIN_HTTPS_EXAMPLE_ORG_U1_P1,
      },
    ],
  });
});

add_task(async function test_sameOriginOnlyHTTPS_noUsername() {
  let loginWithoutUsername = FORM_LOGIN_HTTPS_EXAMPLE_ORG_U1_P1.clone();
  loginWithoutUsername.QueryInterface(Ci.nsILoginMetaInfo).guid = "no-username";
  loginWithoutUsername.username = "";
  await runTestcase({
    formOrigin: loginWithoutUsername.origin,
    savedLogins: [loginWithoutUsername],
    expectedItems: [
      {
        login: loginWithoutUsername,
        time: true,
      },
    ],
  });
});

add_task(async function test_sameOriginOnlyHTTP() {
  await runTestcase({
    formOrigin: FORM_LOGIN_HTTP_EXAMPLE_ORG_U1_P1.origin,
    savedLogins: [FORM_LOGIN_HTTP_EXAMPLE_ORG_U1_P1],
    expectedItems: [
      {
        login: FORM_LOGIN_HTTP_EXAMPLE_ORG_U1_P1,
      },
    ],
  });
});

// Scheme upgrade/downgrade tasks

add_task(async function test_sameOriginDedupeSchemeUpgrade() {
  await runTestcase({
    formOrigin: FORM_LOGIN_HTTPS_EXAMPLE_ORG_U1_P1.origin,
    savedLogins: [
      FORM_LOGIN_HTTPS_EXAMPLE_ORG_U1_P1,
      FORM_LOGIN_HTTP_EXAMPLE_ORG_U1_P1,
    ],
    expectedItems: [
      {
        login: FORM_LOGIN_HTTPS_EXAMPLE_ORG_U1_P1,
      },
    ],
  });
});

add_task(async function test_sameOriginSchemeDowngrade() {
  // Should have no https: when formOrigin is https:
  await runTestcase({
    formOrigin: FORM_LOGIN_HTTP_EXAMPLE_ORG_U1_P1.origin,
    savedLogins: [
      FORM_LOGIN_HTTPS_EXAMPLE_ORG_U1_P1,
      FORM_LOGIN_HTTP_EXAMPLE_ORG_U1_P1,
    ],
    expectedItems: [
      {
        login: FORM_LOGIN_HTTP_EXAMPLE_ORG_U1_P1,
      },
    ],
  });
});

add_task(async function test_sameOriginNotShadowedSchemeUpgrade() {
  await runTestcase({
    formOrigin: FORM_LOGIN_HTTPS_EXAMPLE_ORG_U1_P1.origin,
    savedLogins: [
      FORM_LOGIN_HTTPS_EXAMPLE_ORG_U1_P1,
      FORM_LOGIN_HTTP_EXAMPLE_ORG_U1_P2, // Different password
    ],
    expectedItems: [
      {
        login: FORM_LOGIN_HTTPS_EXAMPLE_ORG_U1_P1,
        time: true,
      },
      {
        login: FORM_LOGIN_HTTP_EXAMPLE_ORG_U1_P2,
        time: true,
      },
    ],
  });
});

add_task(async function test_sameOriginShadowedSchemeDowngrade() {
  // Should have no https: when formOrigin is https:
  await runTestcase({
    formOrigin: FORM_LOGIN_HTTP_EXAMPLE_ORG_U1_P1.origin,
    savedLogins: [
      FORM_LOGIN_HTTPS_EXAMPLE_ORG_U1_P1,
      FORM_LOGIN_HTTP_EXAMPLE_ORG_U1_P2, // Different password
    ],
    expectedItems: [
      {
        login: FORM_LOGIN_HTTP_EXAMPLE_ORG_U1_P2,
      },
    ],
  });
});

// Non-default port tasks

add_task(async function test_sameDomainDifferentPort_onDefault() {
  await runTestcase({
    formOrigin: FORM_LOGIN_HTTPS_EXAMPLE_ORG_U1_P1.origin,
    savedLogins: [
      FORM_LOGIN_HTTPS_EXAMPLE_ORG_U1_P1,
      FORM_LOGIN_HTTPS_EXAMPLE_ORG_8080_U1_P2,
    ],
    expectedItems: [
      {
        login: FORM_LOGIN_HTTPS_EXAMPLE_ORG_U1_P1,
      },
    ],
  });
});

add_task(async function test_sameDomainDifferentPort_onNonDefault() {
  await runTestcase({
    // Swap the formOrigin compared to above
    formOrigin: FORM_LOGIN_HTTPS_EXAMPLE_ORG_8080_U1_P2.origin,
    savedLogins: [
      FORM_LOGIN_HTTPS_EXAMPLE_ORG_U1_P1,
      FORM_LOGIN_HTTPS_EXAMPLE_ORG_8080_U1_P2,
    ],
    expectedItems: [
      {
        login: FORM_LOGIN_HTTPS_EXAMPLE_ORG_8080_U1_P2,
      },
    ],
  });
});

// HTTP auth. suggestions

add_task(async function test_sameOriginOnlyHTTPAuth() {
  await runTestcase({
    formOrigin: FORM_LOGIN_HTTPS_EXAMPLE_ORG_U1_P1.origin,
    savedLogins: [HTTP_LOGIN_HTTPS_EXAMPLE_ORG_U1_P1],
    expectedItems: [
      {
        login: HTTP_LOGIN_HTTPS_EXAMPLE_ORG_U1_P1,
      },
    ],
  });
});

// Helpers

function formLogin(modifications = {}) {
  let mods = Object.assign(
    {},
    {
      timePasswordChanged: 1573821296000,
    },
    modifications
  );
  return TestData.formLogin(mods);
}

function authLogin(modifications = {}) {
  let mods = Object.assign(
    {},
    {
      timePasswordChanged: 1573821296000,
    },
    modifications
  );
  return TestData.authLogin(mods);
}

/**
 * Tests if the LoginManagerContextMenu returns the correct login items.
 */
async function runTestcase({ formOrigin, savedLogins, expectedItems }) {
  const DOCUMENT_CONTENT = "<form><input id='pw' type=password></form>";

  await Services.logins.addLogins(savedLogins);

  // Create the logins menuitems fragment.
  let { fragment, document } = createLoginsFragment(
    formOrigin,
    DOCUMENT_CONTENT
  );

  if (!expectedItems.length) {
    Assert.ok(fragment === null, "Null returned. No logins were found.");
    return;
  }
  let actualItems = [...fragment.children];

  // Check if the items are those expected to be listed.
  checkLoginItems(actualItems, expectedItems);

  document.body.appendChild(fragment);

  // Try to clear the fragment.
  LoginManagerContextMenu.clearLoginsFromMenu(document);
  Assert.equal(
    document.querySelectorAll("menuitem").length,
    0,
    "All items correctly cleared."
  );

  Services.logins.removeAllUserFacingLogins();
}

/**
 * Create a fragment with a menuitem for each login.
 */
function createLoginsFragment(url, content) {
  const CHROME_URL = "chrome://mock-chrome/content/";

  // Create a mock document.
  let document = MockDocument.createTestDocument(
    CHROME_URL,
    content,
    undefined,
    true
  );

  // We also need a simple mock Browser object for this test.
  let browser = {
    ownerDocument: document,
  };

  let formOrigin = LoginHelper.getLoginOrigin(url);
  return {
    document,
    fragment: LoginManagerContextMenu.addLoginsToMenu(
      null,
      browser,
      formOrigin
    ),
  };
}

function checkLoginItems(actualItems, expectedDetails) {
  for (let [i, expectedDetail] of expectedDetails.entries()) {
    let actualElement = actualItems[i];

    Assert.equal(actualElement.localName, "menuitem", "Check localName");

    let expectedLabel = expectedDetail.login.username;
    if (!expectedLabel) {
      expectedLabel += _stringBundle.GetStringFromName("noUsername");
    }
    if (expectedDetail.time) {
      expectedLabel +=
        " (" +
        dateAndTimeFormatter.format(
          new Date(expectedDetail.login.timePasswordChanged)
        ) +
        ")";
    }
    Assert.equal(
      actualElement.getAttribute("label"),
      expectedLabel,
      `Check label ${i}`
    );
  }

  Assert.equal(
    actualItems.length,
    expectedDetails.length,
    "Should have the correct number of menu items"
  );
}
