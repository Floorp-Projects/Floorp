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

let { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
  OHTTPConfigManager: "resource://gre/modules/OHTTPConfigManager.sys.mjs",
  ProductValidator: "chrome://global/content/shopping/ProductValidator.sys.mjs",
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
});

const API_RETRIES = 3;
const API_RETRY_TIMEOUT = 100;

XPCOMUtils.defineLazyServiceGetters(lazy, {
  ohttpService: [
    "@mozilla.org/network/oblivious-http-service;1",
    Ci.nsIObliviousHttpService,
  ],
});

XPCOMUtils.defineLazyGetter(lazy, "decoder", () => new TextDecoder());

const StringInputStream = Components.Constructor(
  "@mozilla.org/io/string-input-stream;1",
  "nsIStringInputStream",
  "setData"
);

const BinaryInputStream = Components.Constructor(
  "@mozilla.org/binaryinputstream;1",
  "nsIBinaryInputStream",
  "setInputStream"
);

function readFromStream(stream, count) {
  let binaryStream = new BinaryInputStream(stream);
  let arrayBuffer = new ArrayBuffer(count);
  while (count > 0) {
    let actuallyRead = binaryStream.readArrayBuffer(count, arrayBuffer);
    if (!actuallyRead) {
      throw new Error("Nothing read from input stream!");
    }
    count -= actuallyRead;
  }
  return lazy.decoder.decode(arrayBuffer);
}

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

    let requestOptions = {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
        Accept: "application/json",
      },
      body: JSON.stringify(bodyObj),
      signal: this._abortController.signal,
    };

    let requestPromise;
    let { useOHTTP, ohttpRelayURL, ohttpConfigURL } =
      lazy.NimbusFeatures.shopping2023.getAllVariables();
    if (useOHTTP && ohttpRelayURL && ohttpConfigURL) {
      let config = await this.getOHTTPConfig(ohttpConfigURL);
      // In the time it took to fetch the OHTTP config, we might have been
      // aborted...
      if (requestOptions.signal.aborted) {
        return null;
      }
      if (!config) {
        console.error(
          new Error(
            "OHTTP was configured for shopping but we couldn't get a valid config."
          )
        );
        return null;
      }
      requestPromise = this.ohttpRequest(
        ohttpRelayURL,
        config,
        apiURL,
        requestOptions
      );
    } else {
      requestPromise = fetch(apiURL, requestOptions);
    }

    let result;
    let responseOk;
    let responseStatus;
    try {
      let response = await requestPromise;
      responseOk = response.ok;
      responseStatus = response.status;
      result = await response.json();

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
    } catch (error) {
      console.error(error);
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

  /**
   * Get a cached, or fetch a copy of, an OHTTP config from a given URL.
   *
   *
   * @param {string} gatewayConfigURL
   *   The URL for the config that needs to be fetched.
   *   The URL should be complete (i.e. include the full path to the config).
   * @returns {Uint8Array}
   *   The config bytes.
   */
  async getOHTTPConfig(gatewayConfigURL) {
    return lazy.OHTTPConfigManager.get(gatewayConfigURL);
  }

  /**
   * Make a request over OHTTP.
   *
   * @param {string} obliviousHTTPRelay
   *   The URL of the OHTTP relay to use.
   * @param {Uint8Array} config
   *   A byte array representing the OHTTP config.
   * @param {string} requestURL
   *   The URL of the request we want to make over the relay.
   * @param {object} options
   * @param {string} options.method
   *   The HTTP method to use for the inner request. Only GET and POST
   *   are supported right now.
   * @param {string} options.body
   *   The body content to send over the request.
   * @param {object} options.headers
   *   The request headers to set. Each property of the object represents
   *   a header, with the key the header name and the value the header value.
   * @param {AbortSignal} options.signal
   *   If the consumer passes an AbortSignal object, aborting the signal
   *   will abort the request.
   *
   * @returns {object}
   *   Returns an object with properties mimicking that of a normal fetch():
   *   .ok = boolean indicating whether the request was successful.
   *   .status = integer representation of the HTTP status code
   *   .headers = object representing the response headers.
   *   .json() = method that returns the parsed JSON response body.
   */
  async ohttpRequest(
    obliviousHTTPRelay,
    config,
    requestURL,
    { method, body, headers, signal } = {}
  ) {
    let relayURI = Services.io.newURI(obliviousHTTPRelay);
    let requestURI = Services.io.newURI(requestURL);
    let obliviousHttpChannel = lazy.ohttpService
      .newChannel(relayURI, requestURI, config)
      .QueryInterface(Ci.nsIHttpChannel);

    if (method == "POST") {
      let uploadChannel = obliviousHttpChannel.QueryInterface(
        Ci.nsIUploadChannel2
      );
      let bodyStream = new StringInputStream(body, body.length);
      uploadChannel.explicitSetUploadStream(
        bodyStream,
        null,
        -1,
        "POST",
        false
      );
    }

    for (let headerName of Object.keys(headers)) {
      obliviousHttpChannel.setRequestHeader(
        headerName,
        headers[headerName],
        false
      );
    }
    let abortHandler = e => {
      obliviousHttpChannel.cancel(Cr.NS_BINDING_ABORTED);
    };
    signal.addEventListener("abort", abortHandler);
    return new Promise((resolve, reject) => {
      let listener = {
        _buffer: "",
        _headers: null,
        QueryInterface: ChromeUtils.generateQI([
          "nsIStreamListener",
          "nsIRequestObserver",
        ]),
        onStartRequest(request) {
          this._headers = {};
          request
            .QueryInterface(Ci.nsIHttpChannel)
            .visitResponseHeaders((header, value) => {
              this._headers[header.toLowerCase()] = value;
            });
        },
        onDataAvailable(request, stream, offset, count) {
          this._buffer += readFromStream(stream, count);
        },
        onStopRequest(request, requestStatus) {
          signal.removeEventListener("abort", abortHandler);
          let result = this._buffer;
          let httpStatus = request.QueryInterface(
            Ci.nsIHttpChannel
          ).responseStatus;
          resolve({
            ok: requestStatus == Cr.NS_OK && httpStatus == 200,
            status: httpStatus,
            headers: this._headers,
            json() {
              return JSON.parse(result);
            },
          });
        },
      };
      obliviousHttpChannel.asyncOpen(listener);
    });
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
