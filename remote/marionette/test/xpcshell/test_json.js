const { json, getKnownElement, getKnownShadowRoot } =
  ChromeUtils.importESModule("chrome://remote/content/marionette/json.sys.mjs");
const { NodeCache } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/webdriver/NodeCache.sys.mjs"
);
const { ShadowRoot, WebElement, WebReference } = ChromeUtils.importESModule(
  "chrome://remote/content/marionette/web-reference.sys.mjs"
);

const MemoryReporter = Cc["@mozilla.org/memory-reporter-manager;1"].getService(
  Ci.nsIMemoryReporterManager
);

function setupTest() {
  const browser = Services.appShell.createWindowlessBrowser(false);
  const nodeCache = new NodeCache();

  const videoEl = browser.document.createElement("video");
  browser.document.body.appendChild(videoEl);

  const svgEl = browser.document.createElementNS(
    "http://www.w3.org/2000/svg",
    "rect"
  );
  browser.document.body.appendChild(svgEl);

  const shadowRoot = videoEl.openOrClosedShadowRoot;

  const iframeEl = browser.document.createElement("iframe");
  browser.document.body.appendChild(iframeEl);
  const childEl = iframeEl.contentDocument.createElement("div");

  return {
    browser,
    browsingContext: browser.browsingContext,
    nodeCache,
    childEl,
    iframeEl,
    seenNodeIds: new Map(),
    shadowRoot,
    svgEl,
    videoEl,
  };
}

function assert_cloned_value(value, clonedValue, nodeCache, seenNodes = []) {
  const { seenNodeIds, serializedValue } = json.clone(value, nodeCache);

  deepEqual(serializedValue, clonedValue);
  deepEqual([...seenNodeIds.values()], seenNodes);
}

add_task(function test_clone_generalTypes() {
  const { nodeCache } = setupTest();

  // null
  assert_cloned_value(undefined, null, nodeCache);
  assert_cloned_value(null, null, nodeCache);

  // primitives
  assert_cloned_value(true, true, nodeCache);
  assert_cloned_value(42, 42, nodeCache);
  assert_cloned_value("foo", "foo", nodeCache);

  // toJSON
  assert_cloned_value(
    {
      toJSON() {
        return "foo";
      },
    },
    "foo",
    nodeCache
  );
});

add_task(function test_clone_ShadowRoot() {
  const { nodeCache, seenNodeIds, shadowRoot } = setupTest();

  const shadowRootRef = nodeCache.getOrCreateNodeReference(
    shadowRoot,
    seenNodeIds
  );
  assert_cloned_value(
    shadowRoot,
    WebReference.from(shadowRoot, shadowRootRef).toJSON(),
    nodeCache,
    seenNodeIds
  );
});

add_task(function test_clone_WebElement() {
  const { videoEl, nodeCache, seenNodeIds, svgEl } = setupTest();

  const videoElRef = nodeCache.getOrCreateNodeReference(videoEl, seenNodeIds);
  assert_cloned_value(
    videoEl,
    WebReference.from(videoEl, videoElRef).toJSON(),
    nodeCache,
    seenNodeIds
  );

  // Check an element with a different namespace
  const svgElRef = nodeCache.getOrCreateNodeReference(svgEl, seenNodeIds);
  assert_cloned_value(
    svgEl,
    WebReference.from(svgEl, svgElRef).toJSON(),
    nodeCache,
    seenNodeIds
  );
});

add_task(function test_clone_Sequences() {
  const { videoEl, nodeCache, seenNodeIds } = setupTest();

  const videoElRef = nodeCache.getOrCreateNodeReference(videoEl, seenNodeIds);

  const input = [
    null,
    true,
    [42],
    videoEl,
    {
      toJSON() {
        return "foo";
      },
    },
    { bar: "baz" },
  ];

  assert_cloned_value(
    input,
    [
      null,
      true,
      [42],
      { [WebElement.Identifier]: videoElRef },
      "foo",
      { bar: "baz" },
    ],
    nodeCache,
    seenNodeIds
  );
});

add_task(function test_clone_objects() {
  const { videoEl, nodeCache, seenNodeIds } = setupTest();

  const videoElRef = nodeCache.getOrCreateNodeReference(videoEl, seenNodeIds);

  const input = {
    null: null,
    boolean: true,
    array: [42],
    element: videoEl,
    toJSON: {
      toJSON() {
        return "foo";
      },
    },
    object: { bar: "baz" },
  };

  assert_cloned_value(
    input,
    {
      null: null,
      boolean: true,
      array: [42],
      element: { [WebElement.Identifier]: videoElRef },
      toJSON: "foo",
      object: { bar: "baz" },
    },
    nodeCache,
    seenNodeIds
  );
});

add_task(function test_clone_сyclicReference() {
  const { nodeCache } = setupTest();

  // object
  Assert.throws(() => {
    const obj = {};
    obj.reference = obj;
    json.clone(obj, nodeCache);
  }, /JavaScriptError/);

  // array
  Assert.throws(() => {
    const array = [];
    array.push(array);
    json.clone(array, nodeCache);
  }, /JavaScriptError/);

  // array in object
  Assert.throws(() => {
    const array = [];
    array.push(array);
    json.clone({ array }, nodeCache);
  }, /JavaScriptError/);

  // object in array
  Assert.throws(() => {
    const obj = {};
    obj.reference = obj;
    json.clone([obj], nodeCache);
  }, /JavaScriptError/);
});

add_task(function test_deserialize_generalTypes() {
  const { browsingContext, nodeCache } = setupTest();

  // null
  equal(json.deserialize(undefined, nodeCache, browsingContext), undefined);
  equal(json.deserialize(null, nodeCache, browsingContext), null);

  // primitives
  equal(json.deserialize(true, nodeCache, browsingContext), true);
  equal(json.deserialize(42, nodeCache, browsingContext), 42);
  equal(json.deserialize("foo", nodeCache, browsingContext), "foo");
});

add_task(function test_deserialize_ShadowRoot() {
  const { browsingContext, nodeCache, seenNodeIds, shadowRoot } = setupTest();
  const seenNodes = new Set();

  // Fails to resolve for unknown elements
  const unknownShadowRootId = { [ShadowRoot.Identifier]: "foo" };
  Assert.throws(() => {
    json.deserialize(
      unknownShadowRootId,
      nodeCache,
      browsingContext,
      seenNodes
    );
  }, /NoSuchShadowRootError/);

  const shadowRootRef = nodeCache.getOrCreateNodeReference(
    shadowRoot,
    seenNodeIds
  );
  const shadowRootEl = { [ShadowRoot.Identifier]: shadowRootRef };

  // Fails to resolve for missing window reference
  Assert.throws(() => json.deserialize(shadowRootEl, nodeCache), /TypeError/);

  // Previously seen element is associated with original web element reference
  seenNodes.add(shadowRootRef);
  const root = json.deserialize(
    shadowRootEl,
    nodeCache,
    browsingContext,
    seenNodes
  );
  deepEqual(root, shadowRoot);
  deepEqual(root, nodeCache.getNode(browsingContext, shadowRootRef));
});

add_task(function test_deserialize_WebElement() {
  const { browser, browsingContext, videoEl, nodeCache, seenNodeIds } =
    setupTest();
  const seenNodes = new Set();

  // Fails to resolve for unknown elements
  const unknownWebElId = { [WebElement.Identifier]: "foo" };
  Assert.throws(() => {
    json.deserialize(unknownWebElId, nodeCache, browsingContext, seenNodes);
  }, /NoSuchElementError/);

  const videoElRef = nodeCache.getOrCreateNodeReference(videoEl, seenNodeIds);
  const htmlWebEl = { [WebElement.Identifier]: videoElRef };

  // Fails to resolve for missing window reference
  Assert.throws(() => json.deserialize(htmlWebEl, nodeCache), /TypeError/);

  // Previously seen element is associated with original web element reference
  seenNodes.add(videoElRef);
  const el = json.deserialize(htmlWebEl, nodeCache, browsingContext, seenNodes);
  deepEqual(el, videoEl);
  deepEqual(el, nodeCache.getNode(browser.browsingContext, videoElRef));
});

add_task(function test_deserialize_Sequences() {
  const { browsingContext, videoEl, nodeCache, seenNodeIds } = setupTest();
  const seenNodes = new Set();

  const videoElRef = nodeCache.getOrCreateNodeReference(videoEl, seenNodeIds);
  seenNodes.add(videoElRef);

  const input = [
    null,
    true,
    [42],
    { [WebElement.Identifier]: videoElRef },
    { bar: "baz" },
  ];

  const actual = json.deserialize(input, nodeCache, browsingContext, seenNodes);

  equal(actual[0], null);
  equal(actual[1], true);
  deepEqual(actual[2], [42]);
  deepEqual(actual[3], videoEl);
  deepEqual(actual[4], { bar: "baz" });
});

add_task(function test_deserialize_objects() {
  const { browsingContext, videoEl, nodeCache, seenNodeIds } = setupTest();
  const seenNodes = new Set();

  const videoElRef = nodeCache.getOrCreateNodeReference(videoEl, seenNodeIds);
  seenNodes.add(videoElRef);

  const input = {
    null: null,
    boolean: true,
    array: [42],
    element: { [WebElement.Identifier]: videoElRef },
    object: { bar: "baz" },
  };

  const actual = json.deserialize(input, nodeCache, browsingContext, seenNodes);

  equal(actual.null, null);
  equal(actual.boolean, true);
  deepEqual(actual.array, [42]);
  deepEqual(actual.element, videoEl);
  deepEqual(actual.object, { bar: "baz" });

  nodeCache.clear({ all: true });
});

add_task(async function test_getKnownElement() {
  const { browser, nodeCache, seenNodeIds, shadowRoot, videoEl } = setupTest();
  const seenNodes = new Set();

  // Unknown element reference
  Assert.throws(() => {
    getKnownElement(browser.browsingContext, "foo", nodeCache, seenNodes);
  }, /NoSuchElementError/);

  // With a ShadowRoot reference
  const shadowRootRef = nodeCache.getOrCreateNodeReference(
    shadowRoot,
    seenNodeIds
  );
  seenNodes.add(shadowRootRef);

  Assert.throws(() => {
    getKnownElement(
      browser.browsingContext,
      shadowRootRef,
      nodeCache,
      seenNodes
    );
  }, /NoSuchElementError/);

  let detachedEl = browser.document.createElement("div");
  const detachedElRef = nodeCache.getOrCreateNodeReference(
    detachedEl,
    seenNodeIds
  );
  seenNodes.add(detachedElRef);

  // Element not connected to the DOM
  Assert.throws(() => {
    getKnownElement(
      browser.browsingContext,
      detachedElRef,
      nodeCache,
      seenNodes
    );
  }, /StaleElementReferenceError/);

  // Element garbage collected
  detachedEl = null;

  await new Promise(resolve => MemoryReporter.minimizeMemoryUsage(resolve));
  Assert.throws(() => {
    getKnownElement(
      browser.browsingContext,
      detachedElRef,
      nodeCache,
      seenNodes
    );
  }, /StaleElementReferenceError/);

  // Known element reference
  const videoElRef = nodeCache.getOrCreateNodeReference(videoEl, seenNodeIds);
  seenNodes.add(videoElRef);

  equal(
    getKnownElement(browser.browsingContext, videoElRef, nodeCache, seenNodes),
    videoEl
  );
});

add_task(async function test_getKnownShadowRoot() {
  const { browser, nodeCache, seenNodeIds, shadowRoot, videoEl } = setupTest();
  const seenNodes = new Set();

  const videoElRef = nodeCache.getOrCreateNodeReference(videoEl, seenNodeIds);
  seenNodes.add(videoElRef);

  // Unknown ShadowRoot reference
  Assert.throws(() => {
    getKnownShadowRoot(browser.browsingContext, "foo", nodeCache, seenNodes);
  }, /NoSuchShadowRootError/);

  // With a videoElement reference
  Assert.throws(() => {
    getKnownShadowRoot(
      browser.browsingContext,
      videoElRef,
      nodeCache,
      seenNodes
    );
  }, /NoSuchShadowRootError/);

  // Known ShadowRoot reference
  const shadowRootRef = nodeCache.getOrCreateNodeReference(
    shadowRoot,
    seenNodeIds
  );
  seenNodes.add(shadowRootRef);

  equal(
    getKnownShadowRoot(
      browser.browsingContext,
      shadowRootRef,
      nodeCache,
      seenNodes
    ),
    shadowRoot
  );

  // Detached ShadowRoot host
  let el = browser.document.createElement("div");
  let detachedShadowRoot = el.attachShadow({ mode: "open" });
  detachedShadowRoot.innerHTML = "<input></input>";

  const detachedShadowRootRef = nodeCache.getOrCreateNodeReference(
    detachedShadowRoot,
    seenNodeIds
  );
  seenNodes.add(detachedShadowRootRef);

  // ... not connected to the DOM
  Assert.throws(() => {
    getKnownShadowRoot(
      browser.browsingContext,
      detachedShadowRootRef,
      nodeCache,
      seenNodes
    );
  }, /DetachedShadowRootError/);

  // ... host and shadow root garbage collected
  el = null;
  detachedShadowRoot = null;

  await new Promise(resolve => MemoryReporter.minimizeMemoryUsage(resolve));
  Assert.throws(() => {
    getKnownShadowRoot(
      browser.browsingContext,
      detachedShadowRootRef,
      nodeCache,
      seenNodes
    );
  }, /DetachedShadowRootError/);
});
