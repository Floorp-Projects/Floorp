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

// Maximum delay in ms between the two taps of a double-tap
const kDoubleClickInterval = 400;

// Amount of time to wait before tap becomes long tap
const kLongTapWait = 500;

// If a tap lasts longer than this duration in ms, treat it as a single-tap
// immediately instead of waiting for a possible double tap.
const kDoubleClickThreshold = 200;

// threshold in pixels for sensing a tap as opposed to a pan
const kTapRadius = Services.prefs.getIntPref("ui.dragThresholdX");

// maximum drag distance in pixels while axis locking can still be reverted
const kAxisLockRevertThreshold = 200;

// Same as NS_EVENT_STATE_ACTIVE from nsIEventStateManager.h
const kStateActive = 0x00000001;

/**
 * InputHandler
 *
 * The input handler is an arbiter between the Fennec chrome window inputs and any
 * registered input modules.  It keeps an array of input module objects.  Incoming
 * input events are wrapped in an EventInfo object and passed down to the input modules
 * in the order of the modules array.  Every registed module thus gets called with
 * an EventInfo for each event that the InputHandler is registered to listen for.
 * Currently, the InputHandler listens for the following events by default.
 *
 * On the Fennec global chrome window:
 *   mousedown
 *   mouseup
 *   mousemove
 *   click
 *
 * On the browserViewContainer:
 *   keydown
 *   keyup
 *   DOMMouseScroll
 *
 *
 * When one of the handlers decides it wants to handle the event, it should call
 * grab() on its owner which will cause it to receive all of the events until it
 * calls ungrab().  Calling grab will notify the other handlers via a
 * cancelPending() notification.  This tells them to stop what they're doing and
 * give up hope for being the one to process the events.
 *
 * Input modules must provide the following interface:
 *
 *   handleEvent(nsIDOMEvent)
 *     Entry point by which InputHandler passes Fennec chrome window events
 *     to the module.
 *
 *   cancelPending()
 *     Called by the InputHandler as a hint to the module that it may wish to reset
 *     whatever state it might have entered by processing events thus far.  For
 *     instance, a module may have grabbed (cf grab()) focus, in which case the
 *     InputHandler will call cancelPending() on all remaining modules.
 *
 * How grabbing works:
 *   An input module may wish to grab event focus of the InputHandler, which means that it
 * wants to process all incoming events for a while.  When the InputHandler is grabbed
 * by one of its modules, only that module will receive incoming events until it ungrabs
 * the InputHandler.  No other modules' handleEvent() function will be called while the
 * InputHandler is grabbed.  Grabs and ungrabs of the InputHandler require an object reference
 * corresponding to the grabbing object.  That is, a module must call inputHandler.grab(this)
 * and .ungrab(this) in order for the calls to succeed.  The object given as the argument
 * will be that which is given event focus.
 */
function InputHandler(browserViewContainer) {
  /* the list of modules that will handle input */
  this._modules = [];

  /* which module, if any, has all events directed to it */
  this._grabber = null;
  this._grabDepth = 0;

  /* when true, don't process any events */
  this._ignoreEvents = false;

  /* when set to true, next click won't be dispatched */
  this._suppressNextClick = false;

  /* these handle dragging of both chrome elements and content */
  window.addEventListener("mousedown", this, true);
  window.addEventListener("mouseup", this, true);
  window.addEventListener("mousemove", this, true);
  window.addEventListener("click", this, true);
  window.addEventListener("MozSwipeGesture", this, true);
  window.addEventListener("MozMagnifyGestureStart", this, true);
  window.addEventListener("MozMagnifyGestureUpdate", this, true);
  window.addEventListener("MozMagnifyGesture", this, true);

  /* these handle key strokes in the browser view (where page content appears) */
  browserViewContainer.addEventListener("keypress", this, false);
  browserViewContainer.addEventListener("keyup", this, false);
  browserViewContainer.addEventListener("keydown", this, false);
  browserViewContainer.addEventListener("DOMMouseScroll", this, true);
  browserViewContainer.addEventListener("MozMousePixelScroll", this, true);

  this.addModule(new MouseModule(this, browserViewContainer));
  this.addModule(new KeyModule(this, browserViewContainer));
  this.addModule(new GestureModule(this, browserViewContainer));
  this.addModule(new ScrollwheelModule(this, browserViewContainer));
}


InputHandler.prototype = {
  /**
   * Add a module.  Module priority is first come, first served, so modules
   * added later have lower priority.
   */
  addModule: function addModule(m) {
    this._modules.push(m);
  },

  /**
   * Have the InputHandler begin/resume listening for events.
   */
  startListening: function startListening() {
    this._ignoreEvents = false;
  },

  /**
   * Stop/pause the InputHandler from listening for events.
   */
  stopListening: function stopListening() {
    this._ignoreEvents = true;
  },

  /**
   * A module calls grab(this) to grab event focus from the input
   * handler.  In grabbed state, the input handler forwards all events
   * directly to the grabber module, and not to any other modules.
   * The this reference passed is essentially a ceritificate to the
   * input handler --- collateral for the grab.
   *
   * Other modules cannot grab a grabbed input handler, and only the
   * grabber module can ungrab the input handler.
   */
  grab: function grab(grabber) {
    if (!this._grabber || this._grabber == grabber) {
      if (!this._grabber) {
        // call cancel on all modules
        let mods = this._modules;
        for (let i = 0, len = mods.length; i < len; ++i)
          if (mods[i] != grabber)
            mods[i].cancelPending();
      }
      this._grabber = grabber;
      return true;
    }
    return false;
  },

  /**
   * A grabber module should ungrab the input handler by calling ungrab(this).
   * Of course, a module other than the original grabber may spoof the ungrab
   * if it has our reference to that module.
   *
   * @param grabber The grabber's object reference, as grabber proof.
   */
  ungrab: function ungrab(grabber) {
    if (this._grabber == grabber) {  // only grabber can ungrab
      this._grabber = null;
    }
  },

  /**
   * Sometimes a module will swallow a mousedown and mouseup, which (when found
   * in sequence) should be followed by a click.  Ideally, this module would
   * listen for the click as well, and ignore it, but this is a convenience method
   * for the module to do so via the InputHandler.  Hopefully the module is doing
   * this under grab (that is, hopefully the module was grabbing while the mousedown
   * and mouseup events came in, *not* just grabbing for making this call).
   */
  suppressNextClick: function suppressNextClick() {
    this._suppressNextClick = true;
  },

  /** Cancels all pending input sequences */
  cancelPending: function cancelPending() {
    let mods = this._modules;
    for (let i = 0, len = mods.length; i < len; ++i)
      if (mods[i])
        mods[i].cancelPending();
  },

  /**
   * Undoes any suppression caused by calling suppressNextClick(), unless the click
   * already happened.
   */
  allowClicks: function allowClicks() {
    this._suppressNextClick = false;
  },

  /**
   * InputHandler's DOM event handler.
   */
  handleEvent: function handleEvent(aEvent) {
    if (this._ignoreEvents)
      return;

    // ignore all events that belong to other windows or documents (e.g. content events)
    if (aEvent.target.localName == "browser")
      return;

    if (this._suppressNextClick && aEvent.type == "click") {
      this._suppressNextClick = false;
      aEvent.stopPropagation();
      aEvent.preventDefault();
      return;
    }

    aEvent.time = Date.now();
    this._passToModules(aEvent);
  },

  /**
   * Utility method for passing an EventInfo to the handlers of all modules beginning
   * with the module at index skipToIndex and increasing (==> decreasing in priority).
   */
  _passToModules: function _passToModules(aEvent, aSkipToIndex) {
    if (this._grabber) {
      this._grabber.handleEvent(aEvent);
    } else {
      let mods = this._modules;
      let i = aSkipToIndex || 0;

      for (let len = mods.length; i < len; ++i) {
        mods[i].handleEvent(aEvent);  // event focus could get grabbed in this invocation
        if (this._grabber)            // so don't pass the event to the rest of modules
          break;
      }
    }
  }
};

/**
 * MouseModule
 *
 * Input handler module that handles all mouse-related input such as dragging and
 * clicking.
 *
 * The Fennec chrome DOM tree has elements that are augmented dynamically with
 * custom JS properties that tell the MouseModule they have custom support for
 * either dragging or clicking.  These JS properties are JS objects that expose
 * an interface supporting dragging or clicking (though currently we only look
 * to drag scrollable elements).
 *
 * The MouseModule grabs event focus of the input handler on mousedown, at which
 * point it will attempt to find such custom draggers by walking up the
 * DOM tree from the event target.  It ungrabs event focus on mouseup.  It
 * redispatches the swallowed mousedown, mouseup events back to chrome, so that
 * chrome elements still get their events.
 *
 * The mousedown and mouseup events happening in the main context are
 * redispatched as soon as they get caught, contrary to events happening on web
 * content which are swallowed before being redispatched as a triple at the end
 * of the mouseup handling.
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
 *   dragMove(dx, dy, scroller)
 *     Signals an input attempt to drag by dx, dy.
 *
 * Between mousedown and mouseup, MouseModule incrementally drags and updates
 * the dragger accordingly, and also determines whether a [double-]click occured
 * (based on whether the input moves have moved outside of a certain drag disk
 * centered at the mousedown position). Custom touch events are dispatched
 * accordingly.
 *
 * There is a default dragger in case a scrollable element is dragged --- see
 * the defaultDragger prototype property.
 */
function MouseModule(owner, browserViewContainer) {
  this._owner = owner;
  this._browserViewContainer = browserViewContainer;
  this._dragData = new DragData(kTapRadius);

  this._dragger = null;

  this._downUpEvents = [];
  this._targetScrollInterface = null;

  var self = this;
  this._kinetic = new KineticController(this._dragBy.bind(this),
                                        this._kineticStop.bind(this));

  this._singleClickTimeout = new Util.Timeout(this._doSingleClick.bind(this));
  this._longClickTimeout = new Util.Timeout(this._doLongClick.bind(this));
}


MouseModule.prototype = {
  handleEvent: function handleEvent(aEvent) {
    if (aEvent.button !== 0 && aEvent.type != "contextmenu" && aEvent.type != "MozBeforePaint")
      return;

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
      case "MozBeforePaint":
        this._waitingForPaint = false;
        removeEventListener("MozBeforePaint", this, false);
        break;
    }
  },

  /**
   * This gets invoked by the input handler if another module grabs.  We should
   * reset our state or something here.  This is probably doing the wrong thing
   * in its current form.
   */
  cancelPending: function cancelPending() {
    if (this._kinetic.isActive())
      this._kinetic.end();

    this._dragData.reset();
    this._targetScrollInterface = null;

    this._cleanClickBuffer();
  },

  /** Begin possible pan and send tap down event. */
  _onMouseDown: function _onMouseDown(aEvent) {
    this._owner.allowClicks();

    let dragData = this._dragData;
    if (dragData.dragging) {
      // Somehow a mouse up was missed.
      let [sX, sY] = dragData.panPosition();
      this._doDragStop();
    }
    dragData.reset();

    // walk up the DOM tree in search of nearest scrollable ancestor.  nulls are
    // returned if none found.
    let [targetScrollbox, targetScrollInterface, dragger]
      = this.getScrollboxFromElement(aEvent.target);

    // stop kinetic panning if targetScrollbox has changed
    if (this._kinetic.isActive() && this._dragger != dragger)
      this._kinetic.end();

    this._targetScrollInterface = targetScrollInterface;

    // Do tap
    let event = document.createEvent("Events");
    event.initEvent("TapDown", true, true);
    event.clientX = aEvent.clientX;
    event.clientY = aEvent.clientY;
    let success = aEvent.target.dispatchEvent(event);
    if (success) {
      this._recordEvent(aEvent);
      this._target = aEvent.target;
      this._longClickTimeout.once(kLongTapWait);
    } else {
      // cancel all pending content clicks
      this._cleanClickBuffer();
    }

    // Do pan
    if (dragger) {
      let draggable = dragger.isDraggable(targetScrollbox, targetScrollInterface);
      dragData.locked = !draggable.x || !draggable.y;
      if (draggable.x || draggable.y) {
        this._dragger = dragger;
        this._doDragStart(aEvent);
      }
    }

    this._owner.grab(this);
  },

  /** Send tap up event and any necessary full taps. */
  _onMouseUp: function _onMouseUp(aEvent) {
    this._onMouseMove(aEvent);

    let dragData = this._dragData;
    if (dragData.dragging)
      this._doDragStop();

    // Do tap
    if (this._target) {
      let event = document.createEvent("Events");
      event.initEvent("TapUp", true, true);
      event.clientX = aEvent.clientX
      event.clientY = aEvent.clientY;
      let success = aEvent.target.dispatchEvent(event);
      if (!success) {
        this._cleanClickBuffer();
      } else {
        this._recordEvent(aEvent);
        let commitToClicker = dragData.isClick() && (this._downUpEvents.length > 1);
        if (commitToClicker)
          // commit this click to the doubleclick timewait buffer
          this._commitAnotherClick();
        else
          // clean the click buffer ourselves
          this._cleanClickBuffer();
      }
    }

    // Do pan
    if (dragData.isPan()) {
      // User was panning around, do not allow click
      // XXX Instead of having suppressNextClick, we could grab until click is seen
      // and THEN ungrab so that owner does not need to know anything about clicking.
      let generatesClick = aEvent.detail;
      if (generatesClick)
        this._owner.suppressNextClick();
    }

    this._owner.ungrab(this);
  },

  /**
   * If we're in a drag, do what we have to do to drag on.
   */
  _onMouseMove: function _onMouseMove(aEvent) {
    let dragData = this._dragData;

    if (dragData.dragging && !this._waitingForPaint) {
      let oldIsPan = dragData.isPan();
      dragData.setDragPosition(aEvent.screenX, aEvent.screenY);
      if (dragData.isPan()) {
        // Only pan when mouse event isn't part of a click. Prevent jittering on tap.
        let [sX, sY] = dragData.panPosition();
        this._doDragMove();

        // Let everyone know when mousemove begins a pan
        if (!oldIsPan && dragData.isPan()) {
          this._longClickTimeout.clear();

          let event = document.createEvent("Events");
          event.initEvent("PanBegin", true, false);
          aEvent.target.dispatchEvent(event);
        }
      }
    }
  },

  /**
   * Inform our dragger of a dragStart.
   */
  _doDragStart: function _doDragStart(event) {
    let dragData = this._dragData;
    dragData.setDragStart(event.screenX, event.screenY);
    this._kinetic.addData(0, 0);
    if (!this._kinetic.isActive())
      this._dragger.dragStart(event.clientX, event.clientY, event.target, this._targetScrollInterface);
  },

  /** Finish a drag. */
  _doDragStop: function _doDragStop() {
    this._dragData.endDrag();

    let dragData = this._dragData;
    if (!dragData.isPan()) {
      // There was no pan, so just stop dragger.
      this._dragger.dragStop(0, 0, this._targetScrollInterface);
    } else {
      // Start kinetic pan.
      let [sX, sY] = dragData.panPosition();
      let dX = dragData.prevPanX - sX;
      let dY = dragData.prevPanY - sY;
      this._kinetic.addData(-dX, -dY);
      this._kinetic.start();
    }
  },

  /**
   * Update kinetic with new data and drag.
   */
  _doDragMove: function _doDragMove() {
    let dragData = this._dragData;
    let [sX, sY] = dragData.panPosition();
    let dX = dragData.prevPanX - sX;
    let dY = dragData.prevPanY - sY;
    this._kinetic.addData(-dX, -dY);
    this._dragBy(dX, dY);
  },

  /**
   * Used by _doDragMove() above and by KineticController's timer to do the
   * actual dragMove signalling to the dragger.  We'd put this in _doDragMove()
   * but then KineticController would be adding to its own data as it signals
   * the dragger of dragMove()s.
   */
  _dragBy: function _dragBy(dX, dY) {
    let dragData = this._dragData;
    let dragged = this._dragger.dragMove(dX, dY, this._targetScrollInterface);
    if (dragged && !this._waitingForPaint) {
      this._waitingForPaint = true;
      mozRequestAnimationFrame();
      addEventListener("MozBeforePaint", this, false);
    }
    return dragged;
  },

  /** Callback for kinetic scroller. */
  _kineticStop: function _kineticStop() {
    // Kinetic panning could finish while user is panning, so don't finish
    // the pan just yet.
    let dragData = this._dragData;
    if (!dragData.dragging) {
      this._dragger.dragStop(0, 0, this._targetScrollInterface);
      let event = document.createEvent("Events");
      event.initEvent("PanFinished", true, false);
      document.dispatchEvent(event);
    }
  },

  /** Called when tap down times out and becomes a long tap. */
  _doLongClick: function _doLongClick() {
    let ev = this._downUpEvents[0];

    let event = document.createEvent("Events");
    event.initEvent("TapLong", true, false);
    event.clientX = ev.clientX;
    event.clientY = ev.clientY;
    ev.target.dispatchEvent(event);
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
    let ev = this._downUpEvents[1];
    this._cleanClickBuffer();

    // borrowed from nsIDOMNSEvent.idl
    let modifiers =
      (ev.altKey   ? Ci.nsIDOMNSEvent.ALT_MASK     : 0) |
      (ev.ctrlKey  ? Ci.nsIDOMNSEvent.CONTROL_MASK : 0) |
      (ev.shiftKey ? Ci.nsIDOMNSEvent.SHIFT_MASK   : 0) |
      (ev.metaKey  ? Ci.nsIDOMNSEvent.META_MASK    : 0);

    let event = document.createEvent("Events");
    event.initEvent("TapSingle", true, false);
    event.clientX = ev.clientX;
    event.clientY = ev.clientY;
    event.modifiers = modifiers;
    ev.target.dispatchEvent(event);
  },

  /** Endpoint of _commitAnotherClick().  Finalize a double tap.  */
  _doDoubleClick: function _doDoubleClick() {
    let mouseUp1 = this._downUpEvents[1];
    // sometimes the second press event is not dispatched at all
    let mouseUp2 = this._downUpEvents[Math.min(3, this._downUpEvents.length - 1)];
    this._cleanClickBuffer();

    let event = document.createEvent("Events");
    event.initEvent("TapDouble", true, false);
    event.clientX1 = mouseUp1.clientX;
    event.clientY1 = mouseUp1.clientY;
    event.clientX2 = mouseUp1.clientX;
    event.clientY2 = mouseUp1.clientY;
    mouseUp1.target.dispatchEvent(event);
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
    this._longClickTimeout.clear();
    this._downUpEvents.splice(0);
  },

  /**
   * The default dragger object used by MouseModule when dragging a scrollable
   * element that provides no customDragger.  Simply performs the expected
   * regular scrollBy calls on the scroller.
   */
  _defaultDragger: {
    isDraggable: function isDraggable(target, scroller) {
      let sX = {}, sY = {};
      scroller.getScrolledSize(sX, sY);
      let rect = target.getBoundingClientRect();
      return { x: sX.value > rect.width, y: sY.value > rect.height };
    },

    dragStart: function dragStart(cx, cy, target, scroller) {},

    dragStop : function dragStop(dx, dy, scroller) {
      return this.dragMove(dx, dy, scroller);
    },

    dragMove : function dragMove(dx, dy, scroller) {
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
    }
  },

  // -----------------------------------------------------------
  // -- Utility functions

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
        if (elem.ignoreDrag) {
          break;
        }

        if (elem.scrollBoxObject) {
          scrollbox = elem;
          qinterface = elem.scrollBoxObject;
          break;
        } else if (elem.boxObject) {
          let qi = (elem._cachedSBO) ? elem._cachedSBO
                                     : elem.boxObject.QueryInterface(Ci.nsIScrollBoxObject);
          if (qi) {
            scrollbox = elem;
            scrollbox._cachedSBO = qinterface = qi;
            break;
          }
        } else if (elem.customDragger) {
          scrollbox = elem;
          break;
        }
      } catch (e) { /* we aren't here to deal with your exceptions, we'll just keep
                       traversing until we find something more well-behaved, as we
                       prefer default behaviour to whiny scrollers. */ }
    }
    return [scrollbox, qinterface, (scrollbox ? (scrollbox.customDragger || this._defaultDragger) : null)];
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

/**
 * DragData handles processing drags on the screen, handling both
 * locking of movement on one axis, and click detection.
 */
function DragData(dragRadius) {
  this._dragRadius = dragRadius;
  this._domUtils = Cc["@mozilla.org/inspector/dom-utils;1"].getService(Ci.inIDOMUtils);
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
      let distanceSquared = (Math.pow(sX - this._originX, 2) + Math.pow(sY - this._originY, 2));
      this._isPan = (distanceSquared > Math.pow(this._dragRadius, 2));
      if (this._isPan)
        this._resetActive();
    }

    // If now a pan, mark previous position where panning was.
    if (this._isPan) {
      let absX = Math.abs(this._originX - sX);
      let absY = Math.abs(this._originY - sY);

      if (absX > kAxisLockRevertThreshold || absY > kAxisLockRevertThreshold)
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

      // After pan lock, figure out previous panning position. Base it on last drag
      // position so there isn't a jump in panning.
      let [prevX, prevY] = this._lockAxis(this.sX, this.sY);
      this.prevPanX = prevX;
      this.prevPanY = prevY;
    }

    this.sX = sX;
    this.sY = sY;
  },

  setDragStart: function setDragStart(screenX, screenY) {
    this.sX = this._originX = screenX;
    this.sY = this._originY = screenY;
    this.dragging = true;
    this.locked = false;
    this.stayLocked = false;
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
  // "Friction" of the scroll pane. The lower, the less friction and the further distance traveled.
  this._decelerationRate = Services.prefs.getIntPref("browser.ui.kinetic.decelerationRate") / 10000;
  // A multiplier for the initial velocity of the movement.
  this._speedSensitivity = Services.prefs.getIntPref("browser.ui.kinetic.speedSensitivity") / 100;
  // Number of milliseconds that can contain a swipe. Movements earlier than this are disregarded.
  this._swipeLength = Services.prefs.getIntPref("browser.ui.kinetic.swipeLength");

  this._reset();
}

KineticController.prototype = {
  _reset: function _reset() {
    if (this._callback) {
      removeEventListener("MozBeforePaint", this._callback, false);
      this._callback = null;
    }

    this.momentumBuffer = [];
    this._velocity.set(0, 0);
  },

  isActive: function isActive() {
    return !!this._callback;
  },

  _startTimer: function _startTimer() {
    // Use closed form of a parabola to calculate each position for panning.
    // x(t) = v0*t + .5*t^2*a
    // where: v0 is initial velocity
    //        a is acceleration
    //        t is time elapsed
    //
    // x(t)
    //  ^
    //  |                |
    //  |
    //  |                |
    //  |           ....^^^^....
    //  |      ...^^     |      ^^...
    //  |  ...^                      ^...
    //  |..              |               ..
    //   -----------------------------------> t
    //  t0             tf=-v0/a
    //
    // Using this formula, distance moved is independent of the time between each frame, unlike time
    // step approaches. Once the time is up, set the position to x(tf) and stop the timer.

    let lastx = this._position;  // track last position vector because pan takes differences
    let v0 = this._velocity;  // initial velocity
    let a = this._acceleration;  // acceleration

    // Temporary "bins" so that we don't create new objects during pan.
    let aBin = new Point(0, 0);
    let v0Bin = new Point(0, 0);
    let self = this;

    let callback = {
      handleEvent: function kineticHandleEvent(event) {

        if (!self.isActive())  // someone called end() on us between timer intervals
          return;

        // To make animation end fast enough but to keep smoothness, average the ideal
        // time frame (smooth animation) with the actual time lapse (end fast enough).
        // Animation will never take longer than 2 times the ideal length of time.
        let realt = event.timeStamp - self._initialTime;
        self._time += self._updateInterval;
        let t = (self._time + realt) / 2;

        // Calculate new position using x(t) formula.
        let x = v0Bin.set(v0).scale(t).add(aBin.set(a).scale(0.5 * t * t));
        let dx = x.x - lastx.x;
        let dy = x.y - lastx.y;
        lastx.set(x);

        // Test to see if movement is finished for each component. As seen in graph, we want the
        // final position to be at tf.
        if (t >= -v0.x / a.x) {
          // Plug in t=-v0/a into x(t) to get final position.
          dx = -v0.x * v0.x / 2 / a.x - lastx.x;
          // Reset components. Next frame: a's component will be 0 and t >= NaN will be false.
          lastx.x = 0;
          v0.x = 0;
          a.x = 0;
        }
        // Symmetric to above case.
        if (t >= -v0.y / a.y) {
          dy = -v0.y * v0.y / 2 / a.y - lastx.y;
          lastx.y = 0;
          v0.y = 0;
          a.y = 0;
        }

        let panned = false;
        try { panned = self._panBy(Math.round(-dx), Math.round(-dy)); } catch (e) {}
        if (!panned)
          self.end();
        else
          mozRequestAnimationFrame();
      }
    };

    this._callback = callback;
    addEventListener("MozBeforePaint", callback, false);
    mozRequestAnimationFrame();
  },

  start: function start() {
    function sign(x) {
      return x ? ((x > 0) ? 1 : -1) : 0;
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

    // Only allow kinetic scrolling to speed up if kinetic scrolling is active.
    this._velocity.x = (distanceX < 0 ? Math.min : Math.max)((distanceX / swipeLength) * this._speedSensitivity, this._velocity.x);
    this._velocity.y = (distanceY < 0 ? Math.min : Math.max)((distanceY / swipeLength) * this._speedSensitivity, this._velocity.y);

    // Set acceleration vector to opposite signs of velocity
    this._acceleration.set(this._velocity.clone().map(sign).scale(-this._decelerationRate));

    this._position.set(0, 0);
    this._initialTime = mozAnimationStartTime;
    this._time = 0;
    this.momentumBuffer = [];

    if (!this.isActive())
      this._startTimer();

    return true;
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
 * Input module for basic key input.
 */
function KeyModule(owner, browserViewContainer) {
  this._owner = owner;
  this._browserViewContainer = browserViewContainer;
}

KeyModule.prototype = {
  handleEvent: function handleEvent(aEvent) {
    if (aEvent.type == "keydown" || aEvent.type == "keyup" || aEvent.type == "keypress") {
      let keyer = this._browserViewContainer.customKeySender;
      if (keyer) {
        keyer.dispatchKeyEvent(aEvent);
        aEvent.stopPropagation();
        aEvent.preventDefault();
      }
    }
  },

  /* We don't have much state to reset if we lose event focus */
  cancelPending: function cancelPending() {}
};


/**
 * Input module for basic scrollwheel input.  Currently just zooms the browser
 * view accordingly.
 */
function ScrollwheelModule(owner, browserViewContainer) {
  this._owner = owner;
  this._browserViewContainer = browserViewContainer;
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

function GestureModule(owner, browserViewContainer) {
  this._owner = owner;
  this._browserViewContainer = browserViewContainer;
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
          let gesture = Ci.nsIDOMSimpleGestureEvent;
          switch (aEvent.direction) {
            case gesture.DIRECTION_UP:
              Browser.scrollContentToBottom();
              break;
            case gesture.DIRECTION_DOWN:
              Browser.scrollContentToTop();
              break;
            case gesture.DIRECTION_LEFT:
              CommandUpdater.doCommand("cmd_back");
              break;
            case gesture.DIRECTION_RIGHT:
              CommandUpdater.doCommand("cmd_forward");
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
    // terminate pinch zoom if ongoing
    if (this._pinchZoom) {
      this._pinchZoom.finish();
      this._pinchZoom = null;
    }
  },

  _pinchStart: function _pinchStart(aEvent) {
    if (this._pinchZoom)
      return;

    // grab events during pinch, stopping other mouse events in their tracks
    this._owner.grab(this);

    if ((aEvent.target instanceof XULElement) || !Browser.selectedTab.allowZoom) {
      this._owner.ungrab(this);
      return;
    }

    // create the AnimatedZoom object for fast arbitrary zooming
    this._pinchZoom = AnimatedZoom;
    this._pinchZoomRect = AnimatedZoom.getStartRect()

    // start from current zoom level
    this._pinchStartScale = this._pinchScale = getBrowser().scale;
    this._ignoreNextUpdate = true; // first update gives useless, huge delta

    // cache gesture limit values
    this._maxGrowth = Services.prefs.getIntPref("browser.ui.pinch.maxGrowth");
    this._maxShrink = Services.prefs.getIntPref("browser.ui.pinch.maxShrink");
    this._scalingFactor = Services.prefs.getIntPref("browser.ui.pinch.scalingFactor");

    // save the initial gesture start point as reference
    this._pinchClientX = aEvent.clientX;
    this._pinchClientY = aEvent.clientY;
  },

  _pinchUpdate: function _pinchUpdate(aEvent) {
    if (!this._pinchZoom || !aEvent.delta)
      return;

    // decrease the pinchDelta min/max values to limit zooming out/in speed
    let delta = Util.clamp(aEvent.delta, -this._maxShrink, this._maxGrowth);

    let oldScale = this._pinchScale;
    let newScale = Browser.selectedTab.clampZoomLevel(oldScale * (1 + delta / this._scalingFactor));

    let scaleRatio = oldScale / newScale;
    let [cX, cY] = [aEvent.clientX, aEvent.clientY];

    // Calculate the new zoom rect.
    let rect = this._pinchZoomRect.clone();
    rect.translate(this._pinchClientX - cX + (1-scaleRatio) * cX * rect.width / window.innerWidth,
                   this._pinchClientY - cY + (1-scaleRatio) * cY * rect.height / window.innerHeight);

    rect.width *= scaleRatio;
    rect.height *= scaleRatio;

    let startScale = this._pinchStartScale;
    rect.translateInside(new Rect(0, 0, getBrowser().contentDocumentWidth * startScale,
                                        getBrowser().contentDocumentHeight * startScale));

    // redraw zoom canvas according to new zoom rect
    this._pinchZoomRect = rect;
    this._pinchZoom.updateTo(this._pinchZoomRect);

    this._pinchScale = newScale;
    this._pinchClientX = cX;
    this._pinchClientY = cY;
  },

  _pinchEnd: function _pinchEnd(aEvent) {
    // release grab
    this._owner.ungrab(this);

    // stop ongoing animated zoom
    if (this._pinchZoom) {
      // zoom to current level for real
      this._pinchZoom.finish();
      this._pinchZoom = null;
    }
  }
};

