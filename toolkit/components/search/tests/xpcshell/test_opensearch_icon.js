/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(async function () {
  let server = useHttpServer();
  server.registerContentType("sjs", "sjs");
  await AddonTestUtils.promiseStartupManager();
  await Services.search.init();
});

const ICON_TESTS = [
  {
    name: "Big Icon",
    image: "bigIcon.ico",
    expected: "data:image/png;base64,",
  },
  {
    name: "Remote Icon",
    image: "remoteIcon.ico",
    expected: "data:image/x-icon;base64,",
  },
  {
    name: "SVG Icon",
    image: "svgIcon.svg",
    expected: "data:image/svg+xml;base64,",
  },
];

add_task(async function test_icon_types() {
  for (let test of ICON_TESTS) {
    info(`Testing ${test.name}`);

    let promiseEngineAdded = SearchTestUtils.promiseSearchNotification(
      SearchUtils.MODIFIED_TYPE.ADDED,
      SearchUtils.TOPIC_ENGINE_MODIFIED
    );
    let promiseEngineChanged = SearchTestUtils.promiseSearchNotification(
      SearchUtils.MODIFIED_TYPE.CHANGED,
      SearchUtils.TOPIC_ENGINE_MODIFIED
    );
    const engineData = {
      baseURL: gDataUrl,
      image: test.image,
      name: test.name,
      method: "GET",
    };
    // The easiest way to test adding the icon is via a generated xml, otherwise
    // we have to somehow insert the address of the server into it.
    SearchTestUtils.promiseNewSearchEngine({
      url: `${gDataUrl}engineMaker.sjs?${JSON.stringify(engineData)}`,
    });
    let engine = await promiseEngineAdded;
    // Ensure this is a nsISearchEngine.
    engine.QueryInterface(Ci.nsISearchEngine);
    await promiseEngineChanged;

    Assert.ok(engine.getIconURL(), `${test.name} engine has an icon`);
    Assert.ok(
      engine.getIconURL().startsWith(test.expected),
      `${test.name} iconURI starts with the expected information`
    );
  }
});

add_task(async function test_multiple_icons_in_file() {
  let engine = await SearchTestUtils.promiseNewSearchEngine({
    url: `${gDataUrl}engineImages.xml`,
  });

  Assert.ok(engine.getIconURL().includes("ico16"));
  Assert.ok(engine.getIconURL(16).includes("ico16"));
  Assert.ok(engine.getIconURL(32).includes("ico32"));
  Assert.ok(engine.getIconURL(74).includes("ico74"));

  info("Invalid dimensions should return null until bug 1655070 is fixed.");
  Assert.equal(null, engine.getIconURL(50));
});

add_task(async function test_icon_not_in_opensearch_file() {
  let engineUrl = gDataUrl + "engine-fr.xml";
  let engine = await Services.search.addOpenSearchEngine(
    engineUrl,
    "data:image/x-icon;base64,ico16"
  );

  // Even though the icon wasn't specified inside the XML file, it should be
  // available both in the iconURI attribute and with getIconURLBySize.
  Assert.ok(engine.getIconURL(16).includes("ico16"));
});
