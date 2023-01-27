const { NodeCache } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/webdriver/NodeCache.sys.mjs"
);

function setupTest() {
  const browser = Services.appShell.createWindowlessBrowser(false);
  const nodeCache = new NodeCache();

  const htmlEl = browser.document.createElement("video");
  htmlEl.setAttribute("id", "foo");
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

add_test(function getOrCreateNodeReference_invalid() {
  const { htmlEl, nodeCache } = setupTest();

  const invalidValues = [
    null,
    undefined,
    "foo",
    42,
    true,
    [],
    {},
    htmlEl.attributes[0],
  ];

  for (const value of invalidValues) {
    info(`Testing value: ${value}`);
    Assert.throws(() => nodeCache.getOrCreateNodeReference(value), /TypeError/);
  }

  run_next_test();
});

add_test(function getOrCreateNodeReference_supportedNodeTypes() {
  const { htmlEl, nodeCache, shadowRoot } = setupTest();

  const htmlElRef = nodeCache.getOrCreateNodeReference(htmlEl);
  equal(nodeCache.size, 1);

  const shadowRootRef = nodeCache.getOrCreateNodeReference(shadowRoot);
  equal(nodeCache.size, 2);

  notEqual(htmlElRef, shadowRootRef);

  run_next_test();
});

add_test(function getOrCreateNodeReference_referenceAlreadyCreated() {
  const { htmlEl, nodeCache } = setupTest();

  const htmlElRef = nodeCache.getOrCreateNodeReference(htmlEl);
  const htmlElRefOther = nodeCache.getOrCreateNodeReference(htmlEl);
  equal(nodeCache.size, 1);
  equal(htmlElRefOther, htmlElRef);

  run_next_test();
});

add_test(function getOrCreateNodeReference_differentReferencePerNodeCache() {
  const { browser, htmlEl, nodeCache } = setupTest();
  const nodeCache2 = new NodeCache();

  const htmlElRef1 = nodeCache.getOrCreateNodeReference(htmlEl);
  const htmlElRef2 = nodeCache2.getOrCreateNodeReference(htmlEl);

  notEqual(htmlElRef1, htmlElRef2);
  equal(
    nodeCache.getNode(browser.browsingContext, htmlElRef1),
    nodeCache2.getNode(browser.browsingContext, htmlElRef2)
  );

  equal(nodeCache.getNode(browser.browsingContext, htmlElRef2), null);

  run_next_test();
});

add_test(function clear() {
  const { browser, htmlEl, nodeCache, svgEl } = setupTest();

  nodeCache.getOrCreateNodeReference(htmlEl);
  nodeCache.getOrCreateNodeReference(svgEl);
  equal(nodeCache.size, 2);

  // Clear requires explicit arguments.
  Assert.throws(() => nodeCache.clear(), /Error/);

  // Clear references for a different browsing context
  const browser2 = Services.appShell.createWindowlessBrowser(false);
  const imgEl = browser2.document.createElement("img");
  const imgElRef = nodeCache.getOrCreateNodeReference(imgEl);
  equal(nodeCache.size, 3);

  nodeCache.clear({ browsingContext: browser.browsingContext });
  equal(nodeCache.size, 1);
  equal(nodeCache.getNode(browser2.browsingContext, imgElRef), imgEl);

  // Clear all references
  nodeCache.getOrCreateNodeReference(htmlEl);
  equal(nodeCache.size, 2);

  nodeCache.clear({ all: true });
  equal(nodeCache.size, 0);

  run_next_test();
});

add_test(function getNode_multiple_nodes() {
  const { browser, htmlEl, nodeCache, svgEl } = setupTest();

  const htmlElRef = nodeCache.getOrCreateNodeReference(htmlEl);
  const svgElRef = nodeCache.getOrCreateNodeReference(svgEl);

  equal(nodeCache.getNode(browser.browsingContext, svgElRef), svgEl);
  equal(nodeCache.getNode(browser.browsingContext, htmlElRef), htmlEl);

  run_next_test();
});

add_test(function getNode_differentBrowsingContextInSameGroup() {
  const { iframeEl, htmlEl, nodeCache } = setupTest();

  const htmlElRef = nodeCache.getOrCreateNodeReference(htmlEl);
  equal(nodeCache.size, 1);

  equal(
    nodeCache.getNode(iframeEl.contentWindow.browsingContext, htmlElRef),
    htmlEl
  );

  run_next_test();
});

add_test(function getNode_differentBrowsingContextInOtherGroup() {
  const { htmlEl, nodeCache } = setupTest();

  const htmlElRef = nodeCache.getOrCreateNodeReference(htmlEl);
  equal(nodeCache.size, 1);

  const browser2 = Services.appShell.createWindowlessBrowser(false);
  equal(nodeCache.getNode(browser2.browsingContext, htmlElRef), null);

  run_next_test();
});

add_test(async function getNode_nodeDeleted() {
  const { browser, nodeCache } = setupTest();
  let el = browser.document.createElement("div");

  const elRef = nodeCache.getOrCreateNodeReference(el);

  // Delete element and force a garbage collection
  el = null;

  await doGC();

  equal(nodeCache.getNode(browser.browsingContext, elRef), null);

  run_next_test();
});

add_test(function getNodeDetails_forTopBrowsingContext() {
  const { browser, htmlEl, nodeCache } = setupTest();

  const htmlElRef = nodeCache.getOrCreateNodeReference(htmlEl);

  const nodeDetails = nodeCache.getReferenceDetails(htmlElRef);
  equal(nodeDetails.browserId, browser.browsingContext.browserId);
  equal(nodeDetails.browsingContextGroupId, browser.browsingContext.group.id);
  equal(nodeDetails.browsingContextId, browser.browsingContext.id);
  ok(nodeDetails.isTopBrowsingContext);
  ok(nodeDetails.nodeWeakRef);
  equal(nodeDetails.nodeWeakRef.get(), htmlEl);

  run_next_test();
});

add_test(async function getNodeDetails_forChildBrowsingContext() {
  const { browser, iframeEl, childEl, nodeCache } = setupTest();

  const childElRef = nodeCache.getOrCreateNodeReference(childEl);

  const nodeDetails = nodeCache.getReferenceDetails(childElRef);
  equal(nodeDetails.browserId, browser.browsingContext.browserId);
  equal(nodeDetails.browsingContextGroupId, browser.browsingContext.group.id);
  equal(
    nodeDetails.browsingContextId,
    iframeEl.contentWindow.browsingContext.id
  );
  ok(!nodeDetails.isTopBrowsingContext);
  ok(nodeDetails.nodeWeakRef);
  equal(nodeDetails.nodeWeakRef.get(), childEl);

  run_next_test();
});
