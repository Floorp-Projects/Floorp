function test() {
  let tempScope = {};
  Components.utils.import("resource://gre/modules/InlineSpellChecker.jsm", tempScope);
  let InlineSpellChecker = tempScope.InlineSpellChecker;

  ok(InlineSpellChecker, "InlineSpellChecker class exists");
  for (var fname in tests) {
    tests[fname]();
  }
}

var tests = {
  // Test various possible dictionary name to ensure they display as expected.
  // XXX: This only works for the 'en-US' locale, as the testing involves localized output.
  testDictionaryDisplayNames: function() {
    let isc = new InlineSpellChecker();

    // Check non-well-formed language tag.
    is(isc.getDictionaryDisplayName("-invalid-"), "-invalid-", "'-invalid-' should display as '-invalid-'");

    // XXX: It isn't clear how we'd ideally want to display variant subtags.

    // Check valid language subtag.
    is(isc.getDictionaryDisplayName("en"), "English", "'en' should display as 'English'");
    is(isc.getDictionaryDisplayName("en-fonipa"), "English (fonipa)", "'en-fonipa' should display as 'English (fonipa)'");
    is(isc.getDictionaryDisplayName("en-qxqaaaaz"), "English (qxqaaaaz)", "'en-qxqaaaaz' should display as 'English (qxqaaaaz)'");

    // Check valid language subtag and valid region subtag.
    is(isc.getDictionaryDisplayName("en-US"), "English (United States)", "'en-US' should display as 'English (United States)'");
    is(isc.getDictionaryDisplayName("en-US-fonipa"), "English (United States) (fonipa)", "'en-US-fonipa' should display as 'English (United States) (fonipa)'");
    is(isc.getDictionaryDisplayName("en-US-qxqaaaaz"), "English (United States) (qxqaaaaz)", "'en-US-qxqaaaaz' should display as 'English (United States) (qxqaaaaz)'");

    // Check valid language subtag and invalid but well-formed region subtag.
    is(isc.getDictionaryDisplayName("en-WO"), "English (WO)", "'en-WO' should display as 'English (WO)'");
    is(isc.getDictionaryDisplayName("en-WO-fonipa"), "English (WO) (fonipa)", "'en-WO-fonipa' should display as 'English (WO) (fonipa)'");
    is(isc.getDictionaryDisplayName("en-WO-qxqaaaaz"), "English (WO) (qxqaaaaz)", "'en-WO-qxqaaaaz' should display as 'English (WO) (qxqaaaaz)'");

    // Check valid language subtag and valid script subtag.
    todo_is(isc.getDictionaryDisplayName("en-Cyrl"), "English / Cyrillic", "'en-Cyrl' should display as 'English / Cyrillic'");
    todo_is(isc.getDictionaryDisplayName("en-Cyrl-fonipa"), "English / Cyrillic (fonipa)", "'en-Cyrl-fonipa' should display as 'English / Cyrillic (fonipa)'");
    todo_is(isc.getDictionaryDisplayName("en-Cyrl-qxqaaaaz"), "English / Cyrillic (qxqaaaaz)", "'en-Cyrl-qxqaaaaz' should display as 'English / Cyrillic (qxqaaaaz)'");
    todo_is(isc.getDictionaryDisplayName("en-Cyrl-US"), "English (United States) / Cyrillic", "'en-Cyrl-US' should display as 'English (United States) / Cyrillic'");
    todo_is(isc.getDictionaryDisplayName("en-Cyrl-US-fonipa"), "English (United States) / Cyrillic (fonipa)", "'en-Cyrl-US-fonipa' should display as 'English (United States) / Cyrillic (fonipa)'");
    todo_is(isc.getDictionaryDisplayName("en-Cyrl-US-qxqaaaaz"), "English (United States) / Cyrillic (qxqaaaaz)", "'en-Cyrl-US-qxqaaaaz' should display as 'English (United States) / Cyrillic (qxqaaaaz)'");
    todo_is(isc.getDictionaryDisplayName("en-Cyrl-WO"), "English (WO) / Cyrillic", "'en-Cyrl-WO' should display as 'English (WO) / Cyrillic'");
    todo_is(isc.getDictionaryDisplayName("en-Cyrl-WO-fonipa"), "English (WO) / Cyrillic (fonipa)", "'en-Cyrl-WO-fonipa' should display as 'English (WO) / Cyrillic (fonipa)'");
    todo_is(isc.getDictionaryDisplayName("en-Cyrl-WO-qxqaaaaz"), "English (WO) / Cyrillic (qxqaaaaz)", "'en-Cyrl-WO-qxqaaaaz' should display as 'English (WO) / Cyrillic (qxqaaaaz)'");

    // Check valid language subtag and invalid but well-formed script subtag.
    is(isc.getDictionaryDisplayName("en-Qaaz"), "English / Qaaz", "'en-Qaaz' should display as 'English / Qaaz'");
    is(isc.getDictionaryDisplayName("en-Qaaz-fonipa"), "English / Qaaz (fonipa)", "'en-Qaaz-fonipa' should display as 'English / Qaaz (fonipa)'");
    is(isc.getDictionaryDisplayName("en-Qaaz-qxqaaaaz"), "English / Qaaz (qxqaaaaz)", "'en-Qaaz-qxqaaaaz' should display as 'English / Qaaz (qxqaaaaz)'");
    is(isc.getDictionaryDisplayName("en-Qaaz-US"), "English (United States) / Qaaz", "'en-Qaaz-US' should display as 'English (United States) / Qaaz'");
    is(isc.getDictionaryDisplayName("en-Qaaz-US-fonipa"), "English (United States) / Qaaz (fonipa)", "'en-Qaaz-US-fonipa' should display as 'English (United States) / Qaaz (fonipa)'");
    is(isc.getDictionaryDisplayName("en-Qaaz-US-qxqaaaaz"), "English (United States) / Qaaz (qxqaaaaz)", "'en-Qaaz-US-qxqaaaaz' should display as 'English (United States) / Qaaz (qxqaaaaz)'");
    is(isc.getDictionaryDisplayName("en-Qaaz-WO"), "English (WO) / Qaaz", "'en-Qaaz-WO' should display as 'English (WO) / Qaaz'");
    is(isc.getDictionaryDisplayName("en-Qaaz-WO-fonipa"), "English (WO) / Qaaz (fonipa)", "'en-Qaaz-WO-fonipa' should display as 'English (WO) / Qaaz (fonipa)'");
    is(isc.getDictionaryDisplayName("en-Qaaz-WO-qxqaaaaz"), "English (WO) / Qaaz (qxqaaaaz)", "'en-Qaaz-WO-qxqaaaaz' should display as 'English (WO) / Qaaz (qxqaaaaz)'");

    // Check invalid but well-formed language subtag.
    is(isc.getDictionaryDisplayName("qaz"), "qaz", "'qaz' should display as 'qaz'");
    is(isc.getDictionaryDisplayName("qaz-fonipa"), "qaz (fonipa)", "'qaz-fonipa' should display as 'qaz (fonipa)'");
    is(isc.getDictionaryDisplayName("qaz-qxqaaaaz"), "qaz (qxqaaaaz)", "'qaz-qxqaaaaz' should display as 'qaz (qxqaaaaz)'");

    // Check invalid but well-formed language subtag and valid region subtag.
    is(isc.getDictionaryDisplayName("qaz-US"), "qaz (United States)", "'qaz-US' should display as 'qaz (United States)'");
    is(isc.getDictionaryDisplayName("qaz-US-fonipa"), "qaz (United States) (fonipa)", "'qaz-US-fonipa' should display as 'qaz (United States) (fonipa)'");
    is(isc.getDictionaryDisplayName("qaz-US-qxqaaaaz"), "qaz (United States) (qxqaaaaz)", "'qaz-US-qxqaaaaz' should display as 'qaz (United States) (qxqaaaaz)'");

    // Check invalid but well-formed language subtag and invalid but well-formed region subtag.
    is(isc.getDictionaryDisplayName("qaz-WO"), "qaz (WO)", "'qaz-WO' should display as 'qaz (WO)'");
    is(isc.getDictionaryDisplayName("qaz-WO-fonipa"), "qaz (WO) (fonipa)", "'qaz-WO-fonipa' should display as 'qaz (WO) (fonipa)'");
    is(isc.getDictionaryDisplayName("qaz-WO-qxqaaaaz"), "qaz (WO) (qxqaaaaz)", "'qaz-WO-qxqaaaaz' should display as 'qaz (WO) (qxqaaaaz)'");

    // Check invalid but well-formed language subtag and valid script subtag.
    todo_is(isc.getDictionaryDisplayName("qaz-Cyrl"), "qaz / Cyrillic", "'qaz-Cyrl' should display as 'qaz / Cyrillic'");
    todo_is(isc.getDictionaryDisplayName("qaz-Cyrl-fonipa"), "qaz / Cyrillic (fonipa)", "'qaz-Cyrl-fonipa' should display as 'qaz / Cyrillic (fonipa)'");
    todo_is(isc.getDictionaryDisplayName("qaz-Cyrl-qxqaaaaz"), "qaz / Cyrillic (qxqaaaaz)", "'qaz-Cyrl-qxqaaaaz' should display as 'qaz / Cyrillic (qxqaaaaz)'");
    todo_is(isc.getDictionaryDisplayName("qaz-Cyrl-US"), "qaz (United States) / Cyrillic", "'qaz-Cyrl-US' should display as 'qaz (United States) / Cyrillic'");
    todo_is(isc.getDictionaryDisplayName("qaz-Cyrl-US-fonipa"), "qaz (United States) / Cyrillic (fonipa)", "'qaz-Cyrl-US-fonipa' should display as 'qaz (United States) / Cyrillic (fonipa)'");
    todo_is(isc.getDictionaryDisplayName("qaz-Cyrl-US-qxqaaaaz"), "qaz (United States) / Cyrillic (qxqaaaaz)", "'qaz-Cyrl-US-qxqaaaaz' should display as 'qaz (United States) / Cyrillic (qxqaaaaz)'");
    todo_is(isc.getDictionaryDisplayName("qaz-Cyrl-WO"), "qaz (WO) / Cyrillic", "'qaz-Cyrl-WO' should display as 'qaz (WO) / Cyrillic'");
    todo_is(isc.getDictionaryDisplayName("qaz-Cyrl-WO-fonipa"), "qaz (WO) / Cyrillic (fonipa)", "'qaz-Cyrl-WO-fonipa' should display as 'qaz (WO) / Cyrillic (fonipa)'");
    todo_is(isc.getDictionaryDisplayName("qaz-Cyrl-WO-qxqaaaaz"), "qaz (WO) / Cyrillic (qxqaaaaz)", "'qaz-Cyrl-WO-qxqaaaaz' should display as 'qaz (WO) / Cyrillic (qxqaaaaz)'");

    // Check invalid but well-formed language subtag and invalid but well-formed script subtag.
    is(isc.getDictionaryDisplayName("qaz-Qaaz"), "qaz / Qaaz", "'qaz-Qaaz' should display as 'qaz / Qaaz'");
    is(isc.getDictionaryDisplayName("qaz-Qaaz-fonipa"), "qaz / Qaaz (fonipa)", "'qaz-Qaaz-fonipa' should display as 'qaz / Qaaz (fonipa)'");
    is(isc.getDictionaryDisplayName("qaz-Qaaz-qxqaaaaz"), "qaz / Qaaz (qxqaaaaz)", "'qaz-Qaaz-qxqaaaaz' should display as 'qaz / Qaaz (qxqaaaaz)'");
    is(isc.getDictionaryDisplayName("qaz-Qaaz-US"), "qaz (United States) / Qaaz", "'qaz-Qaaz-US' should display as 'qaz (United States) / Qaaz'");
    is(isc.getDictionaryDisplayName("qaz-Qaaz-US-fonipa"), "qaz (United States) / Qaaz (fonipa)", "'qaz-Qaaz-US-fonipa' should display as 'qaz (United States) / Qaaz (fonipa)'");
    is(isc.getDictionaryDisplayName("qaz-Qaaz-US-qxqaaaaz"), "qaz (United States) / Qaaz (qxqaaaaz)", "'qaz-Qaaz-US-qxqaaaaz' should display as 'qaz (United States) / Qaaz (qxqaaaaz)'");
    is(isc.getDictionaryDisplayName("qaz-Qaaz-WO"), "qaz (WO) / Qaaz", "'qaz-Qaaz-WO' should display as 'qaz (WO) / Qaaz'");
    is(isc.getDictionaryDisplayName("qaz-Qaaz-WO-fonipa"), "qaz (WO) / Qaaz (fonipa)", "'qaz-Qaaz-WO-fonipa' should display as 'qaz (WO) / Qaaz (fonipa)'");
    is(isc.getDictionaryDisplayName("qaz-Qaaz-WO-qxqaaaaz"), "qaz (WO) / Qaaz (qxqaaaaz)", "'qaz-Qaaz-WO-qxqaaaaz' should display as 'qaz (WO) / Qaaz (qxqaaaaz)'");

    // Check multiple variant subtags.
    todo_is(isc.getDictionaryDisplayName("en-Cyrl-US-fonipa-fonxsamp"), "English (United States) / Cyrillic (fonipa / fonxsamp)", "'en-Cyrl-US-fonipa-fonxsamp' should display as 'English (United States) / Cyrillic (fonipa / fonxsamp)'");
    todo_is(isc.getDictionaryDisplayName("en-Cyrl-US-fonipa-qxqaaaaz"), "English (United States) / Cyrillic (fonipa / qxqaaaaz)", "'en-Cyrl-US-fonipa-qxqaaaaz' should display as 'English (United States) / Cyrillic (fonipa / qxqaaaaz)'");
    todo_is(isc.getDictionaryDisplayName("en-Cyrl-US-fonipa-fonxsamp-qxqaaaaz"), "English (United States) / Cyrillic (fonipa / fonxsamp / qxqaaaaz)", "'en-Cyrl-US-fonipa-fonxsamp-qxqaaaaz' should display as 'English (United States) / Cyrillic (fonipa / fonxsamp / qxqaaaaz)'");
    is(isc.getDictionaryDisplayName("qaz-Qaaz-WO-fonipa-fonxsamp"), "qaz (WO) / Qaaz (fonipa / fonxsamp)", "'qaz-Qaaz-WO-fonipa-fonxsamp' should display as 'qaz (WO) / Qaaz (fonipa / fonxsamp)'");
    is(isc.getDictionaryDisplayName("qaz-Qaaz-WO-fonipa-qxqaaaaz"), "qaz (WO) / Qaaz (fonipa / qxqaaaaz)", "'qaz-Qaaz-WO-fonipa-qxqaaaaz' should display as 'qaz (WO) / Qaaz (fonipa / qxqaaaaz)'");
    is(isc.getDictionaryDisplayName("qaz-Qaaz-WO-fonipa-fonxsamp-qxqaaaaz"), "qaz (WO) / Qaaz (fonipa / fonxsamp / qxqaaaaz)", "'qaz-Qaaz-WO-fonipa-fonxsamp-qxqaaaaz' should display as 'qaz (WO) / Qaaz (fonipa / fonxsamp / qxqaaaaz)'");

    // Check numeric region subtag.
    todo_is(isc.getDictionaryDisplayName("es-419"), "Spanish (Latin America and the Caribbean)", "'es-419' should display as 'Spanish (Latin America and the Caribbean)'");

    // Check that extension subtags are ignored.
    todo_is(isc.getDictionaryDisplayName("en-Cyrl-t-en-latn-m0-ungegn-2007"), "English / Cyrillic", "'en-Cyrl-t-en-latn-m0-ungegn-2007' should display as 'English / Cyrillic'");

    // Check that privateuse subtags are ignored.
    is(isc.getDictionaryDisplayName("en-x-ignore"), "English", "'en-x-ignore' should display as 'English'");
    is(isc.getDictionaryDisplayName("en-x-ignore-this"), "English", "'en-x-ignore-this' should display as 'English'");
    is(isc.getDictionaryDisplayName("en-x-ignore-this-subtag"), "English", "'en-x-ignore-this-subtag' should display as 'English'");

    // Check that both extension and privateuse subtags are ignored.
    todo_is(isc.getDictionaryDisplayName("en-Cyrl-t-en-latn-m0-ungegn-2007-x-ignore-this-subtag"), "English / Cyrillic", "'en-Cyrl-t-en-latn-m0-ungegn-2007-x-ignore-this-subtag' should display as 'English / Cyrillic'");

    // XXX: Check grandfathered tags.
  },
};
