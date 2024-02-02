/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
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
} from "chrome://global/content/shopping/ProductConfig.mjs";

let { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

let { EventEmitter } = ChromeUtils.importESModule(
  "resource://gre/modules/EventEmitter.sys.mjs"
);

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  HPKEConfigManager: "resource://gre/modules/HPKEConfigManager.sys.mjs",
  ProductValidator: "chrome://global/content/shopping/ProductValidator.sys.mjs",
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
});

const API_RETRIES = 3;
const API_RETRY_TIMEOUT = 100;
const API_POLL_ATTEMPTS = 260;
const API_POLL_INITIAL_WAIT = 1000;
const API_POLL_WAIT = 1000;

XPCOMUtils.defineLazyServiceGetters(lazy, {
  ohttpService: [
    "@mozilla.org/network/oblivious-http-service;1",
    Ci.nsIObliviousHttpService,
  ],
});

ChromeUtils.defineLazyGetter(lazy, "decoder", () => new TextDecoder());

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
  return arrayBuffer;
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
export class ShoppingProduct extends EventEmitter {
  /**
   * Creates a product.
   *
   * @param {URL} url
   *  URL to get the product info from.
   * @param {object} [options]
   * @param {boolean} [options.allowValidationFailure=true]
   *  Should validation failures be allowed or return null
   */
  constructor(url, options = { allowValidationFailure: true }) {
    super();

    this.allowValidationFailure = !!options.allowValidationFailure;

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
   * @param {Product} product
   *  Product to request for (defaults to the instances product).
   * @param {object} options
   *  Override default API url and schema.
   * @returns {object} result
   *  Parsed JSON API result or null.
   */
  async requestAnalysis(
    product = this.product,
    options = {
      url: ShoppingEnvironment.ANALYSIS_API,
      requestSchema: ANALYSIS_REQUEST_SCHEMA,
      responseSchema: ANALYSIS_RESPONSE_SCHEMA,
    }
  ) {
    if (!product) {
      return null;
    }

    let requestOptions = {
      product_id: product.id,
      website: product.host,
    };

    let { url, requestSchema, responseSchema } = options;
    let { allowValidationFailure } = this;

    let result = await ShoppingProduct.request(url, requestOptions, {
      requestSchema,
      responseSchema,
      allowValidationFailure,
    });

    return result;
  }

  /**
   * Request recommended related products from the API.
   * Currently only provides recommendations for Amazon products,
   * which may be paid ads.
   *
   * @param {Product} product
   *  Product to request for (defaults to the instances product).
   * @param {object} options
   *  Override default API url and schema.
   * @returns {object} result
   *  Parsed JSON API result or null.
   */
  async requestRecommendations(
    product = this.product,
    options = {
      url: ShoppingEnvironment.RECOMMENDATIONS_API,
      requestSchema: RECOMMENDATIONS_REQUEST_SCHEMA,
      responseSchema: RECOMMENDATIONS_RESPONSE_SCHEMA,
    }
  ) {
    if (!product) {
      return null;
    }

    let requestOptions = {
      product_id: product.id,
      website: product.host,
    };
    let { url, requestSchema, responseSchema } = options;
    let { allowValidationFailure } = this;
    let result = await ShoppingProduct.request(url, requestOptions, {
      requestSchema,
      responseSchema,
      allowValidationFailure,
    });

    for (let ad of result) {
      ad.image_blob = await ShoppingProduct.requestImageBlob(ad.image_url, {
        signal: this._abortController.signal,
      });
    }

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
   * @param {AbortSignal} [options.signal]
   *  Signal to check if the request needs to be aborted.
   * @param {boolean} [options.allowValidationFailure=true]
   *  Should validation failures be allowed.
   * @returns {object} result
   *  Parsed JSON API result or null.
   */
  static async request(apiURL, bodyObj = {}, options = {}) {
    let {
      requestSchema,
      responseSchema,
      failCount = 0,
      maxRetries = API_RETRIES,
      retryTimeout = API_RETRY_TIMEOUT,
      signal = new AbortController().signal,
      allowValidationFailure = true,
    } = options;

    if (signal.aborted) {
      return null;
    }

    if (bodyObj && requestSchema) {
      let validRequest = await lazy.ProductValidator.validate(
        bodyObj,
        requestSchema,
        allowValidationFailure
      );
      if (!validRequest && !allowValidationFailure) {
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
      signal,
    };

    let requestPromise;
    let ohttpRelayURL = Services.prefs.getStringPref(
      "toolkit.shopping.ohttpRelayURL",
      ""
    );
    let ohttpConfigURL = Services.prefs.getStringPref(
      "toolkit.shopping.ohttpConfigURL",
      ""
    );
    if (ohttpRelayURL && ohttpConfigURL) {
      let config = await ShoppingProduct.getOHTTPConfig(ohttpConfigURL);
      // In the time it took to fetch the OHTTP config, we might have been
      // aborted...
      if (signal.aborted) {
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
      requestPromise = ShoppingProduct.ohttpRequest(
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
          allowValidationFailure
        );
        if (!validResponse && !allowValidationFailure) {
          return null;
        }
      }
    } catch (error) {
      console.error(error);
    }

    // Retry 500 errors.
    if (!responseOk && responseStatus >= 500) {
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
      result = await ShoppingProduct.request(apiURL, bodyObj, options);
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
  static async getOHTTPConfig(gatewayConfigURL) {
    return lazy.HPKEConfigManager.get(gatewayConfigURL);
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
  static async ohttpRequest(
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
        _buffer: [],
        _headers: null,
        QueryInterface: ChromeUtils.generateQI([
          "nsIStreamListener",
          "nsIRequestObserver",
        ]),
        onStartRequest(request) {
          this._headers = {};
          try {
            request
              .QueryInterface(Ci.nsIHttpChannel)
              .visitResponseHeaders((header, value) => {
                this._headers[header.toLowerCase()] = value;
              });
          } catch (error) {
            this._headers = null;
          }
        },
        onDataAvailable(request, stream, offset, count) {
          this._buffer.push(readFromStream(stream, count));
        },
        onStopRequest(request, requestStatus) {
          signal.removeEventListener("abort", abortHandler);
          let result = this._buffer;
          try {
            let ohttpStatus = request.QueryInterface(Ci.nsIObliviousHttpChannel)
              .relayChannel.responseStatus;
            if (ohttpStatus == 200) {
              let httpStatus = request.QueryInterface(
                Ci.nsIHttpChannel
              ).responseStatus;
              resolve({
                ok: requestStatus == Cr.NS_OK && httpStatus == 200,
                status: httpStatus,
                headers: this._headers,
                json() {
                  let decodedBuffer = result.reduce((accumulator, currVal) => {
                    return accumulator + lazy.decoder.decode(currVal);
                  }, "");
                  return JSON.parse(decodedBuffer);
                },
                blob() {
                  return new Blob(result, { type: "image/jpeg" });
                },
              });
            } else {
              resolve({
                ok: false,
                status: ohttpStatus,
                json() {
                  return null;
                },
                blob() {
                  return null;
                },
              });
            }
          } catch (error) {
            reject(error);
          }
        },
      };
      obliviousHttpChannel.asyncOpen(listener);
    });
  }

  /**
   * Requests an image for a recommended product.
   *
   * @param {string} imageUrl
   * @returns {blob} A blob of the image
   */
  static async requestImageBlob(imageUrl, options = {}) {
    let { signal = new AbortController().signal } = options;
    let ohttpRelayURL = Services.prefs.getStringPref(
      "toolkit.shopping.ohttpRelayURL",
      ""
    );
    let ohttpConfigURL = Services.prefs.getStringPref(
      "toolkit.shopping.ohttpConfigURL",
      ""
    );

    let imgRequestPromise;
    if (ohttpRelayURL && ohttpConfigURL) {
      let config = await ShoppingProduct.getOHTTPConfig(ohttpConfigURL);
      if (!config) {
        console.error(
          new Error(
            "OHTTP was configured for shopping but we couldn't get a valid config."
          )
        );
        return null;
      }

      let imgRequestOptions = {
        signal,
        headers: {
          Accept: "image/jpeg",
          "Content-Type": "image/jpeg",
        },
      };

      imgRequestPromise = ShoppingProduct.ohttpRequest(
        ohttpRelayURL,
        config,
        imageUrl,
        imgRequestOptions
      );
    } else {
      imgRequestPromise = fetch(imageUrl);
    }

    let imgResult;
    try {
      let response = await imgRequestPromise;
      imgResult = await response.blob();
    } catch (error) {
      console.error(error);
    }

    return imgResult;
  }

  /**
   * Poll Analysis Status API until an analysis has finished.
   *
   * After an initial wait keep checking the api for results,
   * until we have reached a maximum of tries.
   *
   * Passes all arguments to requestAnalysisCreationStatus().
   *
   * @example
   *  let analysis;
   *  let { status } = await product.pollForAnalysisCompleted();
   *  // Check if analysis has finished
   *  if(status != "pending" && status != "in_progress") {
   *    // Get the new analysis
   *    analysis = await product.requestAnalysis();
   *  }
   *
   * @example
   * // Check the current status
   * let { status } = await product.requestAnalysisCreationStatus();
   * if(status == "pending" && status == "in_progress") {
   *    // Start polling without the initial timeout if the analysis
   *    // is already in progress.
   *    await product.pollForAnalysisCompleted({
   *      pollInitialWait: analysisStatus == "in_progress" ? 0 : undefined,
   *    });
   * }
   * @param {object} options
   *  Override default API url and schema.
   * @returns {object} result
   *  Parsed JSON API result or null.
   */
  async pollForAnalysisCompleted(options) {
    let pollCount = 0;
    let initialWait = options?.pollInitialWait || API_POLL_INITIAL_WAIT;
    let pollTimeout = options?.pollTimeout || API_POLL_WAIT;
    let pollAttempts = options?.pollAttempts || API_POLL_ATTEMPTS;
    let isFinished = false;
    let result;

    while (!isFinished && pollCount < pollAttempts) {
      if (this._abortController.signal.aborted) {
        return null;
      }
      let backOff = pollCount == 0 ? initialWait : pollTimeout;
      if (backOff) {
        await new Promise(resolve => lazy.setTimeout(resolve, backOff));
      }
      try {
        result = await this.requestAnalysisCreationStatus(undefined, options);
        if (result?.progress) {
          this.emit("analysis-progress", result.progress);
        }
        isFinished =
          result &&
          result.status != "pending" &&
          result.status != "in_progress";
      } catch (error) {
        console.error(error);
        return null;
      }
      pollCount++;
    }
    return result;
  }

  /**
   * Request that the API creates an analysis for a product.
   *
   * Once the processing status indicates that analyzing is complete,
   * the new analysis data that can be requested with `requestAnalysis`.
   *
   * If the product is currently being analyzed, this will return a
   * status of "in_progress" and not trigger a reanalyzing the product.
   *
   * @param {Product} product
   *  Product to request for (defaults to the instances product).
   * @param {object} options
   *  Override default API url and schema.
   * @returns {object} result
   *  Parsed JSON API result or null.
   */
  async requestCreateAnalysis(product = this.product, options = {}) {
    let url = options?.url || ShoppingEnvironment.ANALYZE_API;
    let requestSchema = options?.requestSchema || ANALYZE_REQUEST_SCHEMA;
    let responseSchema = options?.responseSchema || ANALYZE_RESPONSE_SCHEMA;
    let signal = options?.signal || this._abortController.signal;
    let allowValidationFailure = this.allowValidationFailure;

    if (!product) {
      return null;
    }

    let requestOptions = {
      product_id: product.id,
      website: product.host,
    };

    let result = await ShoppingProduct.request(url, requestOptions, {
      requestSchema,
      responseSchema,
      signal,
      allowValidationFailure,
    });

    return result;
  }

  /**
   * Check the status of creating an analysis for a product.
   *
   * API returns a progress of 0-100 complete and the processing status.
   *
   * @param {Product} product
   *  Product to request for (defaults to the instances product).
   * @param {object} options
   *  Override default API url and schema.
   * @returns {object} result
   *  Parsed JSON API result or null.
   */
  async requestAnalysisCreationStatus(product = this.product, options = {}) {
    let url = options?.url || ShoppingEnvironment.ANALYSIS_STATUS_API;
    let requestSchema =
      options?.requestSchema || ANALYSIS_STATUS_REQUEST_SCHEMA;
    let responseSchema =
      options?.responseSchema || ANALYSIS_STATUS_RESPONSE_SCHEMA;
    let signal = options?.signal || this._abortController.signal;
    let allowValidationFailure = this.allowValidationFailure;

    if (!product) {
      return null;
    }

    let requestOptions = {
      product_id: product.id,
      website: product.host,
    };

    let result = await ShoppingProduct.request(url, requestOptions, {
      requestSchema,
      responseSchema,
      signal,
      allowValidationFailure,
    });

    return result;
  }

  /**
   * Send an event to the Ad Attribution API
   *
   * @param {string} eventName
   *  Event name options are:
   *  - "impression"
   *  - "click"
   * @param {string} aid
   *  The aid (Ad ID) from the recommendation.
   * @param {string} [source]
   *  Source of the event
   * @param {object} [options]
   *  Override default API url and schema.
   * @returns {object} result
   *  Parsed JSON API result or null.
   */
  static async sendAttributionEvent(
    eventName,
    aid,
    source = "firefox_sidebar",
    options = {}
  ) {
    let {
      url = ShoppingEnvironment.ATTRIBUTION_API,
      requestSchema = ATTRIBUTION_REQUEST_SCHEMA,
      responseSchema = ATTRIBUTION_RESPONSE_SCHEMA,
      signal = new AbortController().signal,
      allowValidationFailure = true,
    } = options;

    if (!eventName) {
      throw new Error("An event name is required.");
    }
    if (!aid) {
      throw new Error("An Ad ID is required.");
    }

    let requestBody = {
      event_source: source,
    };

    switch (eventName) {
      case "impression":
        requestBody.event_name = "trusted_deals_impression";
        requestBody.aidvs = [aid];
        break;
      case "click":
        requestBody.event_name = "trusted_deals_link_clicked";
        requestBody.aid = aid;
        break;
      case "placement":
        requestBody.event_name = "trusted_deals_placement";
        requestBody.aidvs = [aid];
        break;
      default:
        throw new Error(`"${eventName}" is not a valid event name`);
    }

    let result = await ShoppingProduct.request(url, requestBody, {
      requestSchema,
      responseSchema,
      signal,
      allowValidationFailure,
    });

    return result;
  }

  /**
   * Send a report that a product is back in stock.
   *
   * @param {Product} product
   *  Product to request for (defaults to the instances product).
   * @param {object} options
   *  Override default API url and schema.
   * @returns {object} result
   *  Parsed JSON API result or null.
   */
  async sendReport(product = this.product, options = {}) {
    if (!product) {
      return null;
    }

    let url = options?.url || ShoppingEnvironment.REPORTING_API;
    let requestSchema = options?.requestSchema || REPORTING_REQUEST_SCHEMA;
    let responseSchema = options?.responseSchema || REPORTING_RESPONSE_SCHEMA;
    let signal = options?.signal || this._abortController.signal;
    let allowValidationFailure = this.allowValidationFailure;

    let requestOptions = {
      product_id: product.id,
      website: product.host,
    };

    let result = await ShoppingProduct.request(url, requestOptions, {
      requestSchema,
      responseSchema,
      signal,
      allowValidationFailure,
    });

    return result;
  }

  uninit() {
    this._abortController.abort();
    this.product = null;
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
