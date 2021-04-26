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

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  capture: "chrome://marionette/content/capture.js",
  element: "chrome://marionette/content/element.js",
  error: "chrome://marionette/content/error.js",
  evaluate: "chrome://marionette/content/evaluate.js",
  Log: "chrome://marionette/content/log.js",
  modal: "chrome://marionette/content/modal.js",
});

XPCOMUtils.defineLazyGetter(this, "logger", () => Log.get());
XPCOMUtils.defineLazyGetter(this, "elementIdCache", () => {
  return new element.ReferenceStore();
});

class MarionetteCommandsParent extends JSWindowActorParent {
  actorCreated() {
    this._resolveDialogOpened = null;

    this.dialogObserver = new modal.DialogObserver();
    this.dialogObserver.add(this.onDialog.bind(this));

    this.topWindow = this.browsingContext.top.embedderElement?.ownerGlobal;
    this.topWindow?.addEventListener("TabClose", _onTabClose);
  }

  dialogOpenedPromise() {
    return new Promise(resolve => {
      this._resolveDialogOpened = resolve;
    });
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

  didDestroy() {
    this.dialogObserver.remove(this.onDialog);
    this.dialogObserver.unregister();

    this.topWindow?.removeEventListener("TabClose", _onTabClose);
  }

  onDialog(action, dialogRef, win) {
    if (
      this._resolveDialogOpened &&
      action == "opened" &&
      win == this.browsingContext.topChromeWindow
    ) {
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
 * Clear all the entries from the element id cache.
 */
function clearElementIdCache() {
  elementIdCache.clear();
}

function _onTabClose(event) {
  elementIdCache.clear(event.target.linkedBrowser.browsingContext);
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
              // TODO: Scenarios where the window/tab got closed and
              // currentWindowGlobal is null will be handled in Bug 1662808.
              const actor = browsingContextFn().currentWindowGlobal.getActor(
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
                return null;
              }

              if (++attempts > MAX_ATTEMPTS) {
                const browsingContextId = browsingContextFn()?.id;
                logger.trace(
                  `[${browsingContextId}] Querying "${methodName} "` +
                    `reached the limit of retry attempts (${MAX_ATTEMPTS})`
                );
                throw e;
              }

              logger.trace(`Retrying "${methodName}", attempt: ${attempts}`);
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
          "chrome://marionette/content/actors/MarionetteCommandsParent.jsm",
      },
      child: {
        moduleURI:
          "chrome://marionette/content/actors/MarionetteCommandsChild.jsm",
      },

      allFrames: true,
      includeChrome: true,
    });
  } catch (e) {
    if (e.name === "NotSupportedError") {
      logger.warn(`MarionetteCommands actor is already registered!`);
    } else {
      throw e;
    }
  }
}

function unregisterCommandsActor() {
  ChromeUtils.unregisterWindowActor("MarionetteCommands");
}
