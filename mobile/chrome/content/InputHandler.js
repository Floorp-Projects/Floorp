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
 * Everything that is registed in _modules gets called with each event that the
 * InputHandler is registered to listen for.
 *
 * When one of the handlers decides it wants to handle the event, it should call
 * grab() on its owner which will cause it to receive all of the events until it
 * calls ungrab().  Calling grab will notify the other handlers via a
 * cancelPending() notification.  This tells them to stop what they're doing and
 * give up hope for being the one to process the events.
 */

function InputHandler() {
  /* the list of modules that will handle input */
  this._modules = [];

  /* which module, if any, has all events directed to it */
  this._grabber = null;
  this._grabDepth = 0;

  /* when true, don't process any events */
  this._ignoreEvents = false;

  /* when set to true, next click won't be dispatched */
  this._suppressNextClick = true;

  /* used to cancel actions with browser window changes */
  window.addEventListener("URLChanged", this, true);
  window.addEventListener("TabSelect", this, true);

  /* used to stop everything if mouse leaves window on desktop */
  window.addEventListener("mouseout", this, true);

  /* these handle dragging of both chrome elements and content */
  window.addEventListener("mousedown", this, true);
  window.addEventListener("mouseup", this, true);
  window.addEventListener("mousemove", this, true);
  window.addEventListener("click", this, true);
  window.addEventListener("DOMMouseScroll", this, true);

  // TODO move elsewhere
  let browserCanvas = document.getElementById("tile-container");
  browserCanvas.addEventListener("keydown", this, true);
  browserCanvas.addEventListener("keyup", this, true);

  let useEarlyMouseMoves = gPrefService.getBoolPref("browser.ui.panning.fixup.mousemove");

  this._modules.push(new MouseModule(this));
  //this._modules.push(new ContentPanningModule(this, browserCanvas, useEarlyMouseMoves));
  //this._modules.push(new ContentClickingModule(this));
  //this._modules.push(new ScrollwheelModule(this, browserCanvas));
}


/**
 * The input handler is an arbiter between the Fennec chrome window inputs and any
 * registered input modules.  It keeps an array of input module objects.  Incoming
 * input events are wrapped in an EventInfo object and passed down to the input modules
 * in the order of the modules array.
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
 *
 *
 * An input module may wish to grab event focus of the InputHandler, which means that it
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
InputHandler.prototype = {

  /**
   * A module calls grab(this) to grab event focus from the input handler.
   * In grabbed state, the input handler forwards all events directly to
   * the grabber module, and not to any other modules.  The this reference
   * passed is essentially a ceritificate to the input handler --- collateral
   * for the grab.  A grabber module may make nested calls to grab() but
   * should symmetrically ungrab().  Other modules cannot grab a grabbed input
   * handler, and only the grabber module can ungrab the input handler.
   *
   * Returns true if the grab succeeded, false otherwise.
   */
  // XXX froystig: grab(null) is supported because the old grab() supported
  //       it, but I'm not sure why.  The comment on that was "grab(null) is
  //       allowed because of mouseout handling".  Feel free to remove if that
  //       is no longer relevant, or remove this comment if it still is.
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
   */
  // XXX froystig: ungrab(null) is supported here too because the old ungrab()
  //       happened to support it (not sure if intentionally; there was no
  //       comment it), so cf the corresponding comment on grab().
  ungrab: function ungrab(grabber, restoreEventInfos) {
    if (this._grabber == null && grabber == null) {
      this._grabber = null;
      this._grabDepth = 1;  // decremented to 0 below
    }

    if (this._grabber == grabber) {
      this._grabDepth--;
      if (this._grabDepth == 0) {
        this._grabber = null;

        if (restoreEventInfos) {
          let mods = this._modules;
          let grabberIndex = 0;

          for (let i = 0, len = mods.length; i < len; ++i) {
            if (mods[i] == grabber) {
              grabberIndex = i;
              break;
            }
          }

          for (i = 0, len = restoreEventInfos.length; i < len; ++i)
            this.passToModules(restoreEventInfos[i], grabberIndex + 1);
        }
      }
    }
  },

  suppressNextClick: function suppressNextClick() {
    this._suppressNextClick = true;
  },

  allowClicks: function allowClicks() {
    this._suppressNextClick = false;
  },

  startListening: function startListening() {
    this._ignoreEvents = false;
  },

  stopListening: function stopListening() {
    this._ignoreEvents = true;
  },

  handleEvent: function handleEvent(aEvent) {
    if (this._ignoreEvents)
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

    this.passToModules(new InputHandler.EventInfo(aEvent));
  },

  passToModules: function passToModules(evInfo, skipToIndex) {
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


InputHandler.EventInfo = function EventInfo(aEvent, timestamp) {
  this.event = aEvent;
  this.time = timestamp || Date.now();
};


function MouseModule(owner) {
  this._owner = owner;
  this._dragData = new DragData(this, 50, 200);

  this._dragger = null;
  this._clicker = null;

  this._downUpEvents = [];
  this._downUpDispatchedIndex = 0;
  this._targetScrollInterface = null;

  this._dragging = false;
  this._fastPath = false;

  var self = this;
  this._kinetic = new KineticController(
    function _dragByBound(dx, dy) { return self._dragBy(dx, dy); },
    function _dragStopBound() { return self._doDragStop(0, 0, true); }
  );
}


MouseModule.prototype = {
  handleEvent: function handleEvent(evInfo) {
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
   * This gets invoked by owner if another module grabs.  Reset our state or something.
   * This is probably silly and unexpected, and should just be a noop.
   */
  cancelPending: function cancelPending() {
    this._kinetic.end();
    this._dragData.reset();
    this._targetScrollInterface = null;
  },

  _onMouseDown: function _onMouseDown(evInfo) {
    this._owner.allowClicks();
    if (this._kinetic.isActive())
      this._kinetic.end();

    // walk up the DOM tree in search of nearest scrollable ancestor.  nulls are
    // returned if none found.
    let [targetScrollbox, targetScrollInterface]
      = this.getScrollboxFromElement(evInfo.event.target);

    let targetClicker = this.getClickerFromElement(evInfo.event.target);

    this._targetScrollInterface = targetScrollInterface;

    this._dragger = (targetScrollInterface) ? (targetScrollbox.customDragger || this._defaultDragger)
                                            : null;
    this._clicker = (targetClicker) ? targetClicker.customClicker : null;

    evInfo.event.stopPropagation();
    evInfo.event.preventDefault();

    this._owner.grab(this);

    if (targetScrollInterface) {
      this._doDragStart(evInfo.event.screenX, evInfo.event.screenY);
    }

    this._recordEvent(evInfo);
  },

  _onMouseUp: function _onMouseUp(evInfo) {
    let dragData = this._dragData;

    evInfo.event.stopPropagation();
    evInfo.event.preventDefault();

    // we are swallowing mousedown and mouseup, so we should swallow their
    // corresponding click, too
    this._owner.suppressNextClick();

    let [sX, sY] = [evInfo.event.screenX, evInfo.event.screenY];

    let movedOutOfRadius = dragData.isPointOutsideRadius(sX, sY);

    if (dragData.dragging)
      this._doDragStop(sX, sY);

    dragData.reset();

    this._recordEvent(evInfo);

    this._doClick(movedOutOfRadius);

    this._owner.ungrab(this);
  },

  _onMouseMove: function _onMouseMove(evInfo) {
    let dragData = this._dragData;

    if (dragData.dragging) {
      evInfo.event.stopPropagation();
      evInfo.event.preventDefault();
      this._doDragMove(evInfo.event.screenX, evInfo.event.screenY);
    }
  },

  _recordEvent: function _recordEvent(evInfo) {
    this._downUpEvents.push(evInfo);
  },

  _redispatchDownUpEvents: function _redispatchDownUpEvents() {
    let evQueue = this._downUpEvents;

    this._owner.stopListening();

    let len = evQueue.length;

    for (let i = this._downUpDispatchedIndex; i < len; ++i)
      this._redispatchChromeMouseEvent(evQueue[i].event);

    this._downUpDispatchedIndex = len;

    this._owner.startListening();
  },

  _clearDownUpEvents: function _clearDownUpEvents() {
    this._downUpEvents.splice(0);
    this._downUpDispatchedIndex = 0;
  },

  _redispatchChromeMouseEvent: function _redispatchChromeMouseEvent(aEvent) {
    if (!(aEvent instanceof MouseEvent)) {
      Cu.reportError("_redispatchChromeMouseEvent called with a non-mouse event");
      return;
    }

    // we ignore the root scroll frame in this redispatch
    Browser.windowUtils.sendMouseEvent(aEvent.type, aEvent.clientX, aEvent.clientY,
                                       aEvent.button, aEvent.detail, 0, true);
  },

  _doDragStart: function _doDragStart(sX, sY) {
    let dragData = this._dragData;

    dragData.setDragStart(sX, sY);
    this._kinetic.addData(sX, sY);

    this._dragger.dragStart(this._targetScrollInterface);
  },

  _doDragStop: function _doDragStop(sX, sY, kineticStop) {
    if (!kineticStop) {    // we're not really done, since now it is
                           // kinetic's turn to scroll about
      let dragData = this._dragData;

      let dx = dragData.sX - sX;
      let dy = dragData.sY - sY;

      dragData.reset();
      this._kinetic.addData(sX, sY);

      this._kinetic.start();

    } else {               // now we're done, says our secret 3rd argument
      this._dragger.dragStop(0, 0, this._targetScrollInterface);
    }
  },

  _doDragMove: function _doDragMove(sX, sY) {
    let dragData = this._dragData;
    let dX = dragData.sX - sX;
    let dY = dragData.sY - sY;
    this._kinetic.addData(sX, sY);
    return this._dragBy(dX, dY);
  },

  _dragBy: function _dragBy(dX, dY) {
    let dragData = this._dragData;
    let sX = dragData.sX - dX;
    let sY = dragData.sY - dY;

    dragData.setDragPosition(sX, sY);

    return this._dragger.dragMove(dX, dY, this._targetScrollInterface);
  },

  _doClick: function _doClick(movedOutOfRadius) {
    let commitToClicker = this._clicker && !movedOutOfRadius;

    if (commitToClicker) {
      this._commitAnotherClick();  // commit this a click to the doubleclick timewait buffer
    }

    this._redispatchDownUpEvents();

    if (!commitToClicker) {
      this._cleanClickBuffer();    // clean the click buffer ourselves, since there was no clicker
                                   // to commit to.  when there is one, the path taken through
    }                              // _commitAndClick takes care of this.
  },

  /**
   *
   */
  _commitAnotherClick: function _commitAnotherClick() {
    this._doSingleClick();
    /*
    if (this._clickTimeout) {   // we're waiting for a second click for double
      window.clearTimeout(this._clickTimeout);
      this._doDoubleClick();
    } else {
      this._clickTimeout = window.setTimeout(function _clickTimeout(self) { self._doSingleClick(); }, 400, this);
    }*/
  },

  /**
   * Helper to _doClick().  Finalize a single click and tell the customClicker
   */
  _doSingleClick: function _doSingleClick() {
    dump('doing single click with ' + this._downUpEvents.length + '\n');
    for (let i = 0; i < this._downUpEvents.length; ++i)
      dump('      ' + this._downUpEvents[i].event.type
           + " :: " + this._downUpEvents[i].event.button
           + " :: " + this._downUpEvents[i].event.detail + '\n');

    let ev = this._downUpEvents[1].event;
    this._cleanClickBuffer();
    this._clicker.singleClick(ev.clientX, ev.clientY);
  },

  /**
   * Helper to _doClick().  Finalize a double click and tell the customClicker
   */
  _doDoubleClick: function _doDoubleClick() {
    dump('doing double click with ' + this._downUpEvents.length + '\n');
    for (let i = 0; i < this._downUpEvents.length; ++i)
      dump('      ' + this._downUpEvents[i].event.type
           + " :: " + this._downUpEvents[i].event.button
           + " :: " + this._downUpEvents[i].event.detail + '\n');

    let mouseUp1 = this._downUpEvents[1].event;
    let mouseUp2 = this._downUpEvents[3].event;
    this._cleanClickBuffer();
    this._clicker.doubleClick(mouseUp1.clientX, mouseUp1.clientY,
			      mouseUp2.clientX, mouseUp2.clientY);
  },

  _cleanClickBuffer: function _cleanClickBuffer() {
    delete this._clickTimeout;
    this._clearDownUpEvents();
  },

  _defaultDragger: {
    dragStart: function dragStart(scroller) {},
    dragStop : function dragStop(dx, dy, scroller) { return this.dragMove(dx, dy, scroller); },
    dragMove : function dragMove(dx, dy, scroller) {
      if (scroller.getPosition) {
        let oldX = {}, oldY = {};
        scroller.getPosition(oldX, oldY);

        scroller.scrollBy(dx, dy);

        let newX = {}, newY = {};
        scroller.getPosition(newX, newY);

        return (newX.value != oldX.value) || (newY.value != oldY.value);
      } else {
        scroller.scrollBy(dx, dy);
        /* always say we scrolled if we can't get the position */
        return true;
      }
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

    while (elem.parentNode) {
      try {

        if (elem.scrollBoxObject) {

          scrollbox = elem;
          qinterface = elem.scrollBoxObject;
          break;
        } else if (elem.boxObject) {

          let qi = (elem._cachedSBO) ? elem._cachedSBO
                                     : elem.boxObject.QueryInterface(Ci.nsIScrollBoxObject);
          if (qi) {
            scrollbox = elem;
            elem._cachedSBO = qinterface = qi;
            break;
          }
        }
      } catch (e) { /* we aren't here to deal with your exceptions, we'll just keep
                       traversing until we find something more well-behaved, as we
                       prefer default behaviour to whiny scrollers. */ }

      elem = elem.parentNode;
    }
    return [scrollbox, qinterface];
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

  /**
   * Initialize a new mouse event the same as that given so that it can be
   * re-dispatched.
   */
  cloneMouseEvent: function cloneMouseEvent(aEvent) {
    let clickEvent = document.createEvent("MouseEvent");
    clickEvent.initMouseEvent(aEvent.type, aEvent.bubbles, aEvent.cancelable,
                              aEvent.view, aEvent.detail,
                              aEvent.screenX, aEvent.screenY, aEvent.clientX, aEvent.clientY,
                              aEvent.ctrlKey, aEvent.altKey, aEvent.shiftKeyArg, aEvent.metaKeyArg,
                              aEvent.button, aEvent.relatedTarget);
    return clickEvent;
  }

};


/**
 * Drag Data is used by both chrome and content input modules
 */
function DragData(owner, dragRadius, dragStartTimeoutLength) {
  this._owner = owner;
  this._dragRadius = dragRadius;
  this.reset();
};

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
    this.setDragPosition(screenX, screenY);
    this._originX = screenX;
    this._originY = screenY;
    this.dragging = true;
  },

  lockMouseMove: function lockMouseMove(sX, sY) {
    if (this.lockedX !== null)
      sX = this.lockedX;
    else if (this.lockedY !== null)
      sY = this.lockedY;
    return [sX, sY];
  },

  lockAxis: function lockAxis(sX, sY) {
    if (this.alreadyLocked)
      return this.lockMouseMove(sX, sY);

    // look at difference from stored coord to lock movement, but only
    // do it if initial movement is sufficient to detect intent
    let absX = Math.abs(this.sX - sX);
    let absY = Math.abs(this.sY - sY);

    // lock panning if we move more than half of the drag radius and that direction
    // contributed more than 2/3rd to the radial movement
    if ((absX > (this._dragRadius / 2)) && ((absX * absX) > (2 * absY * absY))) {
      this.lockedY = this.sY;
      sY = this.sY;
    }
    else if ((absY > (this._dragRadius / 2)) && ((absY * absY) > (2 * absX * absX))) {
      this.lockedX = this.sX;
      sX = this.sX;
    }
    this.alreadyLocked = true;

    return [sX, sY];
  },

  isPointOutsideRadius: function isPointOutsideRadius(sX, sY) {
    if (this._originX == undefined)
      return false;
    return (Math.pow(sX - this._originX, 2) + Math.pow(sY - this._originY, 2)) >
      (2 * Math.pow(this._dragRadius, 2));
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

        //dump("             speeds: " + self._speedX + " " + self._speedY + "\n");

        if (self._speedX == 0 && self._speedY == 0) {
          self.end();
          return;
        }
        let dx = Math.round(self._speedX * self._updateInterval);
        let dy = Math.round(self._speedY * self._updateInterval);
        //dump("dx, dy: " + dx + " " + dy + "\n");

        let panned = self._panBy(-dx, -dy);
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

    this.momentumBuffer.push({'t': now, 'sx' : sx, 'sy' : sy});
  }
};


function ContentPanningModule(owner, browserCanvas, useEarlyMouseMoves) {
  this._owner = owner;
  this._browserCanvas = browserCanvas;
  this._dragData = new DragData(this, 50, 200);
  this._useEarlyMouseMoves = useEarlyMouseMoves;

  var self = this;
  this._kinetic = new KineticController( function (dx, dy) { return self._dragBy(dx, dy); } );
}

ContentPanningModule.prototype = {
  handleEvent: function handleEvent(aEvent) {
    // exit early for events outside displayed content area
    if (aEvent.target !== this._browserCanvas)
      return;

    switch (aEvent.type) {
      case "mousedown":
        this._onMouseDown(aEvent);
        break;
      case "mousemove":
        this._onMouseMove(aEvent);
        break;
      case "mouseout":
      case "mouseup":
        this._onMouseUp(aEvent);
        break;
    }
  },


  /* If someone else grabs events ahead of us, cancel any pending
   * timeouts we may have.
   */
  cancelPending: function cancelPending() {
    this._kinetic.end();
    this._dragData.reset();
  },

  _dragStart: function _dragStart(sX, sY) {
    let dragData = this._dragData;

    dragData.setDragStart(sX, sY);

    [sX, sY] = dragData.lockAxis(sX, sY);

    //ws.dragStart(sX, sY);

    //Browser.canvasBrowser.startPanning();
  },

  _dragStop: function _dragStop(sX, sY) {
    let dragData = this._dragData;

    this._owner.ungrab(this);

    [sX, sY] = dragData.lockMouseMove(sX, sY);

    // start kinetic scrolling here for canvas only
    this._kinetic.start(sX, sY);

    dragData.reset();
  },

  _dragBy: function _dragBy(dx, dy) {
    /* XXX
    let panned = ws.dragBy(dx, dy);
    return panned;
    */
    return false;
  },

  _dragMove: function _dragMove(sX, sY) {
    let dragData = this._dragData;
    [sX, sY] = dragData.lockMouseMove(sX, sY);
    //XXX let panned = ws.dragMove(sX, sY);
    let panned = false;
    dragData.setDragPosition(sX, sY);
    return panned;
  },

  _onMouseDown: function _onMouseDown(aEvent) {
    let dragData = this._dragData;
    // if we're in the process of kineticly scrolling, stop and start over
    if (this._kinetic.isActive()) {
      this._kinetic.end();
      this._owner.ungrab(this);
      dragData.reset();
    }

    this._dragStart(aEvent.screenX, aEvent.screenY);
    this._onMouseMove(aEvent); // treat this as a mouse move too
  },

  _onMouseUp: function _onMouseUp(aEvent) {
    let dragData = this._dragData;

    if (dragData.dragging) {
      this._onMouseMove(aEvent); // treat this as a mouse move, incase our x/y are different
      this._dragStop(aEvent.screenX, aEvent.screenY);
    }

    dragData.reset(); // be sure to reset the timer
  },

  _onMouseMove: function _onMouseMove(aEvent) {
    // don't do anything if we're in the process of kineticly scrolling
    if (this._kinetic.isActive())
      return;

    let dragData = this._dragData;

    // if we move enough, start a grab to prevent click from getting events
    if (dragData.isPointOutsideRadius(aEvent.screenX, aEvent.screenY))
      this._owner.grab(this);

    // if we never received a mouseDown, we need to go ahead and set this data
    if (!dragData.sX)
      dragData.setDragPosition(aEvent.screenX, aEvent.screenY);

    let [sX, sY] = dragData.lockMouseMove(aEvent.screenX, aEvent.screenY);

    // even if we haven't started dragging yet, we should queue up the
    // mousemoves in case we do start
    if (this._useEarlyMouseMoves || dragData.dragging)
      this._kinetic.addData(sX, sY);

    if (dragData.dragging)
      this._dragMove(sX, sY);
  }
};


/**
 * Mouse click handlers
 */
function ContentClickingModule(owner) {
  this._owner = owner;
  this._clickTimeout = -1;
  this._events = [];
  this._zoomedTo = null;
}

ContentClickingModule.prototype = {
  handleEvent: function handleEvent(aEvent) {
    switch (aEvent.type) {
      // UI panning events
      case "mousedown":
        this._events.push({event: aEvent, time: Date.now()});


      case "mouseup":
        // keep an eye out for mouseups that didn't start with a mousedown
        if (!(this._events.length % 2)) {
          this._reset();
            break;
        }

        this._events.push({event: aEvent, time: Date.now()});

        if (this._clickTimeout == -1) {
          this._clickTimeout = window.setTimeout(function _clickTimeout(self) { self._sendSingleClick(); }, 400, this);
        } else {
          window.clearTimeout(this._clickTimeout);
          this._clickTimeout = -1;
          this._sendDoubleClick();
        }
        break;
    }
  },

  /* If someone else grabs events ahead of us, cancel any pending
   * timeouts we may have.
   */
  cancelPending: function cancelPending() {
    this._reset();
  },

  _reset: function _reset() {
    if (this._clickTimeout != -1)
      window.clearTimeout(this._clickTimeout);
    this._clickTimeout = -1;

    this._events = [];
  },

  _sendSingleClick: function _sendSingleClick() {
    this._owner.grab(this);
    this._dispatchContentMouseEvent(this._events[0].event);
    this._dispatchContentMouseEvent(this._events[1].event);
    this._owner.ungrab(this);

    this._reset();
  },

  _sendDoubleClick: function _sendDoubleClick() {
    this._owner.grab(this);

    function optimalElementForPoint(cX, cY) {
      var element = Browser.canvasBrowser.elementFromPoint(cX, cY);
      return element;
    }

    let firstEvent = this._events[0].event;
    let zoomElement = optimalElementForPoint(firstEvent.clientX, firstEvent.clientY);

    if (zoomElement) {
      if (zoomElement != this._zoomedTo) {
        this._zoomedTo = zoomElement;
        Browser.canvasBrowser.zoomToElement(zoomElement);
      } else {
        this._zoomedTo = null;
        Browser.canvasBrowser.zoomFromElement(zoomElement);
      }
    }

    this._owner.ungrab(this);

    this._reset();
  },


  _dispatchContentMouseEvent: function _dispatchContentMouseEvent(aEvent, aType) {
    if (!(aEvent instanceof MouseEvent)) {
      Cu.reportError("_dispatchContentMouseEvent called with a non-mouse event");
      return;
    }

    let cb = Browser.canvasBrowser;
    var [x, y] = cb._clientToContentCoords(aEvent.clientX, aEvent.clientY);
    var cwu = cb.contentDOMWindowUtils;

    // Redispatch the mouse event, ignoring the root scroll frame
    cwu.sendMouseEvent(aType || aEvent.type,
                       x, y,
                       aEvent.button || 0,
                       aEvent.detail || 1,
                       0, true);
  }
};

/**
 * Scrollwheel zooming handler
 */
function ScrollwheelModule(owner, browserCanvas) {
  this._owner = owner;
  this._browserCanvas = browserCanvas;
}

ScrollwheelModule.prototype = {
  handleEvent: function handleEvent(aEvent) {
    if (aEvent.target !== this._browserCanvas)
      return;

    switch (aEvent.type) {
      // UI panning events
      case "DOMMouseScroll":
        this._owner.grab(this);
        Browser.canvasBrowser.zoom(aEvent.detail);
        this._owner.ungrab(this);
        break;
    }
  },

  /* If someone else grabs events ahead of us, cancel any pending
   * timeouts we may have.
   */
  cancelPending: function cancelPending() {
  }
};
