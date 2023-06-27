/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const ANALYSIS_API =
  "https://staging-trustwerty.fakespot.io/api/v1/fx/analysis";
const ANALYSIS_SCHEMA = "chrome://global/content/shopping/analysis.schema.json";
const RECOMMENDATIONS_API =
  "https://staging-affiliates.fakespot.io/v1/fx/sp_search";
const RECOMMENDATIONS_SCHEMA =
  "chrome://global/content/shopping/recommendations.schema.json";

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

Object.freeze(ProductConfig);

export {
  ANALYSIS_API,
  ANALYSIS_SCHEMA,
  RECOMMENDATIONS_API,
  RECOMMENDATIONS_SCHEMA,
  ProductConfig,
};
