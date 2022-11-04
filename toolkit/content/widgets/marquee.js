/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/*
 * This is the class of entry. It will construct the actual implementation
 * according to the value of the "direction" property.
 */
this.MarqueeWidget = class {
  constructor(shadowRoot) {
    this.shadowRoot = shadowRoot;
    this.element = shadowRoot.host;
  }

  /*
   * Callback called by UAWidgets right after constructor.
   */
  onsetup() {
    this.switchImpl();
  }

  /*
   * Callback called by UAWidgetsChild wheen the direction property
   * changes.
   */
  onchange() {
    this.switchImpl();
  }

  switchImpl() {
    let newImpl;
    switch (this.element.direction) {
      case "up":
      case "down":
        newImpl = MarqueeVerticalImplWidget;
        break;
      case "left":
      case "right":
        newImpl = MarqueeHorizontalImplWidget;
        break;
    }

    // Skip if we are asked to load the same implementation.
    // This can happen if the property is set again w/o value change.
    if (this.impl && this.impl.constructor == newImpl) {
      return;
    }
    this.teardown();
    if (newImpl) {
      this.impl = new newImpl(this.shadowRoot);
      this.impl.onsetup();
    }
  }

  teardown() {
    if (!this.impl) {
      return;
    }
    this.impl.teardown();
    this.shadowRoot.firstChild.remove();
    delete this.impl;
  }
};

this.MarqueeBaseImplWidget = class {
  constructor(shadowRoot) {
    this.shadowRoot = shadowRoot;
    this.element = shadowRoot.host;
    this.document = this.element.ownerDocument;
    this.window = this.document.defaultView;
  }

  onsetup() {
    this.generateContent();

    // Set up state.
    this._currentDirection = this.element.direction || "left";
    this._currentLoop = this.element.loop;
    this.dirsign = 1;
    this.startAt = 0;
    this.stopAt = 0;
    this.newPosition = 0;
    this.runId = 0;
    this.originalHeight = 0;
    this.invalidateCache = true;

    this._mutationObserver = new this.window.MutationObserver(aMutations =>
      this._mutationActor(aMutations)
    );
    this._mutationObserver.observe(this.element, {
      attributes: true,
      attributeOldValue: true,
      attributeFilter: ["loop", "", "behavior", "direction", "width", "height"],
    });

    // init needs to be run after the page has loaded in order to calculate
    // the correct height/width
    if (this.document.readyState == "complete") {
      this.init();
    } else {
      this.window.addEventListener("load", this, { once: true });
    }

    this.shadowRoot.addEventListener("marquee-start", this);
    this.shadowRoot.addEventListener("marquee-stop", this);
  }

  teardown() {
    this._mutationObserver.disconnect();
    this.window.clearTimeout(this.runId);

    this.window.removeEventListener("load", this);
    this.shadowRoot.removeEventListener("marquee-start", this);
    this.shadowRoot.removeEventListener("marquee-stop", this);
  }

  handleEvent(aEvent) {
    if (!aEvent.isTrusted) {
      return;
    }

    switch (aEvent.type) {
      case "load":
        this.init();
        break;
      case "marquee-start":
        this.doStart();
        break;
      case "marquee-stop":
        this.doStop();
        break;
    }
  }

  get outerDiv() {
    return this.shadowRoot.firstChild;
  }

  get innerDiv() {
    return this.shadowRoot.getElementById("innerDiv");
  }

  get scrollDelayWithTruespeed() {
    if (this.element.scrollDelay < 60 && !this.element.trueSpeed) {
      return 60;
    }
    return this.element.scrollDelay;
  }

  doStart() {
    if (this.runId == 0) {
      var lambda = () => this._doMove(false);
      this.runId = this.window.setTimeout(
        lambda,
        this.scrollDelayWithTruespeed - this._deltaStartStop
      );
      this._deltaStartStop = 0;
    }
  }

  doStop() {
    if (this.runId != 0) {
      this._deltaStartStop = Date.now() - this._lastMoveDate;
      this.window.clearTimeout(this.runId);
    }

    this.runId = 0;
  }

  _fireEvent(aName, aBubbles, aCancelable) {
    var e = this.document.createEvent("Events");
    e.initEvent(aName, aBubbles, aCancelable);
    this.element.dispatchEvent(e);
  }

  _doMove(aResetPosition) {
    this._lastMoveDate = Date.now();

    // invalidateCache is true at first load and whenever an attribute
    // is changed
    if (this.invalidateCache) {
      this.invalidateCache = false; // we only want this to run once every scroll direction change

      var corrvalue = 0;

      switch (this._currentDirection) {
        case "up":
        case "down": {
          let height = this.window.getComputedStyle(this.element).height;
          this.outerDiv.style.height = height;
          if (this.originalHeight > this.outerDiv.offsetHeight) {
            corrvalue = this.originalHeight - this.outerDiv.offsetHeight;
          }
          this.innerDiv.style.padding = height + " 0";
          let isUp = this._currentDirection == "up";
          if (isUp) {
            this.dirsign = 1;
            this.startAt =
              this.element.behavior == "alternate"
                ? this.originalHeight - corrvalue
                : 0;
            this.stopAt =
              this.element.behavior == "alternate" ||
              this.element.behavior == "slide"
                ? parseInt(height) + corrvalue
                : this.originalHeight + parseInt(height);
          } else {
            this.dirsign = -1;
            this.startAt =
              this.element.behavior == "alternate"
                ? parseInt(height) + corrvalue
                : this.originalHeight + parseInt(height);
            this.stopAt =
              this.element.behavior == "alternate" ||
              this.element.behavior == "slide"
                ? this.originalHeight - corrvalue
                : 0;
          }
          break;
        }
        case "left":
        case "right":
        default: {
          let isRight = this._currentDirection == "right";
          // NOTE: It's important to use getComputedStyle() to not account for the padding.
          let innerWidth = parseInt(
            this.window.getComputedStyle(this.innerDiv).width
          );
          if (innerWidth > this.outerDiv.offsetWidth) {
            corrvalue = innerWidth - this.outerDiv.offsetWidth;
          }
          let rtl =
            this.window.getComputedStyle(this.element).direction == "rtl";
          if (isRight != rtl) {
            this.dirsign = -1;
            this.stopAt =
              this.element.behavior == "alternate" ||
              this.element.behavior == "slide"
                ? innerWidth - corrvalue
                : 0;
            this.startAt =
              this.outerDiv.offsetWidth +
              (this.element.behavior == "alternate"
                ? corrvalue
                : innerWidth + this.stopAt);
          } else {
            this.dirsign = 1;
            this.startAt =
              this.element.behavior == "alternate" ? innerWidth - corrvalue : 0;
            this.stopAt =
              this.outerDiv.offsetWidth +
              (this.element.behavior == "alternate" ||
              this.element.behavior == "slide"
                ? corrvalue
                : innerWidth + this.startAt);
          }
          if (rtl) {
            this.startAt = -this.startAt;
            this.stopAt = -this.stopAt;
            this.dirsign = -this.dirsign;
          }
          break;
        }
      }

      if (aResetPosition) {
        this.newPosition = this.startAt;
        this._fireEvent("start", false, false);
      }
    } // end if

    this.newPosition =
      this.newPosition + this.dirsign * this.element.scrollAmount;

    if (
      (this.dirsign == 1 && this.newPosition > this.stopAt) ||
      (this.dirsign == -1 && this.newPosition < this.stopAt)
    ) {
      switch (this.element.behavior) {
        case "alternate":
          // lets start afresh
          this.invalidateCache = true;

          // swap direction
          const swap = { left: "right", down: "up", up: "down", right: "left" };
          this._currentDirection = swap[this._currentDirection] || "left";
          this.newPosition = this.stopAt;

          if (
            this._currentDirection == "up" ||
            this._currentDirection == "down"
          ) {
            this.outerDiv.scrollTop = this.newPosition;
          } else {
            this.outerDiv.scrollLeft = this.newPosition;
          }

          if (this._currentLoop != 1) {
            this._fireEvent("bounce", false, true);
          }
          break;

        case "slide":
          if (this._currentLoop > 1) {
            this.newPosition = this.startAt;
          }
          break;

        default:
          this.newPosition = this.startAt;

          if (
            this._currentDirection == "up" ||
            this._currentDirection == "down"
          ) {
            this.outerDiv.scrollTop = this.newPosition;
          } else {
            this.outerDiv.scrollLeft = this.newPosition;
          }

          // dispatch start event, even when this._currentLoop == 1, comp. with IE6
          this._fireEvent("start", false, false);
      }

      if (this._currentLoop > 1) {
        this._currentLoop--;
      } else if (this._currentLoop == 1) {
        if (
          this._currentDirection == "up" ||
          this._currentDirection == "down"
        ) {
          this.outerDiv.scrollTop = this.stopAt;
        } else {
          this.outerDiv.scrollLeft = this.stopAt;
        }
        this.element.stop();
        this._fireEvent("finish", false, true);
        return;
      }
    } else if (
      this._currentDirection == "up" ||
      this._currentDirection == "down"
    ) {
      this.outerDiv.scrollTop = this.newPosition;
    } else {
      this.outerDiv.scrollLeft = this.newPosition;
    }

    var myThis = this;
    var lambda = function myTimeOutFunction() {
      myThis._doMove(false);
    };
    this.runId = this.window.setTimeout(lambda, this.scrollDelayWithTruespeed);
  }

  init() {
    this.element.stop();

    if (this._currentDirection == "up" || this._currentDirection == "down") {
      // store the original height before we add padding
      this.innerDiv.style.padding = 0;
      this.originalHeight = this.innerDiv.offsetHeight;
    }

    this._doMove(true);
  }

  _mutationActor(aMutations) {
    while (aMutations.length) {
      var mutation = aMutations.shift();
      var attrName = mutation.attributeName.toLowerCase();
      var oldValue = mutation.oldValue;
      var target = mutation.target;
      var newValue = target.getAttribute(attrName);

      if (oldValue != newValue) {
        this.invalidateCache = true;
        switch (attrName) {
          case "loop":
            this._currentLoop = target.loop;
            break;
          case "direction":
            this._currentDirection = target.direction;
            break;
        }
      }
    }
  }
};

this.MarqueeHorizontalImplWidget = class extends MarqueeBaseImplWidget {
  generateContent() {
    // White-space isn't allowed because a marquee could be
    // inside 'white-space: pre'
    this.shadowRoot.innerHTML = `<div class="outerDiv horizontal"
        ><link rel="stylesheet" href="chrome://global/content/elements/marquee.css"
          /><div class="innerDiv" id="innerDiv"
            ><slot
          /></div
      ></div>`;
  }
};

this.MarqueeVerticalImplWidget = class extends MarqueeBaseImplWidget {
  generateContent() {
    // White-space isn't allowed because a marquee could be
    // inside 'white-space: pre'
    this.shadowRoot.innerHTML = `<div class="outerDiv vertical"
        ><link rel="stylesheet" href="chrome://global/content/elements/marquee.css"
          /><div class="innerDiv" id="innerDiv"
            ><slot
          /></div
      ></div>`;
  }
};
