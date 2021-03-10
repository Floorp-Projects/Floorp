/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-disable no-restricted-globals */

"use strict";

const EXPORTED_SYMBOLS = ["MarionetteCommandsChild", "clearActionInputState"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  action: "chrome://marionette/content/action.js",
  atom: "chrome://marionette/content/atom.js",
  element: "chrome://marionette/content/element.js",
  error: "chrome://marionette/content/error.js",
  evaluate: "chrome://marionette/content/evaluate.js",
  event: "chrome://marionette/content/event.js",
  interaction: "chrome://marionette/content/interaction.js",
  legacyaction: "chrome://marionette/content/legacyaction.js",
  Log: "chrome://marionette/content/log.js",
  sandbox: "chrome://marionette/content/evaluate.js",
  Sandboxes: "chrome://marionette/content/evaluate.js",
});

XPCOMUtils.defineLazyGetter(this, "logger", () => Log.get());

let inputStateIsDirty = false;

class MarionetteCommandsChild extends JSWindowActorChild {
  constructor() {
    super();

    // sandbox storage and name of the current sandbox
    this.sandboxes = new Sandboxes(() => this.document.defaultView);
  }

  get innerWindowId() {
    return this.manager.innerWindowId;
  }

  /**
   * Lazy getter to create a legacyaction Chain instance for touch events.
   */
  get legacyactions() {
    if (!this._legacyactions) {
      this._legacyactions = new legacyaction.Chain();
    }

    return this._legacyactions;
  }

  actorCreated() {
    logger.trace(
      `[${this.browsingContext.id}] MarionetteCommands actor created ` +
        `for window id ${this.innerWindowId}`
    );

    clearActionInputState();
  }

  async receiveMessage(msg) {
    if (!this.contentWindow) {
      throw new DOMException("Actor is no longer active", "InactiveActor");
    }

    try {
      let result;

      const { name, data: serializedData } = msg;
      const data = evaluate.fromJSON(
        serializedData,
        null,
        this.document.defaultView
      );

      switch (name) {
        case "MarionetteCommandsParent:clearElement":
          this.clearElement(data);
          break;
        case "MarionetteCommandsParent:clickElement":
          result = await this.clickElement(data);
          break;
        case "MarionetteCommandsParent:executeScript":
          result = await this.executeScript(data);
          break;
        case "MarionetteCommandsParent:findElement":
          result = await this.findElement(data);
          break;
        case "MarionetteCommandsParent:findElements":
          result = await this.findElements(data);
          break;
        case "MarionetteCommandsParent:getActiveElement":
          result = await this.getActiveElement();
          break;
        case "MarionetteCommandsParent:getElementAttribute":
          result = await this.getElementAttribute(data);
          break;
        case "MarionetteCommandsParent:getElementProperty":
          result = await this.getElementProperty(data);
          break;
        case "MarionetteCommandsParent:getElementRect":
          result = await this.getElementRect(data);
          break;
        case "MarionetteCommandsParent:getElementTagName":
          result = await this.getElementTagName(data);
          break;
        case "MarionetteCommandsParent:getElementText":
          result = await this.getElementText(data);
          break;
        case "MarionetteCommandsParent:getElementValueOfCssProperty":
          result = await this.getElementValueOfCssProperty(data);
          break;
        case "MarionetteCommandsParent:getPageSource":
          result = await this.getPageSource();
          break;
        case "MarionetteCommandsParent:getScreenshotRect":
          result = await this.getScreenshotRect(data);
          break;
        case "MarionetteCommandsParent:isElementDisplayed":
          result = await this.isElementDisplayed(data);
          break;
        case "MarionetteCommandsParent:isElementEnabled":
          result = await this.isElementEnabled(data);
          break;
        case "MarionetteCommandsParent:isElementSelected":
          result = await this.isElementSelected(data);
          break;
        case "MarionetteCommandsParent:performActions":
          result = await this.performActions(data);
          break;
        case "MarionetteCommandsParent:releaseActions":
          result = await this.releaseActions();
          break;
        case "MarionetteCommandsParent:sendKeysToElement":
          result = await this.sendKeysToElement(data);
          break;
        case "MarionetteCommandsParent:singleTap":
          result = await this.singleTap(data);
          break;
        case "MarionetteCommandsParent:switchToFrame":
          result = await this.switchToFrame(data);
          break;
        case "MarionetteCommandsParent:switchToParentFrame":
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
      sb = sandbox.createMutable(this.document.defaultView);
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

    const container = { frame: this.document.defaultView };
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

    const container = { frame: this.document.defaultView };
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
      x: rect.x + this.document.defaultView.pageXOffset,
      y: rect.y + this.document.defaultView.pageYOffset,
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

    return atom.getElementText(elem, this.document.defaultView);
  }

  /**
   * Get the value of a css property for the given element.
   */
  async getElementValueOfCssProperty(options = {}) {
    const { name, elem } = options;

    const style = this.document.defaultView.getComputedStyle(elem);
    return style.getPropertyValue(name);
  }

  /**
   * Get the source of the current browsing context's document.
   */
  async getPageSource() {
    return this.document.documentElement.outerHTML;
  }

  /**
   * Returns the rect of the element to screenshot.
   *
   * Because the screen capture takes place in the parent process the dimensions
   * for the screenshot have to be determined in the appropriate child process.
   *
   * Also it takes care of scrolling an element into view if requested.
   *
   * @param {Object} options
   * @param {Element} options.elem
   *     Optional element to take a screenshot of.
   * @param {boolean=} options.full
   *     True to take a screenshot of the entire document element.
   *     Defaults to true.
   * @param {boolean=} options.scroll
   *     When <var>elem</var> is given, scroll it into view.
   *     Defaults to true.
   *
   * @return {DOMRect}
   *     The area to take a snapshot from.
   */
  async getScreenshotRect(options = {}) {
    const { elem, full = true, scroll = true } = options;
    const win = elem
      ? this.document.defaultView
      : this.browsingContext.top.window;

    let rect;

    if (elem) {
      if (scroll) {
        element.scrollIntoView(elem);
      }
      rect = this.getElementRect({ elem });
    } else if (full) {
      const docEl = win.document.documentElement;
      rect = new DOMRect(0, 0, docEl.scrollWidth, docEl.scrollHeight);
    } else {
      // viewport
      rect = new DOMRect(
        win.pageXOffset,
        win.pageYOffset,
        win.innerWidth,
        win.innerHeight
      );
    }

    return rect;
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

  /**
   * Perform a series of grouped actions at the specified points in time.
   *
   * @param {Object} options
   * @param {Object} options.actions
   *     Array of objects with each representing an action sequence.
   * @param {Object} options.capabilities
   *     Object with a list of WebDriver session capabilities.
   */
  async performActions(options = {}) {
    const { actions, capabilities } = options;

    await action.dispatch(
      action.Chain.fromJSON(actions),
      this.document.defaultView,
      !capabilities["moz:useNonSpecCompliantPointerOrigin"]
    );
    inputStateIsDirty =
      action.inputsToCancel.length || action.inputStateMap.size;
  }

  /**
   * The release actions command is used to release all the keys and pointer
   * buttons that are currently depressed. This causes events to be fired
   * as if the state was released by an explicit series of actions. It also
   * clears all the internal state of the virtual devices.
   */
  async releaseActions() {
    await action.dispatchTickActions(
      action.inputsToCancel.reverse(),
      0,
      this.document.defaultView
    );
    clearActionInputState();

    event.DoubleClickTracker.resetClick();
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
   * Perform a single tap.
   */
  async singleTap(options = {}) {
    const { capabilities, elem, x, y } = options;
    return this.legacyactions.singleTap(elem, x, y, capabilities);
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

    // For in-process iframes the window global is lazy-loaded for optimization
    // reasons. As such force the currentWindowGlobal to be created so we always
    // have a window (bug 1691348).
    browsingContext.window;

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

/**
 * Reset Action API input state
 */
function clearActionInputState() {
  // Avoid loading the action module before it is needed by a command
  if (inputStateIsDirty) {
    action.inputStateMap.clear();
    action.inputsToCancel.length = 0;
    inputStateIsDirty = false;
  }
}
