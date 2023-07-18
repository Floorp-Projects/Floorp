/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
/* global createHttpServer, readFile */
const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

const {
  ANALYSIS_RESPONSE_SCHEMA,
  ANALYSIS_REQUEST_SCHEMA,
  RECOMMENDATIONS_RESPONSE_SCHEMA,
  RECOMMENDATIONS_REQUEST_SCHEMA,
} = ChromeUtils.importESModule(
  "chrome://global/content/shopping/ProductConfig.mjs"
);

const { ShoppingProduct, isProductURL } = ChromeUtils.importESModule(
  "chrome://global/content/shopping/ShoppingProduct.mjs"
);

const ANALYSIS_API_MOCK = "http://example.com/api/analysis_response.json";
const RECOMMENDATIONS_API_MOCK =
  "http://example.com/api/recommendations_response.json";
const ANALYSIS_API_MOCK_INVALID =
  "http://example.com/api/invalid_analysis_response.json";
const API_SERVICE_UNAVAILABLE =
  "http://example.com/errors/service_unavailable.json";
const API_ERROR_ONCE = "http://example.com/errors/error_once.json";
const API_ERROR_BAD_REQUEST = "http://example.com/errors/bad_request.json";
const API_ERROR_UNPROCESSABLE =
  "http://example.com/errors/unprocessable_entity.json";

const server = createHttpServer({ hosts: ["example.com"] });
server.registerDirectory("/api/", do_get_file("/data"));

// Path to test API call that will always fail.
server.registerPathHandler(
  new URL(API_SERVICE_UNAVAILABLE).pathname,
  (request, response) => {
    response.setStatusLine(request.httpVersion, 503, "Service Unavailable");
    response.setHeader(
      "Content-Type",
      "application/json; charset=utf-8",
      false
    );
    response.write(readFile("data/service_unavailable.json", false));
  }
);

// Path to test API call that will fail once and then succeeded.
let apiErrors = 0;
server.registerPathHandler(
  new URL(API_ERROR_ONCE).pathname,
  (request, response) => {
    response.setHeader(
      "Content-Type",
      "application/json; charset=utf-8",
      false
    );
    if (apiErrors == 0) {
      response.setStatusLine(request.httpVersion, 503, "Service Unavailable");
      response.write(readFile("/data/service_unavailable.json"));
      apiErrors++;
    } else {
      response.setStatusLine(request.httpVersion, 200, "OK");
      response.write(readFile("/data/analysis_response.json"));
      apiErrors = 0;
    }
  }
);

// Request is missing required parameters.
server.registerPathHandler(
  new URL(API_ERROR_BAD_REQUEST).pathname,
  (request, response) => {
    response.setStatusLine(request.httpVersion, 400, "Bad Request");
    response.setHeader(
      "Content-Type",
      "application/json; charset=utf-8",
      false
    );
    response.write(readFile("data/bad_request.json", false));
  }
);

// Request contains a nonsense product identifier or non supported website.
server.registerPathHandler(
  new URL(API_ERROR_UNPROCESSABLE).pathname,
  (request, response) => {
    response.setStatusLine(request.httpVersion, 422, "Unprocessable entity");
    response.setHeader(
      "Content-Type",
      "application/json; charset=utf-8",
      false
    );
    response.write(readFile("data/unprocessable_entity.json", false));
  }
);

add_task(async function test_product_requestAnalysis() {
  let uri = new URL("https://www.walmart.com/ip/926485654");
  let product = new ShoppingProduct(uri, { allowValidationFailure: false });

  if (product.isProduct()) {
    let analysis = await product.requestAnalysis(true, undefined, {
      url: ANALYSIS_API_MOCK,
      requestSchema: ANALYSIS_REQUEST_SCHEMA,
      responseSchema: ANALYSIS_RESPONSE_SCHEMA,
    });

    Assert.ok(
      typeof analysis == "object",
      "Analysis object is loaded from JSON and validated"
    );
  }
});

add_task(async function test_product_requestAnalysis_invalid() {
  let uri = new URL("https://www.walmart.com/ip/926485654");
  let product = new ShoppingProduct(uri, { allowValidationFailure: false });

  if (product.isProduct()) {
    let analysis = await product.requestAnalysis(true, undefined, {
      url: ANALYSIS_API_MOCK_INVALID,
      requestSchema: ANALYSIS_REQUEST_SCHEMA,
      responseSchema: ANALYSIS_RESPONSE_SCHEMA,
    });

    Assert.equal(analysis, undefined, "Analysis object is invalidated");
  }
});

add_task(async function test_product_requestRecommendations() {
  let uri = new URL("https://www.walmart.com/ip/926485654");
  let product = new ShoppingProduct(uri, { allowValidationFailure: false });
  if (product.isProduct()) {
    let recommendations = await product.requestRecommendations(
      true,
      undefined,
      {
        url: RECOMMENDATIONS_API_MOCK,
        requestSchema: RECOMMENDATIONS_REQUEST_SCHEMA,
        responseSchema: RECOMMENDATIONS_RESPONSE_SCHEMA,
      }
    );
    Assert.ok(
      Array.isArray(recommendations),
      "Recommendations array is loaded from JSON and validated"
    );
  }
});

add_task(async function test_product_requestAnalysis_retry_failure() {
  const TEST_TIMEOUT = 100;
  const RETRIES = 3;
  let uri = new URL("https://www.walmart.com/ip/926485654");
  let product = new ShoppingProduct(uri, { allowValidationFailure: false });
  let spy = sinon.spy(product, "request");
  let startTime = Cu.now();
  let totalTime = TEST_TIMEOUT * Math.pow(2, RETRIES - 1);

  if (product.isProduct()) {
    let analysis = await product.requestAnalysis(true, undefined, {
      url: API_SERVICE_UNAVAILABLE,
      requestSchema: ANALYSIS_REQUEST_SCHEMA,
      responseSchema: ANALYSIS_RESPONSE_SCHEMA,
    });
    Assert.equal(analysis, null, "Analysis object is null");
    Assert.equal(
      spy.callCount,
      RETRIES + 1,
      `Request was retried ${RETRIES} times after a failure`
    );
    Assert.ok(
      Cu.now() - startTime >= totalTime,
      `Waited for at least ${totalTime}ms`
    );
  }
});

add_task(async function test_product_requestAnalysis_retry_success() {
  let uri = new URL("https://www.walmart.com/ip/926485654");
  let product = new ShoppingProduct(uri, { allowValidationFailure: false });
  let spy = sinon.spy(product, "request");
  // Make sure API error count is reset
  apiErrors = 0;
  if (product.isProduct()) {
    let analysis = await product.requestAnalysis(true, undefined, {
      url: API_ERROR_ONCE,
      requestSchema: ANALYSIS_REQUEST_SCHEMA,
      responseSchema: ANALYSIS_RESPONSE_SCHEMA,
    });
    Assert.equal(spy.callCount, 2, `Request succeeded after a failure`);
    Assert.ok(
      typeof analysis == "object",
      "Analysis object is loaded from JSON and validated"
    );
  }
});

add_task(async function test_product_bad_request() {
  let uri = new URL("https://www.walmart.com/ip/926485654");
  let product = new ShoppingProduct(uri, { allowValidationFailure: false });

  if (product.isProduct()) {
    let errorResult = await product.requestAnalysis(true, undefined, {
      url: API_ERROR_BAD_REQUEST,
      requestSchema: ANALYSIS_REQUEST_SCHEMA,
      responseSchema: ANALYSIS_RESPONSE_SCHEMA,
    });
    Assert.ok(
      typeof errorResult == "object",
      "Error object is loaded from JSON"
    );
    Assert.equal(errorResult.status, 400, "Error status is passed");
    Assert.equal(errorResult.error, "Bad Request", "Error message is passed");
  }
});

add_task(async function test_product_unprocessable_entity() {
  let uri = new URL("https://www.walmart.com/ip/926485654");
  let product = new ShoppingProduct(uri, { allowValidationFailure: false });

  if (product.isProduct()) {
    let errorResult = await product.requestAnalysis(true, undefined, {
      url: API_ERROR_UNPROCESSABLE,
      requestSchema: ANALYSIS_REQUEST_SCHEMA,
      responseSchema: ANALYSIS_RESPONSE_SCHEMA,
    });
    Assert.ok(
      typeof errorResult == "object",
      "Error object is loaded from JSON"
    );
    Assert.equal(errorResult.status, 422, "Error status is passed");
    Assert.equal(
      errorResult.error,
      "Unprocessable entity",
      "Error message is passed"
    );
  }
});

add_task(function test_product_fromUrl() {
  Assert.deepEqual(
    ShoppingProduct.fromURL(),
    { valid: false },
    "Passing a nothing returns empty result object"
  );

  Assert.deepEqual(
    ShoppingProduct.fromURL(12345),
    { valid: false },
    "Passing a number returns empty result object"
  );

  Assert.deepEqual(
    ShoppingProduct.fromURL("https://www.walmart.com/ip/926485654"),
    { valid: false },
    "String urls returns empty result object"
  );

  Assert.deepEqual(
    ShoppingProduct.fromURL(new URL("https://www.example.com/")),
    { host: "example.com", valid: false },
    "Invalid Url returns a full result object"
  );

  Assert.deepEqual(
    ShoppingProduct.fromURL(new URL("https://www.walmart.com/ip/926485654"))
      .host,
    "walmart.com",
    "WWW in host is ignored"
  );

  Assert.deepEqual(
    ShoppingProduct.fromURL(
      new URL("https://staging.walmart.com/ip/926485654")
    ),
    { host: "staging.walmart.com", valid: false },
    "Subdomain in valid Product Url returns partial result"
  );

  Assert.deepEqual(
    ShoppingProduct.fromURL(new URL("https://walmart.co.uk/ip/926485654")),
    { host: "walmart.co.uk", sitename: "walmart", valid: false },
    "Invalid in Product TLD returns partial result"
  );

  Assert.deepEqual(
    ShoppingProduct.fromURL(new URL("https://walmart.com")),
    { host: "walmart.com", sitename: "walmart", tld: "com", valid: false },
    "Non-Product page returns partial result"
  );

  Assert.deepEqual(
    ShoppingProduct.fromURL(new URL("https://walmart.com/ip/926485654")),
    {
      host: "walmart.com",
      sitename: "walmart",
      tld: "com",
      id: "926485654",
      valid: true,
    },
    "Valid Product Url returns a full result object"
  );

  Assert.deepEqual(
    ShoppingProduct.fromURL(new URL("http://walmart.com/ip/926485654")),
    {
      host: "walmart.com",
      sitename: "walmart",
      tld: "com",
      id: "926485654",
      valid: true,
    },
    "Protocol is not checked"
  );
});

add_task(function test_product_isProduct() {
  let product = {
    host: "walmart.com",
    sitename: "walmart",
    tld: "com",
    id: "926485654",
    valid: true,
  };
  Assert.equal(
    ShoppingProduct.isProduct(product),
    true,
    "Passing a Product object returns true"
  );
  Assert.equal(
    ShoppingProduct.isProduct({ host: "walmart.com", sitename: "walmart" }),
    false,
    "Passing an incomplete ShoppingProduct object returns false"
  );
  Assert.equal(
    ShoppingProduct.isProduct(),
    false,
    "Passing nothing returns false"
  );
});

add_task(function test_amazon_product_urls() {
  let product;
  let url_com = new URL(
    "https://www.amazon.com/Furmax-Electric-Adjustable-Standing-Computer/dp/B09TJGHL5F/"
  );
  let url_ca = new URL(
    "https://www.amazon.ca/JBL-Flip-Essential-Waterproof-Bluetooth/dp/B0C3NNGWFN/"
  );
  let url_uk = new URL(
    "https://www.amazon.co.uk/placeholder_title/dp/B0B8KGPHS7/"
  );
  let url_content = new URL("https://www.amazon.com/stores/node/20648519011");

  product = ShoppingProduct.fromURL(url_com);
  Assert.equal(ShoppingProduct.isProduct(product), true, "Url is a product");
  Assert.equal(product.id, "B09TJGHL5F", "Product id was found in Url");

  product = ShoppingProduct.fromURL(url_ca);
  Assert.equal(
    ShoppingProduct.isProduct(product),
    false,
    "Url is not a supported tld"
  );

  product = ShoppingProduct.fromURL(url_uk);
  Assert.equal(
    ShoppingProduct.isProduct(product),
    false,
    "Url is not a supported tld"
  );

  product = ShoppingProduct.fromURL(url_content);
  Assert.equal(
    ShoppingProduct.isProduct(product),
    false,
    "Url is not a product"
  );
});

add_task(function test_walmart_product_urls() {
  let product;
  let url_com = new URL(
    "https://www.walmart.com/ip/Kent-Bicycles-29-Men-s-Trouvaille-Mountain-Bike-Medium-Black-and-Taupe/823391155"
  );
  let url_ca = new URL(
    "https://www.walmart.ca/en/ip/cherries-jumbo/6000187473587"
  );
  let url_content = new URL(
    "https://www.walmart.com/browse/food/grilling-foods/976759_1567409_8808777"
  );

  product = ShoppingProduct.fromURL(url_com);
  Assert.equal(ShoppingProduct.isProduct(product), true, "Url is a product");
  Assert.equal(product.id, "823391155", "Product id was found in Url");

  product = ShoppingProduct.fromURL(url_ca);
  Assert.equal(
    ShoppingProduct.isProduct(product),
    false,
    "Url is not a valid tld"
  );

  product = ShoppingProduct.fromURL(url_content);
  Assert.equal(
    ShoppingProduct.isProduct(product),
    false,
    "Url is not a product"
  );
});

add_task(function test_bestbuy_product_urls() {
  let product;
  let url_com = new URL(
    "https://www.bestbuy.com/site/ge-profile-ultrafast-4-8-cu-ft-large-capacity-all-in-one-washer-dryer-combo-with-ventless-heat-pump-technology-carbon-graphite/6530134.p?skuId=6530134"
  );
  let url_ca = new URL(
    "https://www.bestbuy.ca/en-ca/product/segway-ninebot-kickscooter-f40-electric-scooter-40km-range-30km-h-top-speed-dark-grey/15973012"
  );
  let url_content = new URL(
    "https://www.bestbuy.com/site/home-appliances/major-appliances-sale-event/pcmcat321600050000.c"
  );

  product = ShoppingProduct.fromURL(url_com);
  Assert.equal(ShoppingProduct.isProduct(product), true, "Url is a product");
  Assert.equal(product.id, "6530134.p", "Product id was found in Url");

  product = ShoppingProduct.fromURL(url_ca);
  Assert.equal(
    ShoppingProduct.isProduct(product),
    false,
    "Url is not a valid tld"
  );

  product = ShoppingProduct.fromURL(url_content);
  Assert.equal(
    ShoppingProduct.isProduct(product),
    false,
    "Url is not a product"
  );
});

add_task(function test_isProductURL() {
  let product_string =
    "https://www.amazon.com/Furmax-Electric-Adjustable-Standing-Computer/dp/B09TJGHL5F/";
  let product_url = new URL(product_string);
  let product_uri = Services.io.newURI(product_string);
  Assert.equal(
    isProductURL(product_url),
    true,
    "Passing a product URL returns true"
  );
  Assert.equal(
    isProductURL(product_uri),
    true,
    "Passing a product URI returns true"
  );

  let content_string =
    "https://www.walmart.com/browse/food/grilling-foods/976759_1567409_8808777";
  let content_url = new URL(content_string);
  let content_uri = Services.io.newURI(content_string);
  Assert.equal(
    isProductURL(content_url),
    false,
    "Passing a content URL returns false"
  );
  Assert.equal(
    isProductURL(content_uri),
    false,
    "Passing a content URI returns false"
  );

  Assert.equal(isProductURL(), false, "Passing nothing returns false");

  Assert.equal(isProductURL(1234), false, "Passing a number returns false");

  Assert.equal(
    isProductURL("1234"),
    false,
    "Passing a junk string returns false"
  );
});

add_task(async function test_product_uninit() {
  let product = new ShoppingProduct();

  Assert.equal(
    product._abortController.signal.aborted,
    false,
    "Abort signal is false"
  );

  product.uninit();

  Assert.equal(
    product._abortController.signal.aborted,
    true,
    "Abort signal is given after uninit"
  );
});
