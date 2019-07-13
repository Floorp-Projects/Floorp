/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Ensure all the engines defined in list.json are valid by
// creating a new list.json that contains every engine and
// loading them all.

"use strict";

Cu.importGlobalProperties(["fetch"]);

const { SearchUtils, SearchExtensionLoader } = ChromeUtils.import(
  "resource://gre/modules/SearchUtils.jsm"
);

function traverse(obj, fun) {
  for (var i in obj) {
    fun.apply(this, [i, obj[i]]);
    if (obj[i] !== null && typeof obj[i] == "object") {
      traverse(obj[i], fun);
    }
  }
}

add_task(async function setup() {
  // Read all the builtin engines and locales, create a giant list.json
  // that includes everything.
  let engines = await fetch(SearchUtils.LIST_JSON_URL).then(req => req.json());

  let visibleDefaultEngines = new Set();
  traverse(engines, (key, val) => {
    if (key === "visibleDefaultEngines") {
      val.forEach(engine => visibleDefaultEngines.add(engine));
    }
  });

  let listjson = {
    default: {
      visibleDefaultEngines: Array.from(visibleDefaultEngines),
    },
  };
  SearchUtils.LIST_JSON_URL =
    "data:application/json," + JSON.stringify(listjson);

  // Set strict so the addon install promise is rejected.  This causes
  // search.init to throw the error, and this test fails.
  SearchExtensionLoader._strict = true;
  await AddonTestUtils.promiseStartupManager();
});

add_task(async function test_validate_engines() {
  // All engines should parse and init should work fine.
  await Services.search
    .init()
    .then(() => {
      ok(true, "all engines parsed and loaded");
    })
    .catch(() => {
      ok(false, "an engine failed to parse and load");
    });
});

add_task(async function test_install_timeout_failure() {
  // Set an incredibly unachievable timeout here and make sure
  // that init throws.  We're loading every engine/locale combo under the
  // sun, it's unlikely we could intermittently succeed in loading
  // them all.
  Services.prefs.setIntPref("browser.search.addonLoadTimeout", 1);
  removeCacheFile();
  Services.search.reset();
  await Services.search
    .init()
    .then(() => {
      ok(false, "search init did not time out");
    })
    .catch(error => {
      equal(Cr.NS_ERROR_FAILURE, error, "search init timed out");
    });
});
