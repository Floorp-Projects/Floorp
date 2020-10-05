/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

("use strict");

const EXPORTED_SYMBOLS = ["MarionetteFrameParent"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  element: "chrome://marionette/content/element.js",
  error: "chrome://marionette/content/error.js",
  evaluate: "chrome://marionette/content/evaluate.js",
  EventEmitter: "resource://gre/modules/EventEmitter.jsm",
  Log: "chrome://marionette/content/log.js",
});

XPCOMUtils.defineLazyGetter(this, "logger", () => Log.get());
XPCOMUtils.defineLazyGetter(this, "elementIdCache", () => {
  return new element.ReferenceStore();
});

class MarionetteFrameParent extends JSWindowActorParent {
  constructor() {
    super();

    EventEmitter.decorate(this);
  }

  actorCreated() {
    logger.trace(`[${this.browsingContext.id}] Parent actor created`);
  }

  async receiveMessage(msg) {
    const { name, data } = msg;

    let rv;

    switch (name) {
      case "MarionetteFrameChild:PageLoadEvent":
        this.emit("page-load-event", data);
        break;
      case "MarionetteFrameChild:ElementIdCacheAdd":
        rv = elementIdCache.add(data).toJSON();
    }

    return rv;
  }

  async sendQuery(name, data) {
    const serializedData = evaluate.toJSON(data, elementIdCache);
    const result = await super.sendQuery(name, serializedData);

    if ("error" in result) {
      throw error.WebDriverError.fromJSON(result.error);
    } else {
      return evaluate.fromJSON(result.data, elementIdCache);
    }
  }

  cleanUp() {
    elementIdCache.clear();
  }

  // Proxying methods for WebDriver commands
  // TODO: Maybe using a proxy class instead similar to proxy.js

  clearElement(elem) {
    return this.sendQuery("MarionetteFrameParent:clearElement", { elem });
  }

  clickElement(elem, capabilities) {
    return this.sendQuery("MarionetteFrameParent:clickElement", {
      elem,
      capabilities,
    });
  }

  async executeScript(script, args, opts) {
    return this.sendQuery("MarionetteFrameParent:executeScript", {
      script,
      args,
      opts,
    });
  }

  findElement(strategy, selector, opts) {
    return this.sendQuery("MarionetteFrameParent:findElement", {
      strategy,
      selector,
      opts,
    });
  }

  findElements(strategy, selector, opts) {
    return this.sendQuery("MarionetteFrameParent:findElements", {
      strategy,
      selector,
      opts,
    });
  }

  async getActiveElement() {
    return this.sendQuery("MarionetteFrameParent:getActiveElement");
  }

  async getCurrentUrl() {
    return this.sendQuery("MarionetteFrameParent:getCurrentUrl");
  }

  async getElementAttribute(elem, name) {
    return this.sendQuery("MarionetteFrameParent:getElementAttribute", {
      elem,
      name,
    });
  }

  async getElementProperty(elem, name) {
    return this.sendQuery("MarionetteFrameParent:getElementProperty", {
      elem,
      name,
    });
  }

  async getElementRect(elem) {
    return this.sendQuery("MarionetteFrameParent:getElementRect", { elem });
  }

  async getElementTagName(elem) {
    return this.sendQuery("MarionetteFrameParent:getElementTagName", { elem });
  }

  async getElementText(elem) {
    return this.sendQuery("MarionetteFrameParent:getElementText", { elem });
  }

  async getElementValueOfCssProperty(elem, name) {
    return this.sendQuery(
      "MarionetteFrameParent:getElementValueOfCssProperty",
      {
        elem,
        name,
      }
    );
  }

  async getPageSource() {
    return this.sendQuery("MarionetteFrameParent:getPageSource");
  }

  async isElementDisplayed(elem, capabilities) {
    return this.sendQuery("MarionetteFrameParent:isElementDisplayed", {
      capabilities,
      elem,
    });
  }

  async isElementEnabled(elem, capabilities) {
    return this.sendQuery("MarionetteFrameParent:isElementEnabled", {
      capabilities,
      elem,
    });
  }

  async isElementSelected(elem, capabilities) {
    return this.sendQuery("MarionetteFrameParent:isElementSelected", {
      capabilities,
      elem,
    });
  }

  async sendKeysToElement(elem, text, capabilities) {
    return this.sendQuery("MarionetteFrameParent:sendKeysToElement", {
      capabilities,
      elem,
      text,
    });
  }

  async performActions(actions, capabilities) {
    return this.sendQuery("MarionetteFrameParent:performActions", {
      actions,
      capabilities,
    });
  }

  async releaseActions() {
    return this.sendQuery("MarionetteFrameParent:releaseActions");
  }

  async switchToFrame(id) {
    const {
      browsingContextId,
    } = await this.sendQuery("MarionetteFrameParent:switchToFrame", { id });

    return {
      browsingContext: BrowsingContext.get(browsingContextId),
    };
  }

  async switchToParentFrame() {
    const { browsingContextId } = await this.sendQuery(
      "MarionetteFrameParent:switchToParentFrame"
    );

    return {
      browsingContext: BrowsingContext.get(browsingContextId),
    };
  }
}
