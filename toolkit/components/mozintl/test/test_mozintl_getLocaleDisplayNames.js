/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const fs = [
  {
    path: "resource://mock_source/toolkit/intl/languageNames.ftl",
    source: `
language-name-en = English
  `,
  },
  {
    path: "resource://mock_source/toolkit/intl/regionNames.ftl",
    source: `
region-name-us = United States
region-name-ru = Russia
  `,
  },
];

let locales = Services.locale.packagedLocales;
const mockSource = L10nFileSource.createMock(
  "mock",
  "app",
  locales,
  "resource://mock_source",
  fs
);
L10nRegistry.getInstance().registerSources([mockSource]);

const gLangDN = Services.intl.getLanguageDisplayNames.bind(
  Services.intl,
  undefined
);
const gAvLocDN = Services.intl.getAvailableLocaleDisplayNames.bind(
  Services.intl
);
const gRegDN = Services.intl.getRegionDisplayNames.bind(
  Services.intl,
  undefined
);
const gLocDN = Services.intl.getLocaleDisplayNames.bind(
  Services.intl,
  undefined
);

add_test(function test_native_tag() {
  const options = { preferNative: true };
  deepEqual(gLocDN([], options), []);
  deepEqual(gLocDN(["ca-valencia"], options), ["Català (Valencià)"]);
  deepEqual(gLocDN(["en-US"], options), ["English (US)"]);
  deepEqual(gLocDN(["en-RU"], options), ["English (Russia)"]);
  deepEqual(gLocDN(["ja-JP-mac"], options), ["日本語"]);
  run_next_test();
});

add_test(function test_valid_language_tag() {
  deepEqual(gLocDN([]), []);
  deepEqual(gLocDN(["en"]), ["English"]);
  deepEqual(gLocDN(["und"]), ["und"]);
  run_next_test();
});

add_test(function test_valid_region_tag() {
  deepEqual(gLocDN(["en-US"]), ["English (United States)"]);
  deepEqual(gLocDN(["en-XY"]), ["English (XY)"]);
  run_next_test();
});

add_test(function test_valid_script_tag() {
  deepEqual(gLocDN(["en-Cyrl"]), ["English (Cyrl)"]);
  deepEqual(gLocDN(["en-Cyrl-RU"]), ["English (Cyrl, Russia)"]);
  run_next_test();
});

add_test(function test_valid_variants_tag() {
  deepEqual(gLocDN(["en-Cyrl-macos"]), ["English (Cyrl, macos)"]);
  deepEqual(gLocDN(["en-Cyrl-RU-macos"]), ["English (Cyrl, Russia, macos)"]);
  deepEqual(gLocDN(["en-Cyrl-RU-macos-modern"]), [
    "English (Cyrl, Russia, macos, modern)",
  ]);
  run_next_test();
});

add_test(function test_other_subtags_ignored() {
  deepEqual(gLocDN(["en-x-ignore"]), ["English"]);
  deepEqual(gLocDN(["en-t-en-latn"]), ["English"]);
  deepEqual(gLocDN(["en-u-hc-h24"]), ["English"]);
  run_next_test();
});

add_test(function test_invalid_locales() {
  deepEqual(gLocDN(["2"]), ["2"]);
  deepEqual(gLocDN([""]), [""]);
  Assert.throws(() => gLocDN([2]), /All locale codes must be strings/);
  Assert.throws(() => gLocDN([{}]), /All locale codes must be strings/);
  Assert.throws(() => gLocDN([true]), /All locale codes must be strings/);
  run_next_test();
});

add_test(function test_language_only() {
  deepEqual(gLangDN([]), []);
  deepEqual(gLangDN(["en"]), ["English"]);
  deepEqual(gLangDN(["und"]), ["und"]);
  run_next_test();
});

add_test(function test_invalid_languages() {
  deepEqual(gLangDN(["2"]), ["2"]);
  deepEqual(gLangDN([""]), [""]);
  Assert.throws(() => gLangDN([2]), /All language codes must be strings/);
  Assert.throws(() => gLangDN([{}]), /All language codes must be strings/);
  Assert.throws(() => gLangDN([true]), /All language codes must be strings/);
  run_next_test();
});

add_test(function test_region_only() {
  deepEqual(gRegDN([]), []);
  deepEqual(gRegDN(["US"]), ["United States"]);
  deepEqual(gRegDN(["und"]), ["UND"]);
  run_next_test();
});

add_test(function test_invalid_regions() {
  deepEqual(gRegDN(["2"]), ["2"]);
  deepEqual(gRegDN([""]), [""]);
  Assert.throws(() => gRegDN([2]), /All region codes must be strings/);
  Assert.throws(() => gRegDN([{}]), /All region codes must be strings/);
  Assert.throws(() => gRegDN([true]), /All region codes must be strings/);
  run_next_test();
});

add_test(function test_availableLocaleDisplayNames() {
  let langCodes = gAvLocDN("language");
  equal(
    !!langCodes.length,
    true,
    "There should be some language codes available"
  );
  let regCodes = gAvLocDN("region");
  equal(!!regCodes.length, true, "There should be some region codes available");
  run_next_test();
});
