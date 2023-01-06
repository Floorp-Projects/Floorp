const { WebElement, WebReference } = ChromeUtils.importESModule(
  "chrome://remote/content/marionette/element.sys.mjs"
);
const { json } = ChromeUtils.importESModule(
  "chrome://remote/content/marionette/json.sys.mjs"
);
const { NodeCache } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/webdriver/NodeCache.sys.mjs"
);

const MemoryReporter = Cc["@mozilla.org/memory-reporter-manager;1"].getService(
  Ci.nsIMemoryReporterManager
);

const nodeCache = new NodeCache();

const domEl = browser.document.createElement("div");
const svgEl = browser.document.createElementNS(SVG_NS, "rect");

browser.document.body.appendChild(domEl);
browser.document.body.appendChild(svgEl);

const win = domEl.ownerGlobal;

add_test(function test_clone_generalTypes() {
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

  nodeCache.clear({ all: true });
  run_next_test();
});

add_test(function test_clone_WebElements() {
  const domElSharedId = nodeCache.add(domEl);
  deepEqual(
    json.clone(domEl, nodeCache),
    WebReference.from(domEl, domElSharedId).toJSON()
  );

  const svgElSharedId = nodeCache.add(svgEl);
  deepEqual(
    json.clone(svgEl, nodeCache),
    WebReference.from(svgEl, svgElSharedId).toJSON()
  );

  nodeCache.clear({ all: true });
  run_next_test();
});

add_test(function test_clone_Sequences() {
  const domElSharedId = nodeCache.add(domEl);

  const input = [
    null,
    true,
    [],
    domEl,
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
  deepEqual(actual[3], { [WebElement.Identifier]: domElSharedId });
  equal(actual[4], "foo");
  deepEqual(actual[5], { bar: "baz" });

  nodeCache.clear({ all: true });
  run_next_test();
});

add_test(function test_clone_objects() {
  const domElSharedId = nodeCache.add(domEl);

  const input = {
    null: null,
    boolean: true,
    array: [42],
    element: domEl,
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
  deepEqual(actual.element, { [WebElement.Identifier]: domElSharedId });
  equal(actual.toJSON, "foo");
  deepEqual(actual.object, { bar: "baz" });

  nodeCache.clear({ all: true });
  run_next_test();
});

add_test(function test_deserialize_generalTypes() {
  // null
  equal(json.deserialize(undefined, nodeCache, win), undefined);
  equal(json.deserialize(null, nodeCache, win), null);

  // primitives
  equal(json.deserialize(true, nodeCache, win), true);
  equal(json.deserialize(42, nodeCache, win), 42);
  equal(json.deserialize("foo", nodeCache, win), "foo");

  nodeCache.clear({ all: true });
  run_next_test();
});

add_test(function test_deserialize_WebElements() {
  // Fails to resolve for unknown elements
  const unknownWebElId = { [WebElement.Identifier]: "foo" };
  Assert.throws(() => {
    json.deserialize(unknownWebElId, nodeCache, win);
  }, /NoSuchElementError/);

  const domElSharedId = nodeCache.add(domEl);
  const domWebEl = { [WebElement.Identifier]: domElSharedId };

  // Fails to resolve for missing window reference
  Assert.throws(() => json.deserialize(domWebEl, nodeCache), /TypeError/);

  // Previously seen element is associated with original web element reference
  const el = json.deserialize(domWebEl, nodeCache, win);
  deepEqual(el, domEl);
  deepEqual(el, nodeCache.resolve(domElSharedId));

  // Fails with stale element reference for removed element
  let imgEl = browser.document.createElement("img");
  const imgElSharedId = nodeCache.add(imgEl);
  const imgWebEl = { [WebElement.Identifier]: imgElSharedId };

  // Delete element and force a garbage collection
  imgEl = null;

  MemoryReporter.minimizeMemoryUsage(() => {
    Assert.throws(
      () => json.deserialize(imgWebEl, nodeCache, win),
      /StaleElementReferenceError:/
    );

    nodeCache.clear({ all: true });
    run_next_test();
  });
});

add_test(function test_deserialize_Sequences() {
  const domElSharedId = nodeCache.add(domEl);

  const input = [
    null,
    true,
    [42],
    { [WebElement.Identifier]: domElSharedId },
    { bar: "baz" },
  ];

  const actual = json.deserialize(input, nodeCache, win);

  equal(actual[0], null);
  equal(actual[1], true);
  deepEqual(actual[2], [42]);
  deepEqual(actual[3], domEl);
  deepEqual(actual[4], { bar: "baz" });

  nodeCache.clear({ all: true });
  run_next_test();
});

add_test(function test_deserialize_objects() {
  const domElSharedId = nodeCache.add(domEl);

  const input = {
    null: null,
    boolean: true,
    array: [42],
    element: { [WebElement.Identifier]: domElSharedId },
    object: { bar: "baz" },
  };

  const actual = json.deserialize(input, nodeCache, win);

  equal(actual.null, null);
  equal(actual.boolean, true);
  deepEqual(actual.array, [42]);
  deepEqual(actual.element, domEl);
  deepEqual(actual.object, { bar: "baz" });

  nodeCache.clear({ all: true });
  run_next_test();
});
