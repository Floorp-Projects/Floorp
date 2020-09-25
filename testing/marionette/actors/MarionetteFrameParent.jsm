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

  clearElement(webEl) {
    return this.sendQuery("MarionetteFrameParent:clearElement", {
      webEl,
    });
  }

  clickElement(webEl, capabilities) {
    return this.sendQuery("MarionetteFrameParent:clickElement", {
      webEl,
      capabilities,
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

  async getCurrentUrl() {
    return this.sendQuery("MarionetteFrameParent:getCurrentUrl");
  }

  async getElementAttribute(webEl, name) {
    return this.sendQuery("MarionetteFrameParent:getElementAttribute", {
      name,
      webEl,
    });
  }

  async getElementProperty(webEl, name) {
    return this.sendQuery("MarionetteFrameParent:getElementProperty", {
      name,
      webEl,
    });
  }

  async getElementRect(webEl) {
    return this.sendQuery("MarionetteFrameParent:getElementRect", {
      webEl,
    });
  }

  async getElementTagName(webEl) {
    return this.sendQuery("MarionetteFrameParent:getElementTagName", {
      webEl,
    });
  }

  async getElementText(webEl) {
    return this.sendQuery("MarionetteFrameParent:getElementText", {
      webEl,
    });
  }

  async getElementValueOfCssProperty(webEl, name) {
    return this.sendQuery(
      "MarionetteFrameParent:getElementValueOfCssProperty",
      {
        name,
        webEl,
      }
    );
  }

  async getPageSource() {
    return this.sendQuery("MarionetteFrameParent:getPageSource");
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
