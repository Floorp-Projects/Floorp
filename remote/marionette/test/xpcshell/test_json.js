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
    shadowRoot,
    svgEl,
  };
}

function clone(options = {}) {
  const {
    browsingContext,
    getOrCreateNodeReference = async () =>
      ok(false, "'getOrCreateNodeReference' called"),
    value,
  } = options;

  return json.clone({ browsingContext, getOrCreateNodeReference, value });
}

function deserialize(options = {}) {
  const {
    browsingContext,
    getKnownElement = async () => ok(false, "'getKnownElement' called"),
    getKnownShadowRoot = async () => ok(false, "'getKnownShadowRoot' called"),
    value,
  } = options;

  return json.deserialize({
    browsingContext,
    getKnownElement,
    getKnownShadowRoot,
    value,
  });
}

add_task(async function test_clone_generalTypes() {
  const { browsingContext } = setupTest();

  // null
  equal(await clone({ browsingContext, value: undefined }), null);
  equal(await clone({ browsingContext, value: null }), null);

  // primitives
  equal(await clone({ browsingContext, value: true }), true);
  equal(await clone({ browsingContext, value: 42 }), 42);
  equal(await clone({ browsingContext, value: "foo" }), "foo");

  // toJSON
  equal(
    await clone({
      browsingContext,
      value: {
        toJSON() {
          return "foo";
        },
      },
    }),
    "foo"
  );
});

add_task(async function test_clone_ShadowRoot() {
  const { browsingContext, nodeCache, shadowRoot } = setupTest();

  async function getOrCreateNodeReference(bc, node) {
    equal(bc, browsingContext);
    equal(node, shadowRoot);

    const nodeRef = nodeCache.getOrCreateNodeReference(node);
    return WebReference.from(node, nodeRef);
  }

  // Fails with missing browsing context
  await Assert.rejects(
    json.clone({ getOrCreateNodeReference, value: shadowRoot }),
    /TypeError/,
    "Missing getOrCreateNodeReference callback expected to throw"
  );

  // Fails with missing getOrCreateNodeReference callback
  await Assert.rejects(
    json.clone({ browsingContext, value: shadowRoot }),
    /TypeError/,
    "Missing getOrCreateNodeReference callback expected to throw"
  );

  const shadowRootRef = nodeCache.getOrCreateNodeReference(shadowRoot);
  deepEqual(
    await clone({
      browsingContext,
      getOrCreateNodeReference,
      value: shadowRoot,
    }),
    WebReference.from(shadowRoot, shadowRootRef).toJSON()
  );
});

add_task(async function test_clone_WebElement() {
  const { browsingContext, htmlEl, nodeCache, svgEl } = setupTest();

  async function getOrCreateNodeReference(bc, node) {
    equal(bc, browsingContext);
    ok([htmlEl, svgEl].includes(node));

    const nodeRef = nodeCache.getOrCreateNodeReference(node);
    return WebReference.from(node, nodeRef);
  }

  // Fails with missing browsing context
  await Assert.rejects(
    json.clone({ getOrCreateNodeReference, value: htmlEl }),
    /TypeError/,
    "Missing getOrCreateNodeReference callback expected to throw"
  );

  // Fails with missing getOrCreateNodeReference callback
  await Assert.rejects(
    json.clone({ browsingContext, value: htmlEl }),
    /TypeError/,
    "Missing getOrCreateNodeReference callback expected to throw"
  );

  const htmlElRef = nodeCache.getOrCreateNodeReference(htmlEl);
  deepEqual(
    await clone({
      browsingContext,
      getOrCreateNodeReference,
      value: htmlEl,
    }),
    WebReference.from(htmlEl, htmlElRef).toJSON()
  );

  // Check an element with a different namespace
  const svgElRef = nodeCache.getOrCreateNodeReference(svgEl);
  deepEqual(
    await clone({
      browsingContext,
      getOrCreateNodeReference,
      value: svgEl,
    }),
    WebReference.from(svgEl, svgElRef).toJSON()
  );
});

add_task(async function test_clone_Sequences() {
  const { browsingContext, htmlEl, nodeCache } = setupTest();

  async function getOrCreateNodeReference(bc, node) {
    equal(bc, browsingContext);
    equal(node, htmlEl);

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

  const actual = await clone({
    browsingContext,
    getOrCreateNodeReference,
    value: input,
  });

  equal(actual[0], null);
  equal(actual[1], true);
  deepEqual(actual[2], []);
  deepEqual(actual[3], { [WebElement.Identifier]: htmlElRef });
  equal(actual[4], "foo");
  deepEqual(actual[5], { bar: "baz" });
});

add_task(async function test_clone_objects() {
  const { browsingContext, htmlEl, nodeCache } = setupTest();

  async function getOrCreateNodeReference(bc, node) {
    equal(bc, browsingContext);
    equal(node, htmlEl);

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

  const actual = await clone({
    browsingContext,
    getOrCreateNodeReference,
    value: input,
  });

  equal(actual.null, null);
  equal(actual.boolean, true);
  deepEqual(actual.array, [42]);
  deepEqual(actual.element, { [WebElement.Identifier]: htmlElRef });
  equal(actual.toJSON, "foo");
  deepEqual(actual.object, { bar: "baz" });
});

add_task(async function test_clone_сyclicReference() {
  const { browsingContext } = setupTest();

  const array = [];
  array.push(array);

  const obj = {};
  obj.reference = obj;

  // object
  await Assert.rejects(
    clone({ browsingContext, value: obj }),
    /JavaScriptError/,
    "Cyclic reference expected to throw"
  );

  // array
  await Assert.rejects(
    clone({ browsingContext, value: array }),
    /JavaScriptError/,
    "Cyclic reference expected to throw"
  );

  // array in object
  await Assert.rejects(
    clone({ browsingContext, value: { array } }),
    /JavaScriptError/,
    "Cyclic reference expected to throw"
  );

  // object in array
  await Assert.rejects(
    clone({ browsingContext, value: [obj] }),
    /JavaScriptError/,
    "Cyclic reference expected to throw"
  );
});

add_task(async function test_deserialize_generalTypes() {
  const { browsingContext } = setupTest();

  // null
  equal(await deserialize({ browsingContext, value: undefined }), undefined);
  equal(await deserialize({ browsingContext, value: null }), null);

  // primitives
  equal(await deserialize({ browsingContext, value: true }), true);
  equal(await deserialize({ browsingContext, value: 42 }), 42);
  equal(await deserialize({ browsingContext, value: "foo" }), "foo");
});

add_task(async function test_deserialize_ShadowRoot() {
  const { browsingContext, nodeCache, shadowRoot } = setupTest();

  const getKnownElement = async () => ok(false, "'getKnownElement' called");
  const getKnownShadowRoot = async (bc, nodeId) =>
    nodeCache.getNode(bc, nodeId);

  // Unknown shadow root
  const unknownShadowRootId = { [ShadowRoot.Identifier]: "foo" };
  equal(
    await deserialize({
      browsingContext,
      getKnownElement,
      getKnownShadowRoot,
      value: unknownShadowRootId,
    }),
    null
  );

  const shadowRootRef = nodeCache.getOrCreateNodeReference(shadowRoot);
  const shadowRootEl = { [ShadowRoot.Identifier]: shadowRootRef };

  // Fails with missing browsing context
  await Assert.rejects(
    json.deserialize({
      getKnownElement,
      getKnownShadowRoot,
      value: shadowRootEl,
    }),
    /TypeError/,
    "Missing browsing context expected to throw"
  );

  // Fails with missing getKnownShadowRoot callback
  await Assert.rejects(
    json.deserialize({ browsingContext, getKnownElement, value: shadowRootEl }),
    /TypeError/,
    "Missing getKnownShadowRoot callback expected to throw"
  );

  // Previously seen element is associated with original web element reference
  const root = await deserialize({
    browsingContext,
    getKnownShadowRoot,
    value: shadowRootEl,
  });
  deepEqual(root, nodeCache.getNode(browsingContext, shadowRootRef));
});

add_task(async function test_deserialize_WebElement() {
  const { browsingContext, htmlEl, nodeCache } = setupTest();

  const getKnownElement = async (bc, nodeId) => nodeCache.getNode(bc, nodeId);
  const getKnownShadowRoot = async () =>
    ok(false, "'getKnownShadowRoot' called");

  // Unknown element
  const unknownWebElId = { [WebElement.Identifier]: "foo" };
  equal(
    await json.deserialize({
      browsingContext,
      getKnownElement,
      getKnownShadowRoot,
      value: unknownWebElId,
    }),
    null
  );

  const htmlElRef = nodeCache.getOrCreateNodeReference(htmlEl);
  const htmlWebEl = { [WebElement.Identifier]: htmlElRef };

  // Fails with missing browsing context
  await Assert.rejects(
    json.deserialize({ getKnownElement, getKnownShadowRoot, value: htmlWebEl }),
    /TypeError/,
    "Missing browsing context expected to throw"
  );

  // Fails with missing getKnownElement callback
  await Assert.rejects(
    json.deserialize({ browsingContext, getKnownShadowRoot, value: htmlWebEl }),
    /TypeError/,
    "Missing getKnownElement callback expected to throw"
  );

  // Previously seen element is associated with original web element reference
  const el = await deserialize({
    browsingContext,
    getKnownElement,
    value: htmlWebEl,
  });
  deepEqual(el, htmlEl);
  deepEqual(el, nodeCache.getNode(browsingContext, htmlElRef));
});

add_task(async function test_deserialize_Sequences() {
  const { browsingContext, htmlEl, nodeCache } = setupTest();

  const htmlElRef = nodeCache.getOrCreateNodeReference(htmlEl);

  async function getKnownElement(bc, nodeId) {
    equal(bc, browsingContext);
    equal(nodeId, htmlElRef);

    return nodeCache.getNode(bc, nodeId);
  }

  const input = [
    null,
    true,
    [42],
    { [WebElement.Identifier]: htmlElRef },
    { bar: "baz" },
  ];

  const actual = await deserialize({
    browsingContext,
    getKnownElement,
    value: input,
  });

  equal(actual[0], null);
  equal(actual[1], true);
  deepEqual(actual[2], [42]);
  deepEqual(actual[3], htmlEl);
  deepEqual(actual[4], { bar: "baz" });
});

add_task(async function test_deserialize_objects() {
  const { browsingContext, htmlEl, nodeCache } = setupTest();

  const htmlElRef = nodeCache.getOrCreateNodeReference(htmlEl);

  async function getKnownElement(bc, nodeId) {
    equal(bc, browsingContext);
    equal(nodeId, htmlElRef);

    return nodeCache.getNode(bc, nodeId);
  }

  const input = {
    null: null,
    boolean: true,
    array: [42],
    element: { [WebElement.Identifier]: htmlElRef },
    object: { bar: "baz" },
  };

  const actual = await deserialize({
    browsingContext,
    getKnownElement,
    value: input,
  });

  equal(actual.null, null);
  equal(actual.boolean, true);
  deepEqual(actual.array, [42]);
  deepEqual(actual.element, htmlEl);
  deepEqual(actual.object, { bar: "baz" });

  nodeCache.clear({ all: true });
});
