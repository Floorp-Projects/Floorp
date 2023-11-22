/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
/* global createHttpServer, readFile */
const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

function BinaryHttpResponse(status, headerNames, headerValues, content) {
  this.status = status;
  this.headerNames = headerNames;
  this.headerValues = headerValues;
  this.content = content;
}

BinaryHttpResponse.prototype = {
  QueryInterface: ChromeUtils.generateQI(["nsIBinaryHttpResponse"]),
};

const {
  ANALYSIS_RESPONSE_SCHEMA,
  ANALYSIS_REQUEST_SCHEMA,
  RECOMMENDATIONS_RESPONSE_SCHEMA,
  RECOMMENDATIONS_REQUEST_SCHEMA,
  ATTRIBUTION_RESPONSE_SCHEMA,
  ATTRIBUTION_REQUEST_SCHEMA,
  ANALYZE_RESPONSE_SCHEMA,
  ANALYZE_REQUEST_SCHEMA,
  ANALYSIS_STATUS_RESPONSE_SCHEMA,
  ANALYSIS_STATUS_REQUEST_SCHEMA,
} = ChromeUtils.importESModule(
  "chrome://global/content/shopping/ProductConfig.mjs"
);

const { ShoppingProduct } = ChromeUtils.importESModule(
  "chrome://global/content/shopping/ShoppingProduct.mjs"
);

const ANALYSIS_API_MOCK = "http://example.com/api/analysis_response.json";
const RECOMMENDATIONS_API_MOCK =
  "http://example.com/api/recommendations_response.json";
const ATTRIBUTION_API_MOCK = "http://example.com/api/attribution_response.json";
const ANALYSIS_API_MOCK_INVALID =
  "http://example.com/api/invalid_analysis_response.json";
const API_SERVICE_UNAVAILABLE =
  "http://example.com/errors/service_unavailable.json";
const API_ERROR_ONCE = "http://example.com/errors/error_once.json";
const API_ERROR_BAD_REQUEST = "http://example.com/errors/bad_request.json";
const API_ERROR_UNPROCESSABLE =
  "http://example.com/errors/unprocessable_entity.json";
const API_ERROR_TOO_MANY_REQUESTS =
  "http://example.com/errors/too_many_requests.json";
const API_POLL = "http://example.com/poll/poll_analysis_response.json";
const API_ANALYSIS_IN_PROGRESS =
  "http://example.com/poll/analysis_in_progress.json";
const REPORTING_API_MOCK = "http://example.com/api/report_response.json";
const ANALYZE_API_MOCK = "http://example.com/api/analyze_pending.json";

const TEST_AID =
  "1ALhiNLkZ2yR4al5lcP1Npbtlpl5toDfKRgJOATjeieAL6i5Dul99l9+ZTiIWyybUzGysChAdrOA6BWrMqr0EvjoymiH3veZ++XuOvJnC0y1NB/IQQtUzlYEO028XqVUJWJeJte47nPhnK2pSm2QhbdeKbxEnauKAty1cFQeEaBUP7LkvUgxh1GDzflwcVfuKcgMr7hOM3NzjYR2RN3vhmT385Ps4wUj--cv2ucc+1nozldFrl--i9GYyjuHYFFi+EgXXZ3ZsA==";

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

// Too many requests to the API.
server.registerPathHandler(
  new URL(API_ERROR_TOO_MANY_REQUESTS).pathname,
  (request, response) => {
    response.setStatusLine(request.httpVersion, 429, "Too many requests");
    response.setHeader(
      "Content-Type",
      "application/json; charset=utf-8",
      false
    );
    response.write(readFile("data/too_many_requests.json", false));
  }
);

// Path to test API call that will still be processing twice and then succeeded.
let pollingTries = 0;
server.registerPathHandler(new URL(API_POLL).pathname, (request, response) => {
  response.setHeader("Content-Type", "application/json; charset=utf-8", false);
  if (pollingTries == 0) {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.write(readFile("/data/analysis_status_pending_response.json"));
    pollingTries++;
  } else if (pollingTries == 1) {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.write(readFile("/data/analysis_status_in_progress_response.json"));
    pollingTries++;
  } else {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.write(readFile("/data/analysis_status_completed_response.json"));
    pollingTries = 0;
  }
});

// Path to test API call that will always need analysis.
server.registerPathHandler(
  new URL(API_ANALYSIS_IN_PROGRESS).pathname,
  (request, response) => {
    response.setHeader(
      "Content-Type",
      "application/json; charset=utf-8",
      false
    );
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.write(readFile("/data/analysis_status_in_progress_response.json"));
  }
);

let ohttp = Cc["@mozilla.org/network/oblivious-http;1"].getService(
  Ci.nsIObliviousHttp
);
let ohttpServer = ohttp.server();

server.registerPathHandler(
  new URL(API_OHTTP_CONFIG).pathname,
  (request, response) => {
    let bstream = Cc["@mozilla.org/binaryoutputstream;1"].createInstance(
      Ci.nsIBinaryOutputStream
    );
    bstream.setOutputStream(response.bodyOutputStream);
    bstream.writeByteArray(ohttpServer.encodedConfig);
  }
);

let gExpectedOHTTPMethod = "POST";
let gExpectedProductDetails;
server.registerPathHandler(
  new URL(API_OHTTP_RELAY).pathname,
  async (request, response) => {
    let inputStream = Cc["@mozilla.org/binaryinputstream;1"].createInstance(
      Ci.nsIBinaryInputStream
    );
    inputStream.setInputStream(request.bodyInputStream);
    let requestBody = inputStream.readByteArray(inputStream.available());
    let ohttpRequest = ohttpServer.decapsulate(requestBody);
    let bhttp = Cc["@mozilla.org/network/binary-http;1"].getService(
      Ci.nsIBinaryHttp
    );
    let decodedRequest = bhttp.decodeRequest(ohttpRequest.request);
    Assert.equal(
      decodedRequest.method,
      gExpectedOHTTPMethod,
      "Should get expected HTTP method"
    );
    Assert.deepEqual(decodedRequest.headerNames.sort(), [
      "Accept",
      "Content-Type",
    ]);
    Assert.deepEqual(decodedRequest.headerValues, [
      "application/json",
      "application/json",
    ]);
    if (gExpectedOHTTPMethod == "POST") {
      Assert.equal(
        new TextDecoder().decode(new Uint8Array(decodedRequest.content)),
        gExpectedProductDetails,
        "Expected body content."
      );
    }

    response.processAsync();
    let innerResponse = await fetch("http://example.com" + decodedRequest.path);
    let bytes = new Uint8Array(await innerResponse.arrayBuffer());
    let binaryResponse = new BinaryHttpResponse(
      innerResponse.status,
      ["Content-Type"],
      ["application/json"],
      bytes
    );
    let encResponse = ohttpRequest.encapsulate(
      bhttp.encodeResponse(binaryResponse)
    );
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.setHeader("Content-Type", "message/ohttp-res", false);

    let bstream = Cc["@mozilla.org/binaryoutputstream;1"].createInstance(
      Ci.nsIBinaryOutputStream
    );
    bstream.setOutputStream(response.bodyOutputStream);
    bstream.writeByteArray(encResponse);
    response.finish();
  }
);

add_task(async function test_product_requestAnalysis() {
  let uri = new URL("https://www.walmart.com/ip/926485654");
  let product = new ShoppingProduct(uri, { allowValidationFailure: false });

  Assert.ok(product.isProduct(), "Should recognize a valid product.");

  let analysis = await product.requestAnalysis(undefined, {
    url: ANALYSIS_API_MOCK,
    requestSchema: ANALYSIS_REQUEST_SCHEMA,
    responseSchema: ANALYSIS_RESPONSE_SCHEMA,
  });

  Assert.ok(
    typeof analysis == "object",
    "Analysis object is loaded from JSON and validated"
  );
});

add_task(async function test_product_requestAnalysis_OHTTP() {
  let uri = new URL("https://www.walmart.com/ip/926485654");
  let product = new ShoppingProduct(uri, { allowValidationFailure: false });

  Assert.ok(product.isProduct(), "Should recognize a valid product.");

  gExpectedProductDetails = JSON.stringify({
    product_id: "926485654",
    website: "walmart.com",
  });

  enableOHTTP();

  let analysis = await product.requestAnalysis(undefined, {
    url: ANALYSIS_API_MOCK,
    requestSchema: ANALYSIS_REQUEST_SCHEMA,
    responseSchema: ANALYSIS_RESPONSE_SCHEMA,
  });

  Assert.deepEqual(
    analysis,
    await fetch(ANALYSIS_API_MOCK).then(r => r.json()),
    "Analysis object is loaded from JSON and validated"
  );

  disableOHTTP();
});

add_task(async function test_product_requestAnalysis_invalid() {
  let uri = new URL("https://www.walmart.com/ip/926485654");
  let product = new ShoppingProduct(uri, { allowValidationFailure: false });

  Assert.ok(product.isProduct(), "Should recognize a valid product.");
  let analysis = await product.requestAnalysis(undefined, {
    url: ANALYSIS_API_MOCK_INVALID,
    requestSchema: ANALYSIS_REQUEST_SCHEMA,
    responseSchema: ANALYSIS_RESPONSE_SCHEMA,
  });

  Assert.equal(analysis, undefined, "Analysis object is invalidated");
});

add_task(async function test_product_requestAnalysis_broken_config() {
  let uri = new URL("https://www.walmart.com/ip/926485654");
  let product = new ShoppingProduct(uri, { allowValidationFailure: false });

  Assert.ok(product.isProduct(), "Should recognize a valid product.");

  gExpectedProductDetails = JSON.stringify({
    product_id: "926485654",
    website: "walmart.com",
  });

  enableOHTTP("http://example.com/thisdoesntexist");

  let analysis = await product.requestAnalysis(undefined, {
    url: ANALYSIS_API_MOCK,
    requestSchema: ANALYSIS_REQUEST_SCHEMA,
    responseSchema: ANALYSIS_RESPONSE_SCHEMA,
  });

  // Because the config is missing, the OHTTP request can't be constructed,
  // so we should fail.
  Assert.equal(analysis, undefined, "Analysis object is invalidated");

  disableOHTTP();
});

add_task(async function test_product_requestAnalysis_invalid_ohttp() {
  let uri = new URL("https://www.walmart.com/ip/926485654");
  let product = new ShoppingProduct(uri, { allowValidationFailure: false });

  Assert.ok(product.isProduct(), "Should recognize a valid product.");

  gExpectedProductDetails = JSON.stringify({
    product_id: "926485654",
    website: "walmart.com",
  });

  enableOHTTP();

  let analysis = await product.requestAnalysis(undefined, {
    url: ANALYSIS_API_MOCK_INVALID,
    requestSchema: ANALYSIS_REQUEST_SCHEMA,
    responseSchema: ANALYSIS_RESPONSE_SCHEMA,
  });

  Assert.equal(analysis, undefined, "Analysis object is invalidated");

  disableOHTTP();
});

add_task(async function test_product_requestRecommendations() {
  let uri = new URL("https://www.walmart.com/ip/926485654");
  let product = new ShoppingProduct(uri, { allowValidationFailure: false });
  if (product.isProduct()) {
    let recommendations = await product.requestRecommendations(undefined, {
      url: RECOMMENDATIONS_API_MOCK,
      requestSchema: RECOMMENDATIONS_REQUEST_SCHEMA,
      responseSchema: RECOMMENDATIONS_RESPONSE_SCHEMA,
    });
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
  let sandbox = sinon.createSandbox();
  let spy = sandbox.spy(ShoppingProduct, "request");
  let startTime = Cu.now();
  let totalTime = TEST_TIMEOUT * Math.pow(2, RETRIES - 1);

  if (product.isProduct()) {
    let analysis = await product.requestAnalysis(undefined, {
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
  sandbox.restore();
});

add_task(async function test_product_requestAnalysis_retry_success() {
  let uri = new URL("https://www.walmart.com/ip/926485654");
  let product = new ShoppingProduct(uri, { allowValidationFailure: false });
  let sandbox = sinon.createSandbox();
  let spy = sandbox.spy(ShoppingProduct, "request");
  // Make sure API error count is reset
  apiErrors = 0;
  if (product.isProduct()) {
    let analysis = await product.requestAnalysis(undefined, {
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
  sandbox.restore();
});

add_task(async function test_product_bad_request() {
  let uri = new URL("https://www.walmart.com/ip/926485654");
  let product = new ShoppingProduct(uri, { allowValidationFailure: false });

  if (product.isProduct()) {
    let errorResult = await product.requestAnalysis(undefined, {
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
    let errorResult = await product.requestAnalysis(undefined, {
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

add_task(async function test_ohttp_headers() {
  let uri = new URL("https://www.walmart.com/ip/926485654");
  let product = new ShoppingProduct(uri, { allowValidationFailure: false });

  Assert.ok(product.isProduct(), "Should recognize a valid product.");

  gExpectedProductDetails = JSON.stringify({
    product_id: "926485654",
    website: "walmart.com",
  });

  enableOHTTP();

  let configURL = Services.prefs.getCharPref("toolkit.shopping.ohttpConfigURL");
  let config = await ShoppingProduct.getOHTTPConfig(configURL);
  Assert.ok(config, "Should have gotten a config.");
  let ohttpDetails = await ShoppingProduct.ohttpRequest(
    API_OHTTP_RELAY,
    config,
    ANALYSIS_API_MOCK,
    {
      method: "POST",
      body: gExpectedProductDetails,
      headers: {
        Accept: "application/json",
        "Content-Type": "application/json",
      },
      signal: new AbortController().signal,
    }
  );
  Assert.equal(ohttpDetails.status, 200, "Request should return 200 OK.");
  Assert.ok(ohttpDetails.ok, "Request should succeed.");
  let responseHeaders = ohttpDetails.headers;
  Assert.deepEqual(
    responseHeaders,
    { "content-type": "application/json" },
    "Should have expected response headers."
  );
  disableOHTTP();
});

add_task(async function test_ohttp_too_many_requests() {
  let uri = new URL("https://www.walmart.com/ip/926485654");
  let product = new ShoppingProduct(uri, { allowValidationFailure: false });

  Assert.ok(product.isProduct(), "Should recognize a valid product.");

  gExpectedProductDetails = JSON.stringify({
    product_id: "926485654",
    website: "walmart.com",
  });

  enableOHTTP();

  let configURL = Services.prefs.getCharPref("toolkit.shopping.ohttpConfigURL");
  let config = await ShoppingProduct.getOHTTPConfig(configURL);
  Assert.ok(config, "Should have gotten a config.");
  let ohttpDetails = await ShoppingProduct.ohttpRequest(
    API_OHTTP_RELAY,
    config,
    API_ERROR_TOO_MANY_REQUESTS,
    {
      method: "POST",
      body: gExpectedProductDetails,
      headers: {
        Accept: "application/json",
        "Content-Type": "application/json",
      },
      signal: new AbortController().signal,
    }
  );
  Assert.equal(ohttpDetails.status, 429, "Request should return 429.");
  Assert.equal(ohttpDetails.ok, false, "Request should not be ok.");

  disableOHTTP();
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

add_task(async function test_product_sendAttributionEvent_impression() {
  let uri = new URL("https://www.walmart.com/ip/926485654");
  let product = new ShoppingProduct(uri, { allowValidationFailure: false });
  if (product.isProduct()) {
    let event = await ShoppingProduct.sendAttributionEvent(
      "impression",
      TEST_AID,
      "firefox_toolkit_tests",
      {
        url: ATTRIBUTION_API_MOCK,
        requestSchema: ATTRIBUTION_REQUEST_SCHEMA,
        responseSchema: ATTRIBUTION_RESPONSE_SCHEMA,
      }
    );
    Assert.deepEqual(
      event,
      await fetch(ATTRIBUTION_API_MOCK).then(r => r.json()),
      "Events object is loaded from JSON and validated"
    );
  }
});

add_task(async function test_product_sendAttributionEvent_click() {
  let uri = new URL("https://www.walmart.com/ip/926485654");
  let product = new ShoppingProduct(uri, { allowValidationFailure: false });
  if (product.isProduct()) {
    let event = await ShoppingProduct.sendAttributionEvent(
      "click",
      TEST_AID,
      "firefox_toolkit_tests",
      {
        url: ATTRIBUTION_API_MOCK,
        requestSchema: ATTRIBUTION_REQUEST_SCHEMA,
        responseSchema: ATTRIBUTION_RESPONSE_SCHEMA,
      }
    );
    Assert.deepEqual(
      event,
      await fetch(ATTRIBUTION_API_MOCK).then(r => r.json()),
      "Events object is loaded from JSON and validated"
    );
  }
});

add_task(async function test_product_sendAttributionEvent_impression_OHTTP() {
  let uri = new URL("https://www.walmart.com/ip/926485654");
  let product = new ShoppingProduct(uri, { allowValidationFailure: false });

  Assert.ok(product.isProduct(), "Should recognize a valid product.");

  gExpectedProductDetails = JSON.stringify({
    event_source: "firefox_toolkit_tests",
    event_name: "trusted_deals_impression",
    aidvs: [TEST_AID],
  });

  enableOHTTP();

  let event = await ShoppingProduct.sendAttributionEvent(
    "impression",
    TEST_AID,
    "firefox_toolkit_tests",
    {
      url: ATTRIBUTION_API_MOCK,
      requestSchema: ATTRIBUTION_REQUEST_SCHEMA,
      responseSchema: ATTRIBUTION_RESPONSE_SCHEMA,
    }
  );

  Assert.deepEqual(
    event,
    await fetch(ATTRIBUTION_API_MOCK).then(r => r.json()),
    "Events object is loaded from JSON and validated"
  );

  disableOHTTP();
});

add_task(async function test_product_sendAttributionEvent_click_OHTTP() {
  let uri = new URL("https://www.walmart.com/ip/926485654");
  let product = new ShoppingProduct(uri, { allowValidationFailure: false });

  Assert.ok(product.isProduct(), "Should recognize a valid product.");

  gExpectedProductDetails = JSON.stringify({
    event_source: "firefox_toolkit_tests",
    event_name: "trusted_deals_link_clicked",
    aid: TEST_AID,
  });

  enableOHTTP();

  let event = await ShoppingProduct.sendAttributionEvent(
    "click",
    TEST_AID,
    "firefox_toolkit_tests",
    {
      url: ATTRIBUTION_API_MOCK,
      requestSchema: ATTRIBUTION_REQUEST_SCHEMA,
      responseSchema: ATTRIBUTION_RESPONSE_SCHEMA,
    }
  );

  Assert.deepEqual(
    event,
    await fetch(ATTRIBUTION_API_MOCK).then(r => r.json()),
    "Events object is loaded from JSON and validated"
  );

  disableOHTTP();
});

add_task(async function test_product_requestAnalysis_poll() {
  let uri = new URL("https://www.walmart.com/ip/926485654");
  let product = new ShoppingProduct(uri, { allowValidationFailure: false });
  let sandbox = sinon.createSandbox();
  let spy = sandbox.spy(ShoppingProduct, "request");
  let startTime = Cu.now();
  const INITIAL_TIMEOUT = 100;
  const TIMEOUT = 50;
  const TRIES = 10;
  let totalTime = INITIAL_TIMEOUT + TIMEOUT;

  pollingTries = 0;
  if (!product.isProduct()) {
    return;
  }
  let analysis = await product.pollForAnalysisCompleted({
    url: API_POLL,
    requestSchema: ANALYSIS_STATUS_REQUEST_SCHEMA,
    responseSchema: ANALYSIS_STATUS_RESPONSE_SCHEMA,
    pollInitialWait: INITIAL_TIMEOUT,
    pollTimeout: TIMEOUT,
    pollAttempts: TRIES,
  });

  Assert.equal(spy.callCount, 3, "Request is done processing");
  Assert.ok(
    typeof analysis == "object",
    "Analysis object is loaded from JSON and validated"
  );
  Assert.equal(analysis.status, "completed", "Analysis is completed");
  Assert.equal(analysis.progress, 100.0, "Progress is 100%");
  Assert.ok(
    Cu.now() - startTime >= totalTime,
    `Waited for at least ${totalTime}ms`
  );

  sandbox.restore();
});

add_task(async function test_product_requestAnalysis_poll_max() {
  let uri = new URL("https://www.walmart.com/ip/926485654");
  let product = new ShoppingProduct(uri, { allowValidationFailure: false });
  let sandbox = sinon.createSandbox();
  let spy = sandbox.spy(ShoppingProduct, "request");
  let startTime = Cu.now();

  const INITIAL_TIMEOUT = 100;
  const TIMEOUT = 50;
  const TRIES = 4;
  let totalTime = INITIAL_TIMEOUT + TIMEOUT * 3;

  pollingTries = 0;
  if (!product.isProduct()) {
    return;
  }
  let analysis = await product.pollForAnalysisCompleted({
    url: API_ANALYSIS_IN_PROGRESS,
    requestSchema: ANALYSIS_STATUS_REQUEST_SCHEMA,
    responseSchema: ANALYSIS_STATUS_RESPONSE_SCHEMA,
    pollInitialWait: INITIAL_TIMEOUT,
    pollTimeout: TIMEOUT,
    pollAttempts: TRIES,
  });

  Assert.equal(spy.callCount, TRIES, "Request is done processing");
  Assert.ok(
    typeof analysis == "object",
    "Analysis object is loaded from JSON and validated"
  );
  Assert.equal(analysis.status, "in_progress", "Analysis not done");
  Assert.ok(
    Cu.now() - startTime >= totalTime,
    `Waited for at least ${totalTime}ms`
  );
  sandbox.restore();
});

add_task(async function test_product_requestAnalysisCreationStatus() {
  let uri = new URL("https://www.walmart.com/ip/926485654");
  let product = new ShoppingProduct(uri, { allowValidationFailure: false });
  if (!product.isProduct()) {
    return;
  }
  let analysis = await product.requestAnalysisCreationStatus(undefined, {
    url: API_ANALYSIS_IN_PROGRESS,
    requestSchema: ANALYSIS_STATUS_REQUEST_SCHEMA,
    responseSchema: ANALYSIS_STATUS_RESPONSE_SCHEMA,
  });
  Assert.ok(
    typeof analysis == "object",
    "Analysis object is loaded from JSON and validated"
  );
  Assert.equal(analysis.status, "in_progress", "Analysis is in progress");
  Assert.equal(analysis.progress, 50.0, "Progress is 50%");
});

add_task(async function test_product_requestCreateAnalysis() {
  let uri = new URL("https://www.walmart.com/ip/926485654");
  let product = new ShoppingProduct(uri, { allowValidationFailure: false });
  if (!product.isProduct()) {
    return;
  }
  let analysis = await product.requestCreateAnalysis(undefined, {
    url: ANALYZE_API_MOCK,
    requestSchema: ANALYZE_REQUEST_SCHEMA,
    responseSchema: ANALYZE_RESPONSE_SCHEMA,
  });
  Assert.ok(
    typeof analysis == "object",
    "Analyze object is loaded from JSON and validated"
  );
  Assert.equal(analysis.status, "pending", "Analysis is pending");
});

add_task(async function test_product_sendReport() {
  let uri = new URL("https://www.walmart.com/ip/926485654");
  let product = new ShoppingProduct(uri, { allowValidationFailure: false });

  Assert.ok(product.isProduct(), "Should recognize a valid product.");

  let report = await product.sendReport(undefined, {
    url: REPORTING_API_MOCK,
  });

  Assert.ok(
    typeof report == "object",
    "Report object is loaded from JSON and validated"
  );
  Assert.equal(report.message, "report created", "Report is created.");
});

add_task(async function test_product_sendReport_OHTTP() {
  let uri = new URL("https://www.walmart.com/ip/926485654");
  let product = new ShoppingProduct(uri, { allowValidationFailure: false });

  Assert.ok(product.isProduct(), "Should recognize a valid product.");

  gExpectedProductDetails = JSON.stringify({
    product_id: "926485654",
    website: "walmart.com",
  });

  enableOHTTP();

  let report = await product.sendReport(undefined, {
    url: REPORTING_API_MOCK,
  });

  Assert.ok(
    typeof report == "object",
    "Report object is loaded from JSON and validated"
  );
  Assert.equal(report.message, "report created", "Report is created.");
  disableOHTTP();
});

add_task(async function test_product_analysisProgress_event() {
  let uri = new URL("https://www.walmart.com/ip/926485654");
  let product = new ShoppingProduct(uri, { allowValidationFailure: false });

  const INITIAL_TIMEOUT = 0;
  const TIMEOUT = 0;
  const TRIES = 1;

  if (!product.isProduct()) {
    return;
  }

  let analysisProgressEventData;
  product.on("analysis-progress", (eventName, progress) => {
    analysisProgressEventData = progress;
  });

  await product.pollForAnalysisCompleted({
    url: API_ANALYSIS_IN_PROGRESS,
    requestSchema: ANALYSIS_STATUS_REQUEST_SCHEMA,
    responseSchema: ANALYSIS_STATUS_RESPONSE_SCHEMA,
    pollInitialWait: INITIAL_TIMEOUT,
    pollTimeout: TIMEOUT,
    pollAttempts: TRIES,
  });

  Assert.equal(
    analysisProgressEventData,
    50,
    "Analysis progress event data is emitted"
  );
});
