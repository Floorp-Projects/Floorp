/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

var EXPORTED_SYMBOLS = ["AutoScrollController"];

var AutoScrollController = {
  init: function init() {
    this._scrollable = null;
    this._scrolldir = "";
    this._startX = null;
    this._startY = null;
    this._screenX = null;
    this._screenY = null;
    this._lastFrame = null;
    this._autoscrollHandledByApz = false;
    this._scrollId = null;
    this.autoscrollLoop = this.autoscrollLoop.bind(this);

    Services.els.addSystemEventListener(global, "mousedown", this, true);

    addMessageListener("Autoscroll:Stop", this);
  },

  isAutoscrollBlocker(node) {
    let mmPaste = Services.prefs.getBoolPref("middlemouse.paste");
    let mmScrollbarPosition = Services.prefs.getBoolPref("middlemouse.scrollbarPosition");

    while (node) {
      if ((node instanceof content.HTMLAnchorElement || node instanceof content.HTMLAreaElement) &&
          node.hasAttribute("href")) {
        return true;
      }

      if (mmPaste && (node instanceof content.HTMLInputElement ||
                      node instanceof content.HTMLTextAreaElement)) {
        return true;
      }

      if (node instanceof content.XULElement && mmScrollbarPosition
          && (node.localName == "scrollbar" || node.localName == "scrollcorner")) {
        return true;
      }

      node = node.parentNode;
    }
    return false;
  },

  isScrollableElement(aNode) {
    if (aNode instanceof content.HTMLElement) {
      return !(aNode instanceof content.HTMLSelectElement) || aNode.multiple;
    }

    return aNode instanceof content.XULElement;
  },

  getXBLNodes(parent, array) {
    let anonNodes = content.document.getAnonymousNodes(parent);
    let nodes = Array.from(anonNodes || parent.childNodes || []);
    for (let node of nodes) {
      if (node.nodeName == "children") {
        return true;
      }
      if (this.getXBLNodes(node, array)) {
        array.push(node);
        return true;
      }
    }
    return false;
  },

  * parentNodeIterator(aNode) {
    while (aNode) {
      yield aNode;

      let parent = aNode.parentNode;
      if (parent && parent instanceof content.XULElement) {
        let anonNodes = content.document.getAnonymousNodes(parent);
        if (anonNodes && !Array.from(anonNodes).includes(aNode)) {
          // XBL elements are skipped by parentNode property.
          // Yield elements between parent and <children> here.
          let nodes = [];
          this.getXBLNodes(parent, nodes);
          for (let node of nodes) {
            yield node;
          }
        }
      }

      aNode = parent;
    }
  },

  findNearestScrollableElement(aNode) {
    // this is a list of overflow property values that allow scrolling
    const scrollingAllowed = ["scroll", "auto"];

    // go upward in the DOM and find any parent element that has a overflow
    // area and can therefore be scrolled
    this._scrollable = null;
    for (let node of this.parentNodeIterator(aNode)) {
      // do not use overflow based autoscroll for <html> and <body>
      // Elements or non-html/non-xul elements such as svg or Document nodes
      // also make sure to skip select elements that are not multiline
      if (!this.isScrollableElement(node)) {
        continue;
      }

      var overflowx = node.ownerGlobal
                          .getComputedStyle(node)
                          .getPropertyValue("overflow-x");
      var overflowy = node.ownerGlobal
                          .getComputedStyle(node)
                          .getPropertyValue("overflow-y");
      // we already discarded non-multiline selects so allow vertical
      // scroll for multiline ones directly without checking for a
      // overflow property
      var scrollVert = node.scrollTopMax &&
        (node instanceof content.HTMLSelectElement ||
         scrollingAllowed.includes(overflowy));

      // do not allow horizontal scrolling for select elements, it leads
      // to visual artifacts and is not the expected behavior anyway
      if (!(node instanceof content.HTMLSelectElement) &&
          node.scrollLeftMin != node.scrollLeftMax &&
          scrollingAllowed.includes(overflowx)) {
        this._scrolldir = scrollVert ? "NSEW" : "EW";
        this._scrollable = node;
        break;
      } else if (scrollVert) {
        this._scrolldir = "NS";
        this._scrollable = node;
        break;
      }
    }

    if (!this._scrollable) {
      this._scrollable = aNode.ownerGlobal;
      if (this._scrollable.scrollMaxX != this._scrollable.scrollMinX) {
        this._scrolldir = this._scrollable.scrollMaxY !=
                          this._scrollable.scrollMinY ? "NSEW" : "EW";
      } else if (this._scrollable.scrollMaxY != this._scrollable.scrollMinY) {
        this._scrolldir = "NS";
      } else if (this._scrollable.frameElement) {
        this.findNearestScrollableElement(this._scrollable.frameElement);
      } else {
        this._scrollable = null; // abort scrolling
      }
    }
  },

  startScroll(event) {

    this.findNearestScrollableElement(event.originalTarget);

    if (!this._scrollable)
      return;

    // In some configurations like Print Preview, content.performance
    // (which we use below) is null. Autoscrolling is broken in Print
    // Preview anyways (see bug 1393494), so just don't start it at all.
    if (!content.performance)
      return;

    let domUtils = content.QueryInterface(Ci.nsIInterfaceRequestor)
                          .getInterface(Ci.nsIDOMWindowUtils);
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
    let [result] = sendSyncMessage("Autoscroll:Start",
                                   {scrolldir: this._scrolldir,
                                    screenX: event.screenX,
                                    screenY: event.screenY,
                                    scrollId: this._scrollId,
                                    presShellId});
    if (!result.autoscrollEnabled) {
      this._scrollable = null;
      return;
    }

    Services.els.addSystemEventListener(global, "mousemove", this, true);
    addEventListener("pagehide", this, true);

    this._ignoreMouseEvents = true;
    this._startX = event.screenX;
    this._startY = event.screenY;
    this._screenX = event.screenX;
    this._screenY = event.screenY;
    this._scrollErrorX = 0;
    this._scrollErrorY = 0;
    this._autoscrollHandledByApz = result.usingApz;

    if (!result.usingApz) {
      // If the browser didn't hand the autoscroll off to APZ,
      // scroll here in the main thread.
      this.startMainThreadScroll();
    } else {
      // Even if the browser did hand the autoscroll to APZ,
      // APZ might reject it in which case it will notify us
      // and we need to take over.
      Services.obs.addObserver(this, "autoscroll-rejected-by-apz");
    }
  },

  startMainThreadScroll() {
    this._lastFrame = content.performance.now();
    content.requestAnimationFrame(this.autoscrollLoop);
  },

  stopScroll() {
    if (this._scrollable) {
      this._scrollable.mozScrollSnap();
      this._scrollable = null;

      Services.els.removeSystemEventListener(global, "mousemove", this, true);
      removeEventListener("pagehide", this, true);
      if (this._autoscrollHandledByApz) {
        Services.obs.removeObserver(this, "autoscroll-rejected-by-apz");
      }
    }
  },

  accelerate(curr, start) {
    const speed = 12;
    var val = (curr - start) / speed;

    if (val > 1)
      return val * Math.sqrt(val) - 1;
    if (val < -1)
      return val * Math.sqrt(-val) + 1;
    return 0;
  },

  roundToZero(num) {
    if (num > 0)
      return Math.floor(num);
    return Math.ceil(num);
  },

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
      this._scrollErrorY = (desiredScrollY - actualScrollY);
    }
    if (this._scrolldir != "NS") {
      var x = this.accelerate(this._screenX, this._startX) * timeCompensation;
      var desiredScrollX = this._scrollErrorX + x;
      actualScrollX = this.roundToZero(desiredScrollX);
      this._scrollErrorX = (desiredScrollX - actualScrollX);
    }

    this._scrollable.scrollBy({
      left: actualScrollX,
      top: actualScrollY,
      behavior: "instant"
    });

    content.requestAnimationFrame(this.autoscrollLoop);
  },

  handleEvent(event) {
    if (event.type == "mousemove") {
      this._screenX = event.screenX;
      this._screenY = event.screenY;
    } else if (event.type == "mousedown") {
      if (event.isTrusted &
          !event.defaultPrevented &&
          event.button == 1 &&
          !this._scrollable &&
          !this.isAutoscrollBlocker(event.originalTarget)) {
        this.startScroll(event);
      }
    } else if (event.type == "pagehide") {
      if (this._scrollable) {
        var doc =
          this._scrollable.ownerDocument || this._scrollable.document;
        if (doc == event.target) {
          sendAsyncMessage("Autoscroll:Cancel");
        }
      }
    }
  },

  receiveMessage(msg) {
    switch (msg.name) {
      case "Autoscroll:Stop": {
        this.stopScroll();
        break;
      }
    }
  },

  observe(subject, topic, data) {
    if (topic === "autoscroll-rejected-by-apz") {
      // The caller passes in the scroll id via 'data'.
      if (data == this._scrollId) {
        this._autoscrollHandledByApz = false;
        this.startMainThreadScroll();
        Services.obs.removeObserver(this, "autoscroll-rejected-by-apz");
      }
    }
  },
};
