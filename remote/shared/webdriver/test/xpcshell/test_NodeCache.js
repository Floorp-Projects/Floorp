const { NodeCache } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/webdriver/NodeCache.sys.mjs"
);

const nodeCache = new NodeCache();

const SVG_NS = "http://www.w3.org/2000/svg";

const browser = Services.appShell.createWindowlessBrowser(false);

const domEl = browser.document.createElement("div");
browser.document.body.appendChild(domEl);

const svgEl = browser.document.createElementNS(SVG_NS, "rect");
browser.document.body.appendChild(svgEl);

registerCleanupFunction(() => {
  nodeCache.clear({ all: true });
});

add_test(function addElement() {
  const domElRef = nodeCache.add(domEl);
  equal(nodeCache.size, 1);

  const domElRefOther = nodeCache.add(domEl);
  equal(nodeCache.size, 1);
  equal(domElRefOther, domElRef);

  nodeCache.add(svgEl);
  equal(nodeCache.size, 2);

  run_next_test();
});

add_test(function addInvalidElement() {
  Assert.throws(() => nodeCache.add("foo"), /UnknownError/);

  run_next_test();
});

add_test(function clear() {
  nodeCache.add(domEl);
  nodeCache.add(svgEl);
  equal(nodeCache.size, 2);

  // Clear requires explicit arguments.
  Assert.throws(() => nodeCache.clear(), /Error/);

  // Clear references for a different browsing context
  const browser2 = Services.appShell.createWindowlessBrowser(false);
  let imgEl = browser2.document.createElement("img");
  browser2.document.body.appendChild(imgEl);

  nodeCache.add(imgEl);
  nodeCache.clear({ browsingContext: browser.browsingContext });
  equal(nodeCache.size, 1);

  // Clear all references
  nodeCache.add(domEl);
  equal(nodeCache.size, 2);

  nodeCache.clear({ all: true });
  equal(nodeCache.size, 0);

  run_next_test();
});

add_test(function resolveElement() {
  const domElSharedId = nodeCache.add(domEl);
  deepEqual(nodeCache.resolve(domElSharedId), domEl);

  const svgElSharedId = nodeCache.add(svgEl);
  deepEqual(nodeCache.resolve(svgElSharedId), svgEl);
  deepEqual(nodeCache.resolve(domElSharedId), domEl);

  run_next_test();
});

add_test(function resolveUnknownElement() {
  Assert.throws(() => nodeCache.resolve("foo"), /NoSuchElementError/);

  run_next_test();
});

add_test(function resolveElementNotAttachedToDOM() {
  const imgEl = browser.document.createElement("img");

  const imgElSharedId = nodeCache.add(imgEl);
  deepEqual(nodeCache.resolve(imgElSharedId), imgEl);

  run_next_test();
});

add_test(async function resolveElementRemoved() {
  let imgEl = browser.document.createElement("img");
  const imgElSharedId = nodeCache.add(imgEl);

  // Delete element and force a garbage collection
  imgEl = null;

  await doGC();

  const el = nodeCache.resolve(imgElSharedId);
  deepEqual(el, null);

  run_next_test();
});

add_test(function elementReferencesDifferentPerNodeCache() {
  const sharedId = nodeCache.add(domEl);

  const nodeCache2 = new NodeCache();
  const sharedId2 = nodeCache2.add(domEl);

  notEqual(sharedId, sharedId2);
  equal(nodeCache.resolve(sharedId), nodeCache2.resolve(sharedId2));

  Assert.throws(() => nodeCache.resolve(sharedId2), /NoSuchElementError/);

  nodeCache2.clear({ all: true });

  run_next_test();
});
