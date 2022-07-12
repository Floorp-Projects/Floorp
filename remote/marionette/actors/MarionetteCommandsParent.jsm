/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

("use strict");

const EXPORTED_SYMBOLS = [
  "clearElementIdCache",
  "getMarionetteCommandsActorProxy",
  "MarionetteCommandsParent",
  "registerCommandsActor",
  "unregisterCommandsActor",
];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  capture: "chrome://remote/content/marionette/capture.js",
  element: "chrome://remote/content/marionette/element.js",
  error: "chrome://remote/content/shared/webdriver/Errors.jsm",
  evaluate: "chrome://remote/content/marionette/evaluate.js",
  Log: "chrome://remote/content/shared/Log.jsm",
});

XPCOMUtils.defineLazyGetter(lazy, "logger", () =>
  lazy.Log.get(lazy.Log.TYPES.MARIONETTE)
);
XPCOMUtils.defineLazyGetter(lazy, "elementIdCache", () => {
  return new lazy.element.ReferenceStore();
});

class MarionetteCommandsParent extends JSWindowActorParent {
  actorCreated() {
    this._resolveDialogOpened = null;

    this.topWindow = this.browsingContext.top.embedderElement?.ownerGlobal;
    this.topWindow?.addEventListener("TabClose", _onTabClose);
  }

  dialogOpenedPromise() {
    return new Promise(resolve => {
      this._resolveDialogOpened = resolve;
    });
  }

  async sendQuery(name, data) {
    const serializedData = lazy.evaluate.toJSON(data, lazy.elementIdCache);

    // return early if a dialog is opened
    const result = await Promise.race([
      super.sendQuery(name, serializedData),
      this.dialogOpenedPromise(),
    ]).finally(() => {
      this._resolveDialogOpened = null;
    });

    if ("error" in result) {
      throw lazy.error.WebDriverError.fromJSON(result.error);
    } else {
      return lazy.evaluate.fromJSON({
        obj: result.data,
        seenEls: lazy.elementIdCache,
      });
    }
  }

  didDestroy() {
    this.topWindow?.removeEventListener("TabClose", _onTabClose);
  }

  notifyDialogOpened() {
    if (this._resolveDialogOpened) {
      this._resolveDialogOpened({ data: null });
    }
  }

  // Proxying methods for WebDriver commands

  clearElement(webEl) {
    return this.sendQuery("MarionetteCommandsParent:clearElement", {
      elem: webEl,
    });
  }

  clickElement(webEl, capabilities) {
    return this.sendQuery("MarionetteCommandsParent:clickElement", {
      elem: webEl,
      capabilities,
    });
  }

  async executeScript(script, args, opts) {
    return this.sendQuery("MarionetteCommandsParent:executeScript", {
      script,
      args,
      opts,
    });
  }

  findElement(strategy, selector, opts) {
    return this.sendQuery("MarionetteCommandsParent:findElement", {
      strategy,
      selector,
      opts,
    });
  }

  findElements(strategy, selector, opts) {
    return this.sendQuery("MarionetteCommandsParent:findElements", {
      strategy,
      selector,
      opts,
    });
  }

  async getShadowRoot(webEl) {
    return this.sendQuery("MarionetteCommandsParent:getShadowRoot", {
      elem: webEl,
    });
  }

  async getActiveElement() {
    return this.sendQuery("MarionetteCommandsParent:getActiveElement");
  }

  async getElementAttribute(webEl, name) {
    return this.sendQuery("MarionetteCommandsParent:getElementAttribute", {
      elem: webEl,
      name,
    });
  }

  async getElementProperty(webEl, name) {
    return this.sendQuery("MarionetteCommandsParent:getElementProperty", {
      elem: webEl,
      name,
    });
  }

  async getElementRect(webEl) {
    return this.sendQuery("MarionetteCommandsParent:getElementRect", {
      elem: webEl,
    });
  }

  async getElementTagName(webEl) {
    return this.sendQuery("MarionetteCommandsParent:getElementTagName", {
      elem: webEl,
    });
  }

  async getElementText(webEl) {
    return this.sendQuery("MarionetteCommandsParent:getElementText", {
      elem: webEl,
    });
  }

  async getElementValueOfCssProperty(webEl, name) {
    return this.sendQuery(
      "MarionetteCommandsParent:getElementValueOfCssProperty",
      {
        elem: webEl,
        name,
      }
    );
  }

  async getPageSource() {
    return this.sendQuery("MarionetteCommandsParent:getPageSource");
  }

  async isElementDisplayed(webEl, capabilities) {
    return this.sendQuery("MarionetteCommandsParent:isElementDisplayed", {
      capabilities,
      elem: webEl,
    });
  }

  async isElementEnabled(webEl, capabilities) {
    return this.sendQuery("MarionetteCommandsParent:isElementEnabled", {
      capabilities,
      elem: webEl,
    });
  }

  async isElementSelected(webEl, capabilities) {
    return this.sendQuery("MarionetteCommandsParent:isElementSelected", {
      capabilities,
      elem: webEl,
    });
  }

  async sendKeysToElement(webEl, text, capabilities) {
    return this.sendQuery("MarionetteCommandsParent:sendKeysToElement", {
      capabilities,
      elem: webEl,
      text,
    });
  }

  async performActions(actions, capabilities) {
    return this.sendQuery("MarionetteCommandsParent:performActions", {
      actions,
      capabilities,
    });
  }

  async releaseActions() {
    return this.sendQuery("MarionetteCommandsParent:releaseActions");
  }

  async singleTap(webEl, x, y, capabilities) {
    return this.sendQuery("MarionetteCommandsParent:singleTap", {
      capabilities,
      elem: webEl,
      x,
      y,
    });
  }

  async switchToFrame(id) {
    const {
      browsingContextId,
    } = await this.sendQuery("MarionetteCommandsParent:switchToFrame", { id });

    return {
      browsingContext: BrowsingContext.get(browsingContextId),
    };
  }

  async switchToParentFrame() {
    const { browsingContextId } = await this.sendQuery(
      "MarionetteCommandsParent:switchToParentFrame"
    );

    return {
      browsingContext: BrowsingContext.get(browsingContextId),
    };
  }

  async takeScreenshot(webEl, format, full, scroll) {
    const rect = await this.sendQuery(
      "MarionetteCommandsParent:getScreenshotRect",
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

    let canvas = await lazy.capture.canvas(
      browsingContext.topChromeWindow,
      browsingContext,
      rect.x,
      rect.y,
      rect.width,
      rect.height
    );

    switch (format) {
      case lazy.capture.Format.Hash:
        return lazy.capture.toHash(canvas);

      case lazy.capture.Format.Base64:
        return lazy.capture.toBase64(canvas);

      default:
        throw new TypeError(`Invalid capture format: ${format}`);
    }
  }
}

/**
 * Clear all the entries from the element id cache.
 */
function clearElementIdCache() {
  lazy.elementIdCache.clear();
}

function _onTabClose(event) {
  lazy.elementIdCache.clear(event.target.linkedBrowser.browsingContext);
}

/**
 * Proxy that will dynamically create MarionetteCommands actors for a dynamically
 * provided browsing context until the method can be fully executed by the
 * JSWindowActor pair.
 *
 * @param {function(): BrowsingContext} browsingContextFn
 *     A function that returns the reference to the browsing context for which
 *     the query should run.
 */
function getMarionetteCommandsActorProxy(browsingContextFn) {
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
              const browsingContext = browsingContextFn();
              if (!browsingContext) {
                throw new DOMException(
                  "No BrowsingContext found",
                  "NoBrowsingContext"
                );
              }

              // TODO: Scenarios where the window/tab got closed and
              // currentWindowGlobal is null will be handled in Bug 1662808.
              const actor = browsingContext.currentWindowGlobal.getActor(
                "MarionetteCommands"
              );

              const result = await actor[methodName](...args);
              return result;
            } catch (e) {
              if (!["AbortError", "InactiveActor"].includes(e.name)) {
                // Only retry when the JSWindowActor pair gets destroyed, or
                // gets inactive eg. when the page is moved into bfcache.
                throw e;
              }

              if (NO_RETRY_METHODS.includes(methodName)) {
                const browsingContextId = browsingContextFn()?.id;
                lazy.logger.trace(
                  `[${browsingContextId}] Querying "${methodName}" failed with` +
                    ` ${e.name}, returning "null" as fallback`
                );
                return null;
              }

              if (++attempts > MAX_ATTEMPTS) {
                const browsingContextId = browsingContextFn()?.id;
                lazy.logger.trace(
                  `[${browsingContextId}] Querying "${methodName} "` +
                    `reached the limit of retry attempts (${MAX_ATTEMPTS})`
                );
                throw e;
              }

              lazy.logger.trace(
                `Retrying "${methodName}", attempt: ${attempts}`
              );
            }
          }
        };
      },
    }
  );
}

/**
 * Register the MarionetteCommands actor that holds all the commands.
 */
function registerCommandsActor() {
  try {
    ChromeUtils.registerWindowActor("MarionetteCommands", {
      kind: "JSWindowActor",
      parent: {
        moduleURI:
          "chrome://remote/content/marionette/actors/MarionetteCommandsParent.jsm",
      },
      child: {
        moduleURI:
          "chrome://remote/content/marionette/actors/MarionetteCommandsChild.jsm",
      },

      allFrames: true,
      includeChrome: true,
    });
  } catch (e) {
    if (e.name === "NotSupportedError") {
      lazy.logger.warn(`MarionetteCommands actor is already registered!`);
    } else {
      throw e;
    }
  }
}

function unregisterCommandsActor() {
  ChromeUtils.unregisterWindowActor("MarionetteCommands");
}
