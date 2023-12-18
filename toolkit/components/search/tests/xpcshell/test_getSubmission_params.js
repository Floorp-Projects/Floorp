/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(async function () {
  await SearchTestUtils.useTestEngines("simple-engines");
  await AddonTestUtils.promiseStartupManager();
  await Services.search.init();
});

const searchTerms = "fxsearch";
function checkSubstitution(url, prefix, engine, template, expected) {
  url.template = prefix + template;
  equal(engine.getSubmission(searchTerms).uri.spec, prefix + expected);
}

add_task(async function test_paramSubstitution() {
  let prefix = "https://example.com/?sourceId=Mozilla-search&search=";
  let engine = await Services.search.getEngineByName("Simple Engine");
  let url = engine.wrappedJSObject._getURLOfType("text/html");
  equal(url.getSubmission("foo", engine).uri.spec, prefix + "foo");
  // Reset the engine parameters so we can have a clean template to use for
  // the subsequent tests.
  url.params = [];

  let check = checkSubstitution.bind(this, url, prefix, engine);

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

  check("{moz:locale}", Services.locale.requestedLocale);
});

add_task(async function test_mozParamsFailForNonAppProvided() {
  await SearchTestUtils.installSearchExtension();

  let prefix = "https://example.com/?q=";
  let engine = await Services.search.getEngineByName("Example");
  let url = engine.wrappedJSObject._getURLOfType("text/html");
  equal(url.getSubmission("foo", engine).uri.spec, prefix + "foo");
  // Reset the engine parameters so we can have a clean template to use for
  // the subsequent tests.
  url.params = [];

  let check = checkSubstitution.bind(this, url, prefix, engine);

  // Test moz: parameters (only supported for built-in engines, ie _isDefault == true).
  check("{moz:locale}", "{moz:locale}");

  await promiseAfterSettings();
});
