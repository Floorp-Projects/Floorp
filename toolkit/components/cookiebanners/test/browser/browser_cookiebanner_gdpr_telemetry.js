/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { SiteDataTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/SiteDataTestUtils.sys.mjs"
);

const TEST_CASES = [
  {
    value: "CAISHAgBEhJnd3NfMjAyNDAzMjUtMF9SQzEaAmRlIAEaBgiAw42wBg",
    expected: "Accept",
    region: "de",
    host: ".google.de",
  },
  {
    value: "CAESHAgBEhJnd3NfMjAyNDAzMjUtMF9SQzEaAmRlIAEaBgiAw42wBg",
    expected: "Reject",
    region: "de",
    host: ".google.de",
  },
  {
    value:
      "CAISNQgBEitib3FfaWRlbnRpdHlmcm9udGVuZHVpc2VydmVyXzIwMjQwMzI0LjA4X3AwGgJkZSADGgYIgMONsAY",
    expected: "Custom",
    region: "de",
    host: ".google.de",
  },
  {
    value: "CAISHAgBEhJnd3NfMjAyNDAzMjUtMF9SQzEaAmVuIAEaBgiAw42wBg",
    expected: "Accept",
    region: "en",
    host: ".google.com",
  },
];

add_setup(async function () {
  registerCleanupFunction(async () => {
    // Clear cookies that have been set during testing.
    await SiteDataTestUtils.clear();
  });
});

add_task(async function test_google_gdpr_telemetry() {
  // Clear telemetry before starting telemetry test.
  Services.fog.testResetFOG();

  for (let test of TEST_CASES) {
    // Add a Google SOCS cookie to trigger recording telemetry.
    SiteDataTestUtils.addToCookies({
      name: "SOCS",
      value: test.value,
      host: test.host,
      path: "/",
    });

    // Verify if the google GDPR choice cookie telemetry is recorded properly.
    is(
      Glean.cookieBanners.googleGdprChoiceCookie[test.host].testGetValue(),
      test.expected,
      "The Google GDPR telemetry is recorded properly."
    );

    // Verify the the event telemetry is recorded properly.
    let events = Glean.cookieBanners.googleGdprChoiceCookieEvent.testGetValue();
    let event = events[events.length - 1];

    is(
      event.extra.search_domain,
      test.host,
      "The Google GDPR event telemetry records the search domain properly."
    );

    is(
      event.extra.choice,
      test.expected,
      "The Google GDPR event telemetry records the choice properly."
    );

    is(
      event.extra.region,
      test.region,
      "The Google GDPR event telemetry records the region properly."
    );
  }

  is(
    Glean.cookieBanners.googleGdprChoiceCookieEvent.testGetValue().length,
    TEST_CASES.length,
    "The number of events is expected."
  );
});

add_task(async function test_invalid_google_socs_cookies() {
  // Clear telemetry before starting telemetry test.
  Services.fog.testResetFOG();

  // Add an invalid Google SOCS cookie which is not Base64 encoding.
  SiteDataTestUtils.addToCookies({
    name: "SOCS",
    value: "invalid",
    host: ".google.com",
    path: "/",
  });

  // Ensure no GDPR telemetry is recorded.
  is(
    Glean.cookieBanners.googleGdprChoiceCookie[".google.com"].testGetValue(),
    null,
    "No Google GDPR telemetry is recorded."
  );

  is(
    Glean.cookieBanners.googleGdprChoiceCookieEvent.testGetValue(),
    null,
    "No event telemetry is recorded."
  );

  // Add an invalid Google SOCS cookie which is Base64 encoding but in wrong
  // protobuf format.
  SiteDataTestUtils.addToCookies({
    name: "SOCS",
    value: "CAISAggBGgYIgMONsAY",
    host: ".google.com",
    path: "/",
  });

  // Ensure no GDPR telemetry is recorded.
  is(
    Glean.cookieBanners.googleGdprChoiceCookie[".google.com"].testGetValue(),
    null,
    "No Google GDPR telemetry is recorded."
  );

  is(
    Glean.cookieBanners.googleGdprChoiceCookieEvent.testGetValue(),
    null,
    "No event telemetry is recorded."
  );
});

add_task(async function test_google_gdpr_telemetry_pbm() {
  // Clear telemetry before starting telemetry test.
  Services.fog.testResetFOG();

  for (let test of TEST_CASES) {
    // Add a private Google SOCS cookie to trigger recording telemetry.
    SiteDataTestUtils.addToCookies({
      name: "SOCS",
      value: test.value,
      host: test.host,
      path: "/",
      originAttributes: { privateBrowsingId: 1 },
    });

    // Ensure we don't record google GDPR choice cookie telemetry.
    is(
      Glean.cookieBanners.googleGdprChoiceCookie[test.host].testGetValue(),
      null,
      "No Google GDPR telemetry is recorded."
    );

    // Verify the the event telemetry for PBM is recorded properly.
    let events =
      Glean.cookieBanners.googleGdprChoiceCookieEventPbm.testGetValue();
    let event = events[events.length - 1];

    is(
      event.extra.choice,
      test.expected,
      "The Google GDPR event telemetry records the choice properly."
    );

    is(event.extra.search_domain, undefined, "No search domain is recorded.");
    is(event.extra.region, undefined, "No region is recorded.");
  }

  is(
    Glean.cookieBanners.googleGdprChoiceCookieEventPbm.testGetValue().length,
    TEST_CASES.length,
    "The number of events is expected."
  );

  // We need to notify the "last-pb-context-exited" tp explicitly remove private
  // cookies. Otherwise, the remaining private cookies could affect following
  // tests. The SiteDataTestUtils.clear() doesn't clear for private cookies.
  Services.obs.notifyObservers(null, "last-pb-context-exited");
});
