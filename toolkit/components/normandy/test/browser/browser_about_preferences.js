"use strict";

const OPT_OUT_PREF = "app.shield.optoutstudies.enabled";

function withPrivacyPrefs() {
  return function(testFunc) {
    return async args =>
      BrowserTestUtils.withNewTab("about:preferences#privacy", async browser =>
        testFunc({ ...args, browser })
      );
  };
}

decorate_task(
  withPrefEnv({
    set: [[OPT_OUT_PREF, true]],
  }),
  withPrivacyPrefs(),
  async function testCheckedOnLoad({ browser }) {
    const checkbox = browser.contentDocument.getElementById(
      "optOutStudiesEnabled"
    );
    ok(
      checkbox.checked,
      "Opt-out checkbox is checked on load when the pref is true"
    );
  }
);

decorate_task(
  withPrefEnv({
    set: [[OPT_OUT_PREF, false]],
  }),
  withPrivacyPrefs(),
  async function testUncheckedOnLoad({ browser }) {
    const checkbox = browser.contentDocument.getElementById(
      "optOutStudiesEnabled"
    );
    ok(
      !checkbox.checked,
      "Opt-out checkbox is unchecked on load when the pref is false"
    );
  }
);

decorate_task(
  withPrefEnv({
    set: [[OPT_OUT_PREF, true]],
  }),
  withPrivacyPrefs(),
  async function testCheckboxes({ browser }) {
    const optOutCheckbox = browser.contentDocument.getElementById(
      "optOutStudiesEnabled"
    );

    optOutCheckbox.click();
    ok(
      !Services.prefs.getBoolPref(OPT_OUT_PREF),
      "Unchecking the opt-out checkbox sets the pref to false."
    );
    optOutCheckbox.click();
    ok(
      Services.prefs.getBoolPref(OPT_OUT_PREF),
      "Checking the opt-out checkbox sets the pref to true."
    );
  }
);

decorate_task(
  withPrefEnv({
    set: [[OPT_OUT_PREF, true]],
  }),
  withPrivacyPrefs(),
  async function testPrefWatchers({ browser }) {
    const optOutCheckbox = browser.contentDocument.getElementById(
      "optOutStudiesEnabled"
    );

    Services.prefs.setBoolPref(OPT_OUT_PREF, false);
    ok(
      !optOutCheckbox.checked,
      "Disabling the opt-out pref unchecks the opt-out checkbox."
    );
    Services.prefs.setBoolPref(OPT_OUT_PREF, true);
    ok(
      optOutCheckbox.checked,
      "Enabling the opt-out pref checks the opt-out checkbox."
    );
  }
);

decorate_task(withPrivacyPrefs(), async function testViewStudiesLink({
  browser,
}) {
  browser.contentDocument.getElementById("viewShieldStudies").click();
  await BrowserTestUtils.waitForLocationChange(gBrowser);

  is(
    gBrowser.currentURI.spec,
    "about:studies",
    "Clicking the view studies link opens about:studies in a new tab."
  );

  gBrowser.removeCurrentTab();
});
