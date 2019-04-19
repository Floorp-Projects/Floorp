/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Ensure all the engines defined in list.json are valid by
// creating a new list.json that contains every engine and
// loading them all.

"use strict";

Cu.importGlobalProperties(["fetch"]);

const {SearchService} = ChromeUtils.import("resource://gre/modules/SearchService.jsm");
const LIST_JSON_URL = "resource://search-extensions/list.json";

function traverse(obj, fun) {
  for (var i in obj) {
    fun.apply(this, [i, obj[i]]);
    if (obj[i] !== null && typeof(obj[i]) == "object") {
      traverse(obj[i], fun);
    }
  }
}

const ss = new SearchService();

add_task(async function test_validate_engines() {
  let engines = await fetch(LIST_JSON_URL).then(req => req.json());

  let visibleDefaultEngines = new Set();
  traverse(engines, (key, val) => {
    if (key === "visibleDefaultEngines") {
      val.forEach(engine => visibleDefaultEngines.add(engine));
    }
  });

  let listjson = {default: {
    visibleDefaultEngines: Array.from(visibleDefaultEngines),
  }};
  ss._listJSONURL = "data:application/json," + JSON.stringify(listjson);

  await AddonTestUtils.promiseStartupManager();
  await ss.init();
});
