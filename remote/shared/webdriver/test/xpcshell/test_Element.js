/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { element } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/webdriver/Element.sys.mjs"
);

const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

class MockElement {
  constructor(tagName, attrs = {}) {
    this.tagName = tagName;
    this.localName = tagName;

    this.isConnected = false;
    this.ownerGlobal = {
      document: {
        isActive() {
          return true;
        },
      },
    };

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

class MockXULElement extends MockElement {
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

const xulEl = new MockXULElement("text");

const domElInPrivilegedDocument = new MockElement("input", {
  nodePrincipal: { isSystemPrincipal: true },
});
const xulElInPrivilegedDocument = new MockXULElement("text", {
  nodePrincipal: { isSystemPrincipal: true },
});

function setupTest() {
  const browser = Services.appShell.createWindowlessBrowser(false);

  browser.document.body.innerHTML = `
    <div id="foo" style="margin: 50px">
      <iframe></iframe>
      <video></video>
      <svg xmlns="http://www.w3.org/2000/svg"></svg>
      <textarea></textarea>
    </div>
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
    childEl,
    divEl,
    iframeEl,
    shadowRoot,
    svgEl,
    textareaEl,
    videoEl,
  };
}

add_task(function test_findClosest() {
  const { divEl, videoEl } = setupTest();

  equal(element.findClosest(divEl, "foo"), null);
  equal(element.findClosest(videoEl, "div"), divEl);
});

add_task(function test_isSelected() {
  const { browser, divEl } = setupTest();

  const checkbox = browser.document.createElement("input");
  checkbox.setAttribute("type", "checkbox");

  ok(!element.isSelected(checkbox));
  checkbox.checked = true;
  ok(element.isSelected(checkbox));

  // selected is not a property of <input type=checkbox>
  checkbox.selected = true;
  checkbox.checked = false;
  ok(!element.isSelected(checkbox));

  const option = browser.document.createElement("option");

  ok(!element.isSelected(option));
  option.selected = true;
  ok(element.isSelected(option));

  // checked is not a property of <option>
  option.checked = true;
  option.selected = false;
  ok(!element.isSelected(option));

  // anything else should not be selected
  for (const type of [undefined, null, "foo", true, [], {}, divEl]) {
    ok(!element.isSelected(type));
  }
});

add_task(function test_isElement() {
  const { divEl, iframeEl, shadowRoot, svgEl } = setupTest();

  ok(element.isElement(divEl));
  ok(element.isElement(svgEl));
  ok(element.isElement(xulEl));
  ok(element.isElement(domElInPrivilegedDocument));
  ok(element.isElement(xulElInPrivilegedDocument));

  ok(!element.isElement(shadowRoot));
  ok(!element.isElement(divEl.ownerGlobal));
  ok(!element.isElement(iframeEl.contentWindow));

  for (const type of [true, 42, {}, [], undefined, null]) {
    ok(!element.isElement(type));
  }
});

add_task(function test_isDOMElement() {
  const { divEl, iframeEl, shadowRoot, svgEl } = setupTest();

  ok(element.isDOMElement(divEl));
  ok(element.isDOMElement(svgEl));
  ok(element.isDOMElement(domElInPrivilegedDocument));

  ok(!element.isDOMElement(shadowRoot));
  ok(!element.isDOMElement(divEl.ownerGlobal));
  ok(!element.isDOMElement(iframeEl.contentWindow));
  ok(!element.isDOMElement(xulEl));
  ok(!element.isDOMElement(xulElInPrivilegedDocument));

  for (const type of [true, 42, "foo", {}, [], undefined, null]) {
    ok(!element.isDOMElement(type));
  }
});

add_task(function test_isXULElement() {
  const { divEl, iframeEl, shadowRoot, svgEl } = setupTest();

  ok(element.isXULElement(xulEl));
  ok(element.isXULElement(xulElInPrivilegedDocument));

  ok(!element.isXULElement(divEl));
  ok(!element.isXULElement(domElInPrivilegedDocument));
  ok(!element.isXULElement(svgEl));
  ok(!element.isXULElement(shadowRoot));
  ok(!element.isXULElement(divEl.ownerGlobal));
  ok(!element.isXULElement(iframeEl.contentWindow));

  for (const type of [true, 42, "foo", {}, [], undefined, null]) {
    ok(!element.isXULElement(type));
  }
});

add_task(function test_isDOMWindow() {
  const { divEl, iframeEl, shadowRoot, svgEl } = setupTest();

  ok(element.isDOMWindow(divEl.ownerGlobal));
  ok(element.isDOMWindow(iframeEl.contentWindow));

  ok(!element.isDOMWindow(divEl));
  ok(!element.isDOMWindow(svgEl));
  ok(!element.isDOMWindow(shadowRoot));
  ok(!element.isDOMWindow(domElInPrivilegedDocument));
  ok(!element.isDOMWindow(xulEl));
  ok(!element.isDOMWindow(xulElInPrivilegedDocument));

  for (const type of [true, 42, {}, [], undefined, null]) {
    ok(!element.isDOMWindow(type));
  }
});

add_task(function test_isShadowRoot() {
  const { browser, divEl, iframeEl, shadowRoot, svgEl } = setupTest();

  ok(element.isShadowRoot(shadowRoot));

  ok(!element.isShadowRoot(divEl));
  ok(!element.isShadowRoot(svgEl));
  ok(!element.isShadowRoot(divEl.ownerGlobal));
  ok(!element.isShadowRoot(iframeEl.contentWindow));
  ok(!element.isShadowRoot(xulEl));
  ok(!element.isShadowRoot(domElInPrivilegedDocument));
  ok(!element.isShadowRoot(xulElInPrivilegedDocument));

  for (const type of [true, 42, "foo", {}, [], undefined, null]) {
    ok(!element.isShadowRoot(type));
  }

  const documentFragment = browser.document.createDocumentFragment();
  ok(!element.isShadowRoot(documentFragment));
});

add_task(function test_isReadOnly() {
  const { browser, divEl, textareaEl } = setupTest();

  const input = browser.document.createElement("input");
  input.readOnly = true;
  ok(element.isReadOnly(input));

  textareaEl.readOnly = true;
  ok(element.isReadOnly(textareaEl));

  ok(!element.isReadOnly(divEl));
  divEl.readOnly = true;
  ok(!element.isReadOnly(divEl));

  ok(!element.isReadOnly(null));
});

add_task(function test_isDisabled() {
  const { browser, divEl, svgEl } = setupTest();

  const select = browser.document.createElement("select");
  const option = browser.document.createElement("option");
  select.appendChild(option);
  select.disabled = true;
  ok(element.isDisabled(option));

  const optgroup = browser.document.createElement("optgroup");
  option.parentNode = optgroup;
  ok(element.isDisabled(option));

  optgroup.parentNode = select;
  ok(element.isDisabled(option));

  select.disabled = false;
  ok(!element.isDisabled(option));

  for (const type of ["button", "input", "select", "textarea"]) {
    const elem = browser.document.createElement(type);
    ok(!element.isDisabled(elem));
    elem.disabled = true;
    ok(element.isDisabled(elem));
  }

  ok(!element.isDisabled(divEl));

  svgEl.disabled = true;
  ok(!element.isDisabled(svgEl));

  ok(!element.isDisabled(new MockXULElement("browser", { disabled: true })));
});

add_task(function test_isEditingHost() {
  const { browser, divEl, svgEl } = setupTest();

  ok(!element.isEditingHost(null));

  ok(!element.isEditingHost(divEl));
  divEl.contentEditable = true;
  ok(element.isEditingHost(divEl));

  ok(!element.isEditingHost(svgEl));
  browser.document.designMode = "on";
  ok(element.isEditingHost(svgEl));
});

add_task(function test_isEditable() {
  const { browser, divEl, svgEl, textareaEl } = setupTest();

  ok(!element.isEditable(null));

  for (let type of [
    "checkbox",
    "radio",
    "hidden",
    "submit",
    "button",
    "image",
  ]) {
    const input = browser.document.createElement("input");
    input.setAttribute("type", type);

    ok(!element.isEditable(input));
  }

  const input = browser.document.createElement("input");
  ok(element.isEditable(input));
  input.setAttribute("type", "text");
  ok(element.isEditable(input));

  ok(element.isEditable(textareaEl));

  const textareaDisabled = browser.document.createElement("textarea");
  textareaDisabled.disabled = true;
  ok(!element.isEditable(textareaDisabled));

  const textareaReadOnly = browser.document.createElement("textarea");
  textareaReadOnly.readOnly = true;
  ok(!element.isEditable(textareaReadOnly));

  ok(!element.isEditable(divEl));
  divEl.contentEditable = true;
  ok(element.isEditable(divEl));

  ok(!element.isEditable(svgEl));
  browser.document.designMode = "on";
  ok(element.isEditable(svgEl));
});

add_task(function test_isMutableFormControlElement() {
  const { browser, divEl, textareaEl } = setupTest();

  ok(!element.isMutableFormControl(null));

  ok(element.isMutableFormControl(textareaEl));

  const textareaDisabled = browser.document.createElement("textarea");
  textareaDisabled.disabled = true;
  ok(!element.isMutableFormControl(textareaDisabled));

  const textareaReadOnly = browser.document.createElement("textarea");
  textareaReadOnly.readOnly = true;
  ok(!element.isMutableFormControl(textareaReadOnly));

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
  for (const type of mutableStates) {
    const input = browser.document.createElement("input");
    input.setAttribute("type", type);
    ok(element.isMutableFormControl(input));
  }

  const inputHidden = browser.document.createElement("input");
  inputHidden.setAttribute("type", "hidden");
  ok(!element.isMutableFormControl(inputHidden));

  ok(!element.isMutableFormControl(divEl));
  divEl.contentEditable = true;
  ok(!element.isMutableFormControl(divEl));
  browser.document.designMode = "on";
  ok(!element.isMutableFormControl(divEl));
});

add_task(function test_coordinates() {
  const { divEl } = setupTest();

  let coords = element.coordinates(divEl);
  ok(coords.hasOwnProperty("x"));
  ok(coords.hasOwnProperty("y"));
  equal(typeof coords.x, "number");
  equal(typeof coords.y, "number");

  deepEqual(element.coordinates(divEl), { x: 0, y: 0 });
  deepEqual(element.coordinates(divEl, 10, 10), { x: 10, y: 10 });
  deepEqual(element.coordinates(divEl, -5, -5), { x: -5, y: -5 });

  Assert.throws(() => element.coordinates(null), /node is null/);

  Assert.throws(
    () => element.coordinates(divEl, "string", undefined),
    /Offset must be a number/
  );
  Assert.throws(
    () => element.coordinates(divEl, undefined, "string"),
    /Offset must be a number/
  );
  Assert.throws(
    () => element.coordinates(divEl, "string", "string"),
    /Offset must be a number/
  );
  Assert.throws(
    () => element.coordinates(divEl, {}, undefined),
    /Offset must be a number/
  );
  Assert.throws(
    () => element.coordinates(divEl, undefined, {}),
    /Offset must be a number/
  );
  Assert.throws(
    () => element.coordinates(divEl, {}, {}),
    /Offset must be a number/
  );
  Assert.throws(
    () => element.coordinates(divEl, [], undefined),
    /Offset must be a number/
  );
  Assert.throws(
    () => element.coordinates(divEl, undefined, []),
    /Offset must be a number/
  );
  Assert.throws(
    () => element.coordinates(divEl, [], []),
    /Offset must be a number/
  );
});

add_task(function test_isDetached() {
  const { childEl, iframeEl } = setupTest();

  let detachedShadowRoot = childEl.attachShadow({ mode: "open" });
  detachedShadowRoot.innerHTML = "<input></input>";

  // Connected to the DOM
  ok(!element.isDetached(detachedShadowRoot));

  // Node document (ownerDocument) is not the active document
  iframeEl.remove();
  ok(element.isDetached(detachedShadowRoot));

  // host element is stale (eg. not connected)
  detachedShadowRoot.host.remove();
  equal(childEl.isConnected, false);
  ok(element.isDetached(detachedShadowRoot));
});

add_task(function test_isStale() {
  const { childEl, iframeEl } = setupTest();

  // Connected to the DOM
  ok(!element.isStale(childEl));

  // Not part of the active document
  iframeEl.remove();
  ok(element.isStale(childEl));

  // Not connected to the DOM
  childEl.remove();
  ok(element.isStale(childEl));
});
