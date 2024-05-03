/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let bounceTrackingProtection;

add_setup(async function () {
  bounceTrackingProtection = Cc[
    "@mozilla.org/bounce-tracking-protection;1"
  ].getService(Ci.nsIBounceTrackingProtection);
  bounceTrackingProtection.clearAll();
});

async function testInteractWithSite(origin, expectRecorded) {
  is(
    bounceTrackingProtection.testGetUserActivationHosts({}).length,
    0,
    "No user activation hosts initially"
  );

  let baseDomain;
  let scheme;

  await BrowserTestUtils.withNewTab(origin, async browser => {
    baseDomain = browser.contentPrincipal.baseDomain;
    scheme = browser.contentPrincipal.URI.scheme;

    info(
      `Trigger a user activation, which should ${
        expectRecorded ? "" : "not "
      }be recorded.`
    );
    // We intentionally turn off this a11y check, because the following click
    // is purposefully sent on an arbitrary web content that is not expected
    // to be tested by itself with the browser mochitests, therefore this rule
    // check shall be ignored by a11y_checks suite.
    AccessibilityUtils.setEnv({ mustHaveAccessibleRule: false });
    await BrowserTestUtils.synthesizeMouseAtPoint(50, 50, {}, browser);
    AccessibilityUtils.resetEnv();
  });
  if (expectRecorded) {
    Assert.deepEqual(
      bounceTrackingProtection
        .testGetUserActivationHosts({})
        .map(entry => entry.siteHost),
      [baseDomain],
      `User activation should be recorded for ${scheme} scheme.`
    );
  } else {
    Assert.deepEqual(
      bounceTrackingProtection.testGetUserActivationHosts({}),
      [],
      `User activation should not be recorded for ${scheme} scheme.`
    );
  }

  bounceTrackingProtection.clearAll();
}

/**
 * Test that we only record user activation for supported schemes.
 */
add_task(async function test_userActivationSchemes() {
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  await testInteractWithSite("http://example.com", true);
  await testInteractWithSite("https://example.com", true);

  await testInteractWithSite("about:blank", false);
  await testInteractWithSite("about:robots", false);
  await testInteractWithSite(
    "file://" + Services.dirsvc.get("TmpD", Ci.nsIFile).path,
    false
  );
});
