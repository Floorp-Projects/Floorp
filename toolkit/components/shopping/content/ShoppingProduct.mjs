/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  ANALYSIS_API,
  ANALYSIS_SCHEMA,
  RECOMMENDATIONS_API,
  RECOMMENDATIONS_SCHEMA,
  ProductConfig,
} from "chrome://global/content/shopping/ProductConfig.mjs";

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  ProductValidator: "chrome://global/content/shopping/ProductValidator.sys.mjs",
});

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
 * let product = new Product(url);
 * if (product.isProduct()) {
 *  let analysis = await product.requestAnalysis();
 *  let recommendations = await product.requestRecommendations();
 * }
 * @example
 * let url = new URL(location.href);
 * let parsed = ShoppingProduct.fromURL(url);
 * if (parsed.valid) {
 *  let product = new ShoppingProduct();
 *  product.assignProduct(parsed);
 *  let analysis = await product.requestAnalysis();
 *  let recommendations = await product.requestRecommendations();
 * }
 */
export class ShoppingProduct {
  /**
   * Creates a product.
   *
   * @param {URL} url
   *  URL to get the product info from.
   * @param {object} options
   *  shouldValidate: false - only for debugging API calls.
   */
  constructor(url, options = {}) {
    this.shouldValidate = !!options.shouldValidate;
    this.analysis = undefined;
    this.recommendations = undefined;

    this.abortController = new AbortController();

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
    options = { url: ANALYSIS_API, schema: ANALYSIS_SCHEMA }
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

    let result = await this.#request(
      options.url,
      requestOptions,
      options.schema
    );

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
    options = { url: RECOMMENDATIONS_API, schema: RECOMMENDATIONS_SCHEMA }
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

    let result = await this.#request(
      options.url,
      requestOptions,
      options.schema
    );

    this.recommendations = result;

    return result;
  }

  /**
   * Request method for shopping API.
   *
   * @param {string} ApiURL
   *  URL string for the API to request.
   * @param {object} bodyObj
   *  Options to send to the API.
   * @param {string} SchemaURL
   *  URL string for the JSON Schema to validated with.
   * @returns {object} result
   *  Parsed JSON API result or null.
   */
  async #request(ApiURL, bodyObj = {}, SchemaURL) {
    let responseOk;
    let result = await fetch(ApiURL, {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
        Accept: "application/json",
      },
      body: JSON.stringify(bodyObj),
      signal: this.abortController.signal,
    }).then(resp => {
      responseOk = resp.ok;
      return resp.json();
    });

    let valid = !SchemaURL;
    if (responseOk && SchemaURL) {
      valid = await lazy.ProductValidator.validate(result, SchemaURL);
      if (!valid) {
        return null;
      }
    }

    return result;
  }

  destroy() {
    this.abortController.abort();
  }
}
