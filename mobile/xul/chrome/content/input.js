// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; js2-strict-trailing-comma-warning: nil -*-
/*
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Mobile Browser.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008, 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <stuart@mozilla.com>
 *   Brad Lassey <blassey@mozilla.com>
 *   Mark Finkle <mfinkle@mozilla.com>
 *   Gavin Sharp <gavin.sharp@gmail.com>
 *   Ben Combee <combee@mozilla.com>
 *   Roy Frostig <rfrostig@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

Components.utils.import("resource://gre/modules/Geometry.jsm");

// Maximum delay in ms between the two taps of a double-tap
const kDoubleClickInterval = 400;

// Maximum distance in inches between the taps of a double-tap
const kDoubleClickRadius = 0.4;

// Amount of time to wait before tap is generate a mousemove 
const kOverTapWait = 150;

// Amount of time to wait before tap becomes long tap
const kLongTapWait = 500;

// maximum drag distance in inches while axis locking can still be reverted
const kAxisLockRevertThreshold = 0.8;

// Same as NS_EVENT_STATE_ACTIVE from nsIEventStateManager.h
const kStateActive = 0x00000001;

// After a drag begins, kinetic panning is stopped if the drag doesn't become
// a pan in 300 milliseconds.
const kStopKineticPanOnDragTimeout = 300;

// Min/max velocity of kinetic panning. This is in pixels/millisecond.
const kMinVelocity = 0.4;
const kMaxVelocity = 6;

/**
 * MouseModule
 *
 * Handles all touch-related input such as dragging and tapping.
 *
 * The Fennec chrome DOM tree has elements that are augmented dynamically with
 * custom JS properties that tell the MouseModule they have custom support for
 * either dragging or clicking.  These JS properties are JS objects that expose
 * an interface supporting dragging or clicking (though currently we only look
 * to drag scrollable elements).
 *
 * A custom dragger is a JS property that lives on a scrollable DOM element,
 * accessible as myElement.customDragger.  The customDragger must support the
 * following interface:  (The `scroller' argument is given for convenience, and
 * is the object reference to the element's scrollbox object).
 *
 *   dragStart(cX, cY, target, scroller)
 *     Signals the beginning of a drag.  Coordinates are passed as
 *     client coordinates. target is copied from the event.
 *
 *   dragStop(dx, dy, scroller)
 *     Signals the end of a drag.  The dx, dy parameters may be non-zero to
 *     indicate one last drag movement.
 *
 *   dragMove(dx, dy, scroller, isKinetic)
 *     Signals an input attempt to drag by dx, dy.
 *
 * There is a default dragger in case a scrollable element is dragged --- see
 * the defaultDragger prototype property.
 */
function MouseModule() {
  this._dragData = new DragData();

  this._dragger = null;
  this._inputField = null;

  this._downUpEvents = [];
  this._targetScrollbox = null;
  this._targetScrollInterface = null;
  this._suppressNextMouseUp = false;

  this._kinetic = new KineticController(this._dragBy.bind(this),
                                        this._kineticStop.bind(this));

  this._singleClickTimeout = new Util.Timeout(this._doSingleClick.bind(this));
  this._mouseOverTimeout = new Util.Timeout(this._doMouseOver.bind(this));
  this._longClickTimeout = new Util.Timeout(this._doLongClick.bind(this));

  this._doubleClickRadius = Util.displayDPI * kDoubleClickRadius;

  window.addEventListener("mousedown", this, true);
  window.addEventListener("mouseup", this, true);
  window.addEventListener("mousemove", this, true);
  window.addEventListener("contextmenu", this, false);
  window.addEventListener("CancelTouchSequence", this, true);
}


MouseModule.prototype = {
  _initMouseEventFromEvent: function _initMouseEventFromEvent(aDestEvent, aSrcEvent, aType, aCanBubble, aCancellable) {
    aDestEvent.initMouseEvent(aType, aCanBubble, aCancellable, window, aSrcEvent.detail,
                              aSrcEvent.screenX, aSrcEvent.screenY, aSrcEvent.clientX, aSrcEvent.clientY,
                              aSrcEvent.ctrlKey, aSrcEvent.altKey, aSrcEvent.shiftKey, aSrcEvent.metaKey,
                              aSrcEvent.button, aSrcEvent.relatedTarget);
  },

  handleEvent: function handleEvent(aEvent) {
    switch (aEvent.type) {
      case "contextmenu":
        // bug 598965 - chrome UI should should stop to be pannable once the
        // context menu has appeared
        if (ContextHelper.popupState)
          this.cancelPending();
        break;
      case "CancelTouchSequence":
        this.cancelPending();
        break;

      default: {
        // Filter out mouse events that aren't first button
        if (aEvent.button !== 0)
          break;

        switch (aEvent.type) {
          case "mousedown":
            this._onMouseDown(aEvent);
            break;
          case "mousemove":
            aEvent.stopPropagation();
            aEvent.preventDefault();
            this._onMouseMove(aEvent);
            break;
          case "mouseup":
            this._onMouseUp(aEvent);
            break;
          case "click":
            aEvent.stopPropagation();
            aEvent.preventDefault();
            aEvent.target.removeEventListener("click", this, true);
            break;
        }
      }
    }
  },

  onBeforePaint: function onBeforePaint(aTimeStamp) {
    this._waitingForPaint = false;
  },

  /**
   * This gets invoked by the input handler if another module grabs.  We should
   * reset our state or something here.  This is probably doing the wrong thing
   * in its current form.
   */
  cancelPending: function cancelPending() {
    this._doDragStop();

    // Kinetic panning may have already been active or drag stop above may have
    // made kinetic panning active.
    this._kinetic.end();

    this._targetScrollbox = null;
    this._targetScrollInterface = null;
    this._inputField = null;

    this._cleanClickBuffer();
  },

  /** Begin possible pan and send tap down event. */
  _onMouseDown: function _onMouseDown(aEvent) {
    let dragData = this._dragData;
    if (dragData.dragging) {
      // Somehow a mouse up was missed.
      this._doDragStop();
    }
    dragData.reset();
    this.dX = 0;
    this.dY = 0;

    // walk up the DOM tree in search of nearest scrollable ancestor.  nulls are
    // returned if none found.
    let [targetScrollbox, targetScrollInterface, dragger]
      = ScrollUtils.getScrollboxFromElement(aEvent.originalTarget);

    // stop kinetic panning if targetScrollbox has changed
    if (this._kinetic.isActive() && this._dragger != dragger)
      this._kinetic.end();

    this._targetScrollbox = targetScrollInterface ? targetScrollInterface.element : targetScrollbox;
    this._targetScrollInterface = targetScrollInterface;

    // Do tap
    if (!this._kinetic.isActive()) {
      let event = document.createEvent("MouseEvent");
      this._initMouseEventFromEvent(event, aEvent, "TapDown", true, true);
      let success = aEvent.target.dispatchEvent(event);
      if (success) {
        this._recordEvent(aEvent);
        this._target = aEvent.target;
        this._mouseOverTimeout.once(kOverTapWait);
        this._longClickTimeout.once(kLongTapWait);
      } else {
        // cancel all pending content clicks
        this._cleanClickBuffer();
      }
    }

    // Do pan
    if (dragger) {
      let draggable = dragger.isDraggable(targetScrollbox, targetScrollInterface);
      dragData.locked = !draggable.x || !draggable.y;
      if (draggable.x || draggable.y) {
        this._dragger = dragger;
        this._doDragStart(aEvent, draggable);
      }
    }

    // When panning starts over an input field, focus should not change
    let inputField = this._getTargetInputField(aEvent.originalTarget);
    if (inputField && this._dragger) {
      this._inputField = inputField;
      aEvent.preventDefault();
      aEvent.stopPropagation();
    }
  },

  /** Send tap up event and any necessary full taps. */
  _onMouseUp: function _onMouseUp(aEvent) {
    if (this._suppressNextMouseUp) {
      this._suppressNextMouseUp = false;
      aEvent.stopPropagation();
      aEvent.preventDefault();
    }

    // onMouseMove will not record the delta change if we are waiting for a
    // paint. Since this is the last input for this drag, we override the flag.
    this._waitingForPaint = false;
    this._onMouseMove(aEvent);

    let dragData = this._dragData;
    this._doDragStop();

    // Do tap
    if (this._target) {
      let isClick = dragData.isClick();

      let event = document.createEvent("MouseEvents");
      this._initMouseEventFromEvent(event, aEvent, "TapUp", true, true);
      event.isClick = isClick;

      let success = aEvent.target.dispatchEvent(event);
      if (!success) {
        this._cleanClickBuffer();
      } else {
        this._recordEvent(aEvent);
        let commitToClicker = isClick && (this._downUpEvents.length > 1);
        if (commitToClicker)
          // commit this click to the doubleclick timewait buffer
          this._commitAnotherClick();
        else
          // clean the click buffer ourselves
          this._cleanClickBuffer();
      }
    }

    this._mouseOverTimeout.clear();
    this._longClickTimeout.clear();
    this._target = null;

    // Do pan
    if (dragData.isPan() && this._dragger) {
      // User was panning around, do not allow click
      let generatesClick = aEvent.detail;
      if (generatesClick)
        aEvent.target.addEventListener("click", this, true);
    }

    // Move the caret to the end of the target input field and focus it
    if (this._inputField && !this._dragData.isPan()) {
      let inputField = this._inputField;
      let textLength = inputField.textLength;
      inputField.setSelectionRange(textLength, textLength);
      inputField.focus();
    }
    this._inputField = null;
  },

  /**
   * If we're in a drag, do what we have to do to drag on.
   */
  _onMouseMove: function _onMouseMove(aEvent) {
    let dragData = this._dragData;

    if (dragData.dragging) {
      let oldIsPan = dragData.isPan();
      dragData.setDragPosition(aEvent.screenX, aEvent.screenY);

      // Kinetic panning is sensitive to time. It is more stable if it receives
      // the mousemove events as they come. For dragging though, we only want
      // to call _dragBy if we aren't waiting for a paint (so we don't spam the
      // main browser loop with a bunch of redundant paints).
      //
      // Here, we feed kinetic panning drag differences for mouse events as
      // come; for dragging, we build up a drag buffer in this.dX/this.dY and
      // release it when we are ready to paint.
      //
      let [sX, sY] = dragData.panPosition();
      this.dX += dragData.prevPanX - sX;
      this.dY += dragData.prevPanY - sY;

      if (dragData.isPan()) {
        this.sendMove(aEvent);

        // Only pan when mouse event isn't part of a click. Prevent jittering on tap.
        this._kinetic.addData(sX - dragData.prevPanX, sY - dragData.prevPanY);

        // dragBy will reset dX and dY values to 0
        this._dragBy(this.dX, this.dY);

        // Let everyone know when mousemove begins a pan
        if (!oldIsPan && dragData.isPan()) {
          this._mouseOverTimeout.clear();
          this._longClickTimeout.clear();

          let event = document.createEvent("Events");
          event.initEvent("PanBegin", true, false);
          this._targetScrollbox.dispatchEvent(event);
        }
      }
    }
    else if (!dragData.dragging && this._downUpEvents.length) {
      let oldEvent = this._downUpEvents[0];
      dragData._isPan = ScrollUtils.isPan(new Point(oldEvent.clientX, oldEvent.clientY),
                                          new Point(aEvent.clientX, aEvent.clientY));
      if (dragData.isPan())
        this._longClickTimeout.clear();
    }
  },

  sendMove: function(aEvent) {
    let event = document.createEvent("MouseEvents");
    this._initMouseEventFromEvent(event, aEvent, "TapMove", true, true);
    aEvent.target.dispatchEvent(event);
  },

  /**
   * Inform our dragger of a dragStart.
   */
  _doDragStart: function _doDragStart(aEvent, aDraggable) {
    let dragData = this._dragData;
    dragData.setDragStart(aEvent.screenX, aEvent.screenY, aDraggable);
    this._kinetic.addData(0, 0);
    this._dragStartTime = Date.now();
    if (!this._kinetic.isActive())
      this._dragger.dragStart(aEvent.clientX, aEvent.clientY, aEvent.target, this._targetScrollInterface);
  },

  /** Finish a drag. */
  _doDragStop: function _doDragStop() {
    let dragData = this._dragData;
    if (!dragData.dragging)
      return;

    dragData.endDrag();

    // Note: it is possible for kinetic scrolling to be active from a
    // mousedown/mouseup event previous to this one. In this case, we
    // want the kinetic panner to tell our drag interface to stop.

    if (dragData.isPan()) {
      if (Date.now() - this._dragStartTime > kStopKineticPanOnDragTimeout)
        this._kinetic._velocity.set(0, 0);
      // Start kinetic pan.
      this._kinetic.start();
    } else {
      this._kinetic.end();
      this._dragger.dragStop(0, 0, this._targetScrollInterface);
      this._dragger = null;
    }
  },

  /**
   * Used by _onMouseMove() above and by KineticController's timer to do the
   * actual dragMove signalling to the dragger.  We'd put this in _onMouseMove()
   * but then KineticController would be adding to its own data as it signals
   * the dragger of dragMove()s.
   */
  _dragBy: function _dragBy(dX, dY, aIsKinetic) {
    let dragged = true;
    let dragData = this._dragData;
    if (!this._waitingForPaint || aIsKinetic) {
      let dragData = this._dragData;
      dragged = this._dragger.dragMove(dX, dY, this._targetScrollInterface, aIsKinetic);
      if (dragged && !this._waitingForPaint) {
        this._waitingForPaint = true;
        mozRequestAnimationFrame(this);
      }
      this.dX = 0;
      this.dY = 0;
    }
    if (!dragData.isPan())
      this._kinetic.pause();

    return dragged;
  },

  /** Callback for kinetic scroller. */
  _kineticStop: function _kineticStop() {
    // Kinetic panning could finish while user is panning, so don't finish
    // the pan just yet.
    let dragData = this._dragData;
    if (!dragData.dragging) {
      this._dragger.dragStop(0, 0, this._targetScrollInterface);
      this._dragger = null;

      let event = document.createEvent("Events");
      event.initEvent("PanFinished", true, false);
      this._targetScrollbox.dispatchEvent(event);
    }
  },

  /** Called when tap down times is long enought to generate a mousemove **/
  _doMouseOver: function _doMouseOver() {
    let ev = this._downUpEvents[0];
    this._dispatchTap("TapOver", ev);
  },

  /** Called when tap down times out and becomes a long tap. */
  _doLongClick: function _doLongClick() {
    let ev = this._downUpEvents[0];
    this._suppressNextMouseUp = true;
    this._dispatchTap("TapLong", ev);
  },

  /**
   * Commit another click event to our click buffer.  The `click buffer' is a
   * timeout initiated by the first click.  If the timeout is still alive when
   * another click is committed, then the click buffer forms a double tap, and
   * the timeout is cancelled.  Otherwise, the timeout issues a single tap.
   */
  _commitAnotherClick: function _commitAnotherClick() {
    if (this._singleClickTimeout.isPending()) {   // we're waiting for a second click for double
      this._singleClickTimeout.clear();
      this._doDoubleClick();
    } else {
      this._singleClickTimeout.once(kDoubleClickInterval);
    }
  },

  /** Endpoint of _commitAnotherClick().  Finalize a single tap.  */
  _doSingleClick: function _doSingleClick() {
    let mouseUp = this._downUpEvents[1];
    this._cleanClickBuffer();
    this._dispatchTap("TapSingle", mouseUp);
  },

  /** Endpoint of _commitAnotherClick().  Finalize a double tap.  */
  _doDoubleClick: function _doDoubleClick() {
    let mouseUp1 = this._downUpEvents[1];
    // sometimes the second press event is not dispatched at all
    let mouseUp2 = this._downUpEvents[Math.min(3, this._downUpEvents.length - 1)];
    this._cleanClickBuffer();

    let dx = mouseUp1.clientX - mouseUp2.clientX;
    let dy = mouseUp1.clientY - mouseUp2.clientY;

    let radius = this._doubleClickRadius;
    if (dx*dx + dy*dy < radius*radius) {
      this._dispatchTap("TapDouble", mouseUp1);
    } else {
      this._dispatchTap("TapSingle", mouseUp1);
      this._dispatchTap("TapSingle", mouseUp2);
    }
  },

  _dispatchTap: function _dispatchTap(aType, aMouseEvent) {
    let event = document.createEvent("Events");
    event.initEvent(aType, true, false);
    event.clientX = aMouseEvent.clientX;
    event.clientY = aMouseEvent.clientY;
    event.modifiers = Util.modifierMaskFromEvent(aMouseEvent);
    aMouseEvent.originalTarget.dispatchEvent(event);
  },

  /**
   * Record a mousedown/mouseup event for later redispatch via
   * _redispatchDownUpEvents()
   */
  _recordEvent: function _recordEvent(aEvent) {
    this._downUpEvents.push(aEvent);
  },

  /**
   * Clean out the click buffer.  Should be called after a single, double, or
   * non-click has been processed and all relevant (re)dispatches of events in
   * the recorded down/up event queue have been issued out.
   */
  _cleanClickBuffer: function _cleanClickBuffer() {
    this._singleClickTimeout.clear();
    this._mouseOverTimeout.clear();
    this._longClickTimeout.clear();
    this._downUpEvents.splice(0, this._downUpEvents.length);
  },

  /* XXXvn this can potentially be moved into ScrollUtils */
  _getTargetInputField: function _getTargetInputField(aTarget) {
    let focusedElement = document.commandDispatcher.focusedElement;
    let parentNode = aTarget.parentNode;

    let inputField = null;
    if (aTarget.mozIsTextField && aTarget.mozIsTextField(false) && focusedElement != aTarget)
      inputField = aEventTarget;
    else if (parentNode.mozIsTextField && parentNode.mozIsTextField(false) && focusedElement != parentNode)
      inputField = parentNode;
    else if (aTarget instanceof XULElement && aTarget.inputField)
      inputField = aTarget.inputField;

    return inputField;
  },

  toString: function toString() {
    return '[MouseModule] {'
      + '\n\tdragData=' + this._dragData + ', '
      + 'dragger=' + this._dragger + ', '
      + '\n\tdownUpEvents=' + this._downUpEvents + ', '
      + 'length=' + this._downUpEvents.length + ', '
      + '\n\ttargetScroller=' + this._targetScrollInterface + '}';
  }
};

var ScrollUtils = {
  // threshold in pixels for sensing a tap as opposed to a pan
  get tapRadius() {
    let dpi = Util.displayDPI;

    delete this.tapRadius;
    return this.tapRadius = Services.prefs.getIntPref("ui.dragThresholdX") / 240 * dpi;
  },

  /**
   * Walk up (parentward) the DOM tree from elem in search of a scrollable element.
   * Return the element and its scroll interface if one is found, two nulls otherwise.
   *
   * This function will cache the pointer to the scroll interface on the element itself,
   * so it is safe to call it many times without incurring the same XPConnect overhead
   * as in the initial call.
   */
  getScrollboxFromElement: function getScrollboxFromElement(elem) {
    let scrollbox = null;
    let qinterface = null;

    for (; elem; elem = elem.parentNode) {
      try {
        if (elem.scrollBoxObject) {
          scrollbox = elem;
          qinterface = elem.scrollBoxObject;
          break;
        } else if (elem.customDragger) {
          scrollbox = elem;
          break;
        } else if (elem.boxObject) {
          let qi = (elem._cachedSBO) ? elem._cachedSBO
                                     : elem.boxObject.QueryInterface(Ci.nsIScrollBoxObject);
          if (qi) {
            scrollbox = elem;
            scrollbox._cachedSBO = qinterface = qi;
            break;
          }
        }
      } catch (e) { /* we aren't here to deal with your exceptions, we'll just keep
                       traversing until we find something more well-behaved, as we
                       prefer default behaviour to whiny scrollers. */ }
    }
    return [scrollbox, qinterface, (scrollbox ? (scrollbox.customDragger || this._defaultDragger) : null)];
  },

  /** Determine if the distance moved can be considered a pan */
  isPan: function isPan(aPoint, aPoint2) {
    return (Math.abs(aPoint.x - aPoint2.x) > this.tapRadius ||
            Math.abs(aPoint.y - aPoint2.y) > this.tapRadius);
  },

  /**
   * The default dragger object used by MouseModule when dragging a scrollable
   * element that provides no customDragger.  Simply performs the expected
   * regular scrollBy calls on the scroller.
   */
  _defaultDragger: {
    isDraggable: function isDraggable(target, scroller) {
      let sX = {}, sY = {},
          pX = {}, pY = {};
      scroller.getPosition(pX, pY);
      scroller.getScrolledSize(sX, sY);
      let rect = target.getBoundingClientRect();
      return { x: (sX.value > rect.width  || pX.value != 0),
               y: (sY.value > rect.height || pY.value != 0) };
    },

    dragStart: function dragStart(cx, cy, target, scroller) {
      scroller.element.addEventListener("PanBegin", this._showScrollbars, false);
    },

    dragStop: function dragStop(dx, dy, scroller) {
      scroller.element.removeEventListener("PanBegin", this._showScrollbars, false);
      return this.dragMove(dx, dy, scroller);
    },

    dragMove: function dragMove(dx, dy, scroller) {
      if (scroller.getPosition) {
        try {
          let oldX = {}, oldY = {};
          scroller.getPosition(oldX, oldY);

          scroller.scrollBy(dx, dy);

          let newX = {}, newY = {};
          scroller.getPosition(newX, newY);

          return (newX.value != oldX.value) || (newY.value != oldY.value);

        } catch (e) { /* we have no time for whiny scrollers! */ }
      }

      return false;
    },

    _showScrollbars: function _showScrollbars(aEvent) {
      let scrollbox = aEvent.target;
      scrollbox.setAttribute("panning", "true");

      let hideScrollbars = function() {
        scrollbox.removeEventListener("PanFinished", hideScrollbars, false);
        scrollbox.removeEventListener("CancelTouchSequence", hideScrollbars, false);
        scrollbox.removeAttribute("panning");
      }

      // Wait for panning to be completely finished before removing scrollbars
      scrollbox.addEventListener("PanFinished", hideScrollbars, false);
      scrollbox.addEventListener("CancelTouchSequence", hideScrollbars, false);
    }
  }
};

/**
 * DragData handles processing drags on the screen, handling both
 * locking of movement on one axis, and click detection.
 */
function DragData() {
  this._domUtils = Cc["@mozilla.org/inspector/dom-utils;1"].getService(Ci.inIDOMUtils);
  this._lockRevertThreshold = Util.displayDPI * kAxisLockRevertThreshold;
  this.reset();
};

DragData.prototype = {
  reset: function reset() {
    this.dragging = false;
    this.sX = null;
    this.sY = null;
    this.locked = false;
    this.stayLocked = false;
    this.lockedX = null;
    this.lockedY = null;
    this._originX = null;
    this._originY = null;
    this.prevPanX = null;
    this.prevPanY = null;
    this._isPan = false;
  },

  /** Depending on drag data, locks sX,sY to X-axis or Y-axis of start position. */
  _lockAxis: function _lockAxis(sX, sY) {
    if (this.locked) {
      if (this.lockedX !== null)
        sX = this.lockedX;
      else if (this.lockedY !== null)
        sY = this.lockedY;
      return [sX, sY];
    }
    else {
      return [this._originX, this._originY];
    }
  },

  setDragPosition: function setDragPosition(sX, sY) {
    // Check if drag is now a pan.
    if (!this._isPan) {
      this._isPan = ScrollUtils.isPan(new Point(this._originX, this._originY), new Point(sX, sY));
      if (this._isPan)
        this._resetActive();
    }

    // If now a pan, mark previous position where panning was.
    if (this._isPan) {
      let absX = Math.abs(this._originX - sX);
      let absY = Math.abs(this._originY - sY);

      if (absX > this._lockRevertThreshold || absY > this._lockRevertThreshold)
        this.stayLocked = true;

      // After the first lock, see if locking decision should be reverted.
      if (!this.stayLocked) {
        if (this.lockedX && absX > 3 * absY)
          this.lockedX = null;
        else if (this.lockedY && absY > 3 * absX)
          this.lockedY = null;
      }

      if (!this.locked) {
        // look at difference from origin coord to lock movement, but only
        // do it if initial movement is sufficient to detect intent

        // divide possibilty space into eight parts.  Diagonals will allow
        // free movement, while moving towards a cardinal will lock that
        // axis.  We pick a direction if you move more than twice as far
        // on one axis than another, which should be an angle of about 30
        // degrees from the axis

        if (absX > 2.5 * absY)
          this.lockedY = sY;
        else if (absY > absX)
          this.lockedX = sX;

        this.locked = true;
      }
    }

    // After pan lock, figure out previous panning position. Base it on last drag
    // position so there isn't a jump in panning.
    let [prevX, prevY] = this._lockAxis(this.sX, this.sY);
    this.prevPanX = prevX;
    this.prevPanY = prevY;

    this.sX = sX;
    this.sY = sY;
  },

  setDragStart: function setDragStart(screenX, screenY, aDraggable) {
    this.sX = this._originX = screenX;
    this.sY = this._originY = screenY;
    this.dragging = true;

    // If the target area is pannable only in one direction lock it early
    // on the right axis
    this.lockedX = !aDraggable.x ? screenX : null;
    this.lockedY = !aDraggable.y ? screenY : null;
    this.stayLocked = this.lockedX || this.lockedY;
    this.locked = this.stayLocked;
  },

  endDrag: function endDrag() {
    this._resetActive();
    this.dragging = false;
  },

  /** Returns true if drag should pan scrollboxes.*/
  isPan: function isPan() {
    return this._isPan;
  },

  /** Return true if drag should be parsed as a click. */
  isClick: function isClick() {
    return !this._isPan;
  },

  /**
   * Returns the screen position for a pan. This factors in axis locking.
   * @return Array of screen X and Y coordinates
   */
  panPosition: function panPosition() {
    return this._lockAxis(this.sX, this.sY);
  },

  _resetActive: function _resetActive() {
    // dismiss the active state of the pan element
    let target = document.documentElement;
    let state = this._domUtils.getContentState(target);
    this._domUtils.setContentState(target, state & kStateActive);
  },

  toString: function toString() {
    return '[DragData] { sX,sY=' + this.sX + ',' + this.sY + ', dragging=' + this.dragging + ' }';
  }
};


/**
 * KineticController - a class to take drag position data and use it
 * to do kinetic panning of a scrollable object.
 *
 * aPanBy is a function that will be called with the dx and dy
 * generated by the kinetic algorithm.  It should return true if the
 * object was panned, false if there was no movement.
 *
 * There are two complicated things done here.  One is calculating the
 * initial velocity of the movement based on user input.  Two is
 * calculating the distance to move every frame.
 */
function KineticController(aPanBy, aEndCallback) {
  this._panBy = aPanBy;
  this._beforeEnd = aEndCallback;

  // These are used to calculate the position of the scroll panes during kinetic panning. Think of
  // these points as vectors that are added together and multiplied by scalars.
  this._position = new Point(0, 0);
  this._velocity = new Point(0, 0);
  this._acceleration = new Point(0, 0);
  this._time = 0;
  this._timeStart = 0;

  // How often do we change the position of the scroll pane?  Too often and panning may jerk near
  // the end. Too little and panning will be choppy. In milliseconds.
  this._updateInterval = Services.prefs.getIntPref("browser.ui.kinetic.updateInterval");
  // Constants that affect the "friction" of the scroll pane.
  this._exponentialC = Services.prefs.getIntPref("browser.ui.kinetic.exponentialC");
  this._polynomialC = Services.prefs.getIntPref("browser.ui.kinetic.polynomialC") / 1000000;
  // Number of milliseconds that can contain a swipe. Movements earlier than this are disregarded.
  this._swipeLength = Services.prefs.getIntPref("browser.ui.kinetic.swipeLength");

  this._reset();
}

KineticController.prototype = {
  _reset: function _reset() {
    this._active = false;
    this._paused = false;
    this.momentumBuffer = [];
    this._velocity.set(0, 0);
  },

  isActive: function isActive() {
    return this._active;
  },

  _startTimer: function _startTimer() {
    let self = this;

    let lastp = this._position;  // track last position vector because pan takes deltas
    let v0 = this._velocity;  // initial velocity
    let a = this._acceleration;  // acceleration
    let c = this._exponentialC;
    let p = new Point(0, 0);
    let dx, dy, t, realt;

    function calcP(v0, a, t) {
      // Important traits for this function:
      //   p(t=0) is 0
      //   p'(t=0) is v0
      //
      // We use exponential to get a smoother stop, but by itself exponential
      // is too smooth at the end. Adding a polynomial with the appropriate
      // weight helps to balance
      return v0 * Math.exp(-t / c) * -c + a * t * t + v0 * c;
    }

    this._calcV = function(v0, a, t) {
      return v0 * Math.exp(-t / c) + 2 * a * t;
    }

    let callback = {
      onBeforePaint: function kineticHandleEvent(timeStamp) {
        // Someone called end() on us between timer intervals
        // or we are paused.
        if (!self.isActive() || self._paused)
          return;

        // To make animation end fast enough but to keep smoothness, average the ideal
        // time frame (smooth animation) with the actual time lapse (end fast enough).
        // Animation will never take longer than 2 times the ideal length of time.
        realt = timeStamp - self._initialTime;
        self._time += self._updateInterval;
        t = (self._time + realt) / 2;

        // Calculate new position.
        p.x = calcP(v0.x, a.x, t);
        p.y = calcP(v0.y, a.y, t);
        dx = Math.round(p.x - lastp.x);
        dy = Math.round(p.y - lastp.y);

        // Test to see if movement is finished for each component.
        if (dx * a.x > 0) {
          dx = 0;
          lastp.x = 0;
          v0.x = 0;
          a.x = 0;
        }
        // Symmetric to above case.
        if (dy * a.y > 0) {
          dy = 0;
          lastp.y = 0;
          v0.y = 0;
          a.y = 0;
        }

        if (v0.x == 0 && v0.y == 0) {
          self.end();
        } else {
          let panStop = false;
          if (dx != 0 || dy != 0) {
            try { panStop = !self._panBy(-dx, -dy, true); } catch (e) {}
            lastp.add(dx, dy);
          }

          if (panStop)
            self.end();
          else
            mozRequestAnimationFrame(this);
        }
      }
    };

    this._active = true;
    this._paused = false;
    mozRequestAnimationFrame(callback);
  },

  start: function start() {
    function sign(x) {
      return x ? ((x > 0) ? 1 : -1) : 0;
    }

    function clampFromZero(x, closerToZero, furtherFromZero) {
      if (x >= 0)
        return Math.max(closerToZero, Math.min(furtherFromZero, x));
      return Math.min(-closerToZero, Math.max(-furtherFromZero, x));
    }

    let mb = this.momentumBuffer;
    let mblen = this.momentumBuffer.length;

    let lastTime = mb[mblen - 1].t;
    let distanceX = 0;
    let distanceY = 0;
    let swipeLength = this._swipeLength;

    // determine speed based on recorded input
    let me;
    for (let i = 0; i < mblen; i++) {
      me = mb[i];
      if (lastTime - me.t < swipeLength) {
        distanceX += me.dx;
        distanceY += me.dy;
      }
    }

    let currentVelocityX = 0;
    let currentVelocityY = 0;

    if (this.isActive()) {
      // If active, then we expect this._calcV to be defined.
      let currentTime = Date.now() - this._initialTime;
      currentVelocityX = Util.clamp(this._calcV(this._velocity.x, this._acceleration.x, currentTime), -kMaxVelocity, kMaxVelocity);
      currentVelocityY = Util.clamp(this._calcV(this._velocity.y, this._acceleration.y, currentTime), -kMaxVelocity, kMaxVelocity);
    }

    if (currentVelocityX * this._velocity.x <= 0)
      currentVelocityX = 0;
    if (currentVelocityY * this._velocity.y <= 0)
      currentVelocityY = 0;

    let swipeTime = Math.min(swipeLength, lastTime - mb[0].t);
    this._velocity.x = clampFromZero((distanceX / swipeTime) + currentVelocityX, Math.abs(currentVelocityX), kMaxVelocity);
    this._velocity.y = clampFromZero((distanceY / swipeTime) + currentVelocityY, Math.abs(currentVelocityY), kMaxVelocity);

    if (Math.abs(this._velocity.x) < kMinVelocity)
      this._velocity.x = 0;
    if (Math.abs(this._velocity.y) < kMinVelocity)
      this._velocity.y = 0;

    // Set acceleration vector to opposite signs of velocity
    this._acceleration.set(this._velocity.clone().map(sign).scale(-this._polynomialC));

    this._position.set(0, 0);
    this._initialTime = mozAnimationStartTime;
    this._time = 0;
    this.momentumBuffer = [];

    if (!this.isActive() || this._paused)
      this._startTimer();

    return true;
  },

  pause: function pause() {
    this._paused = true;
  },

  end: function end() {
    if (this.isActive()) {
      if (this._beforeEnd)
        this._beforeEnd();
      this._reset();
    }
  },

  addData: function addData(dx, dy) {
    let mbLength = this.momentumBuffer.length;
    let now = Date.now();

    if (this.isActive()) {
      // Stop active movement when dragging in other direction.
      if (dx * this._velocity.x < 0 || dy * this._velocity.y < 0)
        this.end();
    }

    this.momentumBuffer.push({'t': now, 'dx' : dx, 'dy' : dy});
  }
};


/**
 * Input module for basic scrollwheel input.  Currently just zooms the browser
 * view accordingly.
 */
function ScrollwheelModule(container) {
  container.addEventListener("DOMMouseScroll", this, true);
  container.addEventListener("MozMousePixelScroll", this, true);
}

ScrollwheelModule.prototype = {
  pendingEvent : 0,
  handleEvent: function handleEvent(aEvent) {
    if (aEvent.type == "DOMMouseScroll" || aEvent.type == "MozMousePixelScroll") {
      /*
      * If events come too fast we don't want their handling to lag the zoom in/zoom out execution.
      * With the timeout the zoom is executed as we scroll.
      */
      if (this.pendingEvent)
        clearTimeout(this.pendingEvent);

      this.pendingEvent = setTimeout(function handleEventImpl(self) {
        self.pendingEvent = 0;
        Browser.zoom(aEvent.detail);
      }, 0, this);

      aEvent.stopPropagation();
      aEvent.preventDefault();
    }
  },

  /* We don't have much state to reset if we lose event focus */
  cancelPending: function cancelPending() {}
};


// Simple gestures support
//
// As per bug #412486, web content must not be allowed to receive any
// simple gesture events.  Multi-touch gesture APIs are in their
// infancy and we do NOT want to be forced into supporting an API that
// will probably have to change in the future.  (The current Mac OS X
// API is undocumented and was reverse-engineered.)  Until support is
// implemented in the event dispatcher to keep these events as
// chrome-only, we must listen for the simple gesture events during
// the capturing phase and call stopPropagation on every event.

function GestureModule(aElement) {
  aElement.addEventListener("MozSwipeGesture", this, true);
  aElement.addEventListener("MozMagnifyGestureStart", this, true);
  aElement.addEventListener("MozMagnifyGestureUpdate", this, true);
  aElement.addEventListener("MozMagnifyGesture", this, true);
  aElement.addEventListener("CancelTouchSequence", this, true);
}

GestureModule.prototype = {
  /**
   * Dispatch events based on the type of mouse gesture event. For now, make
   * sure to stop propagation of every gesture event so that web content cannot
   * receive gesture events.
   *
   * @param nsIDOMEvent information structure
   */
  handleEvent: function handleEvent(aEvent) {
    try {
      let consume = false;
      switch (aEvent.type) {
        case "MozSwipeGesture":

          // Cancel other touch sequence events, and be courteous by allowing them
          // to say no.
          let event = document.createEvent("Events");
          event.initEvent("CancelTouchSequence", true, true);
          let success = aEvent.target.dispatchEvent(event);
      
          if (!success)
            return;

          let gesture = Ci.nsIDOMSimpleGestureEvent;
          switch (aEvent.direction) {
            case gesture.DIRECTION_UP:
              Browser.scrollContentToBottom();
              break;
            case gesture.DIRECTION_DOWN:
              Browser.scrollContentToTop();
              break;
            case gesture.DIRECTION_LEFT:
              CommandUpdater.doCommand("cmd_forward");
              break;
            case gesture.DIRECTION_RIGHT:
              CommandUpdater.doCommand("cmd_back");
              break;
          }
          break;

        case "MozMagnifyGestureStart":
          consume = true;
          this._pinchStart(aEvent);
          break;

        case "MozMagnifyGestureUpdate":
          consume = true;
          if (this._ignoreNextUpdate)
            this._ignoreNextUpdate = false;
          else
            this._pinchUpdate(aEvent);
          break;

        case "MozMagnifyGesture":
          consume = true;
          this._pinchEnd(aEvent);
          break;

        case "CancelTouchSequence":
          this.cancelPending();
          break;
      }
      if (consume) {
        // prevent sending of event to content
        aEvent.stopPropagation();
        aEvent.preventDefault();
      }
    }
    catch (e) {
      Util.dumpLn("Error while handling gesture event", aEvent.type,
                  "\nPlease report error at:", e);
      Cu.reportError(e);
    }
  },

  cancelPending: function cancelPending() {
    if (AnimatedZoom.isZooming())
      AnimatedZoom.finish();
  },

  _pinchStart: function _pinchStart(aEvent) {
    if (AnimatedZoom.isZooming())
      return;

    // Cancel other touch sequence events, and be courteous by allowing them
    // to say no.
    let event = document.createEvent("Events");
    event.initEvent("CancelTouchSequence", true, true);
    let success = aEvent.target.dispatchEvent(event);

    if (!success || !Browser.selectedTab.allowZoom)
      return;

    AnimatedZoom.start();
    this._pinchDelta = 0;

    this._ignoreNextUpdate = true; // first update gives useless, huge delta

    // cache gesture limit values
    this._maxGrowth = Services.prefs.getIntPref("browser.ui.pinch.maxGrowth");
    this._maxShrink = Services.prefs.getIntPref("browser.ui.pinch.maxShrink");
    this._scalingFactor = Services.prefs.getIntPref("browser.ui.pinch.scalingFactor");

    // Adjust the client coordinates to be relative to the browser element's top left corner.
    this._browserBCR = getBrowser().getBoundingClientRect();
    this._pinchStartX = aEvent.clientX - this._browserBCR.left;
    this._pinchStartY = aEvent.clientY - this._browserBCR.top;
  },

  _pinchUpdate: function _pinchUpdate(aEvent) {
    if (!AnimatedZoom.isZooming() || !aEvent.delta)
      return;

    let delta = 0;
    let browser = AnimatedZoom.browser;
    let oldScale = browser.scale;
    let bcr = this._browserBCR;

    // Accumulate pinch delta. Small changes are just jitter.
    this._pinchDelta += aEvent.delta;
    if (Math.abs(this._pinchDelta) >= oldScale) {
      delta = this._pinchDelta;
      this._pinchDelta = 0;
    }

    // decrease the pinchDelta min/max values to limit zooming out/in speed
    delta = Util.clamp(delta, -this._maxShrink, this._maxGrowth);

    let newScale = Browser.selectedTab.clampZoomLevel(oldScale * (1 + delta / this._scalingFactor));
    let startScale = AnimatedZoom.startScale;
    let scaleRatio = startScale / newScale;
    let cX = aEvent.clientX - bcr.left;
    let cY = aEvent.clientY - bcr.top;

    // Calculate the new zoom rect.
    let rect = AnimatedZoom.zoomFrom.clone();
    rect.translate(this._pinchStartX - cX + (1-scaleRatio) * cX * rect.width / bcr.width,
                   this._pinchStartY - cY + (1-scaleRatio) * cY * rect.height / bcr.height);

    rect.width *= scaleRatio;
    rect.height *= scaleRatio;

    this.translateInside(rect, new Rect(0, 0, browser.contentDocumentWidth * startScale,
                                              browser.contentDocumentHeight * startScale));

    // redraw zoom canvas according to new zoom rect
    AnimatedZoom.updateTo(rect);
  },

  _pinchEnd: function _pinchEnd(aEvent) {
    if (AnimatedZoom.isZooming())
      AnimatedZoom.finish();
  },

  /**
   * Ensure r0 is inside r1, if possible. Preserves w, h.
   * Same as Rect.prototype.translateInside except it aligns the top left
   * instead of the bottom right if r0 is bigger than r1.
   */
  translateInside: function translateInside(r0, r1) {
    let offsetX = (r0.left < r1.left ? r1.left - r0.left :
        (r0.right > r1.right ? Math.max(r1.left - r0.left, r1.right - r0.right) : 0));
    let offsetY = (r0.top < r1.top ? r1.top - r0.top :
        (r0.bottom > r1.bottom ? Math.max(r1.top - r0.top, r1.bottom - r0.bottom) : 0));
    return r0.translate(offsetX, offsetY);
  }
};
