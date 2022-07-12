/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["MarionetteReftestChild"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  Log: "chrome://remote/content/shared/Log.jsm",
});

XPCOMUtils.defineLazyGetter(lazy, "logger", () =>
  lazy.Log.get(lazy.Log.TYPES.MARIONETTE)
);

/**
 * Child JSWindowActor to handle navigation for reftests relying on marionette.
 */
class MarionetteReftestChild extends JSWindowActorChild {
  constructor() {
    super();

    // This promise will resolve with the URL recorded in the "load" event
    // handler. This URL will not be impacted by any hash modification that
    // might be performed by the test script.
    // The harness should be loaded before loading any test page, so the actors
    // should be registered before the "load" event is received for a test page.
    this._loadedURLPromise = new Promise(
      r => (this._resolveLoadedURLPromise = r)
    );
  }

  handleEvent(event) {
    if (event.type == "load") {
      const url = event.target.location.href;
      lazy.logger.debug(`Handle load event with URL ${url}`);
      this._resolveLoadedURLPromise(url);
    }
  }

  actorCreated() {
    lazy.logger.trace(
      `[${this.browsingContext.id}] Reftest actor created ` +
        `for window id ${this.manager.innerWindowId}`
    );
  }

  async receiveMessage(msg) {
    const { name, data } = msg;

    let result;
    switch (name) {
      case "MarionetteReftestParent:flushRendering":
        result = await this.flushRendering(data);
        break;
      case "MarionetteReftestParent:reftestWait":
        result = await this.reftestWait(data);
        break;
    }
    return result;
  }

  /**
   * Wait for a reftest page to be ready for screenshots:
   * - wait for the loadedURL to be available (see handleEvent)
   * - check if the URL matches the expected URL
   * - if present, wait for the "reftest-wait" classname to be removed from the
   *   document element
   *
   * @param {Object} options
   * @param {String} options.url
   *        The expected test page URL
   * @param {Boolean} options.useRemote
   *        True when using e10s
   * @return {Boolean}
   *         Returns true when the correct page is loaded and ready for
   *         screenshots. Returns false if the page loaded bug does not have the
   *         expected URL.
   */
  async reftestWait(options = {}) {
    const { url, useRemote } = options;
    const loadedURL = await this._loadedURLPromise;
    if (loadedURL !== url) {
      lazy.logger.debug(
        `Window URL does not match the expected URL "${loadedURL}" !== "${url}"`
      );
      return false;
    }

    const documentElement = this.document.documentElement;
    const hasReftestWait = documentElement.classList.contains("reftest-wait");

    lazy.logger.debug("Waiting for event loop to spin");
    await new Promise(resolve =>
      this.document.defaultView.setTimeout(resolve, 0)
    );

    await this.paintComplete({ useRemote, ignoreThrottledAnimations: true });

    if (hasReftestWait) {
      const event = new this.document.defaultView.Event("TestRendered", {
        bubbles: true,
      });
      documentElement.dispatchEvent(event);
      lazy.logger.info("Emitted TestRendered event");
      await this.reftestWaitRemoved();
      await this.paintComplete({ useRemote, ignoreThrottledAnimations: false });
    }
    if (
      this.document.defaultView.innerWidth < documentElement.scrollWidth ||
      this.document.defaultView.innerHeight < documentElement.scrollHeight
    ) {
      lazy.logger.warn(
        `${url} overflows viewport (width: ${documentElement.scrollWidth}, height: ${documentElement.scrollHeight})`
      );
    }
    return true;
  }

  paintComplete({ useRemote, ignoreThrottledAnimations }) {
    lazy.logger.debug("Waiting for rendering");
    let windowUtils = this.document.defaultView.windowUtils;
    return new Promise(resolve => {
      let maybeResolve = () => {
        this.flushRendering({ ignoreThrottledAnimations });
        if (useRemote) {
          // Flush display (paint)
          lazy.logger.debug("Force update of layer tree");
          windowUtils.updateLayerTree();
        }

        if (windowUtils.isMozAfterPaintPending) {
          lazy.logger.debug("isMozAfterPaintPending: true");
          this.document.defaultView.addEventListener(
            "MozAfterPaint",
            maybeResolve,
            {
              once: true,
            }
          );
        } else {
          // resolve at the start of the next frame in case of leftover paints
          lazy.logger.debug("isMozAfterPaintPending: false");
          this.document.defaultView.requestAnimationFrame(() => {
            this.document.defaultView.requestAnimationFrame(resolve);
          });
        }
      };
      maybeResolve();
    });
  }

  reftestWaitRemoved() {
    lazy.logger.debug("Waiting for reftest-wait removal");
    return new Promise(resolve => {
      const documentElement = this.document.documentElement;
      let observer = new this.document.defaultView.MutationObserver(() => {
        if (!documentElement.classList.contains("reftest-wait")) {
          observer.disconnect();
          lazy.logger.debug("reftest-wait removed");
          this.document.defaultView.setTimeout(resolve, 0);
        }
      });
      if (documentElement.classList.contains("reftest-wait")) {
        observer.observe(documentElement, { attributes: true });
      } else {
        this.document.defaultView.setTimeout(resolve, 0);
      }
    });
  }

  /**
   * Ensure layout is flushed in each frame
   *
   * @param {Object} options
   * @param {Boolean} options.ignoreThrottledAnimations Don't flush
   *        the layout of throttled animations. We can end up in a
   *        situation where flushing a throttled animation causes
   *        mozAfterPaint events even when all rendering we care about
   *        should have ceased. See
   *        https://searchfox.org/mozilla-central/rev/d58860eb739af613774c942c3bb61754123e449b/layout/tools/reftest/reftest-content.js#723-729
   *        for more detail.
   */
  flushRendering(options = {}) {
    let { ignoreThrottledAnimations } = options;
    lazy.logger.debug(
      `flushRendering ignoreThrottledAnimations:${ignoreThrottledAnimations}`
    );
    let anyPendingPaintsGeneratedInDescendants = false;

    let windowUtils = this.document.defaultView.windowUtils;

    function flushWindow(win) {
      let utils = win.windowUtils;
      let afterPaintWasPending = utils.isMozAfterPaintPending;

      let root = win.document.documentElement;
      if (root) {
        try {
          if (ignoreThrottledAnimations) {
            utils.flushLayoutWithoutThrottledAnimations();
          } else {
            root.getBoundingClientRect();
          }
        } catch (e) {
          lazy.logger.error("flushWindow failed", e);
        }
      }

      if (!afterPaintWasPending && utils.isMozAfterPaintPending) {
        anyPendingPaintsGeneratedInDescendants = true;
      }

      for (let i = 0; i < win.frames.length; ++i) {
        // Skip remote frames, flushRendering will be called on their individual
        // MarionetteReftest actor via _recursiveFlushRendering performed from
        // the topmost MarionetteReftest actor.
        if (!Cu.isRemoteProxy(win.frames[i])) {
          flushWindow(win.frames[i]);
        }
      }
    }
    flushWindow(this.document.defaultView);

    if (
      anyPendingPaintsGeneratedInDescendants &&
      !windowUtils.isMozAfterPaintPending
    ) {
      lazy.logger.error(
        "Descendant frame generated a MozAfterPaint event, " +
          "but the root document doesn't have one!"
      );
    }
  }
}
