// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { utils: Cu, interfaces: Ci } = Components;

Cu.import("resource://gre/modules/Services.jsm");

/**
 * If we ever start running tests with a different default OS locale,
 * this test will break.
 */

function getOSLocale() {
  try {
    return Services.prefs.getCharPref("intl.locale.os");
  } catch (ex) {
    return null;
  }
}

function getAcceptLanguages() {
  return Services.prefs.getComplexValue("intl.accept_languages",
                                        Ci.nsIPrefLocalizedString).data;
}

/**
 * Ensure that by the time we come to run, the OS locale
 * has been sent over the wire and persisted in prefs.
 */
add_test(function test_OSLocale() {
  let osObserver = function () {
    Services.prefs.removeObserver("intl.locale.os", osObserver);
    do_check_eq(getOSLocale(), "en-US");
    run_next_test();
  };

  let osLocale = getOSLocale();
  if (osLocale) {
    // Proceed with the test.
    run_next_test();
    return;
  }

  // Otherwise, wait for it. We're probably running before the message has
  // been handled.
  Services.prefs.addObserver("intl.locale.os", osObserver, false);
});

add_test(function test_AcceptLanguages() {
  // Fake that we have an app locale, and fake
  // a different OS locale to make it interesting.

  // The correct set here depends on whether the
  // browser was built with multiple locales or not.
  // This is exasperating, but hey.

  // Expected, from es-ES's intl.properties:
  const ES_ES_EXPECTED_BASE = "es-ES,es,en-US,en";

  // Expected, from en-US (the default).
  const MONO_EXPECTED_BASE = "en-US,en";

  // This will always be the same: it's deduced from our language setting and
  // the fake OS.
  const SELECTED_LOCALES = "es-es,fr,";

  // And so the two expected results:
  const ES_ES_EXPECTED = SELECTED_LOCALES +
                         "es,en-us,en";      // Remainder of ES_ES_EXPECTED_BASE.
  const MONO_EXPECTED = SELECTED_LOCALES +
                        "en-us,en";          // Remainder of MONO_EXPECTED_BASE.

  let observer = function () {
    Services.prefs.removeObserver("intl.accept_languages", observer);

    // And so intl.accept_languages should be one of these two...
    let acc = getAcceptLanguages();
    let is_es = ES_ES_EXPECTED == acc;
    let is_en = MONO_EXPECTED == acc;

    do_check_true(is_es || is_en);
    do_check_true(is_es != is_en);
    run_next_test();
  };

  Services.prefs.setCharPref("intl.locale.os", "fr");
  Services.prefs.addObserver("intl.accept_languages", observer, false);
  Services.obs.notifyObservers(null, "Locale:Changed", "es-ES");
});

run_next_test();
