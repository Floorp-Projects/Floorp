/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_big_icon() {
  let srv = useHttpServer();
  srv.registerContentType("ico", "image/x-icon");
  await AddonTestUtils.promiseStartupManager();
  await Services.search.init();

  let promiseChanged = TestUtils.topicObserved("browser-search-engine-modified",
    (engine, verb) => {
      engine.QueryInterface(Ci.nsISearchEngine);
      return verb == "engine-changed" && engine.name == "BigIcon" && engine.iconURI;
    });

  let iconUrl = gDataUrl + "big_icon.ico";
  await addTestEngines([
    { name: "BigIcon",
      details: [iconUrl, "", "Big icon", "GET",
                "http://test_big_icon/search?q={searchTerms}"] },
  ]);

  await promiseAfterCache();

  let [engine] = await promiseChanged;
  engine.QueryInterface(Ci.nsISearchEngine);
  Assert.ok(engine.iconURI.spec.startsWith("data:image/png"),
            "The icon is saved as a PNG data url");
});
