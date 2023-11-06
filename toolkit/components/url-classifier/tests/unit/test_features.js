/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/
*/

"use strict";

add_test(async _ => {
  ok(
    Services.cookies,
    "Force the cookie service to be initialized to avoid issues later. " +
      "See https://bugzilla.mozilla.org/show_bug.cgi?id=1621759#c3"
  );

  let classifier = Cc["@mozilla.org/url-classifier/dbservice;1"].getService(
    Ci.nsIURIClassifier
  );
  ok(!!classifier, "We have the URI-Classifier");

  var tests = [
    { name: "a", expectedResult: false },
    { name: "tracking-annotation", expectedResult: true },
    { name: "tracking-protection", expectedResult: true },
  ];

  tests.forEach(test => {
    let feature;
    try {
      feature = classifier.getFeatureByName(test.name);
    } catch (e) {}

    equal(
      !!feature,
      test.expectedResult,
      "Exceptected result for: " + test.name
    );
    if (feature) {
      equal(feature.name, test.name, "Feature name matches");
    }
  });

  let uri = Services.io.newURI("https://example.com");

  let feature = classifier.getFeatureByName("tracking-protection");

  let results = await new Promise(resolve => {
    classifier.asyncClassifyLocalWithFeatures(
      uri,
      [feature],
      Ci.nsIUrlClassifierFeature.blocklist,
      r => {
        resolve(r);
      }
    );
  });
  equal(results.length, 0, "No tracker");

  Services.prefs.setCharPref(
    "urlclassifier.trackingTable.testEntries",
    "example.com"
  );

  feature = classifier.getFeatureByName("tracking-protection");

  results = await new Promise(resolve => {
    classifier.asyncClassifyLocalWithFeatures(
      uri,
      [feature],
      Ci.nsIUrlClassifierFeature.blocklist,
      r => {
        resolve(r);
      }
    );
  });
  equal(results.length, 1, "Tracker");
  let result = results[0];
  equal(result.feature.name, "tracking-protection", "Correct feature");
  equal(result.list, "tracking-blocklist-pref", "Correct list");

  Services.prefs.clearUserPref("browser.safebrowsing.password.enabled");
  run_next_test();
});
