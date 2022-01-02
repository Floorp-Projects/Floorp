/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//
//  HTTP Accept-Language header test
//

"use strict";

var testpath = "/bug672448";

function run_test() {
  let intlPrefs = Services.prefs.getBranch("intl.");

  // Save old value of preference for later.
  let oldPref = intlPrefs.getCharPref("accept_languages");

  // Test different numbers of languages, to test different fractions.
  let acceptLangTests = [
    "qaa", // 1
    "qaa,qab", // 2
    "qaa,qab,qac,qad", // 4
    "qaa,qab,qac,qad,qae,qaf,qag,qah", // 8
    "qaa,qab,qac,qad,qae,qaf,qag,qah,qai,qaj", // 10
    "qaa,qab,qac,qad,qae,qaf,qag,qah,qai,qaj,qak", // 11
    "qaa,qab,qac,qad,qae,qaf,qag,qah,qai,qaj,qak,qal,qam,qan,qao,qap,qaq,qar,qas,qat,qau", // 21
    oldPref, // Restore old value of preference (and test it).
  ];

  let acceptLangTestsNum = acceptLangTests.length;

  for (let i = 0; i < acceptLangTestsNum; i++) {
    // Set preference to test value.
    intlPrefs.setCharPref("accept_languages", acceptLangTests[i]);

    // Test value.
    test_accepted_languages();
  }
}

function test_accepted_languages() {
  let channel = setupChannel(testpath);

  let AcceptLanguage = channel.getRequestHeader("Accept-Language");

  let acceptedLanguages = AcceptLanguage.split(",");

  let acceptedLanguagesLength = acceptedLanguages.length;

  for (let i = 0; i < acceptedLanguagesLength; i++) {
    let qualityValue;

    try {
      // The q-value must conform to the definition in HTTP/1.1 Section 3.9.
      [, , qualityValue] = acceptedLanguages[i]
        .trim()
        .match(/^([a-z0-9_-]*?)(?:;q=(1(?:\.0{0,3})?|0(?:\.[0-9]{0,3})))?$/i);
    } catch (e) {
      do_throw("Invalid language tag or quality value: " + e);
    }

    if (i == 0) {
      // The first language shouldn't have a quality value.
      Assert.equal(qualityValue, undefined);
    } else {
      let decimalPlaces;

      // When the number of languages is small, we keep the quality value to only one decimal place.
      // Otherwise, it can be up to two decimal places.
      if (acceptedLanguagesLength < 10) {
        Assert.ok(qualityValue.length == 3);

        decimalPlaces = 1;
      } else {
        Assert.ok(qualityValue.length >= 3);
        Assert.ok(qualityValue.length <= 4);

        decimalPlaces = 2;
      }

      // All the other languages should have an evenly-spaced quality value.
      Assert.equal(
        parseFloat(qualityValue).toFixed(decimalPlaces),
        (1.0 - (1 / acceptedLanguagesLength) * i).toFixed(decimalPlaces)
      );
    }
  }
}

function setupChannel(path) {
  let chan = NetUtil.newChannel({
    uri: "http://localhost:4444" + path,
    loadUsingSystemPrincipal: true,
  });

  chan.QueryInterface(Ci.nsIHttpChannel);
  return chan;
}
