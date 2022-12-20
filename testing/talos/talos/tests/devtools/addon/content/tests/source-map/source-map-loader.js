/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This test cover the performance of the "Source Map Loader".
 * The layer on top of source-map npm package, specific to mozilla-central/gecko
 * from devtools/client/shared/source-map-loader.
 */

const { testSetup, testTeardown, runTest } = require("damp-test/tests/head");

// source-map module has to be loaded via the browser loader in order to use the Worker API
const { BrowserLoader } = ChromeUtils.import(
  "resource://devtools/shared/loader/browser-loader.js"
);
const { require: browserRequire } = BrowserLoader({
  baseURI: "resource://devtools/client/shared/",
  window: dampWindow,
});
const sourceMap = browserRequire(
  "devtools/client/shared/source-map-loader/index"
);

module.exports = async function() {
  await testSetup("data:text/html,source-map");

  const fakeGeneratedSource = {
    id: "fake-id",
    url:
      "http://example.com/tests/devtools/addon/content/tests/source-map/angular-min.js",
    sourceMapBaseURL:
      "http://example.com/tests/devtools/addon/content/tests/source-map/",
    sourceMapURL: "angular-min.js.map",
  };

  {
    const test = runTest(`source-map-loader.init.DAMP`);
    await sourceMap.getOriginalURLs(fakeGeneratedSource);
    test.done();
  }

  {
    const test = runTest(`source-map-loader.getOriginalLocation.DAMP`);
    for (let x = 0; x < 1000; x++) {
      await sourceMap.getOriginalLocation({
        sourceId: fakeGeneratedSource.id,
        line: 10,
        column: 0,
        sourceUrl: fakeGeneratedSource.url,
      });
    }
    test.done();
  }
  sourceMap.clearSourceMaps();

  await testTeardown();
};
