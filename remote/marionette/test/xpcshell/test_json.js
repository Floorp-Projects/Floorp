const { json } = ChromeUtils.importESModule(
  "chrome://remote/content/marionette/json.sys.mjs"
);
const { NodeCache } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/webdriver/NodeCache.sys.mjs"
);
const { WebElement, WebReference } = ChromeUtils.importESModule(
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

  return { browser, nodeCache, childEl, iframeEl, htmlEl, shadowRoot, svgEl };
}

add_test(function test_clone_generalTypes() {
  const { nodeCache } = setupTest();

  // null
  equal(json.clone(undefined, nodeCache), null);
  equal(json.clone(null, nodeCache), null);

  // primitives
  equal(json.clone(true, nodeCache), true);
  equal(json.clone(42, nodeCache), 42);
  equal(json.clone("foo", nodeCache), "foo");

  // toJSON
  equal(
    json.clone({
      toJSON() {
        return "foo";
      },
    }),
    "foo"
  );

  run_next_test();
});

add_test(function test_clone_WebElements() {
  const { htmlEl, nodeCache, svgEl } = setupTest();

  const htmlElRef = nodeCache.getOrCreateNodeReference(htmlEl);
  deepEqual(
    json.clone(htmlEl, nodeCache),
    WebReference.from(htmlEl, htmlElRef).toJSON()
  );

  // Check an element with a different namespace
  const svgElRef = nodeCache.getOrCreateNodeReference(svgEl);
  deepEqual(
    json.clone(svgEl, nodeCache),
    WebReference.from(svgEl, svgElRef).toJSON()
  );

  run_next_test();
});

add_test(function test_clone_Sequences() {
  const { htmlEl, nodeCache } = setupTest();

  const htmlElRef = nodeCache.getOrCreateNodeReference(htmlEl);

  const input = [
    null,
    true,
    [],
    htmlEl,
    {
      toJSON() {
        return "foo";
      },
    },
    { bar: "baz" },
  ];

  const actual = json.clone(input, nodeCache);

  equal(actual[0], null);
  equal(actual[1], true);
  deepEqual(actual[2], []);
  deepEqual(actual[3], { [WebElement.Identifier]: htmlElRef });
  equal(actual[4], "foo");
  deepEqual(actual[5], { bar: "baz" });

  run_next_test();
});

add_test(function test_clone_objects() {
  const { htmlEl, nodeCache } = setupTest();

  const htmlElRef = nodeCache.getOrCreateNodeReference(htmlEl);

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

  const actual = json.clone(input, nodeCache);

  equal(actual.null, null);
  equal(actual.boolean, true);
  deepEqual(actual.array, [42]);
  deepEqual(actual.element, { [WebElement.Identifier]: htmlElRef });
  equal(actual.toJSON, "foo");
  deepEqual(actual.object, { bar: "baz" });

  run_next_test();
});

add_test(function test_clone_сyclicReference() {
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

  run_next_test();
});

add_test(function test_deserialize_generalTypes() {
  const { browser, nodeCache } = setupTest();
  const win = browser.document.ownerGlobal;

  // null
  equal(json.deserialize(undefined, nodeCache, win), undefined);
  equal(json.deserialize(null, nodeCache, win), null);

  // primitives
  equal(json.deserialize(true, nodeCache, win), true);
  equal(json.deserialize(42, nodeCache, win), 42);
  equal(json.deserialize("foo", nodeCache, win), "foo");

  run_next_test();
});

add_test(function test_deserialize_WebElements() {
  const { browser, htmlEl, nodeCache } = setupTest();
  const win = browser.document.ownerGlobal;

  // Fails to resolve for unknown elements
  const unknownWebElId = { [WebElement.Identifier]: "foo" };
  Assert.throws(() => {
    json.deserialize(unknownWebElId, nodeCache, win);
  }, /NoSuchElementError/);

  const htmlElRef = nodeCache.getOrCreateNodeReference(htmlEl);
  const htmlWebEl = { [WebElement.Identifier]: htmlElRef };
  // Fails to resolve for missing window reference
  Assert.throws(() => json.deserialize(htmlWebEl, nodeCache), /TypeError/);

  // Previously seen element is associated with original web element reference
  const el = json.deserialize(htmlWebEl, nodeCache, win);
  deepEqual(el, htmlEl);
  deepEqual(el, nodeCache.getNode(browser.browsingContext, htmlElRef));

  run_next_test();
});

add_test(function test_deserialize_Sequences() {
  const { browser, htmlEl, nodeCache } = setupTest();
  const win = browser.document.ownerGlobal;

  const htmlElRef = nodeCache.getOrCreateNodeReference(htmlEl);

  const input = [
    null,
    true,
    [42],
    { [WebElement.Identifier]: htmlElRef },
    { bar: "baz" },
  ];

  const actual = json.deserialize(input, nodeCache, win);

  equal(actual[0], null);
  equal(actual[1], true);
  deepEqual(actual[2], [42]);
  deepEqual(actual[3], htmlEl);
  deepEqual(actual[4], { bar: "baz" });

  run_next_test();
});

add_test(function test_deserialize_objects() {
  const { browser, htmlEl, nodeCache } = setupTest();
  const win = browser.document.ownerGlobal;

  const htmlElRef = nodeCache.getOrCreateNodeReference(htmlEl);

  const input = {
    null: null,
    boolean: true,
    array: [42],
    element: { [WebElement.Identifier]: htmlElRef },
    object: { bar: "baz" },
  };

  const actual = json.deserialize(input, nodeCache, win);

  equal(actual.null, null);
  equal(actual.boolean, true);
  deepEqual(actual.array, [42]);
  deepEqual(actual.element, htmlEl);
  deepEqual(actual.object, { bar: "baz" });

  nodeCache.clear({ all: true });
  run_next_test();
});
