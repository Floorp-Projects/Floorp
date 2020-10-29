/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

("use strict");

const EXPORTED_SYMBOLS = [
  "getMarionetteFrameActorProxy",
  "MarionetteFrameParent",
];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  capture: "chrome://marionette/content/capture.js",
  element: "chrome://marionette/content/element.js",
  error: "chrome://marionette/content/error.js",
  evaluate: "chrome://marionette/content/evaluate.js",
  EventEmitter: "resource://gre/modules/EventEmitter.jsm",
  Log: "chrome://marionette/content/log.js",
  modal: "chrome://marionette/content/modal.js",
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
    this._resolveDialogOpened = null;

    this.dialogObserver = new modal.DialogObserver();
    this.dialogObserver.add((action, dialogRef, win) => {
      if (
        this._resolveDialogOpened &&
        action == "opened" &&
        win == this.browsingContext.topChromeWindow
      ) {
        this._resolveDialogOpened({ data: null });
      }
    });

    logger.trace(`[${this.browsingContext.id}] Parent actor created`);
  }

  dialogOpenedPromise() {
    return new Promise(resolve => {
      this._resolveDialogOpened = resolve;
    });
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

    // return early if a dialog is opened
    const result = await Promise.race([
      super.sendQuery(name, serializedData),
      this.dialogOpenedPromise(),
    ]).finally(() => {
      this._resolveDialogOpened = null;
    });

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
      elem: webEl,
    });
  }

  clickElement(webEl, capabilities) {
    return this.sendQuery("MarionetteFrameParent:clickElement", {
      elem: webEl,
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

  async getElementAttribute(webEl, name) {
    return this.sendQuery("MarionetteFrameParent:getElementAttribute", {
      elem: webEl,
      name,
    });
  }

  async getElementProperty(webEl, name) {
    return this.sendQuery("MarionetteFrameParent:getElementProperty", {
      elem: webEl,
      name,
    });
  }

  async getElementRect(webEl) {
    return this.sendQuery("MarionetteFrameParent:getElementRect", {
      elem: webEl,
    });
  }

  async getElementTagName(webEl) {
    return this.sendQuery("MarionetteFrameParent:getElementTagName", {
      elem: webEl,
    });
  }

  async getElementText(webEl) {
    return this.sendQuery("MarionetteFrameParent:getElementText", {
      elem: webEl,
    });
  }

  async getElementValueOfCssProperty(webEl, name) {
    return this.sendQuery(
      "MarionetteFrameParent:getElementValueOfCssProperty",
      {
        elem: webEl,
        name,
      }
    );
  }

  async getPageSource() {
    return this.sendQuery("MarionetteFrameParent:getPageSource");
  }

  async isElementDisplayed(webEl, capabilities) {
    return this.sendQuery("MarionetteFrameParent:isElementDisplayed", {
      capabilities,
      elem: webEl,
    });
  }

  async isElementEnabled(webEl, capabilities) {
    return this.sendQuery("MarionetteFrameParent:isElementEnabled", {
      capabilities,
      elem: webEl,
    });
  }

  async isElementSelected(webEl, capabilities) {
    return this.sendQuery("MarionetteFrameParent:isElementSelected", {
      capabilities,
      elem: webEl,
    });
  }

  async sendKeysToElement(webEl, text, capabilities) {
    return this.sendQuery("MarionetteFrameParent:sendKeysToElement", {
      capabilities,
      elem: webEl,
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

  async singleTap(webEl, x, y, capabilities) {
    return this.sendQuery("MarionetteFrameParent:singleTap", {
      capabilities,
      elem: webEl,
      x,
      y,
    });
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

  async takeScreenshot(webEl, format, full, scroll) {
    const rect = await this.sendQuery(
      "MarionetteFrameParent:getScreenshotRect",
      {
        elem: webEl,
        full,
        scroll,
      }
    );

    // If no element has been specified use the top-level browsing context.
    // Otherwise use the browsing context from the currently selected frame.
    const browsingContext = webEl
      ? this.browsingContext
      : this.browsingContext.top;

    let canvas = await capture.canvas(
      browsingContext.topChromeWindow,
      browsingContext,
      rect.x,
      rect.y,
      rect.width,
      rect.height
    );

    switch (format) {
      case capture.Format.Hash:
        return capture.toHash(canvas);

      case capture.Format.Base64:
        return capture.toBase64(canvas);

      default:
        throw new TypeError(`Invalid capture format: ${format}`);
    }
  }
}

/**
 * Proxy that will dynamically create MarionetteFrame actors for a dynamically
 * provided browsing context until the method can be fully executed by the
 * JSWindowActor pair.
 *
 * @param {function(): BrowsingContext} browsingContextFn
 *     A function that returns the reference to the browsing context for which
 *     the query should run.
 */
function getMarionetteFrameActorProxy(browsingContextFn) {
  const MAX_ATTEMPTS = 10;

  /**
   * Methods which modify the content page cannot be retried safely.
   * See Bug 1673345.
   */
  const NO_RETRY_METHODS = [
    "clickElement",
    "executeScript",
    "performActions",
    "releaseActions",
    "sendKeysToElement",
    "singleTap",
  ];

  return new Proxy(
    {},
    {
      get(target, methodName) {
        return async (...args) => {
          let attempts = 0;
          while (true) {
            try {
              // TODO: Scenarios where the window/tab got closed and
              // currentWindowGlobal is null will be handled in Bug 1662808.
              const actor = browsingContextFn().currentWindowGlobal.getActor(
                "MarionetteFrame"
              );
              const result = await actor[methodName](...args);
              return result;
            } catch (e) {
              if (e.name !== "AbortError") {
                // Only AbortError(s) are retried, let any other error through.
                throw e;
              }

              if (NO_RETRY_METHODS.includes(methodName)) {
                return null;
              }

              if (++attempts > MAX_ATTEMPTS) {
                const browsingContextId = browsingContextFn()?.id;
                logger.trace(
                  `[${browsingContextId}] Query "${methodName}" reached the limit of retry attempts (${MAX_ATTEMPTS})`
                );
                throw e;
              }
            }
          }
        };
      },
    }
  );
}
