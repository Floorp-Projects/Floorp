/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { testSetup, testTeardown, runTest } = require("damp-test/tests/head");

const sourceMap = require("devtools/client/shared/vendor/source-map/source-map");

module.exports = async function () {
  await testSetup("data:text/html,source-map");

  sourceMap.SourceMapConsumer.initialize({
    "lib/mappings.wasm":
      "resource://devtools/client/shared/vendor/source-map/lib/mappings.wasm",
  });

  // source-map library only accepts the sourcemap JSON object
  const testSourceMapString = require("raw!damp-test/tests/source-map/angular-min.js.map");
  const testSourceMap = JSON.parse(testSourceMapString);
  const lines = testSourceMap.mappings;

  // Track performance of SourceMapConsumer construction, one location mapping and destruction
  {
    const test = runTest(`source-map.simple.DAMP`);
    for (let x = 0; x < 30; x++) {
      const consumer = await new sourceMap.SourceMapConsumer(testSourceMap);
      consumer.originalPositionFor({ line: 1, column: 0 });
      consumer.destroy();
    }
    test.done();
  }

  {
    const test = runTest(`source-map.constructor.DAMP`);
    for (let x = 0; x < 100; x++) {
      const consumer = await new sourceMap.SourceMapConsumer(testSourceMap);
      consumer.destroy();
    }
    test.done();
  }

  const testMapping = await getTestMapping(testSourceMap);
  const consumer = await new sourceMap.SourceMapConsumer(testSourceMap);

  {
    const test = runTest(`source-map.originalPositionFor.DAMP`);
    for (let x = 0; x < 200000; x++) {
      const i = Math.floor(Math.random() * lines.length);
      const line = lines[i];
      const j = Math.floor(Math.random() * line.length);
      const column = line[j][0];

      consumer.originalPositionFor({ line: i + 1, column });
    }
    test.done();
  }

  {
    const test = runTest(`source-map.allGeneratedPositionsFor.DAMP`);
    for (let x = 0; x < 20000; x++) {
      consumer.allGeneratedPositionsFor({
        source: testMapping.source,
        line: testMapping.originalLine,
      });
    }
    test.done();
  }

  {
    const test = runTest(`source-map.eachMapping.DAMP`);
    for (let x = 0; x < 5; x++) {
      let maxLine = 0;
      let maxCol = 0;
      consumer.eachMapping(m => {
        maxLine = Math.max(maxLine, m.generatedLine);
        maxLine = Math.max(maxLine, m.originalLine);
        maxCol = Math.max(maxCol, m.generatedColumn);
        maxCol = Math.max(maxCol, m.originalColumn);
      });
    }
    test.done();
  }

  const generator = sourceMap.SourceMapGenerator.fromSourceMap(consumer);
  {
    const test = runTest(`source-map.SourceMapGenerator-toString.DAMP`);
    for (let x = 0; x < 2; x++) {
      generator.toString();
    }
    test.done();
  }

  consumer.destroy();

  await testTeardown();
};

async function getTestMapping(testSourceMap) {
  let smc = await new sourceMap.SourceMapConsumer(testSourceMap);

  let mappings = [];
  smc.eachMapping(
    [].push,
    mappings,
    sourceMap.SourceMapConsumer.ORIGINAL_ORDER
  );

  let testMapping = mappings[Math.floor(mappings.length / 13)];
  smc.destroy();
  return testMapping;
}
