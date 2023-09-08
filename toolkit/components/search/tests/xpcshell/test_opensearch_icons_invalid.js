/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* Test that an installed engine can't use a resource URL for an icon */

"use strict";

add_task(async function setup() {
  let server = useHttpServer("");
  server.registerContentType("sjs", "sjs");
  await AddonTestUtils.promiseStartupManager();
});

add_task(async function test_installedresourceicon() {
  // Attempts to load a resource:// url as an icon.
  let engine1 = await SearchTestUtils.promiseNewSearchEngine({
    url: `${gDataUrl}opensearch/resourceicon.xml`,
  });
  // Attempts to load a chrome:// url as an icon.
  let engine2 = await SearchTestUtils.promiseNewSearchEngine({
    url: `${gDataUrl}opensearch/chromeicon.xml`,
  });

  Assert.equal(null, engine1.iconURI);
  Assert.equal(null, engine2.iconURI);
});

add_task(async function test_installedhttpplace() {
  let observed = TestUtils.consoleMessageObserved(msg => {
    return msg.wrappedJSObject.arguments[0].includes(
      "Content type does not match expected"
    );
  });

  // The easiest way to test adding the icon is via a generated xml, otherwise
  // we have to somehow insert the address of the server into it.
  // Attempts to load a non-image page into an image icon.
  let engine = await SearchTestUtils.promiseNewSearchEngine({
    url:
      `${gDataUrl}data/engineMaker.sjs?` +
      JSON.stringify({
        baseURL: gDataUrl,
        image: "head_search.js",
        name: "invalidicon",
        method: "GET",
      }),
  });

  await observed;

  Assert.equal(null, engine.iconURI, "Should not have set an iconURI");
});
