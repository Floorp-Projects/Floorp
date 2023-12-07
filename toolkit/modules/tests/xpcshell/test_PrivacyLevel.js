/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const kPref = "browser.sessionstore.privacy_level";

// Collect data from all sites (http and https).
const PRIVACY_NONE = 0;
// Collect data from unencrypted sites (http), only.
const PRIVACY_ENCRYPTED = 1;
// Collect no data.
const PRIVACY_FULL = 2;

const { PrivacyLevel } = ChromeUtils.importESModule(
  "resource://gre/modules/sessionstore/PrivacyLevel.sys.mjs"
);

const { PrivacyFilter } = ChromeUtils.importESModule(
  "resource://gre/modules/sessionstore/PrivacyFilter.sys.mjs"
);

const kFormInsecureData = {
  url: "http://example.com/",
  other: "hello",
};

const kFormSecureData = {
  url: "https://example.com/",
  other: "secure hello",
};

add_task(async function test_PrivacyLevelAndFilter() {
  Services.prefs.setIntPref(kPref, PRIVACY_NONE);
  Assert.ok(
    PrivacyLevel.check(
      "http://example.com/",
      "Can save http page with no privacy"
    )
  );
  Assert.ok(
    PrivacyLevel.check("https://example.com/"),
    "Can save https page with no privacy"
  );

  Assert.deepEqual(
    PrivacyFilter.filterFormData(kFormInsecureData),
    kFormInsecureData,
    "Filtered data matches for insecure data with no privacy."
  );

  Assert.deepEqual(
    PrivacyFilter.filterFormData(kFormSecureData),
    kFormSecureData,
    "Filtered data matches for secure data with no privacy."
  );

  // Specialcase: empty object.
  Assert.equal(
    PrivacyFilter.filterFormData({}),
    null,
    "Filtering an empty object returns null."
  );

  Services.prefs.setIntPref(kPref, PRIVACY_ENCRYPTED);
  Assert.ok(
    PrivacyLevel.check(
      "http://example.com/",
      "Can save http page with encrypted privacy"
    )
  );
  Assert.ok(
    !PrivacyLevel.check("https://example.com/"),
    "Can't save https page with encrypted privacy"
  );

  Assert.deepEqual(
    PrivacyFilter.filterFormData(kFormInsecureData),
    kFormInsecureData,
    "Filtered data matches for insecure data with encrypted privacy."
  );

  Assert.deepEqual(
    PrivacyFilter.filterFormData(kFormSecureData),
    null,
    "Filtered data matches for secure data with encrypted privacy."
  );

  Services.prefs.setIntPref(kPref, PRIVACY_FULL);
  Assert.ok(
    !PrivacyLevel.check(
      "http://example.com/",
      "Can't save http page with full privacy"
    )
  );
  Assert.ok(
    !PrivacyLevel.check("https://example.com/"),
    "Can't save https page with full privacy"
  );

  Assert.deepEqual(
    PrivacyFilter.filterFormData(kFormInsecureData),
    null,
    "No filtered data for insecure data with full privacy."
  );

  Assert.deepEqual(
    PrivacyFilter.filterFormData(kFormSecureData),
    null,
    "No filtered data for secure data with full privacy."
  );
});
