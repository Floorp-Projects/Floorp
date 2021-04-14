/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

var EXPORTED_SYMBOLS = ["AutoScrollChild"];

class AutoScrollChild extends JSWindowActorChild {
  constructor() {
    super();

    this._scrollable = null;
    this._scrolldir = "";
    this._startX = null;
    this._startY = null;
    this._screenX = null;
    this._screenY = null;
    this._lastFrame = null;
    this._autoscrollHandledByApz = false;
    this._scrollId = null;

    this.observer = new AutoScrollObserver(this);
    this.autoscrollLoop = this.autoscrollLoop.bind(this);
  }

  isAutoscrollBlocker(node) {
    let mmPaste = Services.prefs.getBoolPref("middlemouse.paste");
    let mmScrollbarPosition = Services.prefs.getBoolPref(
      "middlemouse.scrollbarPosition"
    );
    let content = node.ownerGlobal;

    while (node) {
      if (
        (node instanceof content.HTMLAnchorElement ||
          node instanceof content.HTMLAreaElement) &&
        node.hasAttribute("href")
      ) {
        return true;
      }

      if (
        mmPaste &&
        (node instanceof content.HTMLInputElement ||
          node instanceof content.HTMLTextAreaElement)
      ) {
        return true;
      }

      if (
        node instanceof content.XULElement &&
        ((mmScrollbarPosition &&
          (node.localName == "scrollbar" ||
            node.localName == "scrollcorner")) ||
          node.localName == "treechildren")
      ) {
        return true;
      }

      node = node.parentNode;
    }
    return false;
  }

  isScrollableElement(aNode) {
    let content = aNode.ownerGlobal;
    if (aNode instanceof content.HTMLElement) {
      return !(aNode instanceof content.HTMLSelectElement) || aNode.multiple;
    }

    return aNode instanceof content.XULElement;
  }

  computeWindowScrollDirection(global) {
    if (!global.scrollbars.visible) {
      return null;
    }
    if (global.scrollMaxX != global.scrollMinX) {
      return global.scrollMaxY != global.scrollMinY ? "NSEW" : "EW";
    }
    if (global.scrollMaxY != global.scrollMinY) {
      return "NS";
    }
    return null;
  }

  computeNodeScrollDirection(node) {
    if (!this.isScrollableElement(node)) {
      return null;
    }

    let global = node.ownerGlobal;

    // this is a list of overflow property values that allow scrolling
    const scrollingAllowed = ["scroll", "auto"];

    let cs = global.getComputedStyle(node);
    let overflowx = cs.getPropertyValue("overflow-x");
    let overflowy = cs.getPropertyValue("overflow-y");
    // we already discarded non-multiline selects so allow vertical
    // scroll for multiline ones directly without checking for a
    // overflow property
    let scrollVert =
      node.scrollTopMax &&
      (node instanceof global.HTMLSelectElement ||
        scrollingAllowed.includes(overflowy));

    // do not allow horizontal scrolling for select elements, it leads
    // to visual artifacts and is not the expected behavior anyway
    if (
      !(node instanceof global.HTMLSelectElement) &&
      node.scrollLeftMin != node.scrollLeftMax &&
      scrollingAllowed.includes(overflowx)
    ) {
      return scrollVert ? "NSEW" : "EW";
    }

    if (scrollVert) {
      return "NS";
    }

    return null;
  }

  findNearestScrollableElement(aNode) {
    // go upward in the DOM and find any parent element that has a overflow
    // area and can therefore be scrolled
    this._scrollable = null;
    for (let node = aNode; node; node = node.flattenedTreeParentNode) {
      // do not use overflow based autoscroll for <html> and <body>
      // Elements or non-html/non-xul elements such as svg or Document nodes
      // also make sure to skip select elements that are not multiline
      let direction = this.computeNodeScrollDirection(node);
      if (direction) {
        this._scrolldir = direction;
        this._scrollable = node;
        break;
      }
    }

    if (!this._scrollable) {
      let direction = this.computeWindowScrollDirection(aNode.ownerGlobal);
      if (direction) {
        this._scrolldir = direction;
        this._scrollable = aNode.ownerGlobal;
      } else if (aNode.ownerGlobal.frameElement) {
        // Note, in case of out of process iframes frameElement is null, and
        // a caller is supposed to communicate to iframe's parent on its own to
        // support cross process scrolling.
        this.findNearestScrollableElement(aNode.ownerGlobal.frameElement);
      }
    }
  }

  async startScroll(event) {
    this.findNearestScrollableElement(event.originalTarget);
    if (!this._scrollable) {
      this.sendAsyncMessage("Autoscroll:MaybeStartInParent", {
        browsingContextId: this.browsingContext.id,
        screenX: event.screenX,
        screenY: event.screenY,
      });
      return;
    }

    let content = event.originalTarget.ownerGlobal;

    // In some configurations like Print Preview, content.performance
    // (which we use below) is null. Autoscrolling is broken in Print
    // Preview anyways (see bug 1393494), so just don't start it at all.
    if (!content.performance) {
      return;
    }

    let domUtils = content.windowUtils;
    let scrollable = this._scrollable;
    if (scrollable instanceof Ci.nsIDOMWindow) {
      // getViewId() needs an element to operate on.
      scrollable = scrollable.document.documentElement;
    }
    this._scrollId = null;
    try {
      this._scrollId = domUtils.getViewId(scrollable);
    } catch (e) {
      // No view ID - leave this._scrollId as null. Receiving side will check.
    }
    let presShellId = domUtils.getPresShellId();
    let { autoscrollEnabled, usingApz } = await this.sendQuery(
      "Autoscroll:Start",
      {
        scrolldir: this._scrolldir,
        screenX: event.screenX,
        screenY: event.screenY,
        scrollId: this._scrollId,
        presShellId,
        browsingContext: this.browsingContext,
      }
    );
    if (!autoscrollEnabled) {
      this._scrollable = null;
      return;
    }

    Services.els.addSystemEventListener(this.document, "mousemove", this, true);
    Services.els.addSystemEventListener(this.document, "mouseup", this, true);
    this.document.addEventListener("pagehide", this, true);

    this._startX = event.screenX;
    this._startY = event.screenY;
    this._screenX = event.screenX;
    this._screenY = event.screenY;
    this._scrollErrorX = 0;
    this._scrollErrorY = 0;
    this._autoscrollHandledByApz = usingApz;

    if (!usingApz) {
      // If the browser didn't hand the autoscroll off to APZ,
      // scroll here in the main thread.
      this.startMainThreadScroll();
    } else {
      // Even if the browser did hand the autoscroll to APZ,
      // APZ might reject it in which case it will notify us
      // and we need to take over.
      Services.obs.addObserver(this.observer, "autoscroll-rejected-by-apz");
    }

    if (Cu.isInAutomation) {
      Services.obs.notifyObservers(content, "autoscroll-start");
    }
  }

  startMainThreadScroll() {
    let content = this.document.defaultView;
    this._lastFrame = content.performance.now();
    content.requestAnimationFrame(this.autoscrollLoop);

    const kAutoscroll = 15; // defined in mozilla/layers/ScrollInputMethods.h
    Services.telemetry
      .getHistogramById("SCROLL_INPUT_METHODS")
      .add(kAutoscroll);
  }

  stopScroll() {
    if (this._scrollable) {
      this._scrollable.mozScrollSnap();
      this._scrollable = null;

      Services.els.removeSystemEventListener(
        this.document,
        "mousemove",
        this,
        true
      );
      Services.els.removeSystemEventListener(
        this.document,
        "mouseup",
        this,
        true
      );
      this.document.removeEventListener("pagehide", this, true);
      if (this._autoscrollHandledByApz) {
        Services.obs.removeObserver(
          this.observer,
          "autoscroll-rejected-by-apz"
        );
      }
    }
  }

  accelerate(curr, start) {
    const speed = 12;
    var val = (curr - start) / speed;

    if (val > 1) {
      return val * Math.sqrt(val) - 1;
    }
    if (val < -1) {
      return val * Math.sqrt(-val) + 1;
    }
    return 0;
  }

  roundToZero(num) {
    if (num > 0) {
      return Math.floor(num);
    }
    return Math.ceil(num);
  }

  autoscrollLoop(timestamp) {
    if (!this._scrollable) {
      // Scrolling has been canceled
      return;
    }

    // avoid long jumps when the browser hangs for more than
    // |maxTimeDelta| ms
    const maxTimeDelta = 100;
    var timeDelta = Math.min(maxTimeDelta, timestamp - this._lastFrame);
    // we used to scroll |accelerate()| pixels every 20ms (50fps)
    var timeCompensation = timeDelta / 20;
    this._lastFrame = timestamp;

    var actualScrollX = 0;
    var actualScrollY = 0;
    // don't bother scrolling vertically when the scrolldir is only horizontal
    // and the other way around
    if (this._scrolldir != "EW") {
      var y = this.accelerate(this._screenY, this._startY) * timeCompensation;
      var desiredScrollY = this._scrollErrorY + y;
      actualScrollY = this.roundToZero(desiredScrollY);
      this._scrollErrorY = desiredScrollY - actualScrollY;
    }
    if (this._scrolldir != "NS") {
      var x = this.accelerate(this._screenX, this._startX) * timeCompensation;
      var desiredScrollX = this._scrollErrorX + x;
      actualScrollX = this.roundToZero(desiredScrollX);
      this._scrollErrorX = desiredScrollX - actualScrollX;
    }

    this._scrollable.scrollBy({
      left: actualScrollX,
      top: actualScrollY,
      behavior: "instant",
    });

    this._scrollable.ownerGlobal.requestAnimationFrame(this.autoscrollLoop);
  }

  handleEvent(event) {
    switch (event.type) {
      case "mousemove":
        this._screenX = event.screenX;
        this._screenY = event.screenY;
        break;
      case "mousedown":
        if (
          event.isTrusted &&
          !event.defaultPrevented &&
          event.button === 1 &&
          !event.clickEventPrevented() &&
          !this._scrollable &&
          !this.isAutoscrollBlocker(event.originalTarget)
        ) {
          this.startScroll(event);
        }
      // fallthrough
      case "mouseup":
        if (this._scrollable) {
          // Middle mouse click event shouldn't be fired in web content for
          // compatibility with Chrome.
          event.preventClickEvent();
        }
        break;
      case "pagehide":
        if (this._scrollable) {
          var doc = this._scrollable.ownerDocument || this._scrollable.document;
          if (doc == event.target) {
            this.sendAsyncMessage("Autoscroll:Cancel");
            this.stopScroll();
          }
        }
        break;
    }
  }

  receiveMessage(msg) {
    let data = msg.data;
    switch (msg.name) {
      case "Autoscroll:MaybeStart":
        for (let child of this.browsingContext.children) {
          if (data.browsingContextId == child.id) {
            this.startScroll({
              screenX: data.screenX,
              screenY: data.screenY,
              originalTarget: child.embedderElement,
            });
            break;
          }
        }
        break;
      case "Autoscroll:Stop": {
        this.stopScroll();
        break;
      }
    }
  }

  rejectedByApz(data) {
    // The caller passes in the scroll id via 'data'.
    if (data == this._scrollId) {
      this._autoscrollHandledByApz = false;
      this.startMainThreadScroll();
      Services.obs.removeObserver(this.observer, "autoscroll-rejected-by-apz");
    }
  }
}

class AutoScrollObserver {
  constructor(actor) {
    this.actor = actor;
  }

  observe(subject, topic, data) {
    if (topic === "autoscroll-rejected-by-apz") {
      this.actor.rejectedByApz(data);
    }
  }
}
