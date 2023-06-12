/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  assert: "chrome://remote/content/shared/webdriver/Assert.sys.mjs",
  dom: "chrome://remote/content/shared/DOM.sys.mjs",
  error: "chrome://remote/content/shared/webdriver/Errors.sys.mjs",
  generateUUID: "chrome://remote/content/shared/UUID.sys.mjs",
  pprint: "chrome://remote/content/shared/Format.sys.mjs",
});

/**
 * A web reference is an abstraction used to identify an element when
 * it is transported via the protocol, between remote- and local ends.
 *
 * In Marionette this abstraction can represent DOM elements,
 * WindowProxies, and XUL elements.
 */
export class WebReference {
  /**
   * @param {string} uuid
   *     Identifier that must be unique across all browsing contexts
   *     for the contract to be upheld.
   */
  constructor(uuid) {
    this.uuid = lazy.assert.string(uuid);
  }

  /**
   * Performs an equality check between this web element and
   * <var>other</var>.
   *
   * @param {WebReference} other
   *     Web element to compare with this.
   *
   * @returns {boolean}
   *     True if this and <var>other</var> are the same.  False
   *     otherwise.
   */
  is(other) {
    return other instanceof WebReference && this.uuid === other.uuid;
  }

  toString() {
    return `[object ${this.constructor.name} uuid=${this.uuid}]`;
  }

  /**
   * Returns a new {@link WebReference} reference for a DOM or XUL element,
   * <code>WindowProxy</code>, or <code>ShadowRoot</code>.
   *
   * @param {(Element|ShadowRoot|WindowProxy|MockXULElement)} node
   *     Node to construct a web element reference for.
   * @param {string=} uuid
   *     Optional unique identifier of the WebReference if already known.
   *     If not defined a new unique identifier will be created.
   *
   * @returns {WebReference}
   *     Web reference for <var>node</var>.
   *
   * @throws {InvalidArgumentError}
   *     If <var>node</var> is neither a <code>WindowProxy</code>,
   *     DOM or XUL element, or <code>ShadowRoot</code>.
   */
  static from(node, uuid) {
    if (uuid === undefined) {
      uuid = lazy.generateUUID();
    }

    if (lazy.dom.isShadowRoot(node) && !lazy.dom.isInPrivilegedDocument(node)) {
      // When we support Chrome Shadowroots we will need to
      // do a check here of shadowroot.host being in a privileged document
      // See Bug 1743541
      return new ShadowRoot(uuid);
    } else if (lazy.dom.isElement(node)) {
      return new WebElement(uuid);
    } else if (lazy.dom.isDOMWindow(node)) {
      if (node.parent === node) {
        return new WebWindow(uuid);
      }
      return new WebFrame(uuid);
    }

    throw new lazy.error.InvalidArgumentError(
      "Expected DOM window/element " + lazy.pprint`or XUL element, got: ${node}`
    );
  }

  /**
   * Unmarshals a JSON Object to one of {@link ShadowRoot}, {@link WebElement},
   * {@link WebFrame}, or {@link WebWindow}.
   *
   * @param {Object<string, string>} json
   *     Web reference, which is supposed to be a JSON Object
   *     where the key is one of the {@link WebReference} concrete
   *     classes' UUID identifiers.
   *
   * @returns {WebReference}
   *     Web reference for the JSON object.
   *
   * @throws {InvalidArgumentError}
   *     If <var>json</var> is not a web reference.
   */
  static fromJSON(json) {
    lazy.assert.object(json);
    if (json instanceof WebReference) {
      return json;
    }
    let keys = Object.keys(json);

    for (let key of keys) {
      switch (key) {
        case ShadowRoot.Identifier:
          return ShadowRoot.fromJSON(json);

        case WebElement.Identifier:
          return WebElement.fromJSON(json);

        case WebFrame.Identifier:
          return WebFrame.fromJSON(json);

        case WebWindow.Identifier:
          return WebWindow.fromJSON(json);
      }
    }

    throw new lazy.error.InvalidArgumentError(
      lazy.pprint`Expected web reference, got: ${json}`
    );
  }

  /**
   * Checks if <var>obj<var> is a {@link WebReference} reference.
   *
   * @param {Object<string, string>} obj
   *     Object that represents a {@link WebReference}.
   *
   * @returns {boolean}
   *     True if <var>obj</var> is a {@link WebReference}, false otherwise.
   */
  static isReference(obj) {
    if (Object.prototype.toString.call(obj) != "[object Object]") {
      return false;
    }

    if (
      ShadowRoot.Identifier in obj ||
      WebElement.Identifier in obj ||
      WebFrame.Identifier in obj ||
      WebWindow.Identifier in obj
    ) {
      return true;
    }
    return false;
  }
}

/**
 * Shadow Root elements are represented as shadow root references when they are
 * transported over the wire protocol
 */
export class ShadowRoot extends WebReference {
  toJSON() {
    return { [ShadowRoot.Identifier]: this.uuid };
  }

  static fromJSON(json) {
    const { Identifier } = ShadowRoot;

    if (!(Identifier in json)) {
      throw new lazy.error.InvalidArgumentError(
        lazy.pprint`Expected shadow root reference, got: ${json}`
      );
    }

    let uuid = json[Identifier];
    return new ShadowRoot(uuid);
  }

  /**
   * Constructs a {@link ShadowRoot} from a string <var>uuid</var>.
   *
   * This whole function is a workaround for the fact that clients
   * to Marionette occasionally pass <code>{id: <uuid>}</code> JSON
   * Objects instead of shadow root representations.
   *
   * @param {string} uuid
   *     UUID to be associated with the web reference.
   *
   * @returns {ShadowRoot}
   *     The shadow root reference.
   *
   * @throws {InvalidArgumentError}
   *     If <var>uuid</var> is not a string.
   */
  static fromUUID(uuid) {
    lazy.assert.string(uuid);

    return new ShadowRoot(uuid);
  }
}

ShadowRoot.Identifier = "shadow-6066-11e4-a52e-4f735466cecf";

/**
 * DOM elements are represented as web elements when they are
 * transported over the wire protocol.
 */
export class WebElement extends WebReference {
  toJSON() {
    return { [WebElement.Identifier]: this.uuid };
  }

  static fromJSON(json) {
    const { Identifier } = WebElement;

    if (!(Identifier in json)) {
      throw new lazy.error.InvalidArgumentError(
        lazy.pprint`Expected web element reference, got: ${json}`
      );
    }

    let uuid = json[Identifier];
    return new WebElement(uuid);
  }

  /**
   * Constructs a {@link WebElement} from a string <var>uuid</var>.
   *
   * This whole function is a workaround for the fact that clients
   * to Marionette occasionally pass <code>{id: <uuid>}</code> JSON
   * Objects instead of web element representations.
   *
   * @param {string} uuid
   *     UUID to be associated with the web reference.
   *
   * @returns {WebElement}
   *     The web element reference.
   *
   * @throws {InvalidArgumentError}
   *     If <var>uuid</var> is not a string.
   */
  static fromUUID(uuid) {
    return new WebElement(uuid);
  }
}

WebElement.Identifier = "element-6066-11e4-a52e-4f735466cecf";

/**
 * Nested browsing contexts, such as the <code>WindowProxy</code>
 * associated with <tt>&lt;frame&gt;</tt> and <tt>&lt;iframe&gt;</tt>,
 * are represented as web frames over the wire protocol.
 */
export class WebFrame extends WebReference {
  toJSON() {
    return { [WebFrame.Identifier]: this.uuid };
  }

  static fromJSON(json) {
    if (!(WebFrame.Identifier in json)) {
      throw new lazy.error.InvalidArgumentError(
        lazy.pprint`Expected web frame reference, got: ${json}`
      );
    }
    let uuid = json[WebFrame.Identifier];
    return new WebFrame(uuid);
  }
}

WebFrame.Identifier = "frame-075b-4da1-b6ba-e579c2d3230a";

/**
 * Top-level browsing contexts, such as <code>WindowProxy</code>
 * whose <code>opener</code> is null, are represented as web windows
 * over the wire protocol.
 */
export class WebWindow extends WebReference {
  toJSON() {
    return { [WebWindow.Identifier]: this.uuid };
  }

  static fromJSON(json) {
    if (!(WebWindow.Identifier in json)) {
      throw new lazy.error.InvalidArgumentError(
        lazy.pprint`Expected web window reference, got: ${json}`
      );
    }
    let uuid = json[WebWindow.Identifier];
    return new WebWindow(uuid);
  }
}

WebWindow.Identifier = "window-fcc6-11e5-b4f8-330a88ab9d7f";
