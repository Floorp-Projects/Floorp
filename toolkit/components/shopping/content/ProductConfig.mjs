/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

let { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const ANALYSIS_RESPONSE_SCHEMA =
  "chrome://global/content/shopping/analysis_response.schema.json";
const ANALYSIS_REQUEST_SCHEMA =
  "chrome://global/content/shopping/analysis_request.schema.json";

const ANALYZE_RESPONSE_SCHEMA =
  "chrome://global/content/shopping/analyze_response.schema.json";
const ANALYZE_REQUEST_SCHEMA =
  "chrome://global/content/shopping/analyze_request.schema.json";

const ANALYSIS_STATUS_RESPONSE_SCHEMA =
  "chrome://global/content/shopping/analysis_status_response.schema.json";
const ANALYSIS_STATUS_REQUEST_SCHEMA =
  "chrome://global/content/shopping/analysis_status_request.schema.json";

const RECOMMENDATIONS_RESPONSE_SCHEMA =
  "chrome://global/content/shopping/recommendations_response.schema.json";
const RECOMMENDATIONS_REQUEST_SCHEMA =
  "chrome://global/content/shopping/recommendations_request.schema.json";

const ATTRIBUTION_RESPONSE_SCHEMA =
  "chrome://global/content/shopping/attribution_response.schema.json";
const ATTRIBUTION_REQUEST_SCHEMA =
  "chrome://global/content/shopping/attribution_request.schema.json";

const REPORTING_RESPONSE_SCHEMA =
  "chrome://global/content/shopping/reporting_response.schema.json";
const REPORTING_REQUEST_SCHEMA =
  "chrome://global/content/shopping/reporting_request.schema.json";

// Allow switching to the stage or test environments by changing the
// "toolkit.shopping.environment" pref from "prod" to "stage" or "test".
const lazy = {};
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "env",
  "toolkit.shopping.environment",
  "prod",
  null,
  // If the pref is set to an unexpected string value, "prod" will be used.
  prefValue =>
    ["prod", "stage", "test"].includes(prefValue) ? prefValue : "prod"
);

// prettier-ignore
const Environments = {
  prod: {
    ANALYSIS_API:        "https://trustwerty.com/api/v2/fx/analysis",
    ANALYSIS_STATUS_API: "https://trustwerty.com/api/v1/fx/analysis_status",
    ANALYZE_API:         "https://trustwerty.com/api/v1/fx/analyze",
    ATTRIBUTION_API:     "https://pe.fakespot.com/api/v1/fx/events",
    RECOMMENDATIONS_API: "https://a.fakespot.com/v1/fx/sp_search",
    REPORTING_API:       "https://trustwerty.com/api/v1/fx/report",
  },
  stage: {
    ANALYSIS_API:        "https://staging.trustwerty.com/api/v2/fx/analysis",
    ANALYSIS_STATUS_API: "https://staging.trustwerty.com/api/v1/fx/analysis_status",
    ANALYZE_API:         "https://staging.trustwerty.com/api/v1/fx/analyze",
    ATTRIBUTION_API:     "https://staging-partner-ads.fakespot.io/api/v1/fx/events",
    RECOMMENDATIONS_API: "https://staging-affiliates.fakespot.io/v1/fx/sp_search",
    REPORTING_API:       "https://staging.trustwerty.com/api/v1/fx/report",
  },
  test: {
    ANALYSIS_API:        "https://example.com/browser/toolkit/components/shopping/test/browser/analysis.sjs",
    ANALYSIS_STATUS_API: "https://example.com/browser/toolkit/components/shopping/test/browser/analysis_status.sjs",
    ANALYZE_API:         "https://example.com/browser/toolkit/components/shopping/test/browser/analyze.sjs",
    ATTRIBUTION_API:     "https://example.com/browser/toolkit/components/shopping/test/browser/attribution.sjs",
    RECOMMENDATIONS_API: "https://example.com/browser/toolkit/components/shopping/test/browser/recommendations.sjs",
    REPORTING_API:       "https://example.com/browser/toolkit/components/shopping/test/browser/reporting.sjs",
  },
};

const ShoppingEnvironment = {
  get ANALYSIS_API() {
    return Environments[lazy.env].ANALYSIS_API;
  },
  get ANALYSIS_STATUS_API() {
    return Environments[lazy.env].ANALYSIS_STATUS_API;
  },
  get ANALYZE_API() {
    return Environments[lazy.env].ANALYZE_API;
  },
  get ATTRIBUTION_API() {
    return Environments[lazy.env].ATTRIBUTION_API;
  },
  get RECOMMENDATIONS_API() {
    return Environments[lazy.env].RECOMMENDATIONS_API;
  },
  get REPORTING_API() {
    return Environments[lazy.env].REPORTING_API;
  },
};

const ProductConfig = {
  amazon: {
    productIdFromURLRegex:
      /(?:[\/]|$|%2F)(?<productId>[A-Z0-9]{10})(?:[\/]|$|\#|\?|%2F)/,
    validTLDs: ["com", "de", "fr"],
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

if (lazy.env == "test") {
  // Also allow example.com to allow for testing.
  ProductConfig.example = ProductConfig.amazon;
}

export {
  ANALYSIS_RESPONSE_SCHEMA,
  ANALYSIS_REQUEST_SCHEMA,
  ANALYZE_RESPONSE_SCHEMA,
  ANALYZE_REQUEST_SCHEMA,
  ANALYSIS_STATUS_RESPONSE_SCHEMA,
  ANALYSIS_STATUS_REQUEST_SCHEMA,
  RECOMMENDATIONS_RESPONSE_SCHEMA,
  RECOMMENDATIONS_REQUEST_SCHEMA,
  ATTRIBUTION_RESPONSE_SCHEMA,
  ATTRIBUTION_REQUEST_SCHEMA,
  REPORTING_RESPONSE_SCHEMA,
  REPORTING_REQUEST_SCHEMA,
  ProductConfig,
  ShoppingEnvironment,
};
