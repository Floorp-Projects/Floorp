/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["MarionetteFrameChild"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  atom: "chrome://marionette/content/atom.js",
  element: "chrome://marionette/content/element.js",
  error: "chrome://marionette/content/error.js",
  evaluate: "chrome://marionette/content/evaluate.js",
  interaction: "chrome://marionette/content/interaction.js",
  Log: "chrome://marionette/content/log.js",
  sandbox: "chrome://marionette/content/evaluate.js",
  Sandboxes: "chrome://marionette/content/evaluate.js",
});

XPCOMUtils.defineLazyGetter(this, "logger", () => Log.get());

class MarionetteFrameChild extends JSWindowActorChild {
  constructor() {
    super();

    // sandbox storage and name of the current sandbox
    this.sandboxes = new Sandboxes(() => this.contentWindow);
  }

  get innerWindowId() {
    return this.manager.innerWindowId;
  }

  actorCreated() {
    logger.trace(
      `[${this.browsingContext.id}] Child actor created for window id ${this.innerWindowId}`
    );
  }

  handleEvent(event) {
    switch (event.type) {
      case "beforeunload":
      case "pagehide":
      case "DOMContentLoaded":
      case "pageshow":
        this.sendAsyncMessage("MarionetteFrameChild:PageLoadEvent", {
          browsingContext: this.browsingContext,
          innerWindowId: this.innerWindowId,
          type: event.type,
        });
        break;
    }
  }

  async receiveMessage(msg) {
    try {
      let result;

      const { name, data: serializedData } = msg;
      const data = evaluate.fromJSON(serializedData);

      switch (name) {
        case "MarionetteFrameParent:clearElement":
          this.clearElement(data);
          break;
        case "MarionetteFrameParent:clickElement":
          result = await this.clickElement(data);
          break;
        case "MarionetteFrameParent:executeScript":
          result = await this.executeScript(data);
          break;
        case "MarionetteFrameParent:findElement":
          result = await this.findElement(data);
          break;
        case "MarionetteFrameParent:findElements":
          result = await this.findElements(data);
          break;
        case "MarionetteFrameParent:getCurrentUrl":
          result = await this.getCurrentUrl();
          break;
        case "MarionetteFrameParent:getActiveElement":
          result = await this.getActiveElement();
          break;
        case "MarionetteFrameParent:getElementAttribute":
          result = await this.getElementAttribute(data);
          break;
        case "MarionetteFrameParent:getElementProperty":
          result = await this.getElementProperty(data);
          break;
        case "MarionetteFrameParent:getElementRect":
          result = await this.getElementRect(data);
          break;
        case "MarionetteFrameParent:getElementTagName":
          result = await this.getElementTagName(data);
          break;
        case "MarionetteFrameParent:getElementText":
          result = await this.getElementText(data);
          break;
        case "MarionetteFrameParent:getElementValueOfCssProperty":
          result = await this.getElementValueOfCssProperty(data);
          break;
        case "MarionetteFrameParent:getPageSource":
          result = await this.getPageSource();
          break;
        case "MarionetteFrameParent:isElementDisplayed":
          result = await this.isElementDisplayed(data);
          break;
        case "MarionetteFrameParent:isElementEnabled":
          result = await this.isElementEnabled(data);
          break;
        case "MarionetteFrameParent:isElementSelected":
          result = await this.isElementSelected(data);
          break;
        case "MarionetteFrameParent:sendKeysToElement":
          result = await this.sendKeysToElement(data);
          break;
        case "MarionetteFrameParent:switchToFrame":
          result = await this.switchToFrame(data);
          break;
        case "MarionetteFrameParent:switchToParentFrame":
          result = await this.switchToParentFrame();
          break;
      }

      // The element reference store lives in the parent process. Calling
      // toJSON() without a second argument here passes element reference ids
      // of DOM nodes to the parent frame.
      return { data: evaluate.toJSON(result) };
    } catch (e) {
      // Always wrap errors as WebDriverError
      return { error: error.wrap(e).toJSON() };
    }
  }

  // Implementation of WebDriver commands

  /** Clear the text of an element.
   *
   * @param {Object} options
   * @param {Element} options.elem
   */
  clearElement(options = {}) {
    const { elem } = options;

    interaction.clearElement(elem);
  }

  /**
   * Click an element.
   */
  async clickElement(options = {}) {
    const { capabilities, elem } = options;

    return interaction.clickElement(
      elem,
      capabilities["moz:accessibilityChecks"],
      capabilities["moz:webdriverClick"]
    );
  }

  /**
   * Executes a JavaScript function.
   */
  async executeScript(options = {}) {
    const { args, opts = {}, script } = options;

    let sb;
    if (opts.sandboxName) {
      sb = this.sandboxes.get(opts.sandboxName, opts.newSandbox);
    } else {
      sb = sandbox.createMutable(this.contentWindow);
    }

    return evaluate.sandbox(sb, script, args, opts);
  }

  /**
   * Find an element in the current browsing context's document using the
   * given search strategy.
   *
   * @param {Object} options
   * @param {Object} options.opts
   * @param {Element} opts.startNode
   * @param {string} opts.strategy
   * @param {string} opts.selector
   *
   */
  async findElement(options = {}) {
    const { strategy, selector, opts } = options;

    opts.all = false;

    const container = { frame: this.contentWindow };
    return element.find(container, strategy, selector, opts);
  }

  /**
   * Find elements in the current browsing context's document using the
   * given search strategy.
   *
   * @param {Object} options
   * @param {Object} options.opts
   * @param {Element} opts.startNode
   * @param {string} opts.strategy
   * @param {string} opts.selector
   *
   */
  async findElements(options = {}) {
    const { strategy, selector, opts } = options;

    opts.all = true;

    const container = { frame: this.contentWindow };
    return element.find(container, strategy, selector, opts);
  }

  /**
   * Return the active element in the document.
   */
  async getActiveElement() {
    let elem = this.document.activeElement;
    if (!elem) {
      throw new error.NoSuchElementError();
    }

    return elem;
  }

  /**
   * Get the current URL.
   */
  async getCurrentUrl() {
    return this.contentWindow.location.href;
  }

  /**
   * Get the value of an attribute for the given element.
   */
  async getElementAttribute(options = {}) {
    const { name, elem } = options;

    if (element.isBooleanAttribute(elem, name)) {
      if (elem.hasAttribute(name)) {
        return "true";
      }
      return null;
    }
    return elem.getAttribute(name);
  }

  /**
   * Get the value of a property for the given element.
   */
  async getElementProperty(options = {}) {
    const { name, elem } = options;

    return typeof elem[name] != "undefined" ? elem[name] : null;
  }

  /**
   * Get the position and dimensions of the element.
   */
  async getElementRect(options = {}) {
    const { elem } = options;

    const rect = elem.getBoundingClientRect();
    return {
      x: rect.x + this.contentWindow.pageXOffset,
      y: rect.y + this.contentWindow.pageYOffset,
      width: rect.width,
      height: rect.height,
    };
  }

  /**
   * Get the tagName for the given element.
   */
  async getElementTagName(options = {}) {
    const { elem } = options;

    return elem.tagName.toLowerCase();
  }

  /**
   * Get the text content for the given element.
   */
  async getElementText(options = {}) {
    const { elem } = options;

    return atom.getElementText(elem, this.contentWindow);
  }

  /**
   * Get the value of a css property for the given element.
   */
  async getElementValueOfCssProperty(options = {}) {
    const { name, elem } = options;

    const style = this.contentWindow.getComputedStyle(elem);
    return style.getPropertyValue(name);
  }

  /**
   * Get the source of the current browsing context's document.
   */
  async getPageSource() {
    return this.document.documentElement.outerHTML;
  }

  /**
   * Determine the element displayedness of the given web element.
   */
  async isElementDisplayed(options = {}) {
    const { capabilities, elem } = options;

    return interaction.isElementDisplayed(
      elem,
      capabilities["moz:accessibilityChecks"]
    );
  }

  /**
   * Check if element is enabled.
   */
  async isElementEnabled(options = {}) {
    const { capabilities, elem } = options;

    return interaction.isElementEnabled(
      elem,
      capabilities["moz:accessibilityChecks"]
    );
  }

  /**
   * Determine whether the referenced element is selected or not.
   */
  async isElementSelected(options = {}) {
    const { capabilities, elem } = options;

    return interaction.isElementSelected(
      elem,
      capabilities["moz:accessibilityChecks"]
    );
  }

  /*
   * Send key presses to element after focusing on it.
   */
  async sendKeysToElement(options = {}) {
    const { capabilities, elem, text } = options;

    const opts = {
      strictFileInteractability: capabilities.strictFileInteractability,
      accessibilityChecks: capabilities["moz:accessibilityChecks"],
      webdriverClick: capabilities["moz:webdriverClick"],
    };

    return interaction.sendKeysToElement(elem, text, opts);
  }

  /**
   * Switch to the specified frame.
   *
   * @param {Object=} options
   * @param {(number|Element)=} options.id
   *     If it's a number treat it as the index for all the existing frames.
   *     If it's an Element switch to this specific frame.
   *     If not specified or `null` switch to the top-level browsing context.
   */
  async switchToFrame(options = {}) {
    const { id } = options;

    const childContexts = this.browsingContext.children;
    let browsingContext;

    if (id == null) {
      browsingContext = this.browsingContext.top;
    } else if (typeof id == "number") {
      if (id < 0 || id >= childContexts.length) {
        throw new error.NoSuchFrameError(
          `Unable to locate frame with index: ${id}`
        );
      }
      browsingContext = childContexts[id];
    } else {
      const context = childContexts.find(context => {
        return context.embedderElement === id;
      });
      if (!context) {
        throw new error.NoSuchFrameError(
          `Unable to locate frame for element: ${id}`
        );
      }
      browsingContext = context;
    }

    return { browsingContextId: browsingContext.id };
  }

  /**
   * Switch to the parent frame.
   */
  async switchToParentFrame() {
    const browsingContext = this.browsingContext.parent || this.browsingContext;

    return { browsingContextId: browsingContext.id };
  }
}
