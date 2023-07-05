/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
/* global loadJSONfromFile */

const {
  ANALYSIS_RESPONSE_SCHEMA,
  ANALYSIS_REQUEST_SCHEMA,
  RECOMMENDATIONS_RESPONSE_SCHEMA,
  RECOMMENDATIONS_REQUEST_SCHEMA,
} = ChromeUtils.importESModule(
  "chrome://global/content/shopping/ProductConfig.mjs"
);

const { ProductValidator } = ChromeUtils.importESModule(
  "chrome://global/content/shopping/ProductValidator.sys.mjs"
);

add_task(async function test_validate_analysis() {
  const json = await loadJSONfromFile("data/analysis_response.json");
  let valid = await ProductValidator.validate(json, ANALYSIS_RESPONSE_SCHEMA);

  Assert.equal(valid, true, "Analysis JSON is valid");
});

add_task(async function test_validate_analysis_invalid() {
  const json = await loadJSONfromFile("data/invalid_analysis_response.json");
  let valid = await ProductValidator.validate(json, ANALYSIS_RESPONSE_SCHEMA);

  Assert.equal(valid, false, "Analysis JSON is invalid");
});

add_task(async function test_validate_recommendations() {
  const json = await loadJSONfromFile("data/recommendations_response.json");
  let valid = await ProductValidator.validate(
    json,
    RECOMMENDATIONS_RESPONSE_SCHEMA
  );

  Assert.equal(valid, true, "Recommendations JSON is valid");
});

add_task(async function test_validate_recommendations_invalid() {
  const json = await loadJSONfromFile(
    "data/invalid_recommendations_response.json"
  );
  let valid = await ProductValidator.validate(
    json,
    RECOMMENDATIONS_RESPONSE_SCHEMA
  );

  Assert.equal(valid, false, "Recommendations JSON is invalid");
});

add_task(async function test_validate_analysis() {
  const json = await loadJSONfromFile("data/analysis_request.json");
  let valid = await ProductValidator.validate(json, ANALYSIS_REQUEST_SCHEMA);

  Assert.equal(valid, true, "Analysis JSON is valid");
});

add_task(async function test_validate_analysis_invalid() {
  const json = await loadJSONfromFile("data/invalid_analysis_request.json");
  let valid = await ProductValidator.validate(json, ANALYSIS_REQUEST_SCHEMA);

  Assert.equal(valid, false, "Analysis JSON is invalid");
});

add_task(async function test_validate_recommendations() {
  const json = await loadJSONfromFile("data/recommendations_request.json");
  let valid = await ProductValidator.validate(
    json,
    RECOMMENDATIONS_REQUEST_SCHEMA
  );

  Assert.equal(valid, true, "Recommendations JSON is valid");
});

add_task(async function test_validate_recommendations_invalid() {
  const json = await loadJSONfromFile(
    "data/invalid_recommendations_request.json"
  );
  let valid = await ProductValidator.validate(
    json,
    RECOMMENDATIONS_REQUEST_SCHEMA
  );

  Assert.equal(valid, false, "Recommendations JSON is invalid");
});
