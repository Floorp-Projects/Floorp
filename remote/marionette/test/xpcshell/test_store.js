const { element, WebReference } = ChromeUtils.import(
  "chrome://remote/content/marionette/element.js"
);

const SVG_NS = "http://www.w3.org/2000/svg";
const XHTML_NS = "http://www.w3.org/1999/xhtml";
const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

class Element {
  constructor(tagName, attrs = {}) {
    this.tagName = tagName;
    this.localName = tagName;

    // Set default properties
    this.isConnected = true;
    this.ownerDocument = {};
    this.ownerGlobal = { document: this.ownerDocument };

    for (let attr in attrs) {
      this[attr] = attrs[attr];
    }
  }

  get nodeType() {
    return 1;
  }
  get ELEMENT_NODE() {
    return 1;
  }
}

class DOMElement extends Element {
  constructor(tagName, attrs = {}) {
    super(tagName, attrs);
    this.namespaceURI = XHTML_NS;
    this.ownerDocument = { documentElement: { namespaceURI: XHTML_NS } };
  }
}

class SVGElement extends Element {
  constructor(tagName, attrs = {}) {
    super(tagName, attrs);
    this.namespaceURI = SVG_NS;
    this.ownerDocument = { documentElement: { namespaceURI: SVG_NS } };
  }
}

class XULElement extends Element {
  constructor(tagName, attrs = {}) {
    super(tagName, attrs);
    this.namespaceURI = XUL_NS;
    this.ownerDocument = { documentElement: { namespaceURI: XUL_NS } };
  }
}

function makeIterator(items) {
  return function*() {
    for (const i of items) {
      yield i;
    }
  };
}

const nestedBrowsingContext = {
  id: 7,
  getAllBrowsingContextsInSubtree: makeIterator([
    { id: 7 },
    { id: 71 },
    { id: 72 },
  ]),
};

const domEl = new DOMElement("p");
const svgEl = new SVGElement("rect");
const xulEl = new XULElement("browser");
const frameEl = new DOMElement("iframe");
const innerEl = new DOMElement("p", { id: "inner" });

const domWebEl = WebReference.from(domEl);
const svgWebEl = WebReference.from(svgEl);
const xulWebEl = WebReference.from(xulEl);
const frameWebEl = WebReference.from(frameEl);
const innerWebEl = WebReference.from(innerEl);

const domElId = { id: 1, browsingContextId: 4, webElRef: domWebEl.toJSON() };
const svgElId = { id: 2, browsingContextId: 15, webElRef: svgWebEl.toJSON() };
const xulElId = { id: 3, browsingContextId: 15, webElRef: xulWebEl.toJSON() };
const frameElId = {
  id: 10,
  browsingContextId: 7,
  webElRef: frameWebEl.toJSON(),
};
const innerElId = {
  id: 11,
  browsingContextId: 72,
  webElRef: innerWebEl.toJSON(),
};

const elementIdCache = new element.ReferenceStore();

registerCleanupFunction(() => {
  elementIdCache.clear();
});

add_test(function test_add_element() {
  elementIdCache.add(domElId);
  equal(elementIdCache.refs.size, 1);
  equal(elementIdCache.domRefs.size, 1);
  deepEqual(elementIdCache.refs.get(domWebEl.uuid), domElId);
  deepEqual(elementIdCache.domRefs.get(domElId.id), domWebEl.toJSON());

  elementIdCache.add(domElId);
  equal(elementIdCache.refs.size, 1);
  equal(elementIdCache.domRefs.size, 1);

  elementIdCache.add(xulElId);
  equal(elementIdCache.refs.size, 2);
  equal(elementIdCache.domRefs.size, 2);

  elementIdCache.clear();
  equal(elementIdCache.refs.size, 0);
  equal(elementIdCache.domRefs.size, 0);

  run_next_test();
});

add_test(function test_get_element() {
  elementIdCache.add(domElId);
  deepEqual(elementIdCache.get(domWebEl), domElId);

  run_next_test();
});

add_test(function test_get_no_such_element() {
  throws(() => elementIdCache.get(frameWebEl), /NoSuchElementError/);

  elementIdCache.add(domElId);
  throws(() => elementIdCache.get(frameWebEl), /NoSuchElementError/);

  run_next_test();
});

add_test(function test_clear_by_unknown_browsing_context() {
  const unknownContext = {
    id: 1000,
    getAllBrowsingContextsInSubtree: makeIterator([{ id: 1000 }]),
  };
  elementIdCache.add(domElId);
  elementIdCache.add(svgElId);
  elementIdCache.add(xulElId);
  elementIdCache.add(frameElId);
  elementIdCache.add(innerElId);

  equal(elementIdCache.refs.size, 5);
  equal(elementIdCache.domRefs.size, 5);

  elementIdCache.clear(unknownContext);

  equal(elementIdCache.refs.size, 5);
  equal(elementIdCache.domRefs.size, 5);

  run_next_test();
});

add_test(function test_clear_by_known_browsing_context() {
  const context = {
    id: 15,
    getAllBrowsingContextsInSubtree: makeIterator([{ id: 15 }]),
  };
  const anotherContext = {
    id: 4,
    getAllBrowsingContextsInSubtree: makeIterator([{ id: 4 }]),
  };
  elementIdCache.add(domElId);
  elementIdCache.add(svgElId);
  elementIdCache.add(xulElId);
  elementIdCache.add(frameElId);
  elementIdCache.add(innerElId);

  equal(elementIdCache.refs.size, 5);
  equal(elementIdCache.domRefs.size, 5);

  elementIdCache.clear(context);

  equal(elementIdCache.refs.size, 3);
  equal(elementIdCache.domRefs.size, 3);
  ok(elementIdCache.has(domWebEl));
  ok(!elementIdCache.has(svgWebEl));
  ok(!elementIdCache.has(xulWebEl));

  elementIdCache.clear(anotherContext);

  equal(elementIdCache.refs.size, 2);
  equal(elementIdCache.domRefs.size, 2);
  ok(!elementIdCache.has(domWebEl));

  run_next_test();
});

add_test(function test_clear_by_nested_browsing_context() {
  elementIdCache.add(domElId);
  elementIdCache.add(svgElId);
  elementIdCache.add(xulElId);
  elementIdCache.add(frameElId);
  elementIdCache.add(innerElId);

  equal(elementIdCache.refs.size, 5);
  equal(elementIdCache.domRefs.size, 5);

  elementIdCache.clear(nestedBrowsingContext);

  equal(elementIdCache.refs.size, 3);
  equal(elementIdCache.domRefs.size, 3);

  ok(elementIdCache.has(domWebEl));
  ok(!elementIdCache.has(frameWebEl));
  ok(!elementIdCache.has(innerWebEl));

  run_next_test();
});
