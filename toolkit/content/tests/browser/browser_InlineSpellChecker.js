function test() {
  let tempScope = {};
  Components.utils.import("resource://gre/modules/InlineSpellChecker.jsm", tempScope);
  let InlineSpellChecker = tempScope.InlineSpellChecker;

  ok(InlineSpellChecker, "InlineSpellChecker class exists");
  for (var fname in tests) {
    tests[fname]();
  }
}

let tests = {
  // Test various possible dictionary name to ensure they display as expected.
  // XXX: This only works for the 'en-US' locale, as the testing involves localized output.
  testDictionaryDisplayNames: function() {
    let isc = new InlineSpellChecker();

    // Check for valid language tag.
    is(isc.getDictionaryDisplayName("-invalid-"), "-invalid-", "'-invalid-' should display as '-invalid-'");

    // Check if display name is available for language subtag.
    is(isc.getDictionaryDisplayName("en"), "English", "'en' should display as 'English'");
    is(isc.getDictionaryDisplayName("qaz"), "qaz", "'qaz' should display as 'qaz'"); // Private use subtag

    // Check if display name is available for region subtag.
    is(isc.getDictionaryDisplayName("en-US"), "English (United States)", "'en-US' should display as 'English (United States)'");
    is(isc.getDictionaryDisplayName("en-QZ"), "English (QZ)", "'en-QZ' should display as 'English (QZ)'"); // Private use subtag
    todo_is(isc.getDictionaryDisplayName("es-419"), "Spanish (Latin America and the Caribbean)", "'es-419' should display as 'Spanish (Latin America and the Caribbean)'");

    // Check if display name is available for script subtag.
    todo_is(isc.getDictionaryDisplayName("en-Cyrl"), "English / Cyrillic", "'en-Cyrl' should display as 'English / Cyrillic'");
    todo_is(isc.getDictionaryDisplayName("en-Cyrl-US"), "English (United States) / Cyrillic", "'en-Cyrl-US' should display as 'English (United States) / Cyrillic'");
    todo_is(isc.getDictionaryDisplayName("en-Cyrl-QZ"), "English (QZ) / Cyrillic", "'en-Cyrl-QZ' should display as 'English (QZ) / Cyrillic'"); // Private use subtag
    todo_is(isc.getDictionaryDisplayName("qaz-Cyrl"), "qaz / Cyrillic", "'qaz-Cyrl' should display as 'qaz / Cyrillic'"); // Private use subtag
    todo_is(isc.getDictionaryDisplayName("qaz-Cyrl-US"), "qaz (United States) / Cyrillic", "'qaz-Cyrl-US' should display as 'qaz (United States) / Cyrillic'"); // Private use subtag
    todo_is(isc.getDictionaryDisplayName("qaz-Cyrl-QZ"), "qaz (QZ) / Cyrillic", "'qaz-Cyrl-QZ' should display as 'qaz (QZ) / Cyrillic'"); // Private use subtags
    is(isc.getDictionaryDisplayName("en-Qaaz"), "English / Qaaz", "'en-Qaaz' should display as 'English / Qaaz'"); // Private use subtag
    is(isc.getDictionaryDisplayName("en-Qaaz-US"), "English (United States) / Qaaz", "'en-Qaaz-US' should display as 'English (United States) / Qaaz'"); // Private use subtag
    is(isc.getDictionaryDisplayName("en-Qaaz-QZ"), "English (QZ) / Qaaz", "'en-Qaaz-QZ' should display as 'English (QZ) / Qaaz'"); // Private use subtags
    is(isc.getDictionaryDisplayName("qaz-Qaaz"), "qaz / Qaaz", "'qaz-Qaaz' should display as 'qaz / Qaaz'"); // Private use subtags
    is(isc.getDictionaryDisplayName("qaz-Qaaz-US"), "qaz (United States) / Qaaz", "'qaz-Qaaz-US' should display as 'qaz (United States) / Qaaz'"); // Private use subtags
    is(isc.getDictionaryDisplayName("qaz-Qaaz-QZ"), "qaz (QZ) / Qaaz", "'qaz-Qaaz-QZ' should display as 'qaz (QZ) / Qaaz'"); // Private use subtags

    // Check if display name is available for variant subtag.
    // XXX: It isn't clear how we'd ideally want to display variant subtags.
    is(isc.getDictionaryDisplayName("de-1996"), "German (1996)", "'de-1996' should display as 'German (1996)'");
    is(isc.getDictionaryDisplayName("de-CH-1996"), "German (Switzerland) (1996)", "'de-CH-1996' should display as 'German (Switzerland) (1996)'");

    // Complex cases.
    // XXX: It isn't clear how we'd ideally want to display variant subtags.
    todo_is(isc.getDictionaryDisplayName("en-Cyrl-US-fonipa"), "English (United States) / Cyrillic (fonipa)", "'en-Cyrl-US-fonipa' should display as 'English (United States) / Cyrillic (fonipa)'");
    todo_is(isc.getDictionaryDisplayName("en-Cyrl-US-fonipa-fonxsamp"), "English (United States) / Cyrillic (fonipa / fonxsamp)", "'en-Cyrl-US-fonipa-fonxsamp' should display as 'English (United States) / Cyrillic (fonipa / fonxsamp)'");
    is(isc.getDictionaryDisplayName("qaz-Qaaz-QZ-fonipa"), "qaz (QZ) / Qaaz (fonipa)", "'qaz-Qaaz-QZ-fonipa' should display as 'qaz (QZ) / Qaaz (fonipa)'"); // Private use subtags
    is(isc.getDictionaryDisplayName("qaz-Qaaz-QZ-fonipa-fonxsamp"), "qaz (QZ) / Qaaz (fonipa / fonxsamp)", "'qaz-Qaaz-QZ-fonipa-fonxsamp' should display as 'qaz (QZ) / Qaaz (fonipa / fonxsamp)'"); // Private use subtags

    // Check if display name is available for grandfathered tags.
    todo_is(isc.getDictionaryDisplayName("en-GB-oed"), "English (United Kingdom) (OED)", "'en-GB-oed' should display as 'English (United Kingdom) (OED)'");
  },
};
