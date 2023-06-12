/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { ShadowRoot, WebElement, WebFrame, WebReference, WebWindow } =
  ChromeUtils.importESModule(
    "chrome://remote/content/marionette/web-reference.sys.mjs"
  );
const { NodeCache } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/webdriver/NodeCache.sys.mjs"
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
    nodeCache: new NodeCache(),
    shadowRoot,
    svgEl,
    textareaEl,
    videoEl,
  };
}

add_task(function test_WebReference_ctor() {
  const el = new WebReference("foo");
  equal(el.uuid, "foo");

  for (let t of [42, true, [], {}, null, undefined]) {
    Assert.throws(() => new WebReference(t), /to be a string/);
  }
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

add_task(function test_WebReference_fromJSON_malformed() {
  Assert.throws(() => WebReference.fromJSON({}), /InvalidArgumentError/);
  Assert.throws(() => WebReference.fromJSON(null), /InvalidArgumentError/);
});

add_task(function test_WebReference_fromJSON_ShadowRoot() {
  const { Identifier } = ShadowRoot;

  const ref = { [Identifier]: "foo" };
  const shadowRootEl = WebReference.fromJSON(ref);
  ok(shadowRootEl instanceof ShadowRoot);
  equal(shadowRootEl.uuid, "foo");

  let identifierPrecedence = {
    [Identifier]: "identifier-uuid",
  };
  const precedenceShadowRoot = WebReference.fromJSON(identifierPrecedence);
  ok(precedenceShadowRoot instanceof ShadowRoot);
  equal(precedenceShadowRoot.uuid, "identifier-uuid");
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

add_task(function test_WebReference_fromJSON_WebFrame() {
  const ref = { [WebFrame.Identifier]: "foo" };
  const frame = WebReference.fromJSON(ref);
  ok(frame instanceof WebFrame);
  equal(frame.uuid, "foo");
});

add_task(function test_WebReference_fromJSON_WebWindow() {
  const ref = { [WebWindow.Identifier]: "foo" };
  const win = WebReference.fromJSON(ref);

  ok(win instanceof WebWindow);
  equal(win.uuid, "foo");
});

add_task(function test_WebReference_is() {
  const a = new WebReference("a");
  const b = new WebReference("b");

  ok(a.is(a));
  ok(b.is(b));
  ok(!a.is(b));
  ok(!b.is(a));

  ok(!a.is({}));
});

add_task(function test_WebReference_isReference() {
  for (let t of [42, true, "foo", [], {}]) {
    ok(!WebReference.isReference(t));
  }

  ok(WebReference.isReference({ [WebElement.Identifier]: "foo" }));
  ok(WebReference.isReference({ [WebWindow.Identifier]: "foo" }));
  ok(WebReference.isReference({ [WebFrame.Identifier]: "foo" }));
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

add_task(function test_ShadowRoot_toJSON() {
  const { Identifier } = ShadowRoot;

  const shadowRoot = new ShadowRoot("foo");
  const json = shadowRoot.toJSON();

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

add_task(function test_WebElement_toJSON() {
  const { Identifier } = WebElement;

  const el = new WebElement("foo");
  const json = el.toJSON();

  ok(Identifier in json);
  equal(json[Identifier], "foo");
});

add_task(function test_WebFrame_fromJSON() {
  const ref = { [WebFrame.Identifier]: "foo" };
  const win = WebFrame.fromJSON(ref);

  ok(win instanceof WebFrame);
  equal(win.uuid, "foo");
});

add_task(function test_WebFrame_toJSON() {
  const frame = new WebFrame("foo");
  const json = frame.toJSON();

  ok(WebFrame.Identifier in json);
  equal(json[WebFrame.Identifier], "foo");
});

add_task(function test_WebWindow_fromJSON() {
  const ref = { [WebWindow.Identifier]: "foo" };
  const win = WebWindow.fromJSON(ref);

  ok(win instanceof WebWindow);
  equal(win.uuid, "foo");
});

add_task(function test_WebWindow_toJSON() {
  const win = new WebWindow("foo");
  const json = win.toJSON();

  ok(WebWindow.Identifier in json);
  equal(json[WebWindow.Identifier], "foo");
});
