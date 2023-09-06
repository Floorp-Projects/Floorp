/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Really const but needs to be overridable for tests.
let ANALYSIS_API = "https://trustwerty.com/api/v1/fx/analysis";
const ANALYSIS_RESPONSE_SCHEMA =
  "chrome://global/content/shopping/analysis_response.schema.json";
const ANALYSIS_REQUEST_SCHEMA =
  "chrome://global/content/shopping/analysis_request.schema.json";

// Really const but needs to be overridable for tests.
let RECOMMENDATIONS_API = "https://a.fakespot.com/v1/fx/sp_search";
const RECOMMENDATIONS_RESPONSE_SCHEMA =
  "chrome://global/content/shopping/recommendations_response.schema.json";
const RECOMMENDATIONS_REQUEST_SCHEMA =
  "chrome://global/content/shopping/recommendations_request.schema.json";
let ATTRIBUTION_API = "https://pe.fakespot.com/api/v1/fx/events";
const ATTRIBUTION_RESPONSE_SCHEMA =
  "chrome://global/content/shopping/attribution_response.schema.json";
const ATTRIBUTION_REQUEST_SCHEMA =
  "chrome://global/content/shopping/attribution_request.schema.json";

let REPORTING_API = "https://trustwerty.com/api/v1/fx/report";
const REPORTING_RESPONSE_SCHEMA =
  "chrome://global/content/shopping/reporting_response.schema.json";
const REPORTING_REQUEST_SCHEMA =
  "chrome://global/content/shopping/reporting_request.schema.json";

const FAKESPOT_BASE_URL = "https://www.fakespot.com/";
const FAKESPOT_ANALYSIS_URL = "https://www.fakespot.com/analyze?url=";

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

// Note (bug 1849401): the fakespot URLs are loaded by about page content,
// where `Cu` is undefined--hence the check here. Would be good to find a
// better approach.
if (typeof Cu !== "undefined" && Cu.isInAutomation) {
  // Also allow example.com to allow for testing.
  ProductConfig.example = ProductConfig.amazon;
  ANALYSIS_API =
    "https://example.com/browser/toolkit/components/shopping/test/browser/analysis.sjs";
  RECOMMENDATIONS_API =
    "https://example.com/browser/toolkit/components/shopping/test/browser/recommendations.sjs";
  ATTRIBUTION_API =
    "https://example.com/browser/toolkit/components/shopping/test/browser/attribution.sjs";
  REPORTING_API =
    "https://example.com/browser/toolkit/components/shopping/test/browser/reporting.sjs";
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
  REPORTING_API,
  REPORTING_RESPONSE_SCHEMA,
  REPORTING_REQUEST_SCHEMA,
  FAKESPOT_BASE_URL,
  FAKESPOT_ANALYSIS_URL,
  ProductConfig,
};
