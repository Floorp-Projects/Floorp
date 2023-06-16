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

const {
  SourceMapLoader,
} = require("resource://devtools/client/shared/source-map-loader/index.js");

module.exports = async function () {
  await testSetup("data:text/html,source-map");
  const sourceMapLoader = new SourceMapLoader();

  const fakeGeneratedSource = {
    id: "fake-id",
    url: "http://example.com/tests/devtools/addon/content/tests/source-map/angular-min.js",
    sourceMapBaseURL:
      "http://example.com/tests/devtools/addon/content/tests/source-map/",
    sourceMapURL: "angular-min.js.map",
  };

  {
    const test = runTest(`source-map-loader.init.DAMP`);
    await sourceMapLoader.getOriginalURLs(fakeGeneratedSource);
    test.done();
  }

  {
    const test = runTest(`source-map-loader.getOriginalLocation.DAMP`);
    for (let x = 0; x < 1000; x++) {
      await sourceMapLoader.getOriginalLocation({
        sourceId: fakeGeneratedSource.id,
        line: 10,
        column: 0,
        sourceUrl: fakeGeneratedSource.url,
      });
    }
    test.done();
  }
  sourceMapLoader.clearSourceMaps();

  await testTeardown();
};
