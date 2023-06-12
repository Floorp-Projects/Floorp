const { NodeCache } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/webdriver/NodeCache.sys.mjs"
);

function setupTest() {
  const browser = Services.appShell.createWindowlessBrowser(false);

  browser.document.body.innerHTML = `
    <div id="foo" style="margin: 50px">
      <iframe></iframe>
      <video></video>
      <svg xmlns="http://www.w3.org/2000/svg"></svg>
      <textarea></textarea>
    </div>
    <div id="with-comment"><!-- Comment --></div>
  `;

  const divEl = browser.document.querySelector("div");
  const svgEl = browser.document.querySelector("svg");
  const textareaEl = browser.document.querySelector("textarea");
  const videoEl = browser.document.querySelector("video");

  const iframeEl = browser.document.querySelector("iframe");
  const childEl = iframeEl.contentDocument.createElement("div");
  iframeEl.contentDocument.body.appendChild(childEl);

  const shadowRoot = videoEl.openOrClosedShadowRoot;

  return {
    browser,
    nodeCache: new NodeCache(),
    childEl,
    divEl,
    iframeEl,
    shadowRoot,
    seenNodeIds: new Map(),
    svgEl,
    textareaEl,
    videoEl,
  };
}

add_task(function getOrCreateNodeReference_invalid() {
  const { nodeCache, seenNodeIds } = setupTest();

  const invalidValues = [null, undefined, "foo", 42, true, [], {}];

  for (const value of invalidValues) {
    info(`Testing value: ${value}`);
    Assert.throws(
      () => nodeCache.getOrCreateNodeReference(value, seenNodeIds),
      /TypeError/
    );
  }
});

add_task(function getOrCreateNodeReference_supportedNodeTypes() {
  const { browser, divEl, nodeCache, seenNodeIds } = setupTest();

  // Bug 1820734: No ownerGlobal is available in XPCShell tests
  // const xmlDocument = new DOMParser().parseFromString(
  //   "<xml></xml>",
  //   "application/xml"
  // );

  const values = [
    { node: divEl, type: Node.ELEMENT_NODE },
    { node: divEl.attributes[0], type: Node.ATTRIBUTE_NODE },
    { node: browser.document.createTextNode("foo"), type: Node.TEXT_NODE },
    // Bug 1820734: No ownerGlobal is available in XPCShell tests
    // {
    //   node: xmlDocument.createCDATASection("foo"),
    //   type: Node.CDATA_SECTION_NODE,
    // },
    {
      node: browser.document.createProcessingInstruction(
        "xml-stylesheet",
        "href='foo.css'"
      ),
      type: Node.PROCESSING_INSTRUCTION_NODE_NODE,
    },
    { node: browser.document.createComment("foo"), type: Node.COMMENT_NODE },
    { node: browser.document, type: Node.Document_NODE },
    {
      node: browser.document.implementation.createDocumentType(
        "foo",
        "bar",
        "dtd"
      ),
      type: Node.DOCUMENT_TYPE_NODE_NODE,
    },
    {
      node: browser.document.createDocumentFragment(),
      type: Node.DOCUMENT_FRAGMENT_NODE,
    },
  ];

  values.forEach((value, index) => {
    info(`Testing value: ${value.type}`);
    const nodeRef = nodeCache.getOrCreateNodeReference(value.node, seenNodeIds);
    equal(nodeCache.size, index + 1);
    equal(typeof nodeRef, "string");
    ok(seenNodeIds.get(browser.browsingContext).includes(nodeRef));
  });
});

add_task(function getOrCreateNodeReference_referenceAlreadyCreated() {
  const { browser, divEl, nodeCache, seenNodeIds } = setupTest();

  const divElRef = nodeCache.getOrCreateNodeReference(divEl, seenNodeIds);
  const divElRefOther = nodeCache.getOrCreateNodeReference(divEl, seenNodeIds);

  equal(divElRefOther, divElRef);
  equal(nodeCache.size, 1);
  equal(seenNodeIds.size, 1);
  ok(seenNodeIds.get(browser.browsingContext).includes(divElRef));
});

add_task(function getOrCreateNodeReference_differentReference() {
  const { browser, divEl, nodeCache, seenNodeIds, shadowRoot } = setupTest();

  const divElRef = nodeCache.getOrCreateNodeReference(divEl, seenNodeIds);
  equal(nodeCache.size, 1);
  equal(seenNodeIds.size, 1);
  ok(seenNodeIds.get(browser.browsingContext).includes(divElRef));

  const shadowRootRef = nodeCache.getOrCreateNodeReference(
    shadowRoot,
    seenNodeIds
  );
  equal(nodeCache.size, 2);
  equal(seenNodeIds.size, 1);
  ok(seenNodeIds.get(browser.browsingContext).includes(divElRef));
  ok(seenNodeIds.get(browser.browsingContext).includes(shadowRootRef));

  notEqual(divElRef, shadowRootRef);
});

add_task(function getOrCreateNodeReference_differentReferencePerNodeCache() {
  const { browser, divEl, nodeCache, seenNodeIds } = setupTest();
  const nodeCache2 = new NodeCache();

  const divElRef1 = nodeCache.getOrCreateNodeReference(divEl, seenNodeIds);
  const divElRef2 = nodeCache2.getOrCreateNodeReference(divEl, seenNodeIds);

  notEqual(divElRef1, divElRef2);
  equal(
    nodeCache.getNode(browser.browsingContext, divElRef1),
    nodeCache2.getNode(browser.browsingContext, divElRef2)
  );

  equal(seenNodeIds.size, 1);
  ok(seenNodeIds.get(browser.browsingContext).includes(divElRef1));
  ok(seenNodeIds.get(browser.browsingContext).includes(divElRef2));

  equal(nodeCache.getNode(browser.browsingContext, divElRef2), null);
});

add_task(function clear() {
  const { browser, divEl, nodeCache, seenNodeIds, svgEl } = setupTest();

  nodeCache.getOrCreateNodeReference(divEl, seenNodeIds);
  nodeCache.getOrCreateNodeReference(svgEl, seenNodeIds);
  equal(nodeCache.size, 2);
  equal(seenNodeIds.size, 1);

  // Clear requires explicit arguments.
  Assert.throws(() => nodeCache.clear(), /Error/);

  // Clear references for a different browsing context
  const browser2 = Services.appShell.createWindowlessBrowser(false);
  const imgEl = browser2.document.createElement("img");
  const imgElRef = nodeCache.getOrCreateNodeReference(imgEl, seenNodeIds);
  equal(nodeCache.size, 3);
  equal(seenNodeIds.size, 2);

  nodeCache.clear({ browsingContext: browser.browsingContext });
  equal(nodeCache.size, 1);
  equal(nodeCache.getNode(browser2.browsingContext, imgElRef), imgEl);

  // Clear all references
  nodeCache.getOrCreateNodeReference(divEl, seenNodeIds);
  equal(nodeCache.size, 2);
  equal(seenNodeIds.size, 2);

  nodeCache.clear({ all: true });
  equal(nodeCache.size, 0);
});

add_task(function getNode_multiple_nodes() {
  const { browser, divEl, nodeCache, seenNodeIds, svgEl } = setupTest();

  const divElRef = nodeCache.getOrCreateNodeReference(divEl, seenNodeIds);
  const svgElRef = nodeCache.getOrCreateNodeReference(svgEl, seenNodeIds);

  equal(nodeCache.getNode(browser.browsingContext, svgElRef), svgEl);
  equal(nodeCache.getNode(browser.browsingContext, divElRef), divEl);
});

add_task(function getNode_differentBrowsingContextInSameGroup() {
  const { iframeEl, divEl, nodeCache, seenNodeIds } = setupTest();

  const divElRef = nodeCache.getOrCreateNodeReference(divEl, seenNodeIds);
  equal(nodeCache.size, 1);

  equal(
    nodeCache.getNode(iframeEl.contentWindow.browsingContext, divElRef),
    divEl
  );
});

add_task(function getNode_differentBrowsingContextInOtherGroup() {
  const { divEl, nodeCache, seenNodeIds } = setupTest();

  const divElRef = nodeCache.getOrCreateNodeReference(divEl, seenNodeIds);
  equal(nodeCache.size, 1);

  const browser2 = Services.appShell.createWindowlessBrowser(false);
  equal(nodeCache.getNode(browser2.browsingContext, divElRef), null);
});

add_task(async function getNode_nodeDeleted() {
  const { browser, nodeCache, seenNodeIds } = setupTest();
  let el = browser.document.createElement("div");

  const elRef = nodeCache.getOrCreateNodeReference(el, seenNodeIds);

  // Delete element and force a garbage collection
  el = null;

  await doGC();

  equal(nodeCache.getNode(browser.browsingContext, elRef), null);
});

add_task(function getNodeDetails_forTopBrowsingContext() {
  const { browser, divEl, nodeCache, seenNodeIds } = setupTest();

  const divElRef = nodeCache.getOrCreateNodeReference(divEl, seenNodeIds);

  const nodeDetails = nodeCache.getReferenceDetails(divElRef);
  equal(nodeDetails.browserId, browser.browsingContext.browserId);
  equal(nodeDetails.browsingContextGroupId, browser.browsingContext.group.id);
  equal(nodeDetails.browsingContextId, browser.browsingContext.id);
  ok(nodeDetails.isTopBrowsingContext);
  ok(nodeDetails.nodeWeakRef);
  equal(nodeDetails.nodeWeakRef.get(), divEl);
});

add_task(async function getNodeDetails_forChildBrowsingContext() {
  const { browser, iframeEl, childEl, nodeCache, seenNodeIds } = setupTest();

  const childElRef = nodeCache.getOrCreateNodeReference(childEl, seenNodeIds);

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
});
