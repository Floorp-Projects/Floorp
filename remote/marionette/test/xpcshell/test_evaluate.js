const { WebElement, WebReference } = ChromeUtils.importESModule(
  "chrome://remote/content/marionette/element.sys.mjs"
);
const { evaluate } = ChromeUtils.importESModule(
  "chrome://remote/content/marionette/evaluate.sys.mjs"
);
const { NodeCache } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/webdriver/NodeCache.sys.mjs"
);

const MemoryReporter = Cc["@mozilla.org/memory-reporter-manager;1"].getService(
  Ci.nsIMemoryReporterManager
);

const nodeCache = new NodeCache();

const domEl = browser.document.createElement("img");
const svgEl = browser.document.createElementNS(SVG_NS, "rect");

browser.document.body.appendChild(domEl);
browser.document.body.appendChild(svgEl);

add_test(function test_acyclic() {
  evaluate.assertAcyclic({});

  Assert.throws(() => {
    const obj = {};
    obj.reference = obj;
    evaluate.assertAcyclic(obj);
  }, /JavaScriptError/);

  // custom message
  const cyclic = {};
  cyclic.reference = cyclic;
  Assert.throws(
    () => evaluate.assertAcyclic(cyclic, "", RangeError),
    RangeError
  );
  Assert.throws(
    () => evaluate.assertAcyclic(cyclic, "foo"),
    /JavaScriptError: foo/
  );
  Assert.throws(
    () => evaluate.assertAcyclic(cyclic, "bar", RangeError),
    /RangeError: bar/
  );

  run_next_test();
});

add_test(function test_toJSON_types() {
  // null
  equal(null, evaluate.toJSON(undefined));
  equal(null, evaluate.toJSON(null));

  // primitives
  equal(true, evaluate.toJSON(true));
  equal(42, evaluate.toJSON(42));
  equal("foo", evaluate.toJSON("foo"));

  // collections
  deepEqual([], evaluate.toJSON([]));

  // toJSON
  equal(
    "foo",
    evaluate.toJSON({
      toJSON() {
        return "foo";
      },
    })
  );

  // arbitrary object
  deepEqual({ foo: "bar" }, evaluate.toJSON({ foo: "bar" }));

  run_next_test();
});

add_test(function test_toJSON_types_NodeCache() {
  const domElSharedId = nodeCache.add(domEl);
  deepEqual(
    evaluate.toJSON(domEl, { seenEls: nodeCache }),
    WebReference.from(domEl, domElSharedId).toJSON()
  );

  const svgElSharedId = nodeCache.add(svgEl);
  deepEqual(
    evaluate.toJSON(svgEl, { seenEls: nodeCache }),
    WebReference.from(svgEl, svgElSharedId).toJSON()
  );

  nodeCache.clear({ all: true });

  run_next_test();
});

add_test(function test_toJSON_sequences() {
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

  const actual = evaluate.toJSON(input, { seenEls: nodeCache });

  equal(null, actual[0]);
  equal(true, actual[1]);
  deepEqual([], actual[2]);
  deepEqual(actual[3], { [WebElement.Identifier]: domElSharedId });
  equal("foo", actual[4]);
  deepEqual({ bar: "baz" }, actual[5]);

  nodeCache.clear({ all: true });

  run_next_test();
});

add_test(function test_toJSON_objects() {
  const domElSharedId = nodeCache.add(domEl);

  const input = {
    null: null,
    boolean: true,
    array: [],
    element: domEl,
    toJSON: {
      toJSON() {
        return "foo";
      },
    },
    object: { bar: "baz" },
  };

  const actual = evaluate.toJSON(input, { seenEls: nodeCache });

  equal(null, actual.null);
  equal(true, actual.boolean);
  deepEqual([], actual.array);
  deepEqual(actual.element, { [WebElement.Identifier]: domElSharedId });
  equal("foo", actual.toJSON);
  deepEqual({ bar: "baz" }, actual.object);

  nodeCache.clear({ all: true });

  run_next_test();
});

add_test(function test_fromJSON_NodeCache() {
  // Fails to resolve for unknown elements
  const unknownWebElId = { [WebElement.Identifier]: "foo" };
  Assert.throws(() => {
    evaluate.fromJSON(unknownWebElId, {
      seenEls: nodeCache,
      win: domEl.ownerGlobal,
    });
  }, /NoSuchElementError/);

  const domElSharedId = nodeCache.add(domEl);
  const domWebEl = { [WebElement.Identifier]: domElSharedId };

  // Fails to resolve for missing window reference
  Assert.throws(() => {
    evaluate.fromJSON(domWebEl, {
      seenEls: nodeCache,
    });
  }, /TypeError/);

  // Previously seen element is associated with original web element reference
  const el = evaluate.fromJSON(domWebEl, {
    seenEls: nodeCache,
    win: domEl.ownerGlobal,
  });
  deepEqual(el, domEl);
  deepEqual(el, nodeCache.resolve(domElSharedId));

  // Fails with stale element reference for removed element
  let imgEl = browser.document.createElement("img");
  const win = imgEl.ownerGlobal;
  const imgElSharedId = nodeCache.add(imgEl);
  const imgWebEl = { [WebElement.Identifier]: imgElSharedId };

  // Delete element and force a garbage collection
  imgEl = null;

  MemoryReporter.minimizeMemoryUsage(() => {
    Assert.throws(() => {
      evaluate.fromJSON(imgWebEl, {
        seenEls: nodeCache,
        win,
      });
    }, /StaleElementReferenceError:/);

    nodeCache.clear({ all: true });

    run_next_test();
  });
});

add_test(function test_isCyclic_noncyclic() {
  for (const type of [true, 42, "foo", [], {}, null, undefined]) {
    ok(!evaluate.isCyclic(type));
  }

  run_next_test();
});

add_test(function test_isCyclic_object() {
  const obj = {};
  obj.reference = obj;
  ok(evaluate.isCyclic(obj));

  run_next_test();
});

add_test(function test_isCyclic_array() {
  const arr = [];
  arr.push(arr);
  ok(evaluate.isCyclic(arr));

  run_next_test();
});

add_test(function test_isCyclic_arrayInObject() {
  const arr = [];
  arr.push(arr);
  ok(evaluate.isCyclic({ arr }));

  run_next_test();
});

add_test(function test_isCyclic_objectInArray() {
  const obj = {};
  obj.reference = obj;
  ok(evaluate.isCyclic([obj]));

  run_next_test();
});
