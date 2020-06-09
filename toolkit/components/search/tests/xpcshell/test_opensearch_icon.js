/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function setup() {
  let server = useHttpServer();
  server.registerContentType("sjs", "sjs");
  await AddonTestUtils.promiseStartupManager();
  await Services.search.init();
});

const tests = [
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

for (const test of tests) {
  add_task(async function() {
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
    addTestEngines([
      {
        name: test.name,
        xmlFileName: "engineMaker.sjs?" + JSON.stringify(engineData),
      },
    ]);
    let engine = await promiseEngineAdded;
    await promiseEngineChanged;

    Assert.ok(engine.iconURI, "the engine has an icon");
    Assert.ok(
      engine.iconURI.spec.startsWith(test.expected),
      "the icon is saved as an x-icon data url"
    );
  });
}
