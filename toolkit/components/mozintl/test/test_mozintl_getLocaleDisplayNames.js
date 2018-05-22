/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

ChromeUtils.import("resource://gre/modules/Services.jsm");


const gLangDN = Services.intl.getLanguageDisplayNames.bind(Services.intl, undefined);
const gRegDN = Services.intl.getRegionDisplayNames.bind(Services.intl, undefined);
const gLocDN = Services.intl.getLocaleDisplayNames.bind(Services.intl, undefined);

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
  deepEqual(gLocDN(["en-Cyrl-RU-macos-modern"]), ["English (Cyrl, Russia, macos, modern)"]);
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
