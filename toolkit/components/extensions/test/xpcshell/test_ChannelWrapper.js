"use strict";

/**
 * The ChannelWrapper API is part of the implementation of WebRequest, and not
 * really meant to be used in isolation. In practice, there are several in-tree
 * uses of ChannelWrapper, so this test serves as a sanity check that
 * ChannelWrapper behaves reasonable in the absence of WebRequest.
 */

const server = createHttpServer({
  hosts: ["origin.example.net", "example.com"],
});
server.registerPathHandler("/home", () => {});
server.registerPathHandler("/dummy", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader(
    "Access-Control-Allow-Origin",
    "http://origin.example.net"
  );
  response.write("Server's reply");
});

// Some properties do not have a straightforward comparison, so we just verify
// that the property is set (or not).
const EXPECT_TRUTHY = Symbol("EXPECT_TRUTHY");
const EXPECT_FALSEY = Symbol("EXPECT_FALSEY");

// Properties in the order specified in ChannelWrapper.webidl
const EXPECTATION_BASIC_FETCH = {
  id: EXPECT_TRUTHY,
  channel: EXPECT_TRUTHY,
  contentType: "",
  method: "GET",
  type: "xmlhttprequest",
  suspended: false,
  finalURI: EXPECT_TRUTHY,
  finalURL: "http://example.com/dummy",
  statusCode: 0,
  statusLine: "",
  errorString: null,
  onerror: null,
  onstart: null,
  onstop: null,
  proxyInfo: EXPECT_TRUTHY, // The xpcshell test server uses a proxy.
  remoteAddress: null, // Not set at start of request
  loadInfo: EXPECT_TRUTHY,
  isServiceWorkerScript: false,
  isSystemLoad: false, // To be removed in bug 1825882.
  originURL: "http://origin.example.net/home",
  documentURL: "http://origin.example.net/home",
  originURI: EXPECT_TRUTHY,
  documentURI: EXPECT_TRUTHY,
  canModify: true,
  frameId: 0, // Top-level frame.
  parentFrameId: -1,
  browserElement: EXPECT_TRUTHY,
  frameAncestors: [], // Top-level frame does not have ancestors.
  urlClassification: {
    firstParty: [],
    thirdParty: [],
  },
  thirdParty: true, // origin.example.net vs example.com is third-party.
  requestSize: 0, // Request not sent yet at start of request.
  responseSize: 0, // Response not received yet at start of request.
};

const EXPECTATION_BASIC_FETCH_COMPLETED = {
  ...EXPECTATION_BASIC_FETCH,
  contentType: "text/plain",
  statusCode: 200,
  statusLine: "HTTP/1.1 200 OK",
  remoteAddress: "127.0.0.1",
  browserElement: EXPECT_FALSEY,
  requestSize: EXPECT_TRUTHY,
  responseSize: EXPECT_TRUTHY,
};

const EXPECTATION_BASIC_FETCH_ABORTED = {
  ...EXPECTATION_BASIC_FETCH,
  errorString: "NS_ERROR_ABORT",
  browserElement: EXPECT_FALSEY,
};

// We don't really care about the values; the main purpose of checking these
// properties is to make sure that something reasonable happens. In particular,
// that we are not hitting assertion failures or crashes.
const EXPECTATION_INVALID_CHANNEL = {
  id: EXPECT_TRUTHY,
  channel: EXPECT_FALSEY,
  contentType: "",
  method: "",
  type: "other",
  suspended: false,
  finalURI: EXPECT_FALSEY,
  finalURL: "",
  statusCode: 0,
  statusLine: "",
  errorString: "NS_ERROR_UNEXPECTED",
  onerror: null,
  onstart: null,
  onstop: null,
  proxyInfo: null,
  remoteAddress: null,
  loadInfo: null,
  isServiceWorkerScript: false,
  isSystemLoad: false, // To be removed in bug 1825882.
  originURL: "",
  documentURL: "",
  originURI: null,
  documentURI: null,
  canModify: false,
  frameId: 0,
  parentFrameId: -1,
  browserElement: EXPECT_FALSEY,
  frameAncestors: null,
  urlClassification: {
    firstParty: [],
    thirdParty: [],
  },
  thirdParty: false,
  requestSize: 0,
  responseSize: 0,
};

function channelWrapperEquals(channelWrapper, expectedProps) {
  for (let [k, v] of Object.entries(expectedProps)) {
    if (v === EXPECT_TRUTHY) {
      Assert.ok(channelWrapper[k], `ChannelWrapper.${k} is truthy`);
    } else if (v === EXPECT_FALSEY) {
      Assert.ok(!channelWrapper[k], `ChannelWrapper.${k} is falsey`);
    } else {
      Assert.deepEqual(channelWrapper[k], v, `ChannelWrapper.${k}`);
    }
  }
}

let gContentPage;

async function forceChannelGC() {
  await Promise.resolve();
  Cu.forceGC();
  Cu.forceCC();
}

function checkChannelWrapperMethodsAfterGC(channelWrapper) {
  // All methods in the order of appearance in ChannelWrapper.webidl.
  // The exact behavior does not matter, as long as it is somewhat reasonable,
  // and in particular does not trigger assertions or crashes.

  const dummyURI = Services.io.newURI("http://example.com/neverloaded");
  const dummyPolicy = new WebExtensionPolicy({
    id: "@dummyPolicy",
    mozExtensionHostname: "c3c73091-8fab-4229-83cf-84c061dd9ead",
    baseURL: "resource://modules/whatever_does_not_need_to_exist",
    allowedOrigins: new MatchPatternSet(["*://*/*"]),
    localizeCallback: () => "",
  });

  Assert.throws(
    () => channelWrapper.cancel(0),
    /NS_ERROR_UNEXPECTED/,
    "channelWrapper.cancel() throws"
  );
  Assert.throws(
    () => channelWrapper.redirectTo(dummyURI),
    /NS_ERROR_UNEXPECTED/,
    "channelWrapper.redirectTo() throws"
  );
  Assert.throws(
    () => channelWrapper.upgradeToSecure(),
    /NS_ERROR_UNEXPECTED/,
    "channelWrapper.upgradeToSecure() throws"
  );
  Assert.throws(
    () => channelWrapper.suspend(""),
    /NS_ERROR_UNEXPECTED/,
    "channelWrapper.suspend() throws"
  );
  // resume() trivially returns because it is no-op when suspend() did not run.
  Assert.equal(
    channelWrapper.resume(),
    undefined,
    "channelWrapper.resume() returns"
  );
  Assert.equal(
    channelWrapper.matches({}, null, {}),
    false,
    "channelWrapper.matches() returns"
  );
  Assert.equal(
    channelWrapper.registerTraceableChannel(dummyPolicy, null),
    undefined,
    "registerTraceableChannel() returns"
  );
  Assert.equal(channelWrapper.errorCheck(), undefined, "errorCheck() returns");
  Assert.throws(
    () => channelWrapper.getRequestHeaders(),
    /NS_ERROR_UNEXPECTED/,
    "channelWrapper.getRequestHeaders() throws"
  );
  Assert.throws(
    () => channelWrapper.getRequestHeader("Content-Type"),
    /NS_ERROR_UNEXPECTED/,
    "channelWrapper.getRequestHeader() throws"
  );
  Assert.throws(
    () => channelWrapper.getResponseHeaders(),
    /NS_ERROR_UNEXPECTED/,
    "channelWrapper.getResponseHeaders() throws"
  );
  Assert.throws(
    () => channelWrapper.setRequestHeader("Content-Type", ""),
    /NS_ERROR_UNEXPECTED/,
    "channelWrapper.setRequestHeader() throws"
  );
  Assert.throws(
    () => channelWrapper.setResponseHeader("Content-Type", ""),
    /NS_ERROR_UNEXPECTED/,
    "channelWrapper.setResponseHeader() throws"
  );
}

function createChannel(url) {
  const dummyPrincipal = Services.scriptSecurityManager.createNullPrincipal({});
  return Services.io.newChannelFromURI(
    Services.io.newURI(url),
    /* loadingNode */ null,
    /* loadingPrincipal */ dummyPrincipal,
    /* triggeringPrincipal */ dummyPrincipal,
    /* securityFlags */ 0,
    /* contentPolicyType */ Ci.nsIContentPolicy.TYPE_FETCH
  );
}

function assertChannelWrapperUnsupportedForChannel(channel) {
  let channelWrapper = ChannelWrapper.get(channel);
  Assert.equal(
    channelWrapper.channel,
    null,
    `ChannelWrapper cannot wrap channel for ${channel.URI.spec}`
  );
  channelWrapperEquals(channelWrapper, EXPECTATION_INVALID_CHANNEL);
}

add_setup(async function create_content_page() {
  gContentPage = await ExtensionTestUtils.loadContentPage(
    "http://origin.example.net/home"
  );
  registerCleanupFunction(() => gContentPage.close());
});

add_task(async function during_basic_fetch() {
  let promise = TestUtils.topicObserved("http-on-modify-request", channel => {
    equal(channel.URI.spec, "http://example.com/dummy", "expected URL");
    let channelWrapper = ChannelWrapper.get(channel);

    channelWrapperEquals(channelWrapper, EXPECTATION_BASIC_FETCH);

    return true;
  });
  await gContentPage.spawn([], async () => {
    let res = await content.fetch("http://example.com/dummy");
    Assert.equal(await res.text(), "Server's reply", "Got response");
  });
  await promise;
});

add_task(async function after_basic_fetch() {
  let promise = TestUtils.topicObserved("http-on-modify-request");
  await gContentPage.spawn([], async () => {
    let res = await content.fetch("http://example.com/dummy");
    Assert.equal(await res.text(), "Server's reply", "Got response");
  });
  let [channel] = await promise;

  equal(channel.URI.spec, "http://example.com/dummy", "expected URL");
  let channelWrapper = ChannelWrapper.get(channel);

  channelWrapperEquals(channelWrapper, EXPECTATION_BASIC_FETCH_COMPLETED);
});

add_task(async function after_cancel_request() {
  let channelWrapper;
  let promise = TestUtils.topicObserved("http-on-modify-request", channel => {
    equal(channel.URI.spec, "http://example.com/dummy", "expected URL");
    channelWrapper = ChannelWrapper.get(channel);
    channel.cancel(Cr.NS_ERROR_ABORT);
    return true;
  });
  await gContentPage.spawn([], async () => {
    await Assert.rejects(
      content.fetch("http://example.com/dummy"),
      /NetworkError when attempting to fetch resource./,
      "Request should be aborted"
    );
  });
  await promise;

  channelWrapperEquals(channelWrapper, EXPECTATION_BASIC_FETCH_ABORTED);
});

add_task(async function after_basic_fetch_and_gc() {
  let promise = TestUtils.topicObserved("http-on-modify-request");
  await gContentPage.spawn([], async () => {
    let res = await content.fetch("http://example.com/dummy");
    Assert.equal(await res.text(), "Server's reply", "Got response");
  });
  let [channel] = await promise;

  equal(channel.URI.spec, "http://example.com/dummy", "expected URL");
  let channelWrapper = ChannelWrapper.get(channel);

  Assert.equal(channelWrapper.channel, channel, "channel not GC'd yet");

  channel = promise = null;
  await forceChannelGC();

  Assert.equal(channelWrapper.channel, null, "Channel has been GC'd");

  channelWrapperEquals(channelWrapper, EXPECTATION_INVALID_CHANNEL);
  checkChannelWrapperMethodsAfterGC(channelWrapper);
});

// getRegisteredChannel should be called before the response has started. In
// this test case we instantiate ChannelWrapper after the request completed,
// and confirm that there are no weird crashes or assertion failures.
add_task(async function getRegisteredChannel_after_response_start() {
  const dummyPolicy = new WebExtensionPolicy({
    id: "@dummyPolicy",
    mozExtensionHostname: "c3c73091-8fab-4229-83cf-84c061dd9ead",
    baseURL: "resource://modules/whatever_does_not_need_to_exist",
    allowedOrigins: new MatchPatternSet(["*://*/*"]),
    localizeCallback: () => "",
  });
  let promise = TestUtils.topicObserved("http-on-modify-request");
  await gContentPage.spawn([], async () => {
    let res = await content.fetch("http://example.com/dummy");
    Assert.equal(await res.text(), "Server's reply", "Got response");
  });
  let [channel] = await promise;

  equal(channel.URI.spec, "http://example.com/dummy", "expected URL");
  let channelWrapper = ChannelWrapper.get(channel);
  let channelId = channelWrapper.id;

  // NOTE: registerTraceableChannel() should return early when a channel is
  // past OnStartRequest, but if ChannelWrapper.get() is called after that,
  // then ChannelWrapper::mResponseStarted is not set, and the implementation
  // is unaware of the fact that the response has already started. While not
  // ideal, it may happen in practice, so confirm that there are no crashes or
  // assertion failures.
  channelWrapper.registerTraceableChannel(dummyPolicy, null);
  Assert.equal(
    ChannelWrapper.getRegisteredChannel(channelId, dummyPolicy, null),
    channelWrapper,
    "getRegisteredChannel() returns wrapper after registerTraceableChannel()"
  );
  // Reset internal cached fields via ChannelWrapper::SetChannel.
  channelWrapper.channel = channel;

  channel = promise = null;
  await forceChannelGC();

  Assert.equal(channelWrapper.channel, null, "Channel has been GC'd");
  Assert.equal(
    ChannelWrapper.getRegisteredChannel(channelId, dummyPolicy, null),
    null,
    "getRegisteredChannel() returns nothing after channel was GC'd"
  );
  channelWrapperEquals(channelWrapper, EXPECTATION_INVALID_CHANNEL);
  checkChannelWrapperMethodsAfterGC(channelWrapper);
});

add_task(async function ChannelWrapper_https_url() {
  // https: and http: are the only channels supported by WebRequest and
  // ChannelWrapper. http: was tested with real requests before, here we also
  // test https: just by simulating a channel for a https:-URL.
  const channel = createChannel("https://example.com/dummyhttps");
  let channelWrapper = ChannelWrapper.get(channel);
  Assert.equal(
    channelWrapper.channel,
    channel,
    "ChannelWrapper can wrap channel for https"
  );
  // The following two expectations are identical. The expectations are repeated
  // twice, to make it easier to see what the difference is between https vs
  // invalid, and https vs the http:-test elsewhere.
  channelWrapperEquals(channelWrapper, {
    ...EXPECTATION_INVALID_CHANNEL,
    channel,
    method: "GET",
    type: "xmlhttprequest",
    finalURI: EXPECT_TRUTHY,
    finalURL: "https://example.com/dummyhttps",
    errorString: null,
    loadInfo: EXPECT_TRUTHY,
    canModify: true,
    thirdParty: true, // null principal from createChannel vs example.com.
  });
  channelWrapperEquals(channelWrapper, {
    ...EXPECTATION_BASIC_FETCH,
    finalURL: "https://example.com/dummyhttps",
    proxyInfo: null,
    originURL: "", // triggeringPrincipal is null principal in createChannel.
    documentURL: "", // triggeringPrincipal is null principal in createChannel.
    originURI: null,
    documentURI: null,
    browserElement: null, // simulated load not associated with any <browser>.
    frameAncestors: null, // simulated load not associated with BrowsingContext.
  });
});

add_task(async function ChannelWrapper_moz_extension_url() {
  const xpi = AddonTestUtils.createTempWebExtensionFile({});
  const dummyPolicy = new WebExtensionPolicy({
    id: "@dummyPolicy",
    mozExtensionHostname: "e17d45dd-fe2a-4ece-8794-d487062cadf4",
    baseURL: `jar:${Services.io.newFileURI(xpi).spec}!/`,
    allowedOrigins: new MatchPatternSet(["*://*/*"]),
    localizeCallback: () => "",
  });
  dummyPolicy.active = true;
  const channel = createChannel(
    "moz-extension://e17d45dd-fe2a-4ece-8794-d487062cadf4/manifest.json"
  );
  Assert.ok(channel instanceof Ci.nsIJARChannel, "Is nsIJARChannel");
  assertChannelWrapperUnsupportedForChannel(channel);
  dummyPolicy.active = false;
});

add_task(async function ChannelWrapper_blob_url() {
  const blobUrl = await gContentPage.spawn([], () => {
    return content.URL.createObjectURL(new content.Blob(new content.Array()));
  });
  const channel = createChannel(blobUrl);
  assertChannelWrapperUnsupportedForChannel(channel);
});

add_task(async function ChannelWrapper_data_url() {
  const channel = createChannel("data:,");
  assertChannelWrapperUnsupportedForChannel(channel);
});

add_task(async function ChannelWrapper_file_url() {
  // Note: "file://C:/" is a valid file:-URL across all platforms.
  const channel = createChannel("file://C:/");
  Assert.ok(channel instanceof Ci.nsIFileChannel, "Is nsIFileChannel");
  assertChannelWrapperUnsupportedForChannel(channel);
});

add_task(async function ChannelWrapper_about_blank_url() {
  const channel = createChannel("about:blank");
  assertChannelWrapperUnsupportedForChannel(channel);
});

add_task(async function ChannelWrapper_javascript_url() {
  const channel = createChannel("javascript://");
  assertChannelWrapperUnsupportedForChannel(channel);
});

add_task(async function ChannelWrapper_resource_url() {
  const channel = createChannel("resource://content-accessible/viewsource.css");
  assertChannelWrapperUnsupportedForChannel(channel);
});

add_task(async function ChannelWrapper_chrome_url() {
  const channel = createChannel(
    "chrome://extensions/content/schemas/web_request.json"
  );
  assertChannelWrapperUnsupportedForChannel(channel);
});

add_task(async function sanity_check_expectations_complete() {
  const channelWrapper = ChannelWrapper.get(createChannel("http://whatever/"));
  channelWrapper.channel = null;
  const uncheckedKeys = new Set(Object.keys(ChannelWrapper.prototype));
  const channelWrapperWithSpy = new Proxy(channelWrapper, {
    get(target, prop) {
      uncheckedKeys.delete(prop);
      let value = Reflect.get(target, prop, target);
      // Methods throw if not invoked on the ChannelWrapper interface, so bind
      // to target (=channelWrapper) instead of channelWrapperWithSpy.
      return typeof value == "function" ? value.bind(target) : value;
    },
  });

  channelWrapperEquals(channelWrapperWithSpy, EXPECTATION_INVALID_CHANNEL);
  checkChannelWrapperMethodsAfterGC(channelWrapperWithSpy);

  Assert.deepEqual(
    Array.from(uncheckedKeys),
    [],
    "All ChannelWrapper properties and methods have been checked"
  );

  // The above channelWrapper(channelWrapperWithSpy, ...) call triggers a lookup
  // for each property listed in EXPECTATION_INVALID_CHANNEL as a way to verify
  // that all properties are accounted for. To make sure that the test case for
  // a valid channel also have complete property coverage, confirm that each
  // property is also present in EXPECTATION_BASIC_FETCH.
  Assert.deepEqual(
    Object.keys(EXPECTATION_BASIC_FETCH),
    Object.keys(EXPECTATION_INVALID_CHANNEL),
    "EXPECTATION_BASIC_FETCH has same properties as EXPECTATION_INVALID_CHANNEL"
  );
});

add_task(async function sanity_check_WebRequest_module_not_loaded() {
  // The purpose of this whole test file is to test the behavior of
  // ChannelWrapper, independently of the webRequest API implementation.
  // So as a sanity check, confirm that we have indeed not loaded that module.
  Assert.equal(
    Cu.isESModuleLoaded("resource://gre/modules/WebRequest.sys.mjs"),
    false,
    "WebRequest.sys.mjs should not have been loaded in this test"
  );
});
