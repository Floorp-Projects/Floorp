/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  ANALYSIS_API,
  ANALYSIS_RESPONSE_SCHEMA,
  ANALYSIS_REQUEST_SCHEMA,
  RECOMMENDATIONS_API,
  RECOMMENDATIONS_RESPONSE_SCHEMA,
  RECOMMENDATIONS_REQUEST_SCHEMA,
  ProductConfig,
} from "chrome://global/content/shopping/ProductConfig.mjs";

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  ProductValidator: "chrome://global/content/shopping/ProductValidator.sys.mjs",
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
});

const API_RETRIES = 3;
const API_RETRY_TIMEOUT = 100;

/**
 * @typedef {object} Product
 *  A parsed product for a URL
 * @property {number} id
 *  The product id of the product.
 * @property {string} host
 *  The host of a product url (without www)
 * @property {string} tld
 *  The top level domain of a URL
 * @property {string} sitename
 *  The name of a website (without TLD or subdomains)
 * @property {boolean} valid
 *  If the product is valid or not
 */

/**
 * Class for working with the products shopping API,
 * with helpers for parsing the product from a url
 * and querying the shopping API for information on it.
 *
 * @example
 * let product = new ShoppingProduct(url);
 * if (product.isProduct()) {
 *  let analysis = await product.requestAnalysis();
 *  let recommendations = await product.requestRecommendations();
 * }
 * @example
 * if (!isProductURL(url)) {
 *  return;
 * }
 * let product = new ShoppingProduct(url);
 * let analysis = await product.requestAnalysis();
 * let recommendations = await product.requestRecommendations();
 */
export class ShoppingProduct {
  /**
   * Creates a product.
   *
   * @param {URL} url
   *  URL to get the product info from.
   * @param {object} [options]
   * @param {object} [options.allowValidationFailure=true]
   *  Should validation failures be allowed or return null
   */
  constructor(url, options = { allowValidationFailure: true }) {
    this.allowValidationFailure = !!options.allowValidationFailure;
    this.analysis = undefined;
    this.recommendations = undefined;

    this._abortController = new AbortController();

    if (url instanceof Ci.nsIURI) {
      url = URL.fromURI(url);
    }

    if (url && URL.isInstance(url)) {
      let product = this.constructor.fromURL(url);
      this.assignProduct(product);
    }
  }

  /**
   * Gets a Product from a URL.
   *
   * @param {URL} url
   *  URL to find a product in.
   * @returns {Product}
   */
  static fromURL(url) {
    let host, sitename, tld;
    let result = { valid: false };

    if (!url || !URL.isInstance(url)) {
      return result;
    }

    // Lowercase hostname and remove www.
    host = url.hostname.replace(/^www\./i, "");
    result.host = host;

    // Get host TLD
    try {
      tld = Services.eTLD.getPublicSuffixFromHost(host);
    } catch (_) {
      return result;
    }
    if (!tld.length) {
      return result;
    }

    // Remove tld and the preceding period to get the sitename
    sitename = host.slice(0, -(tld.length + 1));

    // Check if sitename is one the API has products for
    let siteConfig = ProductConfig[sitename];
    if (!siteConfig) {
      return result;
    }
    result.sitename = sitename;

    // Check if API has products for this TLD
    if (!siteConfig.validTLDs.includes(tld)) {
      return result;
    }
    result.tld = tld;

    // Try to find a product id from the pathname.
    let found = url.pathname.match(siteConfig.productIdFromURLRegex);
    if (!found?.groups) {
      return result;
    }

    let { productId } = found.groups;
    if (!productId) {
      return result;
    }
    result.id = productId;

    // Mark product as valid and complete.
    result.valid = true;

    return result;
  }

  /**
   * Check if a Product is a valid product.
   *
   * @param {Product} product
   *  Product to check.
   * @returns {boolean}
   */
  static isProduct(product) {
    return !!(
      product &&
      product.valid &&
      product.id &&
      product.host &&
      product.sitename &&
      product.tld
    );
  }

  /**
   * Check if a the instances product is a valid product.
   *
   * @returns {boolean}
   */
  isProduct() {
    return this.constructor.isProduct(this.product);
  }

  /**
   * Assign a product to this instance.
   */
  assignProduct(product) {
    if (this.constructor.isProduct(product)) {
      this.product = product;
    }
  }

  /**
   * Request analysis for a product from the API.
   *
   * @param {boolean} force
   *  Force always requesting from API.
   * @param {Product} product
   *  Product to request for (defaults to the instances product).
   * @param {object} options
   *  Override default API url and schema.
   * @returns {object} result
   *  Parsed JSON API result or null.
   */
  async requestAnalysis(
    force = false,
    product = this.product,
    options = {
      url: ANALYSIS_API,
      requestSchema: ANALYSIS_REQUEST_SCHEMA,
      responseSchema: ANALYSIS_RESPONSE_SCHEMA,
    }
  ) {
    if (!product) {
      return null;
    }

    if (!force && this.analysis) {
      return this.analysis;
    }

    let requestOptions = {
      product_id: product.id,
      website: product.host,
    };

    let { url, requestSchema, responseSchema } = options;

    let result = await this.request(url, requestOptions, {
      requestSchema,
      responseSchema,
    });

    this.analysis = result;

    return result;
  }

  /**
   * Request recommended related products from the API.
   * Currently only provides recommendations for Amazon products,
   * which may be paid ads.
   *
   * @param {boolean} force
   *  Force always requesting from API.
   * @param {Product} product
   *  Product to request for (defaults to the instances product).
   * @param {object} options
   *  Override default API url and schema.
   * @returns {object} result
   *  Parsed JSON API result or null.
   */
  async requestRecommendations(
    force = false,
    product = this.product,
    options = {
      url: RECOMMENDATIONS_API,
      requestSchema: RECOMMENDATIONS_REQUEST_SCHEMA,
      responseSchema: RECOMMENDATIONS_RESPONSE_SCHEMA,
    }
  ) {
    if (!product) {
      return null;
    }

    if (!force && this.recommendations) {
      return this.recommendations;
    }

    let requestOptions = {
      product_id: product.id,
      website: product.host,
    };
    let { url, requestSchema, responseSchema } = options;
    let result = await this.request(url, requestOptions, {
      requestSchema,
      responseSchema,
    });

    this.recommendations = result;

    return result;
  }

  /**
   * Request method for shopping API.
   *
   * @param {string} apiURL
   *  URL string for the API to request.
   * @param {object} bodyObj
   *  What to send to the API.
   * @param {object} [options]
   *  Options for validation and retries.
   * @param {string} [options.requestSchema]
   *  URL string for the JSON Schema to validated the request.
   * @param {string} [options.responseSchema]
   *  URL string for the JSON Schema to validated the response.
   * @param {int} [options.failCount]
   *  Current number of failures for this request.
   * @param {int} [options.maxRetries=API_RETRIES]
   *  Max number of allowed failures.
   * @param {int} [options.retryTimeout=API_RETRY_TIMEOUT]
   *  Minimum time to wait.
   * @returns {object} result
   *  Parsed JSON API result or null.
   */
  async request(apiURL, bodyObj = {}, options = {}) {
    let {
      requestSchema,
      responseSchema,
      failCount = 0,
      maxRetries = API_RETRIES,
      retryTimeout = API_RETRY_TIMEOUT,
    } = options;

    if (this._abortController.signal.aborted) {
      return null;
    }

    if (bodyObj && requestSchema) {
      let validRequest = await lazy.ProductValidator.validate(
        bodyObj,
        requestSchema,
        this.allowValidationFailure
      );
      if (!validRequest && !this.allowValidationFailure) {
        return null;
      }
    }

    let resp = await fetch(apiURL, {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
        Accept: "application/json",
      },
      body: JSON.stringify(bodyObj),
      signal: this._abortController.signal,
    });
    let responseOk = resp.ok;
    let responseStatus = resp.status;
    let result = await resp.json();

    if (responseOk && responseSchema) {
      let validResponse = await lazy.ProductValidator.validate(
        result,
        responseSchema,
        this.allowValidationFailure
      );
      if (!validResponse && !this.allowValidationFailure) {
        return null;
      }
    }

    // Retry failed results and 500 errors.
    if (!result || (!responseOk && responseStatus >= 500)) {
      failCount++;
      // Make sure we still want to retry
      if (failCount > maxRetries) {
        return null;
      }
      // Wait for a back off timeout base on the number of failures.
      let backOff = retryTimeout * Math.pow(2, failCount - 1);

      await new Promise(resolve => lazy.setTimeout(resolve, backOff));

      // Try the request again.
      options.failCount = failCount;
      result = await this.request(apiURL, bodyObj, options);
    }

    return result;
  }

  uninit() {
    this._abortController.abort();
  }
}

/**
 * Check if a URL is a valid product.
 *
 * @param {URL | nsIURI } url
 *  URL to check.
 * @returns {boolean}
 */
export function isProductURL(url) {
  if (url instanceof Ci.nsIURI) {
    url = URL.fromURI(url);
  }
  if (!URL.isInstance(url)) {
    return false;
  }
  let productInfo = ShoppingProduct.fromURL(url);
  return ShoppingProduct.isProduct(productInfo);
}
