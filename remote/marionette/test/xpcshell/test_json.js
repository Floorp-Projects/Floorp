const { json } = ChromeUtils.importESModule(
  "chrome://remote/content/marionette/json.sys.mjs"
);
const { NodeCache } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/webdriver/NodeCache.sys.mjs"
);
const { ShadowRoot, WebElement, WebReference } = ChromeUtils.importESModule(
  "chrome://remote/content/marionette/element.sys.mjs"
);

function setupTest() {
  const browser = Services.appShell.createWindowlessBrowser(false);
  const nodeCache = new NodeCache();

  const htmlEl = browser.document.createElement("video");
  browser.document.body.appendChild(htmlEl);

  const svgEl = browser.document.createElementNS(
    "http://www.w3.org/2000/svg",
    "rect"
  );
  browser.document.body.appendChild(svgEl);

  const shadowRoot = htmlEl.openOrClosedShadowRoot;

  const iframeEl = browser.document.createElement("iframe");
  browser.document.body.appendChild(iframeEl);
  const childEl = iframeEl.contentDocument.createElement("div");

  return {
    browser,
    browsingContext: browser.browsingContext,
    nodeCache,
    childEl,
    iframeEl,
    htmlEl,
    seenNodeIds: new Map(),
    shadowRoot,
    svgEl,
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
  const { htmlEl, nodeCache, seenNodeIds, svgEl } = setupTest();

  const htmlElRef = nodeCache.getOrCreateNodeReference(htmlEl, seenNodeIds);
  assert_cloned_value(
    htmlEl,
    WebReference.from(htmlEl, htmlElRef).toJSON(),
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
  const { htmlEl, nodeCache, seenNodeIds } = setupTest();

  const htmlElRef = nodeCache.getOrCreateNodeReference(htmlEl, seenNodeIds);

  const input = [
    null,
    true,
    [42],
    htmlEl,
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
      { [WebElement.Identifier]: htmlElRef },
      "foo",
      { bar: "baz" },
    ],
    nodeCache,
    seenNodeIds
  );
});

add_task(function test_clone_objects() {
  const { htmlEl, nodeCache, seenNodeIds } = setupTest();

  const htmlElRef = nodeCache.getOrCreateNodeReference(htmlEl, seenNodeIds);

  const input = {
    null: null,
    boolean: true,
    array: [42],
    element: htmlEl,
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
      element: { [WebElement.Identifier]: htmlElRef },
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
  const { browser, browsingContext, htmlEl, nodeCache, seenNodeIds } =
    setupTest();
  const seenNodes = new Set();

  // Fails to resolve for unknown elements
  const unknownWebElId = { [WebElement.Identifier]: "foo" };
  Assert.throws(() => {
    json.deserialize(unknownWebElId, nodeCache, browsingContext, seenNodes);
  }, /NoSuchElementError/);

  const htmlElRef = nodeCache.getOrCreateNodeReference(htmlEl, seenNodeIds);
  const htmlWebEl = { [WebElement.Identifier]: htmlElRef };

  // Fails to resolve for missing window reference
  Assert.throws(() => json.deserialize(htmlWebEl, nodeCache), /TypeError/);

  // Previously seen element is associated with original web element reference
  seenNodes.add(htmlElRef);
  const el = json.deserialize(htmlWebEl, nodeCache, browsingContext, seenNodes);
  deepEqual(el, htmlEl);
  deepEqual(el, nodeCache.getNode(browser.browsingContext, htmlElRef));
});

add_task(function test_deserialize_Sequences() {
  const { browsingContext, htmlEl, nodeCache, seenNodeIds } = setupTest();
  const seenNodes = new Set();

  const htmlElRef = nodeCache.getOrCreateNodeReference(htmlEl, seenNodeIds);
  seenNodes.add(htmlElRef);

  const input = [
    null,
    true,
    [42],
    { [WebElement.Identifier]: htmlElRef },
    { bar: "baz" },
  ];

  const actual = json.deserialize(input, nodeCache, browsingContext, seenNodes);

  equal(actual[0], null);
  equal(actual[1], true);
  deepEqual(actual[2], [42]);
  deepEqual(actual[3], htmlEl);
  deepEqual(actual[4], { bar: "baz" });
});

add_task(function test_deserialize_objects() {
  const { browsingContext, htmlEl, nodeCache, seenNodeIds } = setupTest();
  const seenNodes = new Set();

  const htmlElRef = nodeCache.getOrCreateNodeReference(htmlEl, seenNodeIds);
  seenNodes.add(htmlElRef);

  const input = {
    null: null,
    boolean: true,
    array: [42],
    element: { [WebElement.Identifier]: htmlElRef },
    object: { bar: "baz" },
  };

  const actual = json.deserialize(input, nodeCache, browsingContext, seenNodes);

  equal(actual.null, null);
  equal(actual.boolean, true);
  deepEqual(actual.array, [42]);
  deepEqual(actual.element, htmlEl);
  deepEqual(actual.object, { bar: "baz" });

  nodeCache.clear({ all: true });
});
