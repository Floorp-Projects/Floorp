/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-disable no-restricted-globals */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  accessibility: "chrome://remote/content/marionette/accessibility.sys.mjs",
  action: "chrome://remote/content/shared/webdriver/Actions.sys.mjs",
  atom: "chrome://remote/content/marionette/atom.sys.mjs",
  dom: "chrome://remote/content/shared/DOM.sys.mjs",
  error: "chrome://remote/content/shared/webdriver/Errors.sys.mjs",
  evaluate: "chrome://remote/content/marionette/evaluate.sys.mjs",
  interaction: "chrome://remote/content/marionette/interaction.sys.mjs",
  json: "chrome://remote/content/marionette/json.sys.mjs",
  Log: "chrome://remote/content/shared/Log.sys.mjs",
  sandbox: "chrome://remote/content/marionette/evaluate.sys.mjs",
  Sandboxes: "chrome://remote/content/marionette/evaluate.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "logger", () =>
  lazy.Log.get(lazy.Log.TYPES.MARIONETTE)
);

export class MarionetteCommandsChild extends JSWindowActorChild {
  #processActor;

  constructor() {
    super();

    this.#processActor = ChromeUtils.domProcessChild.getActor(
      "WebDriverProcessData"
    );

    // sandbox storage and name of the current sandbox
    this.sandboxes = new lazy.Sandboxes(() => this.document.defaultView);
    // State of the input actions. This is specific to contexts and sessions
    this.actionState = null;
  }

  get innerWindowId() {
    return this.manager.innerWindowId;
  }

  actorCreated() {
    lazy.logger.trace(
      `[${this.browsingContext.id}] MarionetteCommands actor created ` +
        `for window id ${this.innerWindowId}`
    );
  }

  didDestroy() {
    lazy.logger.trace(
      `[${this.browsingContext.id}] MarionetteCommands actor destroyed ` +
        `for window id ${this.innerWindowId}`
    );
  }

  async receiveMessage(msg) {
    if (!this.contentWindow) {
      throw new DOMException("Actor is no longer active", "InactiveActor");
    }

    try {
      let result;
      let waitForNextTick = false;

      const { name, data: serializedData } = msg;

      const data = lazy.json.deserialize(
        serializedData,
        this.#processActor.getNodeCache(),
        this.contentWindow.browsingContext
      );

      switch (name) {
        case "MarionetteCommandsParent:clearElement":
          this.clearElement(data);
          waitForNextTick = true;
          break;
        case "MarionetteCommandsParent:clickElement":
          result = await this.clickElement(data);
          waitForNextTick = true;
          break;
        case "MarionetteCommandsParent:executeScript":
          result = await this.executeScript(data);
          waitForNextTick = true;
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
        case "MarionetteCommandsParent:getComputedLabel":
          result = await this.getComputedLabel(data);
          break;
        case "MarionetteCommandsParent:getComputedRole":
          result = await this.getComputedRole(data);
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
        case "MarionetteCommandsParent:getShadowRoot":
          result = await this.getShadowRoot(data);
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
          waitForNextTick = true;
          break;
        case "MarionetteCommandsParent:releaseActions":
          result = await this.releaseActions();
          break;
        case "MarionetteCommandsParent:sendKeysToElement":
          result = await this.sendKeysToElement(data);
          waitForNextTick = true;
          break;
        case "MarionetteCommandsParent:switchToFrame":
          result = await this.switchToFrame(data);
          waitForNextTick = true;
          break;
        case "MarionetteCommandsParent:switchToParentFrame":
          result = await this.switchToParentFrame();
          waitForNextTick = true;
          break;
      }

      // Inform the content process that the command has completed. It allows
      // it to process async follow-up tasks before the reply is sent.
      if (waitForNextTick) {
        await new Promise(resolve => Services.tm.dispatchToMainThread(resolve));
      }

      const { seenNodeIds, serializedValue, hasSerializedWindows } =
        lazy.json.clone(result, this.#processActor.getNodeCache());

      // Because in WebDriver classic nodes can only be returned from the same
      // browsing context, we only need the seen unique ids as flat array.
      return {
        seenNodeIds: [...seenNodeIds.values()].flat(),
        serializedValue,
        hasSerializedWindows,
      };
    } catch (e) {
      // Always wrap errors as WebDriverError
      return { error: lazy.error.wrap(e).toJSON() };
    }
  }

  // Implementation of WebDriver commands

  /** Clear the text of an element.
   *
   * @param {object} options
   * @param {Element} options.elem
   */
  clearElement(options = {}) {
    const { elem } = options;

    lazy.interaction.clearElement(elem);
  }

  /**
   * Click an element.
   */
  async clickElement(options = {}) {
    const { capabilities, elem } = options;

    return lazy.interaction.clickElement(
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
      sb = lazy.sandbox.createMutable(this.document.defaultView);
    }

    return lazy.evaluate.sandbox(sb, script, args, opts);
  }

  /**
   * Find an element in the current browsing context's document using the
   * given search strategy.
   *
   * @param {object=} options
   * @param {string} options.strategy
   * @param {string} options.selector
   * @param {object} options.opts
   * @param {Element} options.opts.startNode
   *
   */
  async findElement(options = {}) {
    const { strategy, selector, opts } = options;

    opts.all = false;

    const container = { frame: this.document.defaultView };
    return lazy.dom.find(container, strategy, selector, opts);
  }

  /**
   * Find elements in the current browsing context's document using the
   * given search strategy.
   *
   * @param {object=} options
   * @param {string} options.strategy
   * @param {string} options.selector
   * @param {object} options.opts
   * @param {Element} options.opts.startNode
   *
   */
  async findElements(options = {}) {
    const { strategy, selector, opts } = options;

    opts.all = true;

    const container = { frame: this.document.defaultView };
    return lazy.dom.find(container, strategy, selector, opts);
  }

  /**
   * Return the active element in the document.
   */
  async getActiveElement() {
    let elem = this.document.activeElement;
    if (!elem) {
      throw new lazy.error.NoSuchElementError();
    }

    return elem;
  }

  /**
   * Return the accessible label for a given element.
   */
  async getComputedLabel(options = {}) {
    const { elem } = options;

    const accessible = await lazy.accessibility.getAccessible(elem);
    if (!accessible) {
      return null;
    }

    return accessible.name;
  }

  /**
   * Return the accessible role for a given element.
   */
  async getComputedRole(options = {}) {
    const { elem } = options;

    const accessible = await lazy.accessibility.getAccessible(elem);
    if (!accessible) {
      // If it's not in the a11y tree, it's probably presentational.
      return "none";
    }

    return accessible.computedARIARole;
  }

  /**
   * Get the value of an attribute for the given element.
   */
  async getElementAttribute(options = {}) {
    const { name, elem } = options;

    if (lazy.dom.isBooleanAttribute(elem, name)) {
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

    // Waive Xrays to get unfiltered access to the untrusted element.
    const el = Cu.waiveXrays(elem);
    return typeof el[name] != "undefined" ? el[name] : null;
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

    try {
      return lazy.atom.getElementText(elem, this.document.defaultView);
    } catch (e) {
      lazy.logger.warn(`Atom getElementText failed: "${e.message}"`);

      // Fallback in case the atom implementation is broken.
      // As known so far this only happens for XML documents (bug 1794099).
      return elem.textContent;
    }
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
   * @param {object} options
   * @param {Element} options.elem
   *     Optional element to take a screenshot of.
   * @param {boolean=} options.full
   *     True to take a screenshot of the entire document element.
   *     Defaults to true.
   * @param {boolean=} options.scroll
   *     When <var>elem</var> is given, scroll it into view.
   *     Defaults to true.
   *
   * @returns {DOMRect}
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
        lazy.dom.scrollIntoView(elem);
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
   * Return the shadowRoot attached to an element
   */
  async getShadowRoot(options = {}) {
    const { elem } = options;

    return lazy.dom.getShadowRoot(elem);
  }

  /**
   * Determine the element displayedness of the given web element.
   */
  async isElementDisplayed(options = {}) {
    const { capabilities, elem } = options;

    return lazy.interaction.isElementDisplayed(
      elem,
      capabilities["moz:accessibilityChecks"]
    );
  }

  /**
   * Check if element is enabled.
   */
  async isElementEnabled(options = {}) {
    const { capabilities, elem } = options;

    return lazy.interaction.isElementEnabled(
      elem,
      capabilities["moz:accessibilityChecks"]
    );
  }

  /**
   * Determine whether the referenced element is selected or not.
   */
  async isElementSelected(options = {}) {
    const { capabilities, elem } = options;

    return lazy.interaction.isElementSelected(
      elem,
      capabilities["moz:accessibilityChecks"]
    );
  }

  /**
   * Perform a series of grouped actions at the specified points in time.
   *
   * @param {object} options
   * @param {object} options.actions
   *     Array of objects with each representing an action sequence.
   * @param {object} options.capabilities
   *     Object with a list of WebDriver session capabilities.
   */
  async performActions(options = {}) {
    const { actions } = options;
    if (this.actionState === null) {
      this.actionState = new lazy.action.State();
    }
    let actionChain = lazy.action.Chain.fromJSON(this.actionState, actions);

    await actionChain.dispatch(this.actionState, this.document.defaultView);
    // Terminate the current wheel transaction if there is one. Wheel
    // transactions should not live longer than a single action chain.
    ChromeUtils.endWheelTransaction();
  }

  /**
   * The release actions command is used to release all the keys and pointer
   * buttons that are currently depressed. This causes events to be fired
   * as if the state was released by an explicit series of actions. It also
   * clears all the internal state of the virtual devices.
   */
  async releaseActions() {
    if (this.actionState === null) {
      return;
    }
    await this.actionState.release(this.document.defaultView);
    this.actionState = null;
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

    return lazy.interaction.sendKeysToElement(elem, text, opts);
  }

  /**
   * Switch to the specified frame.
   *
   * @param {object=} options
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
        throw new lazy.error.NoSuchFrameError(
          `Unable to locate frame with index: ${id}`
        );
      }
      browsingContext = childContexts[id];
    } else {
      const context = childContexts.find(context => {
        return context.embedderElement === id;
      });
      if (!context) {
        throw new lazy.error.NoSuchFrameError(
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
