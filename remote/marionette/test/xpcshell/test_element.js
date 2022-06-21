/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const {
  ChromeWebElement,
  ContentWebElement,
  ContentWebFrame,
  ContentWebWindow,
  element,
  WebElement,
} = ChromeUtils.import("chrome://remote/content/marionette/element.js");

const SVG_NS = "http://www.w3.org/2000/svg";
const XHTML_NS = "http://www.w3.org/1999/xhtml";
const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

class Element {
  constructor(tagName, attrs = {}) {
    this.tagName = tagName;
    this.localName = tagName;

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

  // this is a severely limited CSS selector
  // that only supports lists of tag names
  matches(selector) {
    let tags = selector.split(",");
    return tags.includes(this.localName);
  }
}

class DOMElement extends Element {
  constructor(tagName, attrs = {}) {
    super(tagName, attrs);

    if (typeof this.namespaceURI == "undefined") {
      this.namespaceURI = XHTML_NS;
    }
    if (typeof this.ownerDocument == "undefined") {
      this.ownerDocument = { designMode: "off" };
    }
    if (typeof this.ownerDocument.documentElement == "undefined") {
      this.ownerDocument.documentElement = { namespaceURI: XHTML_NS };
    }

    if (typeof this.type == "undefined") {
      this.type = "text";
    }

    if (this.localName == "option") {
      this.selected = false;
    }

    if (
      this.localName == "input" &&
      ["checkbox", "radio"].includes(this.type)
    ) {
      this.checked = false;
    }
  }

  getBoundingClientRect() {
    return {
      top: 0,
      left: 0,
      width: 100,
      height: 100,
    };
  }
}

class SVGElement extends Element {
  constructor(tagName, attrs = {}) {
    super(tagName, attrs);
    this.namespaceURI = SVG_NS;
  }
}

class XULElement extends Element {
  constructor(tagName, attrs = {}) {
    super(tagName, attrs);
    this.namespaceURI = XUL_NS;

    if (typeof this.ownerDocument == "undefined") {
      this.ownerDocument = {};
    }
    if (typeof this.ownerDocument.documentElement == "undefined") {
      this.ownerDocument.documentElement = { namespaceURI: XUL_NS };
    }
  }
}

const domEl = new DOMElement("p");
const svgEl = new SVGElement("rect");
const xulEl = new XULElement("text");

const domElInPrivilegedDocument = new Element("input", {
  nodePrincipal: { isSystemPrincipal: true },
});
const xulElInPrivilegedDocument = new XULElement("text", {
  nodePrincipal: { isSystemPrincipal: true },
});

class WindowProxy {
  get parent() {
    return this;
  }
  get self() {
    return this;
  }
  toString() {
    return "[object Window]";
  }
}
const domWin = new WindowProxy();
const domFrame = new (class extends WindowProxy {
  get parent() {
    return domWin;
  }
})();

add_test(function test_findClosest() {
  equal(element.findClosest(domEl, "foo"), null);

  let foo = new DOMElement("foo");
  let bar = new DOMElement("bar");
  bar.parentNode = foo;
  equal(element.findClosest(bar, "foo"), foo);

  run_next_test();
});

add_test(function test_isSelected() {
  let checkbox = new DOMElement("input", { type: "checkbox" });
  ok(!element.isSelected(checkbox));
  checkbox.checked = true;
  ok(element.isSelected(checkbox));

  // selected is not a property of <input type=checkbox>
  checkbox.selected = true;
  checkbox.checked = false;
  ok(!element.isSelected(checkbox));

  let option = new DOMElement("option");
  ok(!element.isSelected(option));
  option.selected = true;
  ok(element.isSelected(option));

  // checked is not a property of <option>
  option.checked = true;
  option.selected = false;
  ok(!element.isSelected(option));

  // anything else should not be selected
  for (let typ of [domEl, undefined, null, "foo", true, [], {}]) {
    ok(!element.isSelected(typ));
  }

  run_next_test();
});

add_test(function test_isElement() {
  ok(element.isElement(domEl));
  ok(element.isElement(svgEl));
  ok(element.isElement(xulEl));
  ok(!element.isElement(domWin));
  ok(!element.isElement(domFrame));
  for (let typ of [true, 42, {}, [], undefined, null]) {
    ok(!element.isElement(typ));
  }

  run_next_test();
});

add_test(function test_isDOMElement() {
  ok(element.isDOMElement(domEl));
  ok(element.isDOMElement(domElInPrivilegedDocument));
  ok(element.isDOMElement(svgEl));
  ok(!element.isDOMElement(xulEl));
  ok(!element.isDOMElement(xulElInPrivilegedDocument));
  ok(!element.isDOMElement(domWin));
  ok(!element.isDOMElement(domFrame));
  for (let typ of [true, 42, {}, [], undefined, null]) {
    ok(!element.isDOMElement(typ));
  }

  run_next_test();
});

add_test(function test_isXULElement() {
  ok(element.isXULElement(xulEl));
  ok(element.isXULElement(xulElInPrivilegedDocument));
  ok(!element.isXULElement(domElInPrivilegedDocument));
  ok(!element.isXULElement(domEl));
  ok(!element.isXULElement(svgEl));
  ok(!element.isXULElement(domWin));
  ok(!element.isXULElement(domFrame));
  for (let typ of [true, 42, {}, [], undefined, null]) {
    ok(!element.isXULElement(typ));
  }

  run_next_test();
});

add_test(function test_isDOMWindow() {
  ok(element.isDOMWindow(domWin));
  ok(element.isDOMWindow(domFrame));
  ok(!element.isDOMWindow(domEl));
  ok(!element.isDOMWindow(domElInPrivilegedDocument));
  ok(!element.isDOMWindow(svgEl));
  ok(!element.isDOMWindow(xulEl));
  for (let typ of [true, 42, {}, [], undefined, null]) {
    ok(!element.isDOMWindow(typ));
  }

  run_next_test();
});

add_test(function test_isReadOnly() {
  ok(!element.isReadOnly(null));
  ok(!element.isReadOnly(domEl));
  ok(!element.isReadOnly(new DOMElement("p", { readOnly: true })));
  ok(element.isReadOnly(new DOMElement("input", { readOnly: true })));
  ok(element.isReadOnly(new DOMElement("textarea", { readOnly: true })));

  run_next_test();
});

add_test(function test_isDisabled() {
  ok(!element.isDisabled(new DOMElement("p")));
  ok(!element.isDisabled(new SVGElement("rect", { disabled: true })));
  ok(!element.isDisabled(new XULElement("browser", { disabled: true })));

  let select = new DOMElement("select", { disabled: true });
  let option = new DOMElement("option");
  option.parentNode = select;
  ok(element.isDisabled(option));

  let optgroup = new DOMElement("optgroup", { disabled: true });
  option.parentNode = optgroup;
  optgroup.parentNode = select;
  select.disabled = false;
  ok(element.isDisabled(option));

  ok(element.isDisabled(new DOMElement("button", { disabled: true })));
  ok(element.isDisabled(new DOMElement("input", { disabled: true })));
  ok(element.isDisabled(new DOMElement("select", { disabled: true })));
  ok(element.isDisabled(new DOMElement("textarea", { disabled: true })));

  run_next_test();
});

add_test(function test_isEditingHost() {
  ok(!element.isEditingHost(null));
  ok(element.isEditingHost(new DOMElement("p", { isContentEditable: true })));
  ok(
    element.isEditingHost(
      new DOMElement("p", { ownerDocument: { designMode: "on" } })
    )
  );

  run_next_test();
});

add_test(function test_isEditable() {
  ok(!element.isEditable(null));
  ok(!element.isEditable(domEl));
  ok(!element.isEditable(new DOMElement("textarea", { readOnly: true })));
  ok(!element.isEditable(new DOMElement("textarea", { disabled: true })));

  for (let type of [
    "checkbox",
    "radio",
    "hidden",
    "submit",
    "button",
    "image",
  ]) {
    ok(!element.isEditable(new DOMElement("input", { type })));
  }
  ok(element.isEditable(new DOMElement("input", { type: "text" })));
  ok(element.isEditable(new DOMElement("input")));

  ok(element.isEditable(new DOMElement("textarea")));
  ok(
    element.isEditable(
      new DOMElement("p", { ownerDocument: { designMode: "on" } })
    )
  );
  ok(element.isEditable(new DOMElement("p", { isContentEditable: true })));

  run_next_test();
});

add_test(function test_isMutableFormControlElement() {
  ok(!element.isMutableFormControl(null));
  ok(
    !element.isMutableFormControl(
      new DOMElement("textarea", { readOnly: true })
    )
  );
  ok(
    !element.isMutableFormControl(
      new DOMElement("textarea", { disabled: true })
    )
  );

  const mutableStates = new Set([
    "color",
    "date",
    "datetime-local",
    "email",
    "file",
    "month",
    "number",
    "password",
    "range",
    "search",
    "tel",
    "text",
    "url",
    "week",
  ]);
  for (let type of mutableStates) {
    ok(element.isMutableFormControl(new DOMElement("input", { type })));
  }
  ok(element.isMutableFormControl(new DOMElement("textarea")));

  ok(
    !element.isMutableFormControl(new DOMElement("input", { type: "hidden" }))
  );
  ok(!element.isMutableFormControl(new DOMElement("p")));
  ok(
    !element.isMutableFormControl(
      new DOMElement("p", { isContentEditable: true })
    )
  );
  ok(
    !element.isMutableFormControl(
      new DOMElement("p", { ownerDocument: { designMode: "on" } })
    )
  );

  run_next_test();
});

add_test(function test_coordinates() {
  let p = element.coordinates(domEl);
  ok(p.hasOwnProperty("x"));
  ok(p.hasOwnProperty("y"));
  equal("number", typeof p.x);
  equal("number", typeof p.y);

  deepEqual({ x: 50, y: 50 }, element.coordinates(domEl));
  deepEqual({ x: 10, y: 10 }, element.coordinates(domEl, 10, 10));
  deepEqual({ x: -5, y: -5 }, element.coordinates(domEl, -5, -5));

  Assert.throws(() => element.coordinates(null), /node is null/);

  Assert.throws(
    () => element.coordinates(domEl, "string", undefined),
    /Offset must be a number/
  );
  Assert.throws(
    () => element.coordinates(domEl, undefined, "string"),
    /Offset must be a number/
  );
  Assert.throws(
    () => element.coordinates(domEl, "string", "string"),
    /Offset must be a number/
  );
  Assert.throws(
    () => element.coordinates(domEl, {}, undefined),
    /Offset must be a number/
  );
  Assert.throws(
    () => element.coordinates(domEl, undefined, {}),
    /Offset must be a number/
  );
  Assert.throws(
    () => element.coordinates(domEl, {}, {}),
    /Offset must be a number/
  );
  Assert.throws(
    () => element.coordinates(domEl, [], undefined),
    /Offset must be a number/
  );
  Assert.throws(
    () => element.coordinates(domEl, undefined, []),
    /Offset must be a number/
  );
  Assert.throws(
    () => element.coordinates(domEl, [], []),
    /Offset must be a number/
  );

  run_next_test();
});

add_test(function test_WebElement_ctor() {
  let el = new WebElement("foo");
  equal(el.uuid, "foo");

  for (let t of [42, true, [], {}, null, undefined]) {
    Assert.throws(() => new WebElement(t), /to be a string/);
  }

  run_next_test();
});

add_test(function test_WebElemenet_is() {
  let a = new WebElement("a");
  let b = new WebElement("b");

  ok(a.is(a));
  ok(b.is(b));
  ok(!a.is(b));
  ok(!b.is(a));

  ok(!a.is({}));

  run_next_test();
});

add_test(function test_WebElement_from() {
  ok(WebElement.from(domEl) instanceof ContentWebElement);
  ok(WebElement.from(xulEl) instanceof ContentWebElement);
  ok(WebElement.from(domWin) instanceof ContentWebWindow);
  ok(WebElement.from(domFrame) instanceof ContentWebFrame);
  ok(WebElement.from(domElInPrivilegedDocument) instanceof ChromeWebElement);
  ok(WebElement.from(xulElInPrivilegedDocument) instanceof ChromeWebElement);

  Assert.throws(() => WebElement.from({}), /InvalidArgumentError/);

  run_next_test();
});

add_test(function test_WebElement_fromJSON_ContentWebElement() {
  const { Identifier } = ContentWebElement;

  let ref = { [Identifier]: "foo" };
  let webEl = WebElement.fromJSON(ref);
  ok(webEl instanceof ContentWebElement);
  equal(webEl.uuid, "foo");

  let identifierPrecedence = {
    [Identifier]: "identifier-uuid",
  };
  let precedenceEl = WebElement.fromJSON(identifierPrecedence);
  ok(precedenceEl instanceof ContentWebElement);
  equal(precedenceEl.uuid, "identifier-uuid");

  run_next_test();
});

add_test(function test_WebElement_fromJSON_ContentWebWindow() {
  let ref = { [ContentWebWindow.Identifier]: "foo" };
  let win = WebElement.fromJSON(ref);
  ok(win instanceof ContentWebWindow);
  equal(win.uuid, "foo");

  run_next_test();
});

add_test(function test_WebElement_fromJSON_ContentWebFrame() {
  let ref = { [ContentWebFrame.Identifier]: "foo" };
  let frame = WebElement.fromJSON(ref);
  ok(frame instanceof ContentWebFrame);
  equal(frame.uuid, "foo");

  run_next_test();
});

add_test(function test_WebElement_fromJSON_ChromeWebElement() {
  let ref = { [ChromeWebElement.Identifier]: "foo" };
  let el = WebElement.fromJSON(ref);
  ok(el instanceof ChromeWebElement);
  equal(el.uuid, "foo");

  run_next_test();
});

add_test(function test_WebElement_fromJSON_malformed() {
  Assert.throws(() => WebElement.fromJSON({}), /InvalidArgumentError/);
  Assert.throws(() => WebElement.fromJSON(null), /InvalidArgumentError/);
  run_next_test();
});

add_test(function test_WebElement_fromUUID() {
  let xulWebEl = WebElement.fromUUID("foo", "chrome");
  ok(xulWebEl instanceof ChromeWebElement);
  equal(xulWebEl.uuid, "foo");

  let domWebEl = WebElement.fromUUID("bar", "content");
  ok(domWebEl instanceof ContentWebElement);
  equal(domWebEl.uuid, "bar");

  Assert.throws(
    () => WebElement.fromUUID("baz", "bah"),
    /InvalidArgumentError/
  );

  run_next_test();
});

add_test(function test_WebElement_isReference() {
  for (let t of [42, true, "foo", [], {}]) {
    ok(!WebElement.isReference(t));
  }

  ok(WebElement.isReference({ [ContentWebElement.Identifier]: "foo" }));
  ok(WebElement.isReference({ [ContentWebWindow.Identifier]: "foo" }));
  ok(WebElement.isReference({ [ContentWebFrame.Identifier]: "foo" }));
  ok(WebElement.isReference({ [ChromeWebElement.Identifier]: "foo" }));

  run_next_test();
});

add_test(function test_WebElement_generateUUID() {
  equal(typeof WebElement.generateUUID(), "string");
  run_next_test();
});

add_test(function test_ContentWebElement_toJSON() {
  const { Identifier } = ContentWebElement;

  let el = new ContentWebElement("foo");
  let json = el.toJSON();

  ok(Identifier in json);
  equal(json[Identifier], "foo");

  run_next_test();
});

add_test(function test_ContentWebElement_fromJSON() {
  const { Identifier } = ContentWebElement;

  let el = ContentWebElement.fromJSON({ [Identifier]: "foo" });
  ok(el instanceof ContentWebElement);
  equal(el.uuid, "foo");

  Assert.throws(() => ContentWebElement.fromJSON({}), /InvalidArgumentError/);

  run_next_test();
});

add_test(function test_ContentWebWindow_toJSON() {
  let win = new ContentWebWindow("foo");
  let json = win.toJSON();
  ok(ContentWebWindow.Identifier in json);
  equal(json[ContentWebWindow.Identifier], "foo");

  run_next_test();
});

add_test(function test_ContentWebWindow_fromJSON() {
  let ref = { [ContentWebWindow.Identifier]: "foo" };
  let win = ContentWebWindow.fromJSON(ref);
  ok(win instanceof ContentWebWindow);
  equal(win.uuid, "foo");

  run_next_test();
});

add_test(function test_ContentWebFrame_toJSON() {
  let frame = new ContentWebFrame("foo");
  let json = frame.toJSON();
  ok(ContentWebFrame.Identifier in json);
  equal(json[ContentWebFrame.Identifier], "foo");

  run_next_test();
});

add_test(function test_ContentWebFrame_fromJSON() {
  let ref = { [ContentWebFrame.Identifier]: "foo" };
  let win = ContentWebFrame.fromJSON(ref);
  ok(win instanceof ContentWebFrame);
  equal(win.uuid, "foo");

  run_next_test();
});

add_test(function test_ChromeWebElement_toJSON() {
  let el = new ChromeWebElement("foo");
  let json = el.toJSON();
  ok(ChromeWebElement.Identifier in json);
  equal(json[ChromeWebElement.Identifier], "foo");

  run_next_test();
});

add_test(function test_ChromeWebElement_fromJSON() {
  let ref = { [ChromeWebElement.Identifier]: "foo" };
  let win = ChromeWebElement.fromJSON(ref);
  ok(win instanceof ChromeWebElement);
  equal(win.uuid, "foo");

  run_next_test();
});
