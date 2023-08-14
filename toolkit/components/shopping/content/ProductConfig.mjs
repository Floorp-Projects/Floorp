/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Really const but needs to be overridable for tests.
let ANALYSIS_API = "https://staging.trustwerty.com/api/v1/fx/analysis";
const ANALYSIS_RESPONSE_SCHEMA =
  "chrome://global/content/shopping/analysis_response.schema.json";
const ANALYSIS_REQUEST_SCHEMA =
  "chrome://global/content/shopping/analysis_request.schema.json";

// Really const but needs to be overridable for tests.
let RECOMMENDATIONS_API =
  "https://staging-affiliates.fakespot.io/v1/fx/sp_search";
const RECOMMENDATIONS_RESPONSE_SCHEMA =
  "chrome://global/content/shopping/recommendations_response.schema.json";
const RECOMMENDATIONS_REQUEST_SCHEMA =
  "chrome://global/content/shopping/recommendations_request.schema.json";
const ATTRIBUTION_API = "https://staging-partner-ads.fakespot.io";
const ATTRIBUTION_RESPONSE_SCHEMA =
  "chrome://global/content/shopping/attribution_response.schema.json";
const ATTRIBUTION_REQUEST_SCHEMA =
  "chrome://global/content/shopping/attribution_request.schema.json";

const ProductConfig = {
  amazon: {
    productIdFromURLRegex:
      /(?:[\/]|$|%2F)(?<productId>[A-Z0-9]{10})(?:[\/]|$|\#|\?|%2F)/,
    validTLDs: ["com"],
  },
  walmart: {
    productIdFromURLRegex:
      /\/ip\/(?:[A-Za-z0-9-]{1,320}\/)?(?<productId>[0-9]{3,13})/,
    validTLDs: ["com"],
  },
  bestbuy: {
    productIdFromURLRegex: /\/(?<productId>\d+\.p)/,
    validTLDs: ["com"],
  },
};

if (Cu.isInAutomation) {
  // Also allow example.com to allow for testing.
  ProductConfig.example = ProductConfig.amazon;
  ANALYSIS_API =
    "https://example.com/browser/toolkit/components/shopping/test/browser/analysis.sjs";
  RECOMMENDATIONS_API =
    "https://example.com/browser/toolkit/components/shopping/test/browser/sp_search.sjs";
}

Object.freeze(ProductConfig);

export {
  ANALYSIS_API,
  ANALYSIS_RESPONSE_SCHEMA,
  ANALYSIS_REQUEST_SCHEMA,
  RECOMMENDATIONS_API,
  RECOMMENDATIONS_RESPONSE_SCHEMA,
  RECOMMENDATIONS_REQUEST_SCHEMA,
  ATTRIBUTION_API,
  ATTRIBUTION_RESPONSE_SCHEMA,
  ATTRIBUTION_REQUEST_SCHEMA,
  ProductConfig,
};
