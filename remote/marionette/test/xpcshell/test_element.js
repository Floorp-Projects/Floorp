/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { element, ShadowRoot, WebElement, WebFrame, WebReference, WebWindow } =
  ChromeUtils.importESModule(
    "chrome://remote/content/marionette/element.sys.mjs"
  );
const { NodeCache } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/webdriver/NodeCache.sys.mjs"
);

const MemoryReporter = Cc["@mozilla.org/memory-reporter-manager;1"].getService(
  Ci.nsIMemoryReporterManager
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

add_task(function test_isNodeReferenceKnown() {
  const { browser, nodeCache, childEl, iframeEl, videoEl } = setupTest();

  // Unknown node reference
  ok(!element.isNodeReferenceKnown(browser.browsingContext, "foo", nodeCache));

  // Known node reference
  const videoElRef = nodeCache.getOrCreateNodeReference(videoEl);
  ok(
    element.isNodeReferenceKnown(browser.browsingContext, videoElRef, nodeCache)
  );

  // Different top-level browsing context
  const browser2 = Services.appShell.createWindowlessBrowser(false);
  ok(
    !element.isNodeReferenceKnown(
      browser2.browsingContext,
      videoElRef,
      nodeCache
    )
  );

  // Different child browsing context
  const childElRef = nodeCache.getOrCreateNodeReference(childEl);
  const childBrowsingContext = iframeEl.contentWindow.browsingContext;
  ok(element.isNodeReferenceKnown(childBrowsingContext, childElRef, nodeCache));

  const iframeEl2 = browser2.document.createElement("iframe");
  browser2.document.body.appendChild(iframeEl2);
  const childBrowsingContext2 = iframeEl2.contentWindow.browsingContext;
  ok(
    !element.isNodeReferenceKnown(childBrowsingContext2, childElRef, nodeCache)
  );
});

add_task(function test_getKnownElement() {
  const { browser, nodeCache, shadowRoot, videoEl } = setupTest();

  // Unknown element reference
  Assert.throws(() => {
    element.getKnownElement(browser.browsingContext, "foo", nodeCache);
  }, /NoSuchElementError/);

  // With a ShadowRoot reference
  const shadowRootRef = nodeCache.getOrCreateNodeReference(shadowRoot);
  Assert.throws(() => {
    element.getKnownElement(browser.browsingContext, shadowRootRef, nodeCache);
  }, /NoSuchElementError/);

  // Deleted element (eg. garbage collected)
  let detachedEl = browser.document.createElement("div");
  const detachedElRef = nodeCache.getOrCreateNodeReference(detachedEl);

  // ... not connected to the DOM
  Assert.throws(() => {
    element.getKnownElement(browser.browsingContext, detachedElRef, nodeCache);
  }, /StaleElementReferenceError/);

  // ... element garbage collected
  detachedEl = null;
  MemoryReporter.minimizeMemoryUsage(() => {
    Assert.throws(() => {
      element.getKnownElement(
        browser.browsingContext,
        detachedElRef,
        nodeCache
      );
    }, /StaleElementReferenceError/);
  });

  // Known element reference
  const videoElRef = nodeCache.getOrCreateNodeReference(videoEl);
  equal(
    element.getKnownElement(browser.browsingContext, videoElRef, nodeCache),
    videoEl
  );
});

add_task(function test_getKnownShadowRoot() {
  const { browser, nodeCache, shadowRoot, videoEl } = setupTest();

  const videoElRef = nodeCache.getOrCreateNodeReference(videoEl);

  // Unknown ShadowRoot reference
  Assert.throws(() => {
    element.getKnownShadowRoot(browser.browsingContext, "foo", nodeCache);
  }, /NoSuchShadowRootError/);

  // With a HTMLElement reference
  Assert.throws(() => {
    element.getKnownShadowRoot(browser.browsingContext, videoElRef, nodeCache);
  }, /NoSuchShadowRootError/);

  // Known ShadowRoot reference
  const shadowRootRef = nodeCache.getOrCreateNodeReference(shadowRoot);
  equal(
    element.getKnownShadowRoot(
      browser.browsingContext,
      shadowRootRef,
      nodeCache
    ),
    shadowRoot
  );

  // Detached ShadowRoot host
  let el = browser.document.createElement("div");
  let detachedShadowRoot = el.attachShadow({ mode: "open" });
  detachedShadowRoot.innerHTML = "<input></input>";

  const detachedShadowRootRef =
    nodeCache.getOrCreateNodeReference(detachedShadowRoot);

  // ... not connected to the DOM
  Assert.throws(() => {
    element.getKnownShadowRoot(
      browser.browsingContext,
      detachedShadowRootRef,
      nodeCache
    );
  }, /DetachedShadowRootError/);

  // ... host and shadow root garbage collected
  el = null;
  detachedShadowRoot = null;
  MemoryReporter.minimizeMemoryUsage(() => {
    Assert.throws(() => {
      element.getKnownShadowRoot(
        browser.browsingContext,
        detachedShadowRootRef,
        nodeCache
      );
    }, /DetachedShadowRootError/);
  });
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

add_task(function test_WebReference_ctor() {
  const el = new WebReference("foo");
  equal(el.uuid, "foo");

  for (let t of [42, true, [], {}, null, undefined]) {
    Assert.throws(() => new WebReference(t), /to be a string/);
  }
});

add_task(function test_WebElemenet_is() {
  const a = new WebReference("a");
  const b = new WebReference("b");

  ok(a.is(a));
  ok(b.is(b));
  ok(!a.is(b));
  ok(!b.is(a));

  ok(!a.is({}));
});

add_task(function test_WebReference_from() {
  const { divEl, iframeEl } = setupTest();

  ok(WebReference.from(divEl) instanceof WebElement);
  ok(WebReference.from(xulEl) instanceof WebElement);
  ok(WebReference.from(divEl.ownerGlobal) instanceof WebWindow);
  ok(WebReference.from(iframeEl.contentWindow) instanceof WebFrame);
  ok(WebReference.from(domElInPrivilegedDocument) instanceof WebElement);
  ok(WebReference.from(xulElInPrivilegedDocument) instanceof WebElement);

  Assert.throws(() => WebReference.from({}), /InvalidArgumentError/);
});

add_task(function test_WebReference_fromJSON_WebElement() {
  const { Identifier } = WebElement;

  const ref = { [Identifier]: "foo" };
  const webEl = WebReference.fromJSON(ref);
  ok(webEl instanceof WebElement);
  equal(webEl.uuid, "foo");

  let identifierPrecedence = {
    [Identifier]: "identifier-uuid",
  };
  const precedenceEl = WebReference.fromJSON(identifierPrecedence);
  ok(precedenceEl instanceof WebElement);
  equal(precedenceEl.uuid, "identifier-uuid");
});

add_task(function test_WebReference_fromJSON_WebWindow() {
  const ref = { [WebWindow.Identifier]: "foo" };
  const win = WebReference.fromJSON(ref);

  ok(win instanceof WebWindow);
  equal(win.uuid, "foo");
});

add_task(function test_WebReference_fromJSON_WebFrame() {
  const ref = { [WebFrame.Identifier]: "foo" };
  const frame = WebReference.fromJSON(ref);
  ok(frame instanceof WebFrame);
  equal(frame.uuid, "foo");
});

add_task(function test_WebReference_fromJSON_malformed() {
  Assert.throws(() => WebReference.fromJSON({}), /InvalidArgumentError/);
  Assert.throws(() => WebReference.fromJSON(null), /InvalidArgumentError/);
});

add_task(function test_WebReference_isReference() {
  for (let t of [42, true, "foo", [], {}]) {
    ok(!WebReference.isReference(t));
  }

  ok(WebReference.isReference({ [WebElement.Identifier]: "foo" }));
  ok(WebReference.isReference({ [WebWindow.Identifier]: "foo" }));
  ok(WebReference.isReference({ [WebFrame.Identifier]: "foo" }));
});

add_task(function test_WebElement_toJSON() {
  const { Identifier } = WebElement;

  const el = new WebElement("foo");
  const json = el.toJSON();

  ok(Identifier in json);
  equal(json[Identifier], "foo");
});

add_task(function test_WebElement_fromJSON() {
  const { Identifier } = WebElement;

  const el = WebElement.fromJSON({ [Identifier]: "foo" });
  ok(el instanceof WebElement);
  equal(el.uuid, "foo");

  Assert.throws(() => WebElement.fromJSON({}), /InvalidArgumentError/);
});

add_task(function test_WebElement_fromUUID() {
  const domWebEl = WebElement.fromUUID("bar");

  ok(domWebEl instanceof WebElement);
  equal(domWebEl.uuid, "bar");

  Assert.throws(() => WebElement.fromUUID(), /InvalidArgumentError/);
});

add_task(function test_ShadowRoot_toJSON() {
  const { Identifier } = ShadowRoot;

  const shadowRoot = new ShadowRoot("foo");
  const json = shadowRoot.toJSON();

  ok(Identifier in json);
  equal(json[Identifier], "foo");
});

add_task(function test_ShadowRoot_fromJSON() {
  const { Identifier } = ShadowRoot;

  const shadowRoot = ShadowRoot.fromJSON({ [Identifier]: "foo" });
  ok(shadowRoot instanceof ShadowRoot);
  equal(shadowRoot.uuid, "foo");

  Assert.throws(() => ShadowRoot.fromJSON({}), /InvalidArgumentError/);
});

add_task(function test_ShadowRoot_fromUUID() {
  const shadowRoot = ShadowRoot.fromUUID("baz");

  ok(shadowRoot instanceof ShadowRoot);
  equal(shadowRoot.uuid, "baz");

  Assert.throws(() => ShadowRoot.fromUUID(), /InvalidArgumentError/);
});

add_task(function test_WebWindow_toJSON() {
  const win = new WebWindow("foo");
  const json = win.toJSON();

  ok(WebWindow.Identifier in json);
  equal(json[WebWindow.Identifier], "foo");
});

add_task(function test_WebWindow_fromJSON() {
  const ref = { [WebWindow.Identifier]: "foo" };
  const win = WebWindow.fromJSON(ref);

  ok(win instanceof WebWindow);
  equal(win.uuid, "foo");
});

add_task(function test_WebFrame_toJSON() {
  const frame = new WebFrame("foo");
  const json = frame.toJSON();

  ok(WebFrame.Identifier in json);
  equal(json[WebFrame.Identifier], "foo");
});

add_task(function test_WebFrame_fromJSON() {
  const ref = { [WebFrame.Identifier]: "foo" };
  const win = WebFrame.fromJSON(ref);

  ok(win instanceof WebFrame);
  equal(win.uuid, "foo");
});
