/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { WindowGlobalBiDiModule } from "chrome://remote/content/webdriver-bidi/modules/WindowGlobalBiDiModule.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  AnimationFramePromise: "chrome://remote/content/shared/Sync.sys.mjs",
  ClipRectangleType:
    "chrome://remote/content/webdriver-bidi/modules/root/browsingContext.sys.mjs",
  LoadListener: "chrome://remote/content/shared/listeners/LoadListener.sys.mjs",
  OriginType:
    "chrome://remote/content/webdriver-bidi/modules/root/browsingContext.sys.mjs",
});

class BrowsingContextModule extends WindowGlobalBiDiModule {
  #loadListener;
  #subscribedEvents;

  constructor(messageHandler) {
    super(messageHandler);

    // Setup the LoadListener as early as possible.
    this.#loadListener = new lazy.LoadListener(this.messageHandler.window);
    this.#loadListener.on("DOMContentLoaded", this.#onDOMContentLoaded);
    this.#loadListener.on("load", this.#onLoad);

    // Set of event names which have active subscriptions.
    this.#subscribedEvents = new Set();
  }

  destroy() {
    this.#loadListener.destroy();
    this.#subscribedEvents = null;
  }

  #getNavigationInfo(data) {
    // Note: the navigation id is collected in the parent-process and will be
    // added via event interception by the windowglobal-in-root module.
    return {
      context: this.messageHandler.context,
      timestamp: Date.now(),
      url: data.target.URL,
    };
  }

  #getOriginRectangle(origin) {
    const win = this.messageHandler.window;

    if (origin === lazy.OriginType.viewport) {
      const viewport = win.visualViewport;
      // Until it's clarified in the scope of the issue:
      // https://github.com/w3c/webdriver-bidi/issues/592
      // if we should take into account scrollbar dimensions, when calculating
      // the viewport size, we match the behavior of WebDriver Classic,
      // meaning we include scrollbar dimensions.
      return new DOMRect(
        viewport.pageLeft,
        viewport.pageTop,
        win.innerWidth,
        win.innerHeight
      );
    }

    const documentElement = win.document.documentElement;
    return new DOMRect(
      0,
      0,
      documentElement.scrollWidth,
      documentElement.scrollHeight
    );
  }

  #startListening() {
    if (this.#subscribedEvents.size == 0) {
      this.#loadListener.startListening();
    }
  }

  #stopListening() {
    if (this.#subscribedEvents.size == 0) {
      this.#loadListener.stopListening();
    }
  }

  #subscribeEvent(event) {
    switch (event) {
      case "browsingContext._documentInteractive":
        this.#startListening();
        this.#subscribedEvents.add("browsingContext._documentInteractive");
        break;
      case "browsingContext.domContentLoaded":
        this.#startListening();
        this.#subscribedEvents.add("browsingContext.domContentLoaded");
        break;
      case "browsingContext.load":
        this.#startListening();
        this.#subscribedEvents.add("browsingContext.load");
        break;
    }
  }

  #unsubscribeEvent(event) {
    switch (event) {
      case "browsingContext._documentInteractive":
        this.#subscribedEvents.delete("browsingContext._documentInteractive");
        break;
      case "browsingContext.domContentLoaded":
        this.#subscribedEvents.delete("browsingContext.domContentLoaded");
        break;
      case "browsingContext.load":
        this.#subscribedEvents.delete("browsingContext.load");
        break;
    }

    this.#stopListening();
  }

  #onDOMContentLoaded = (eventName, data) => {
    if (this.#subscribedEvents.has("browsingContext._documentInteractive")) {
      this.messageHandler.emitEvent("browsingContext._documentInteractive", {
        baseURL: data.target.baseURI,
        contextId: this.messageHandler.contextId,
        documentURL: data.target.URL,
        innerWindowId: this.messageHandler.innerWindowId,
        readyState: data.target.readyState,
      });
    }

    if (this.#subscribedEvents.has("browsingContext.domContentLoaded")) {
      this.emitEvent(
        "browsingContext.domContentLoaded",
        this.#getNavigationInfo(data)
      );
    }
  };

  #onLoad = (eventName, data) => {
    if (this.#subscribedEvents.has("browsingContext.load")) {
      this.emitEvent("browsingContext.load", this.#getNavigationInfo(data));
    }
  };

  /**
   * Normalize rectangle. This ensures that the resulting rect has
   * positive width and height dimensions.
   *
   * @see https://w3c.github.io/webdriver-bidi/#normalise-rect
   *
   * @param {DOMRect} rect
   *     An object which describes the size and position of a rectangle.
   *
   * @returns {DOMRect} Normalized rectangle.
   */
  #normalizeRect(rect) {
    let { x, y, width, height } = rect;

    if (width < 0) {
      x += width;
      width = -width;
    }

    if (height < 0) {
      y += height;
      height = -height;
    }

    return new DOMRect(x, y, width, height);
  }

  /**
   * Create a new rectangle which will be an intersection of
   * rectangles specified as arguments.
   *
   * @see https://w3c.github.io/webdriver-bidi/#rectangle-intersection
   *
   * @param {DOMRect} rect1
   *     An object which describes the size and position of a rectangle.
   * @param {DOMRect} rect2
   *     An object which describes the size and position of a rectangle.
   *
   * @returns {DOMRect} Rectangle, representing an intersection of <var>rect1</var> and <var>rect2</var>.
   */
  #rectangleIntersection(rect1, rect2) {
    rect1 = this.#normalizeRect(rect1);
    rect2 = this.#normalizeRect(rect2);

    const x_min = Math.max(rect1.x, rect2.x);
    const x_max = Math.min(rect1.x + rect1.width, rect2.x + rect2.width);

    const y_min = Math.max(rect1.y, rect2.y);
    const y_max = Math.min(rect1.y + rect1.height, rect2.y + rect2.height);

    const width = Math.max(x_max - x_min, 0);
    const height = Math.max(y_max - y_min, 0);

    return new DOMRect(x_min, y_min, width, height);
  }

  /**
   * Internal commands
   */

  _applySessionData(params) {
    // TODO: Bug 1775231. Move this logic to a shared module or an abstract
    // class.
    const { category } = params;
    if (category === "event") {
      const filteredSessionData = params.sessionData.filter(item =>
        this.messageHandler.matchesContext(item.contextDescriptor)
      );
      for (const event of this.#subscribedEvents.values()) {
        const hasSessionItem = filteredSessionData.some(
          item => item.value === event
        );
        // If there are no session items for this context, we should unsubscribe from the event.
        if (!hasSessionItem) {
          this.#unsubscribeEvent(event);
        }
      }

      // Subscribe to all events, which have an item in SessionData.
      for (const { value } of filteredSessionData) {
        this.#subscribeEvent(value);
      }
    }
  }

  /**
   * Waits until the viewport has reached the new dimensions.
   *
   * @param {object} options
   * @param {number} options.height
   *     Expected height the viewport will resize to.
   * @param {number} options.width
   *     Expected width the viewport will resize to.
   *
   * @returns {Promise}
   *     Promise that resolves when the viewport has been resized.
   */
  async _awaitViewportDimensions(options) {
    const { height, width } = options;

    const win = this.messageHandler.window;
    let resized;

    // Updates for background tabs are throttled, and we also have to make
    // sure that the new browser dimensions have been received by the content
    // process. As such wait for the next animation frame.
    await lazy.AnimationFramePromise(win);

    const checkBrowserSize = () => {
      if (win.innerWidth === width && win.innerHeight === height) {
        resized();
      }
    };

    return new Promise(resolve => {
      resized = resolve;

      win.addEventListener("resize", checkBrowserSize);

      // Trigger a layout flush in case none happened yet.
      checkBrowserSize();
    }).finally(() => {
      win.removeEventListener("resize", checkBrowserSize);
    });
  }

  _getBaseURL() {
    return this.messageHandler.window.document.baseURI;
  }

  _getScreenshotRect(params = {}) {
    const { clip, origin } = params;

    const originRect = this.#getOriginRectangle(origin);
    let clipRect = originRect;

    if (clip !== null) {
      switch (clip.type) {
        case lazy.ClipRectangleType.Box: {
          clipRect = new DOMRect(
            clip.x + originRect.x,
            clip.y + originRect.y,
            clip.width,
            clip.height
          );
          break;
        }

        case lazy.ClipRectangleType.Element: {
          const realm = this.messageHandler.getRealm();
          const element = this.deserialize(realm, clip.element);
          const viewportRect = this.#getOriginRectangle(
            lazy.OriginType.viewport
          );
          const elementRect = element.getBoundingClientRect();

          clipRect = new DOMRect(
            elementRect.x + viewportRect.x,
            elementRect.y + viewportRect.y,
            elementRect.width,
            elementRect.height
          );
          break;
        }
      }
    }

    return this.#rectangleIntersection(originRect, clipRect);
  }
}

export const browsingContext = BrowsingContextModule;
