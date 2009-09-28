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
 *   URLChanged
 *   TabSelect
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
 *   handleEvent(evInfo)
 *     Entry point by which InputHandler passes wrapped Fennec chrome window events
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
 * will be that which is given event focus.  grab/ungrab may be nested (that is, a module can
 * grab as many times as it wants to) provided that they are one-to-one.  That is, if a
 * module grabs four times, it should be sure to ungrab that many times as well.  Lastly,
 * an input module may, in ungrabbing, provide an array of queued EventInfo objects, each of
 * which will be passed by the InputHandler to each of the subsequent modules ("subsequent"
 * as in "next in the ordering within the modules array") via handleEvent().  This can be
 * thought of as the module's way of saying "sorry for grabbing focus, here's everything I
 * kept you from processing this whole time" to the modules of lower priority.  To prevent
 * infinite loops, this event queue is only passed to lower-priority modules.
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

  /* used to cancel actions with browser window changes */
  this.listenFor(window, "URLChanged");
  this.listenFor(window, "TabSelect");

  /* these handle dragging of both chrome elements and content */
  this.listenFor(window, "mousedown");
  this.listenFor(window, "mouseup");
  this.listenFor(window, "mousemove");
  this.listenFor(window, "click");

  /* these handle key strokes in the browser view (where page content appears) */
  this.listenFor(browserViewContainer, "keydown");
  this.listenFor(browserViewContainer, "keyup");
  this.listenFor(browserViewContainer, "DOMMouseScroll");
  this.listenFor(browserViewContainer, "MozMousePixelScroll");

  this.addModule(new MouseModule(this, browserViewContainer));
  this.addModule(new ScrollwheelModule(this, browserViewContainer));
}


InputHandler.prototype = {
  /**
   * Tell the InputHandler to begin handling events from target of type
   * eventType.
   */
  listenFor: function listenFor(target, eventType) {
    target.addEventListener(eventType, this, true);
  },

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
   * input handler --- collateral for the grab.  A grabber module may
   * make nested calls to grab() but should symmetrically ungrab().
   * Other modules cannot grab a grabbed input handler, and only the
   * grabber module can ungrab the input handler.
   *
   * grab(null) aborts all input handlers.  This is used in situations
   * like the page changing to a different URL where you want to abort
   * drags in progress or kinetic movement.
   *
   * Returns true if the grab succeeded, false otherwise.
   */
  grab: function grab(grabber) {
    if (grabber == null) {
      this._grabber = null;
      this._grabDepth = -1;   // incremented to 0 below
    }

    if (!this._grabber || this._grabber == grabber) {

      if (!this._grabber) {
        // call cancel on all modules
        let mods = this._modules;
        for (let i = 0, len = mods.length; i < len; ++i)
          if (mods[i] != grabber)
            mods[i].cancelPending();
      }

      this._grabber = grabber;
      this._grabDepth++;
      return true;
    }

    return false;
  },

  /**
   * A grabber module should ungrab the input handler by calling ungrab(this).
   * Of course, a module other than the original grabber may spoof the ungrab
   * if it has our reference to that module.
   *
   * An optional second parameter gives a list of events to pass to the
   * handlers of all other modules.  This is useful if the grabber module
   * absorbed many events but wants to pass a possibly modified subset of
   * them onward to the input handling modules that didn't get to see the
   * events all along.
   *
   * @param grabber The grabber's object reference, as grabber proof.
   * @param restoreEventInfos An array of EventInfo objects to pass to
   * the handler of every module of lower priority than grabber.
   */
  // XXX ungrab(null) is supported here too because the old ungrab()
  //   happened to support it (not sure if intentionally; there was no
  //   comment it), so cf the corresponding comment on grab().
  ungrab: function ungrab(grabber, restoreEventInfos) {
    if (this._grabber == null && grabber == null) {
      this._grabber = null;
      this._grabDepth = 1;  // decremented to 0 below
    }

    if (this._grabber == grabber) {  // only grabber can ungrab
      this._grabDepth--;

      if (this._grabDepth == 0) {    // all nested grabs gone
        this._grabber = null;

        if (restoreEventInfos) {
          let mods = this._modules;
          let grabberIndex = 0;

          for (let i = 0, len = mods.length; i < len; ++i) {
            if (mods[i] == grabber) {
              grabberIndex = i;      // found the grabber's priority
              break;
            }
          }

          for (i = 0, len = restoreEventInfos.length; i < len; ++i)
            this._passToModules(restoreEventInfos[i], grabberIndex + 1);
        }
      }
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

    /* ignore all events that belong to other windows or documents (e.g. content events) */
    if (aEvent.view != window)
      return;

    /* changing URL or selected a new tab will immediately stop active input handlers */
    if (aEvent.type == "URLChanged" || aEvent.type == "TabSelect") {
      this.grab(null);
      return;
    }

    if (this._suppressNextClick && aEvent.type == "click") {
      this._suppressNextClick = false;
      aEvent.stopPropagation();
      aEvent.preventDefault();
      return;
    }

    this._passToModules(new InputHandler.EventInfo(aEvent));
  },

  /**
   * Utility method for passing an EventInfo to the handlers of all modules beginning
   * with the module at index skipToIndex and increasing (==> decreasing in priority).
   */
  _passToModules: function _passToModules(evInfo, skipToIndex) {
    if (this._grabber) {
      this._grabber.handleEvent(evInfo);
    } else {
      let mods = this._modules;
      let i = skipToIndex || 0;

      for (let len = mods.length; i < len; ++i) {
        mods[i].handleEvent(evInfo);  // event focus could get grabbed in this invocation
        if (this._grabbed)            // so don't pass the event to the rest of modules
          break;
      }
    }
  }
};


/**
 * Helper class to InputHandler.  Wraps a DOM event with some additional data.
 */
InputHandler.EventInfo = function EventInfo(aEvent, timestamp) {
  this.event = aEvent;
  this.time = timestamp || Date.now();
};

InputHandler.EventInfo.prototype = {
  toString: function toString() {
    return '[EventInfo] { event=' + this.event + 'time=' + this.time + ' }';
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
 * point it will attempt to find such custom draggers/clickers by walking up the
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
 * centered at the mousedown position).  If a [double-]click happened, any
 * customClicker will be notified.  The customClicker must support the following
 * interface:
 *
 *   singleClick(cx, cy)
 *     Signals a single (as opposed to double) click occured at client
 *     coordinates cx, cy
 *
 *   doubleClick(cx1, cy1, cx2, cy2)
 *     Signals a doubleclick occured, with the first click at client coordinates
 *     cx1, cy1, and second click at client coordinates cx2, cy2.
 *
 * There is a default dragger in case a scrollable element is dragged --- see
 * the defaultDragger prototype property.  There is no default clicker.
 */
function MouseModule(owner, browserViewContainer) {
  this._owner = owner;
  this._browserViewContainer = browserViewContainer;
  this._dragData = new DragData(this, 15, 200);

  this._dragger = null;
  this._clicker = null;

  this._downUpEvents = [];
  this._targetScrollInterface = null;

  var self = this;
  this._kinetic = new KineticController(
    function _dragByBound(dx, dy) { return self._dragBy(dx, dy); },
    function _dragStopBound() { return self._doDragStop(0, 0, true); }
  );
}


MouseModule.prototype = {
  handleEvent: function handleEvent(evInfo) {
    if (evInfo.event.button !== 0) // avoid all but a clean left click
      return;

    switch (evInfo.event.type) {
      case "mousedown":
        this._onMouseDown(evInfo);
        break;
      case "mousemove":
        this._onMouseMove(evInfo);
        break;
      case "mouseup":
        this._onMouseUp(evInfo);
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

    if (this._clickTimeout)
      window.clearTimeout(this._clickTimeout);

    this._cleanClickBuffer();
  },

  /**
   * Handle a mousedown by stopping any lingering kinetic drag, walking DOM tree
   * in search of a scrollable element (and its custom dragger if available) and
   * a clicker, and initiating a drag if we have said scrollable element.  The
   * mousedown event is entirely swallowed but is saved for later redispatching,
   * once we know right and proper what the input is trying to do to us.
   *
   * We grab() in here.
   */
  _onMouseDown: function _onMouseDown(evInfo) {
    this._owner.allowClicks();
    if (this._kinetic.isActive())
      this._kinetic.end();

    // walk up the DOM tree in search of nearest scrollable ancestor.  nulls are
    // returned if none found.
    let [targetScrollbox, targetScrollInterface]
      = this.getScrollboxFromElement(evInfo.event.target);

    let targetClicker = this.getClickerFromElement(evInfo.event.target);

    this._movedOutOfRadius = false;
    this._targetScrollInterface = targetScrollInterface;
    this._dragger = (targetScrollInterface) ? (targetScrollbox.customDragger || this._defaultDragger)
                                            : null;
    this._clicker = (targetClicker) ? targetClicker.customClicker : null;

    this._owner.grab(this);

    if (this._clicker)
      this._clicker.mouseDown(evInfo.event.clientX, evInfo.event.clientY);

    if (targetScrollInterface) {
      this._doDragStart(evInfo.event);
    }

    if (this._targetIsContent(evInfo.event))
      this._recordEvent(evInfo);
  },

  /**
   * Handle a mouseup by swallowing the event (just as we did the mousedown) as
   * well as the possible DOM click event that follows, making one last drag
   * (which, do note, might just be the beginning of a kinetic drag that will
   * linger long after we are gone), and recording the mousedown for later
   * redispatching.
   *
   * We ungrab() in here.
   */
  _onMouseUp: function _onMouseUp(evInfo) {
    let dragData = this._dragData;

    let [sX, sY] = dragData.lockAxis(evInfo.event.screenX, evInfo.event.screenY);

    this._movedOutOfRadius = this._movedOutOfRadius || dragData.isPointOutsideRadius(sX, sY);

    if (dragData.dragging)       // XXX same check as this._dragger but we
      this._doDragStop(sX, sY);  //  are using both, no good reason

    if (this._clicker)
      this._clicker.mouseUp(evInfo.event.clientX, evInfo.event.clientY);

    if (this._targetIsContent(evInfo.event)) {
      this._recordEvent(evInfo);
      this._doClick(this._movedOutOfRadius);
    }
    else if (this._dragger && this._movedOutOfRadius && evInfo.event.detail)
      this._owner.suppressNextClick();

    this._owner.ungrab(this);
  },

  /**
   * If we're in a drag, do what we have to do to drag on.
   */
  _onMouseMove: function _onMouseMove(evInfo) {
    let dragData = this._dragData;

    if (dragData.dragging) {
      let [sX, sY] = dragData.lockAxis(evInfo.event.screenX, evInfo.event.screenY);
      evInfo.event.stopPropagation();
      evInfo.event.preventDefault();
      this._doDragMove(sX, sY);
    }

    this._movedOutOfRadius = this._movedOutOfRadius || 
      dragData.isPointOutsideRadius(evInfo.event.screenX, evInfo.event.screenY);
  },

  /**
   * Check if the event concern the browser content
   */
  _targetIsContent: function _targetIsContent(aEvent) {
    let target = aEvent.target;
    while (target) {
      if (target === window)
        return false;
      if (target === this._browserViewContainer)
        return true;

      target = target.parentNode;
    }
    return false;
  },

  /**
   * Inform our dragger of a dragStart and update kinetic with new data.
   */
  _doDragStart: function _doDragStart(event) {
    let dragData = this._dragData;

    dragData.setDragStart(event.screenX, event.screenY);
    this._kinetic.addData(event.screenX, event.screenY);

    this._dragger.dragStart(event.clientX, event.clientY, event.target, this._targetScrollInterface);
  },

  /**
   * Finish a drag.  The third parameter is a secret one used to distinguish
   * between the supposed end of drag caused by a mouseup and the real end
   * of drag which happens when KineticController::end() is called.
   */
  _doDragStop: function _doDragStop(sX, sY, kineticStop) {
    let dragData = this._dragData;

    if (!kineticStop) {    // we're not really done, since now it is
                           // kinetic's turn to scroll about
      let dx = dragData.sX - sX;
      let dy = dragData.sY - sY;

      dragData.endDrag();

      this._kinetic.addData(sX, sY);

      this._kinetic.start();
    } else {               // now we're done, says our secret 3rd argument
      this._dragger.dragStop(0, 0, this._targetScrollInterface);
      dragData.reset();
    }
  },

  /**
   * Update kinetic with new data and drag.
   */
  _doDragMove: function _doDragMove(sX, sY) {
    let dragData = this._dragData;
    let dX = dragData.sX - sX;
    let dY = dragData.sY - sY;
    this._kinetic.addData(sX, sY);
    return this._dragBy(dX, dY);
  },

  /**
   * Used by _doDragMove() above and by KineticController's timer to do the
   * actual dragMove signalling to the dragger.  We'd put this in _doDragMove()
   * but then KineticController would be adding to its own data as it signals
   * the dragger of dragMove()s.
   */
  _dragBy: function _dragBy(dX, dY) {
    let dragData = this._dragData;
    let sX = dragData.sX - dX;
    let sY = dragData.sY - dY;

    dragData.setDragPosition(sX, sY);

    return this._dragger.dragMove(dX, dY, this._targetScrollInterface);
  },

  /**
   * Helper function to mouseup, called  at the point where a DOM click should
   * occur.  If movedOutOfRadius is true, then we don't call it an internal
   * clicker-notifiable click.  
   */
  _doClick: function _doClick(movedOutOfRadius) {
    let commitToClicker = this._clicker && !movedOutOfRadius;
    if (commitToClicker)
      this._commitAnotherClick();  // commit this click to the doubleclick timewait buffer
    else
      this._cleanClickBuffer();    // clean the click buffer ourselves, since there was no clicker
                                   // to commit to.  when there is one, the path taken through
                                   // _commitAndClick takes care of this.
  },

  /**
   * Commit another click event to our click buffer.  The `click buffer' is a
   * timeout initiated by the first click.  If the timeout is still alive when
   * another click is committed, then the click buffer forms a double click, and
   * the timeout is cancelled.  Otherwise, the timeout issues a single click to
   * the clicker.
   */
  _commitAnotherClick: function _commitAnotherClick() {
    const doubleClickInterval = 400;

    if (this._clickTimeout) {   // we're waiting for a second click for double
      window.clearTimeout(this._clickTimeout);
      this._doDoubleClick();
    } else {
      this._clickTimeout = window.setTimeout(function _clickTimeout(self) { self._doSingleClick(); },
                                             doubleClickInterval, this);
    }
  },

  /**
   * Endpoint of _commitAnotherClick().  Finalize a single click and tell the clicker.
   */
  _doSingleClick: function _doSingleClick() {
    //dump('doing single click with ' + this._downUpEvents.length + '\n');
    //for (let i = 0; i < this._downUpEvents.length; ++i)
    //  dump('      ' + this._downUpEvents[i].event.type
    //       + " :: " + this._downUpEvents[i].event.button
    //       + " :: " + this._downUpEvents[i].event.detail + '\n');/**/

    let ev = this._downUpEvents[1].event;
    this._cleanClickBuffer(2);
    this._clicker.singleClick(ev.clientX, ev.clientY);
  },

  /**
   * Endpoint of _commitAnotherClick().  Finalize a double click and tell the clicker.
   */
  _doDoubleClick: function _doDoubleClick() {
    //dump('doing double click with ' + this._downUpEvents.length + '\n');
    //for (let i = 0; i < this._downUpEvents.length; ++i)
    //  dump('      ' + this._downUpEvents[i].event.type
    //       + " :: " + this._downUpEvents[i].event.button
    //       + " :: " + this._downUpEvents[i].event.detail + '\n');/**/

    let mouseUp1 = this._downUpEvents[1].event;
    let mouseUp2 = this._downUpEvents[3].event;
    this._cleanClickBuffer(4);
    this._clicker.doubleClick(mouseUp1.clientX, mouseUp1.clientY,
                              mouseUp2.clientX, mouseUp2.clientY);
  },

  /**
   * Record a mousedown/mouseup event for later redispatch via
   * _redispatchDownUpEvents()
   */
  _recordEvent: function _recordEvent(evInfo) {
    this._downUpEvents.push(evInfo);
  },

  /**
   * Clean out the click buffer.  Should be called after a single, double, or
   * non-click has been processed and all relevant (re)dispatches of events in
   * the recorded down/up event queue have been issued out.
   *
   * @param [optional] the number of events to remove from the front of the
   * recorded events queue.
   */
  _cleanClickBuffer: function _cleanClickBuffer(howMany) {
    delete this._clickTimeout;

    if (howMany == undefined)
      howMany = this._downUpEvents.length;

    this._downUpEvents.splice(0, howMany);
  },

  /**
   * The default dragger object used by MouseModule when dragging a scrollable
   * element that provides no customDragger.  Simply performs the expected
   * regular scrollBy calls on the scroller.
   */
  _defaultDragger: {
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
    let prev = null;

    for (; elem; elem = elem.parentNode) {
      try {
        if (elem.ignoreDrag) {
          prev = elem;
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
        }
      } catch (e) { /* we aren't here to deal with your exceptions, we'll just keep
                       traversing until we find something more well-behaved, as we
                       prefer default behaviour to whiny scrollers. */ }
      prev = elem;
    }
    return [scrollbox, qinterface, prev];
  },

  /**
   * Walk up (parentward) the DOM tree from elem in search of an element with
   * a customClicker.  Return the element if found, null elsewise.
   */
  getClickerFromElement: function getClickerFromElement(elem) {
    for (; elem; elem = elem.parentNode)
      if (elem.customClicker)
        break;

    return (elem) ? elem : null;
  },

  toString: function toString() {
    return '[MouseModule] {'
      + '\n\tdragData=' + this._dragData + ', '
      + 'dragger=' + this._dragger + ', '
      + 'clicker=' + this._clicker + ', '
      + '\n\tdownUpEvents=' + this._downUpEvents + ', '
      + 'length=' + this._downUpEvents.length + ', '
      + '\n\ttargetScroller=' + this._targetScrollInterface + ', '
      + '\n\tclickTimeout=' + this._clickTimeout + '\n  }';
  }
};

/**
 * DragData handles processing drags on the screen, handling both
 * locking of movement on one axis, and click detection.
 */
function DragData(owner, dragRadius, dragStartTimeoutLength) {
  this._owner = owner;
  this._dragRadius = dragRadius;
  this.reset();
};

/* milliseconds between mouse down and drag direction determined */
const kMsUntilLock = 50;

DragData.prototype = {
  reset: function reset() {
    this.dragging = false;
    this.sX = null;
    this.sY = null;
    this.alreadyLocked = false;
    this.lockedX = null;
    this.lockedY = null;
    this._originX = null;
    this._originY = null;
  },

  setDragPosition: function setDragPosition(screenX, screenY) {
    this.sX = screenX;
    this.sY = screenY;
  },

  setDragStart: function setDragStart(screenX, screenY) {
    this.sX = this._originX = screenX;
    this.sY = this._originY = screenY;
    this.dragging = true;
    this._dragStartTime = Date.now();
    this.alreadyLocked = false;
  },

  endDrag: function endDrag() {
    this.dragging = false;
  },

  lockAxis: function lockAxis(sX, sY) {
    if (this.alreadyLocked) {
      if (this.lockedX !== null) {
        sX = this.lockedX;
      }
      else if (this.lockedY !== null) {
        sY = this.lockedY;
      }
      return [sX, sY];
    }
    // check to see if mouse move is after the timeout
    let now = Date.now();
    if (now - this._dragStartTime < kMsUntilLock) {
      // Util.dumpLn("*** pre-lock, return no movement");
      return [this.sX, this.sY];      
    }
     
    // Util.dumpLn("*** this.sX/sY: ", this.sX, ",", this.sY, "   sX/sY: ", sX, ",", sY);

    // look at difference from stored coord to lock movement, but only
    // do it if initial movement is sufficient to detect intent
    let absX = Math.abs(this.sX - sX);
    let absY = Math.abs(this.sY - sY);
    
    // Util.dumpLn("*** determining lock with absX/Y: ", absX, ",", absY);

    // divide possibilty space into eight parts.  Diagonals will allow
    // free movement, while moving towards a cardinal will lock that
    // axis.  We pick a direction if you move more than twice as far
    // on one axis than another, which should be an angle of about 30
    // degrees from the axis
    if (absX > 2 * absY) {
      this.lockedY = this.sY;
      sY = this.sY;
    }
    else if (absY > 2 * absX) {
      this.lockedX = this.sX;
      sX = this.sX;
    }

    // don't try to lock again... if you moved diagonal, we're free
    this.alreadyLocked = true;

    return [sX, sY];
  },

  isPointOutsideRadius: function isPointOutsideRadius(sX, sY) {
    if (this._originX === null)
      return false;
    return (Math.pow(sX - this._originX, 2) + Math.pow(sY - this._originY, 2)) >
      (2 * Math.pow(this._dragRadius, 2));
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
 */
function KineticController(aPanBy, aEndCallback) {
  this._panBy = aPanBy;
  this._timer = null;
  this._beforeEnd = aEndCallback;

  try {
    this._updateInterval = gPrefService.getIntPref("browser.ui.kinetic.updateInterval");
    // In preferences this value is an int.  We divide so that it can be between 0 and 1;
    this._emaAlpha = gPrefService.getIntPref("browser.ui.kinetic.ema.alphaValue") / 10;
    // In preferences this value is an int.  We divide so that it can be a percent.
    this._decelerationRate = gPrefService.getIntPref("browser.ui.kinetic.decelerationRate") / 100;
  }
  catch (e) {
    this._updateInterval = 33;
    this._emaAlpha = .8;
    this._decelerationRate = .15;
  };

  this._reset();
}

KineticController.prototype = {
  _reset: function _reset() {
    if (this._timer != null) {
      this._timer.cancel();
      this._timer = null;
    }

    this.momentumBuffer = [];
    this._speedX = 0;
    this._speedY = 0;
  },

  isActive: function isActive() {
    return (this._timer != null);
  },

  _startTimer: function _startTimer() {
    let callback = {
      _self: this,
      notify: function kineticTimerCallback(timer) {
        let self = this._self;

        if (!self.isActive())  // someone called end() on us between timer intervals
          return;

        //dump("             speeds: " + self._speedX + " " + self._speedY + "\n");

        if (self._speedX == 0 && self._speedY == 0) {
          self.end();
          return;
        }
        let dx = Math.round(self._speedX * self._updateInterval);
        let dy = Math.round(self._speedY * self._updateInterval);
        //dump("dx, dy: " + dx + " " + dy + "\n");

        let panned = false;
        try { panned = self._panBy(-dx, -dy); } catch (e) {}
        if (!panned) {
          self.end();
          return;
        }

        if (self._speedX < 0) {
          self._speedX = Math.min(self._speedX + self._decelerationRate, 0);
        } else if (self._speedX > 0) {
          self._speedX = Math.max(self._speedX - self._decelerationRate, 0);
        }
        if (self._speedY < 0) {
          self._speedY = Math.min(self._speedY + self._decelerationRate, 0);
        } else if (self._speedY > 0) {
          self._speedY = Math.max(self._speedY - self._decelerationRate, 0);
        }

        if (self._speedX == 0 && self._speedY == 0)
          self.end();
      }
    };

    this._timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    //initialize our timer with updateInterval
    this._timer.initWithCallback(callback,
                                 this._updateInterval,
                                 this._timer.TYPE_REPEATING_SLACK);
  },

  start: function start() {
    let mb = this.momentumBuffer;
    let mblen = this.momentumBuffer.length;

    // If we don't have at least 2 events do not do kinetic panning
    if (mblen < 2) {
      this.end();
      return false;
    }

    function ema(currentSpeed, lastSpeed, alpha) {
      return alpha * currentSpeed + (1 - alpha) * lastSpeed;
    };

    // build arrays of each movement's speed in pixels/ms
    let prev = mb[0];
    for (let i = 1; i < mblen; i++) {
      let me = mb[i];

      let timeDiff = me.t - prev.t;

      this._speedX = ema( ((me.sx - prev.sx) / timeDiff), this._speedX, this._emaAlpha);
      this._speedY = ema( ((me.sy - prev.sy) / timeDiff), this._speedY, this._emaAlpha);

      prev = me;
    }

    // fire off our kinetic timer which will do all the work
    this._startTimer();

    return true;
  },

  end: function end() {
    if (this._beforeEnd)
      this._beforeEnd();
    this._reset();
  },

  addData: function addData(sx, sy) {
    // if we're active, end that move before adding data
    if (this.isActive())
      this.end();

    let mbLength = this.momentumBuffer.length;
    // avoid adding duplicates which would otherwise slow down the speed
    let now = Date.now();
 
    if (mbLength > 0) {
      let mbLast = this.momentumBuffer[mbLength - 1];
      if ((mbLast.sx == sx && mbLast.sy == sy) || mbLast.t == now)
        return;
    }

    // Util.dumpLn("adding t:", now, ", sx: ", sx, ", sy: ", sy);
    this.momentumBuffer.push({'t': now, 'sx' : sx, 'sy' : sy});
  }
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
  handleEvent: function handleEvent(evInfo) {
    if (evInfo.event.type == "DOMMouseScroll" || evInfo.event.type == "MozMousePixelScroll") {
      /*
      * If events come too fast we don't want their handling to lag the zoom in/zoom out execution.
      * With the timeout the zoom is executed as we scroll.
      */
      if (this.pendingEvent)
        clearTimeout(this.pendingEvent);
      this.pendingEvent = setTimeout(this.handleEventImpl, 0, evInfo.event.detail);
      evInfo.event.stopPropagation();
      evInfo.event.preventDefault();
    }
  },
  handleEventImpl: function handleEventImpl(zoomlevel) {
	this.pendingEvent = 0;
	Browser.zoom(zoomlevel);
  },

  /* We don't have much state to reset if we lose event focus */
  cancelPending: function cancelPending() {}
};
