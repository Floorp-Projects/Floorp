/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { dom } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/DOM.sys.mjs"
);
const { NodeCache } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/webdriver/NodeCache.sys.mjs"
);

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
    nodeCache: new NodeCache(),
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

  equal(dom.findClosest(divEl, "foo"), null);
  equal(dom.findClosest(videoEl, "div"), divEl);
});

add_task(function test_isSelected() {
  const { browser, divEl } = setupTest();

  const checkbox = browser.document.createElement("input");
  checkbox.setAttribute("type", "checkbox");

  ok(!dom.isSelected(checkbox));
  checkbox.checked = true;
  ok(dom.isSelected(checkbox));

  // selected is not a property of <input type=checkbox>
  checkbox.selected = true;
  checkbox.checked = false;
  ok(!dom.isSelected(checkbox));

  const option = browser.document.createElement("option");

  ok(!dom.isSelected(option));
  option.selected = true;
  ok(dom.isSelected(option));

  // checked is not a property of <option>
  option.checked = true;
  option.selected = false;
  ok(!dom.isSelected(option));

  // anything else should not be selected
  for (const type of [undefined, null, "foo", true, [], {}, divEl]) {
    ok(!dom.isSelected(type));
  }
});

add_task(function test_isElement() {
  const { divEl, iframeEl, shadowRoot, svgEl } = setupTest();

  ok(dom.isElement(divEl));
  ok(dom.isElement(svgEl));
  ok(dom.isElement(xulEl));
  ok(dom.isElement(domElInPrivilegedDocument));
  ok(dom.isElement(xulElInPrivilegedDocument));

  ok(!dom.isElement(shadowRoot));
  ok(!dom.isElement(divEl.ownerGlobal));
  ok(!dom.isElement(iframeEl.contentWindow));

  for (const type of [true, 42, {}, [], undefined, null]) {
    ok(!dom.isElement(type));
  }
});

add_task(function test_isDOMElement() {
  const { divEl, iframeEl, shadowRoot, svgEl } = setupTest();

  ok(dom.isDOMElement(divEl));
  ok(dom.isDOMElement(svgEl));
  ok(dom.isDOMElement(domElInPrivilegedDocument));

  ok(!dom.isDOMElement(shadowRoot));
  ok(!dom.isDOMElement(divEl.ownerGlobal));
  ok(!dom.isDOMElement(iframeEl.contentWindow));
  ok(!dom.isDOMElement(xulEl));
  ok(!dom.isDOMElement(xulElInPrivilegedDocument));

  for (const type of [true, 42, "foo", {}, [], undefined, null]) {
    ok(!dom.isDOMElement(type));
  }
});

add_task(function test_isXULElement() {
  const { divEl, iframeEl, shadowRoot, svgEl } = setupTest();

  ok(dom.isXULElement(xulEl));
  ok(dom.isXULElement(xulElInPrivilegedDocument));

  ok(!dom.isXULElement(divEl));
  ok(!dom.isXULElement(domElInPrivilegedDocument));
  ok(!dom.isXULElement(svgEl));
  ok(!dom.isXULElement(shadowRoot));
  ok(!dom.isXULElement(divEl.ownerGlobal));
  ok(!dom.isXULElement(iframeEl.contentWindow));

  for (const type of [true, 42, "foo", {}, [], undefined, null]) {
    ok(!dom.isXULElement(type));
  }
});

add_task(function test_isDOMWindow() {
  const { divEl, iframeEl, shadowRoot, svgEl } = setupTest();

  ok(dom.isDOMWindow(divEl.ownerGlobal));
  ok(dom.isDOMWindow(iframeEl.contentWindow));

  ok(!dom.isDOMWindow(divEl));
  ok(!dom.isDOMWindow(svgEl));
  ok(!dom.isDOMWindow(shadowRoot));
  ok(!dom.isDOMWindow(domElInPrivilegedDocument));
  ok(!dom.isDOMWindow(xulEl));
  ok(!dom.isDOMWindow(xulElInPrivilegedDocument));

  for (const type of [true, 42, {}, [], undefined, null]) {
    ok(!dom.isDOMWindow(type));
  }
});

add_task(function test_isShadowRoot() {
  const { browser, divEl, iframeEl, shadowRoot, svgEl } = setupTest();

  ok(dom.isShadowRoot(shadowRoot));

  ok(!dom.isShadowRoot(divEl));
  ok(!dom.isShadowRoot(svgEl));
  ok(!dom.isShadowRoot(divEl.ownerGlobal));
  ok(!dom.isShadowRoot(iframeEl.contentWindow));
  ok(!dom.isShadowRoot(xulEl));
  ok(!dom.isShadowRoot(domElInPrivilegedDocument));
  ok(!dom.isShadowRoot(xulElInPrivilegedDocument));

  for (const type of [true, 42, "foo", {}, [], undefined, null]) {
    ok(!dom.isShadowRoot(type));
  }

  const documentFragment = browser.document.createDocumentFragment();
  ok(!dom.isShadowRoot(documentFragment));
});

add_task(function test_isReadOnly() {
  const { browser, divEl, textareaEl } = setupTest();

  const input = browser.document.createElement("input");
  input.readOnly = true;
  ok(dom.isReadOnly(input));

  textareaEl.readOnly = true;
  ok(dom.isReadOnly(textareaEl));

  ok(!dom.isReadOnly(divEl));
  divEl.readOnly = true;
  ok(!dom.isReadOnly(divEl));

  ok(!dom.isReadOnly(null));
});

add_task(function test_isDisabled() {
  const { browser, divEl, svgEl } = setupTest();

  const select = browser.document.createElement("select");
  const option = browser.document.createElement("option");
  select.appendChild(option);
  select.disabled = true;
  ok(dom.isDisabled(option));

  const optgroup = browser.document.createElement("optgroup");
  option.parentNode = optgroup;
  ok(dom.isDisabled(option));

  optgroup.parentNode = select;
  ok(dom.isDisabled(option));

  select.disabled = false;
  ok(!dom.isDisabled(option));

  for (const type of ["button", "input", "select", "textarea"]) {
    const elem = browser.document.createElement(type);
    ok(!dom.isDisabled(elem));
    elem.disabled = true;
    ok(dom.isDisabled(elem));
  }

  ok(!dom.isDisabled(divEl));

  svgEl.disabled = true;
  ok(!dom.isDisabled(svgEl));

  ok(!dom.isDisabled(new MockXULElement("browser", { disabled: true })));
});

add_task(function test_isEditingHost() {
  const { browser, divEl, svgEl } = setupTest();

  ok(!dom.isEditingHost(null));

  ok(!dom.isEditingHost(divEl));
  divEl.contentEditable = true;
  ok(dom.isEditingHost(divEl));

  ok(!dom.isEditingHost(svgEl));
  browser.document.designMode = "on";
  ok(dom.isEditingHost(svgEl));
});

add_task(function test_isEditable() {
  const { browser, divEl, svgEl, textareaEl } = setupTest();

  ok(!dom.isEditable(null));

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

    ok(!dom.isEditable(input));
  }

  const input = browser.document.createElement("input");
  ok(dom.isEditable(input));
  input.setAttribute("type", "text");
  ok(dom.isEditable(input));

  ok(dom.isEditable(textareaEl));

  const textareaDisabled = browser.document.createElement("textarea");
  textareaDisabled.disabled = true;
  ok(!dom.isEditable(textareaDisabled));

  const textareaReadOnly = browser.document.createElement("textarea");
  textareaReadOnly.readOnly = true;
  ok(!dom.isEditable(textareaReadOnly));

  ok(!dom.isEditable(divEl));
  divEl.contentEditable = true;
  ok(dom.isEditable(divEl));

  ok(!dom.isEditable(svgEl));
  browser.document.designMode = "on";
  ok(dom.isEditable(svgEl));
});

add_task(function test_isMutableFormControlElement() {
  const { browser, divEl, textareaEl } = setupTest();

  ok(!dom.isMutableFormControl(null));

  ok(dom.isMutableFormControl(textareaEl));

  const textareaDisabled = browser.document.createElement("textarea");
  textareaDisabled.disabled = true;
  ok(!dom.isMutableFormControl(textareaDisabled));

  const textareaReadOnly = browser.document.createElement("textarea");
  textareaReadOnly.readOnly = true;
  ok(!dom.isMutableFormControl(textareaReadOnly));

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
    ok(dom.isMutableFormControl(input));
  }

  const inputHidden = browser.document.createElement("input");
  inputHidden.setAttribute("type", "hidden");
  ok(!dom.isMutableFormControl(inputHidden));

  ok(!dom.isMutableFormControl(divEl));
  divEl.contentEditable = true;
  ok(!dom.isMutableFormControl(divEl));
  browser.document.designMode = "on";
  ok(!dom.isMutableFormControl(divEl));
});

add_task(function test_coordinates() {
  const { divEl } = setupTest();

  let coords = dom.coordinates(divEl);
  ok(coords.hasOwnProperty("x"));
  ok(coords.hasOwnProperty("y"));
  equal(typeof coords.x, "number");
  equal(typeof coords.y, "number");

  deepEqual(dom.coordinates(divEl), { x: 0, y: 0 });
  deepEqual(dom.coordinates(divEl, 10, 10), { x: 10, y: 10 });
  deepEqual(dom.coordinates(divEl, -5, -5), { x: -5, y: -5 });

  Assert.throws(() => dom.coordinates(null), /node is null/);

  Assert.throws(
    () => dom.coordinates(divEl, "string", undefined),
    /Offset must be a number/
  );
  Assert.throws(
    () => dom.coordinates(divEl, undefined, "string"),
    /Offset must be a number/
  );
  Assert.throws(
    () => dom.coordinates(divEl, "string", "string"),
    /Offset must be a number/
  );
  Assert.throws(
    () => dom.coordinates(divEl, {}, undefined),
    /Offset must be a number/
  );
  Assert.throws(
    () => dom.coordinates(divEl, undefined, {}),
    /Offset must be a number/
  );
  Assert.throws(
    () => dom.coordinates(divEl, {}, {}),
    /Offset must be a number/
  );
  Assert.throws(
    () => dom.coordinates(divEl, [], undefined),
    /Offset must be a number/
  );
  Assert.throws(
    () => dom.coordinates(divEl, undefined, []),
    /Offset must be a number/
  );
  Assert.throws(
    () => dom.coordinates(divEl, [], []),
    /Offset must be a number/
  );
});

add_task(function test_isDetached() {
  const { childEl, iframeEl } = setupTest();

  let detachedShadowRoot = childEl.attachShadow({ mode: "open" });
  detachedShadowRoot.innerHTML = "<input></input>";

  // Connected to the DOM
  ok(!dom.isDetached(detachedShadowRoot));

  // Node document (ownerDocument) is not the active document
  iframeEl.remove();
  ok(dom.isDetached(detachedShadowRoot));

  // host element is stale (eg. not connected)
  detachedShadowRoot.host.remove();
  equal(childEl.isConnected, false);
  ok(dom.isDetached(detachedShadowRoot));
});

add_task(function test_isStale() {
  const { childEl, iframeEl } = setupTest();

  // Connected to the DOM
  ok(!dom.isStale(childEl));

  // Not part of the active document
  iframeEl.remove();
  ok(dom.isStale(childEl));

  // Not connected to the DOM
  childEl.remove();
  ok(dom.isStale(childEl));
});
