/* globals sinon */
"use strict";

ChromeUtils.import("resource://gre/modules/CanonicalJSON.jsm", this);
ChromeUtils.import("resource://gre/modules/osfile.jsm", this);
ChromeUtils.import("resource://normandy/lib/NormandyApi.jsm", this);

load("utils.js"); /* globals withMockApiServer, MockResponse, withScriptServer, withServer, makeMockApiServer */

add_task(withMockApiServer(async function test_get(serverUrl) {
  // Test that NormandyApi can fetch from the test server.
  const response = await NormandyApi.get(`${serverUrl}/api/v1/`);
  const data = await response.json();
  equal(data["recipe-list"], "/api/v1/recipe/", "Expected data in response");
}));

add_task(withMockApiServer(async function test_getApiUrl(serverUrl) {
  const apiBase = `${serverUrl}/api/v1`;
  // Test that NormandyApi can use the self-describing API's index
  const recipeListUrl = await NormandyApi.getApiUrl("action-list");
  equal(recipeListUrl, `${apiBase}/action/`, "Can retrieve action-list URL from API");
}));

add_task(withMockApiServer(async function test_getApiUrlSlashes(serverUrl, preferences) {
  const fakeResponse = new MockResponse(JSON.stringify({"test-endpoint": `${serverUrl}/test/`}));
  const mockGet = sinon.stub(NormandyApi, "get").callsFake(async () => fakeResponse);

  // without slash
  {
    NormandyApi.clearIndexCache();
    preferences.set("app.normandy.api_url", `${serverUrl}/api/v1`);
    const endpoint = await NormandyApi.getApiUrl("test-endpoint");
    equal(endpoint, `${serverUrl}/test/`);
    ok(mockGet.calledWithExactly(`${serverUrl}/api/v1/`), "trailing slash was added");
    mockGet.resetHistory();
  }

  // with slash
  {
    NormandyApi.clearIndexCache();
    preferences.set("app.normandy.api_url", `${serverUrl}/api/v1/`);
    const endpoint = await NormandyApi.getApiUrl("test-endpoint");
    equal(endpoint, `${serverUrl}/test/`);
    ok(mockGet.calledWithExactly(`${serverUrl}/api/v1/`), "existing trailing slash was preserved");
    mockGet.resetHistory();
  }

  NormandyApi.clearIndexCache();
  mockGet.restore();
}));

add_task(withMockApiServer(async function test_fetchRecipes() {
  const recipes = await NormandyApi.fetchRecipes();
  equal(recipes.length, 1);
  equal(recipes[0].name, "system-addon-test");
}));

add_task(async function test_fetchSignedObjects_canonical_mismatch() {
  const getApiUrl = sinon.stub(NormandyApi, "getApiUrl");

  // The object is non-canonical (it has whitespace, properties are out of order)
  const response = new MockResponse(`[
    {
      "object": {"b": 1, "a": 2},
      "signature": {"signature": "", "x5u": ""}
    }
  ]`);
  const get = sinon.stub(NormandyApi, "get").resolves(response);

  try {
    await NormandyApi.fetchSignedObjects("object");
    ok(false, "fetchSignedObjects did not throw for canonical JSON mismatch");
  } catch (err) {
    ok(err instanceof NormandyApi.InvalidSignatureError, "Error is an InvalidSignatureError");
    ok(/Canonical/.test(err), "Error is due to canonical JSON mismatch");
  }

  getApiUrl.restore();
  get.restore();
});

// Test validation errors due to validation throwing an exception (e.g. when
// parameters passed to validation are malformed).
add_task(withMockApiServer(async function test_fetchSignedObjects_validation_error() {
  const getApiUrl = sinon.stub(NormandyApi, "getApiUrl").resolves("http://localhost/object/");

  // Mock two URLs: object and the x5u
  const get = sinon.stub(NormandyApi, "get").callsFake(async url => {
    if (url.endsWith("object/")) {
      return new MockResponse(CanonicalJSON.stringify([
        {
          object: {a: 1, b: 2},
          signature: {signature: "invalidsignature", x5u: "http://localhost/x5u/"},
        },
      ]));
    } else if (url.endsWith("x5u/")) {
      return new MockResponse("certchain");
    }

    return null;
  });

  // Validation should fail due to a malformed x5u and signature.
  try {
    await NormandyApi.fetchSignedObjects("object");
    ok(false, "fetchSignedObjects did not throw for a validation error");
  } catch (err) {
    ok(err instanceof NormandyApi.InvalidSignatureError, "Error is an InvalidSignatureError");
    ok(/signature/.test(err), "Error is due to a validation error");
  }

  getApiUrl.restore();
  get.restore();
}));

// Test validation errors due to validation returning false (e.g. when parameters
// passed to validation are correctly formed, but not valid for the data).
const invalidSignatureServer = makeMockApiServer(do_get_file("invalid_recipe_signature_api"));
add_task(withServer(invalidSignatureServer, async function test_fetchSignedObjects_invalid_signature() {
  try {
    await NormandyApi.fetchSignedObjects("recipe");
    ok(false, "fetchSignedObjects did not throw for an invalid signature");
  } catch (err) {
    ok(err instanceof NormandyApi.InvalidSignatureError, "Error is an InvalidSignatureError");
    ok(/signature/.test(err), "Error is due to an invalid signature");
  }
}));

add_task(withMockApiServer(async function test_classifyClient() {
  const classification = await NormandyApi.classifyClient();
  Assert.deepEqual(classification, {
    country: "US",
    request_time: new Date("2017-02-22T17:43:24.657841Z"),
  });
}));

add_task(withMockApiServer(async function test_fetchActions() {
  const actions = await NormandyApi.fetchActions();
  equal(actions.length, 4);
  const actionNames = actions.map(a => a.name);
  ok(actionNames.includes("console-log"));
  ok(actionNames.includes("opt-out-study"));
  ok(actionNames.includes("show-heartbeat"));
  ok(actionNames.includes("preference-experiment"));
}));

add_task(withMockApiServer(async function test_fetchExtensionDetails() {
  const extensionDetails = await NormandyApi.fetchExtensionDetails(1);
  deepEqual(extensionDetails, {
    "id": 1,
    "name": "Normandy Fixture",
    "xpi": "http://example.com/browser/toolkit/components/normandy/test/browser/fixtures/normandy.xpi",
    "extension_id": "normandydriver@example.com",
    "version": "1.0",
    "hash": "ade1c14196ec4fe0aa0a6ba40ac433d7c8d1ec985581a8a94d43dc58991b5171",
    "hash_algorithm": "sha256",
  });
}));

add_task(withScriptServer("query_server.sjs", async function test_getTestServer(serverUrl) {
  // Test that NormandyApi can fetch from the test server.
  const response = await NormandyApi.get(serverUrl);
  const data = await response.json();
  Assert.deepEqual(data, {queryString: {}, body: {}}, "NormandyApi returned incorrect server data.");
}));

add_task(withScriptServer("query_server.sjs", async function test_getQueryString(serverUrl) {
  // Test that NormandyApi can send query string parameters to the test server.
  const response = await NormandyApi.get(serverUrl, {foo: "bar", baz: "biff"});
  const data = await response.json();
  Assert.deepEqual(
    data, {queryString: {foo: "bar", baz: "biff"}, body: {}},
    "NormandyApi sent an incorrect query string."
  );
}));

add_task(withScriptServer("query_server.sjs", async function test_postData(serverUrl) {
  // Test that NormandyApi can POST JSON-formatted data to the test server.
  const response = await NormandyApi.post(serverUrl, {foo: "bar", baz: "biff"});
  const data = await response.json();
  Assert.deepEqual(
    data, {queryString: {}, body: {foo: "bar", baz: "biff"}},
    "NormandyApi sent an incorrect query string."
  );
}));

add_task(withMockApiServer(async function test_fetchImplementation_itWorksWithRealData() {
  const [action] = await NormandyApi.fetchActions();
  const implementation = await NormandyApi.fetchImplementation(action);

  const decoder = new TextDecoder();
  const relativePath = `mock_api${action.implementation_url}`;
  const file = do_get_file(relativePath);
  const expected = decoder.decode(await OS.File.read(file.path));

  equal(implementation, expected);
}));

add_task(withScriptServer(
  "echo_server.sjs",
  async function test_fetchImplementationFail(serverUrl) {
    const action = {
      implementation_url: `${serverUrl}?status=500&body=servererror`,
    };

    try {
      await NormandyApi.fetchImplementation(action);
      ok(false, "fetchImplementation throws for non-200 response status codes");
    } catch (err) {
      // pass
    }
  },
));
