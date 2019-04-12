/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_paramSubstitution() {
  useHttpServer();

  await AddonTestUtils.promiseStartupManager();
  await Services.search.init();

  let prefix = "http://test.moz/search?q=";
  let [engine] = await addTestEngines([
    { name: "test", details: ["", "test", "Search Test", "GET",
                              prefix + "{searchTerms}"] },
  ]);
  let url = engine.wrappedJSObject._getURLOfType("text/html");
  equal(url.template, prefix + "{searchTerms}");

  let searchTerms = "fxsearch";
  function check(template, expected) {
    url.template = prefix + template;
    equal(engine.getSubmission(searchTerms).uri.spec, prefix + expected);
  }

  // The same parameter can be used more than once.
  check("{searchTerms}/{searchTerms}", searchTerms + "/" + searchTerms);

  // Optional parameters are replaced if we known them.
  check("{searchTerms?}", searchTerms);
  check("{unknownOptional?}", "");
  check("{unknownRequired}", "{unknownRequired}");

  check("{language}", Services.locale.requestedLocale);
  check("{language?}", Services.locale.requestedLocale);

  engine.wrappedJSObject._queryCharset = "UTF-8";
  check("{inputEncoding}", "UTF-8");
  check("{inputEncoding?}", "UTF-8");
  check("{outputEncoding}", "UTF-8");
  check("{outputEncoding?}", "UTF-8");

  // 'Unsupported' parameters with hard coded values used only when the parameter is required.
  check("{count}", "20");
  check("{count?}", "");
  check("{startIndex}", "1");
  check("{startIndex?}", "");
  check("{startPage}", "1");
  check("{startPage?}", "");

  // Test moz: parameters (only supported for built-in engines, ie _isDefault == true).
  check("{moz:distributionID}", "{moz:distributionID}");
  check("{moz:official}", "{moz:official}");
  check("{moz:locale}", "{moz:locale}");
  engine.wrappedJSObject._loadPath = "[app]"; // This will make _isDefault return true;
  check("{moz:distributionID}", "");
  Services.prefs.setCharPref("browser.search.distributionID", "xpcshell");
  check("{moz:distributionID}", "xpcshell");
  Services.prefs.setBoolPref("browser.search.official", true);
  check("{moz:official}", "official");
  Services.prefs.setBoolPref("browser.search.official", false);
  check("{moz:official}", "unofficial");
  check("{moz:locale}", Services.locale.requestedLocale);
});
