const {element, WebElement} = ChromeUtils.import("chrome://marionette/content/element.js", {});
const {evaluate} = ChromeUtils.import("chrome://marionette/content/evaluate.js", {});

const SVGNS = "http://www.w3.org/2000/svg";
const XBLNS = "http://www.mozilla.org/xbl";
const XHTMLNS = "http://www.w3.org/1999/xhtml";
const XULNS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

class Element {
  constructor(tagName, attrs = {}) {
    this.tagName = tagName;
    this.localName = tagName;

    for (let attr in attrs) {
      this[attr] = attrs[attr];
    }
  }

  get nodeType() { return 1; }
  get ELEMENT_NODE() { return 1; }
}

class DOMElement extends Element {
  constructor(tagName, attrs = {}) {
    super(tagName, attrs);
    this.namespaceURI = XHTMLNS;
  }
}

class SVGElement extends Element {
  constructor(tagName, attrs = {}) {
    super(tagName, attrs);
    this.namespaceURI = SVGNS;
  }
}

class XULElement extends Element {
  constructor(tagName, attrs = {}) {
    super(tagName, attrs);
    this.namespaceURI = XULNS;
  }
}

class XBLElement extends XULElement {
  constructor(tagName, attrs = {}) {
    super(tagName, attrs);
    this.namespaceURI = XBLNS;
  }
}

const domEl = new DOMElement("p");
const svgEl = new SVGElement("rect");
const xulEl = new XULElement("browser");
const xblEl = new XBLElement("framebox");

const seenEls = new element.Store();

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

  // elements
  ok(WebElement.isReference(evaluate.toJSON(domEl, seenEls)));
  ok(WebElement.isReference(evaluate.toJSON(svgEl, seenEls)));
  ok(WebElement.isReference(evaluate.toJSON(xulEl, seenEls)));
  ok(WebElement.isReference(evaluate.toJSON(xblEl, seenEls)));

  // toJSON
  equal("foo", evaluate.toJSON({toJSON() { return "foo"; }}));

  // arbitrary object
  deepEqual({foo: "bar"}, evaluate.toJSON({foo: "bar"}));

  run_next_test();
});


add_test(function test_toJSON_sequences() {
  let input = [null, true, [], domEl, {toJSON() { return "foo"; }}, {bar: "baz"}];
  let actual = evaluate.toJSON(input, seenEls);

  equal(null, actual[0]);
  equal(true, actual[1]);
  deepEqual([], actual[2]);
  ok(WebElement.isReference(actual[3]));
  equal("foo", actual[4]);
  deepEqual({bar: "baz"}, actual[5]);

  run_next_test();
});

add_test(function test_toJSON_objects() {
  let input = {
    "null": null,
    "boolean": true,
    "array": [],
    "webElement": domEl,
    "toJSON": {toJSON() { return "foo"; }},
    "object": {"bar": "baz"},
  };
  let actual = evaluate.toJSON(input, seenEls);

  equal(null, actual.null);
  equal(true, actual.boolean);
  deepEqual([], actual.array);
  ok(WebElement.isReference(actual.webElement));
  equal("foo", actual.toJSON);
  deepEqual({"bar": "baz"}, actual.object);

  run_next_test();
});
