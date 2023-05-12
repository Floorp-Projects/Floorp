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

  return { browser, nodeCache, childEl, iframeEl, htmlEl, shadowRoot, svgEl };
}

add_task(function test_clone_generalTypes() {
  // null
  equal(json.clone({ value: undefined }), null);
  equal(json.clone({ value: null }), null);

  // primitives
  equal(json.clone({ value: true }), true);
  equal(json.clone({ value: 42 }), 42);
  equal(json.clone({ value: "foo" }), "foo");

  // toJSON
  equal(
    json.clone({
      value: {
        toJSON() {
          return "foo";
        },
      },
    }),
    "foo"
  );
});

add_task(function test_clone_ShadowRoot() {
  const { nodeCache, shadowRoot } = setupTest();

  function getOrCreateNodeReference(node) {
    const nodeRef = nodeCache.getOrCreateNodeReference(node);
    return WebReference.from(node, nodeRef);
  }

  const shadowRootRef = nodeCache.getOrCreateNodeReference(shadowRoot);
  deepEqual(
    json.clone({ value: shadowRoot, getOrCreateNodeReference }),
    WebReference.from(shadowRoot, shadowRootRef).toJSON()
  );
});

add_task(function test_clone_WebElement() {
  const { htmlEl, nodeCache, svgEl } = setupTest();

  function getOrCreateNodeReference(node) {
    const nodeRef = nodeCache.getOrCreateNodeReference(node);
    return WebReference.from(node, nodeRef);
  }

  const htmlElRef = nodeCache.getOrCreateNodeReference(htmlEl);
  deepEqual(
    json.clone({ value: htmlEl, getOrCreateNodeReference }),
    WebReference.from(htmlEl, htmlElRef).toJSON()
  );

  // Check an element with a different namespace
  const svgElRef = nodeCache.getOrCreateNodeReference(svgEl);
  deepEqual(
    json.clone({ value: svgEl, getOrCreateNodeReference }),
    WebReference.from(svgEl, svgElRef).toJSON()
  );
});

add_task(function test_clone_Sequences() {
  const { htmlEl, nodeCache } = setupTest();

  function getOrCreateNodeReference(node) {
    const nodeRef = nodeCache.getOrCreateNodeReference(node);
    return WebReference.from(node, nodeRef);
  }

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

  const actual = json.clone({ value: input, getOrCreateNodeReference });

  equal(actual[0], null);
  equal(actual[1], true);
  deepEqual(actual[2], []);
  deepEqual(actual[3], { [WebElement.Identifier]: htmlElRef });
  equal(actual[4], "foo");
  deepEqual(actual[5], { bar: "baz" });
});

add_task(function test_clone_objects() {
  const { htmlEl, nodeCache } = setupTest();

  function getOrCreateNodeReference(node) {
    const nodeRef = nodeCache.getOrCreateNodeReference(node);
    return WebReference.from(node, nodeRef);
  }

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

  const actual = json.clone({ value: input, getOrCreateNodeReference });

  equal(actual.null, null);
  equal(actual.boolean, true);
  deepEqual(actual.array, [42]);
  deepEqual(actual.element, { [WebElement.Identifier]: htmlElRef });
  equal(actual.toJSON, "foo");
  deepEqual(actual.object, { bar: "baz" });
});

add_task(function test_clone_сyclicReference() {
  // object
  Assert.throws(() => {
    const obj = {};
    obj.reference = obj;
    json.clone({ value: obj });
  }, /JavaScriptError/);

  // array
  Assert.throws(() => {
    const array = [];
    array.push(array);
    json.clone({ value: array });
  }, /JavaScriptError/);

  // array in object
  Assert.throws(() => {
    const array = [];
    array.push(array);
    json.clone({ value: { array } });
  }, /JavaScriptError/);

  // object in array
  Assert.throws(() => {
    const obj = {};
    obj.reference = obj;
    json.clone({ value: [obj] });
  }, /JavaScriptError/);
});

add_task(function test_deserialize_generalTypes() {
  const { browser } = setupTest();
  const win = browser.document.ownerGlobal;

  // null
  equal(json.deserialize({ value: undefined, win }), undefined);
  equal(json.deserialize({ value: null, win }), null);

  // primitives
  equal(json.deserialize({ value: true, win }), true);
  equal(json.deserialize({ value: 42, win }), 42);
  equal(json.deserialize({ value: "foo", win }), "foo");
});

add_task(function test_deserialize_ShadowRoot() {
  const { browser, nodeCache, shadowRoot } = setupTest();
  const win = browser.document.ownerGlobal;

  function getKnownShadowRoot(browsingContext, nodeId) {
    return nodeCache.getNode(browsingContext, nodeId);
  }

  // Unknown shadow root
  const unknownShadowRootId = { [ShadowRoot.Identifier]: "foo" };
  equal(
    json.deserialize({ value: unknownShadowRootId, getKnownShadowRoot, win }),
    null
  );

  const shadowRootRef = nodeCache.getOrCreateNodeReference(shadowRoot);
  const shadowRootEl = { [ShadowRoot.Identifier]: shadowRootRef };

  // Fails to resolve for missing getKnownShadowRoot callback
  Assert.throws(
    () => equal(json.deserialize({ value: shadowRootEl, win })),
    /TypeError/
  );

  // Fails to resolve for missing window reference
  Assert.throws(
    () => equal(json.deserialize({ value: shadowRootEl, getKnownShadowRoot })),
    /TypeError/
  );

  // Previously seen element is associated with original web element reference
  const root = json.deserialize({
    value: shadowRootEl,
    getKnownShadowRoot,
    win,
  });
  deepEqual(root, nodeCache.getNode(browser.browsingContext, shadowRootRef));
});

add_task(function test_deserialize_WebElement() {
  const { browser, htmlEl, nodeCache } = setupTest();
  const win = browser.document.ownerGlobal;

  function getKnownElement(browsingContext, nodeId) {
    return nodeCache.getNode(browsingContext, nodeId);
  }

  // Unknown element
  const unknownWebElId = { [WebElement.Identifier]: "foo" };
  equal(
    json.deserialize({ value: unknownWebElId, getKnownElement, win }),
    null
  );

  const htmlElRef = nodeCache.getOrCreateNodeReference(htmlEl);
  const htmlWebEl = { [WebElement.Identifier]: htmlElRef };

  // Fails to resolve for missing getKnownElement callback
  Assert.throws(
    () => equal(json.deserialize({ value: htmlWebEl, win })),
    /TypeError/
  );

  // Fails to resolve for missing window reference
  Assert.throws(
    () => equal(json.deserialize({ value: htmlWebEl, getKnownElement })),
    /TypeError/
  );

  // Previously seen element is associated with original web element reference
  const el = json.deserialize({ value: htmlWebEl, getKnownElement, win });
  deepEqual(el, htmlEl);
  deepEqual(el, nodeCache.getNode(browser.browsingContext, htmlElRef));
});

add_task(function test_deserialize_Sequences() {
  const { browser, htmlEl, nodeCache } = setupTest();
  const win = browser.document.ownerGlobal;

  function getKnownElement(browsingContext, nodeId) {
    return nodeCache.getNode(browsingContext, nodeId);
  }

  const htmlElRef = nodeCache.getOrCreateNodeReference(htmlEl);

  const input = [
    null,
    true,
    [42],
    { [WebElement.Identifier]: htmlElRef },
    { bar: "baz" },
  ];

  const actual = json.deserialize({ value: input, getKnownElement, win });

  equal(actual[0], null);
  equal(actual[1], true);
  deepEqual(actual[2], [42]);
  deepEqual(actual[3], htmlEl);
  deepEqual(actual[4], { bar: "baz" });
});

add_task(function test_deserialize_objects() {
  const { browser, htmlEl, nodeCache } = setupTest();
  const win = browser.document.ownerGlobal;

  function getKnownElement(browsingContext, nodeId) {
    return nodeCache.getNode(browsingContext, nodeId);
  }

  const htmlElRef = nodeCache.getOrCreateNodeReference(htmlEl);

  const input = {
    null: null,
    boolean: true,
    array: [42],
    element: { [WebElement.Identifier]: htmlElRef },
    object: { bar: "baz" },
  };

  const actual = json.deserialize({ value: input, getKnownElement, win });

  equal(actual.null, null);
  equal(actual.boolean, true);
  deepEqual(actual.array, [42]);
  deepEqual(actual.element, htmlEl);
  deepEqual(actual.object, { bar: "baz" });

  nodeCache.clear({ all: true });
});
