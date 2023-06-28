/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
/* global loadJSONfromFile */

const { ANALYSIS_SCHEMA, RECOMMENDATIONS_SCHEMA } = ChromeUtils.importESModule(
  "chrome://global/content/shopping/ProductConfig.mjs"
);

const { ProductValidator } = ChromeUtils.importESModule(
  "chrome://global/content/shopping/ProductValidator.sys.mjs"
);

add_task(async function test_validate_analysis() {
  const json = await loadJSONfromFile("data/analysis.json");
  let valid = await ProductValidator.validate(json, ANALYSIS_SCHEMA);

  Assert.equal(valid, true, "Analysis JSON is valid");
});

add_task(async function test_validate_analysis_invalid() {
  const json = await loadJSONfromFile("data/invalid_analysis.json");
  let valid = await ProductValidator.validate(json, ANALYSIS_SCHEMA);

  Assert.equal(valid, false, "Analysis JSON is invalid");
});

add_task(async function test_validate_recommendations() {
  const json = await loadJSONfromFile("data/recommendations.json");
  let valid = await ProductValidator.validate(json, RECOMMENDATIONS_SCHEMA);

  Assert.equal(valid, true, "Recommendations JSON is valid");
});

add_task(async function test_validate_recommendations_invalid() {
  const json = await loadJSONfromFile("data/invalid_recommendations.json");
  let valid = await ProductValidator.validate(json, RECOMMENDATIONS_SCHEMA);

  Assert.equal(valid, false, "Recommendations JSON is invalid");
});
