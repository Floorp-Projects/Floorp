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

function getScrollboxFromElement(elem) {
  // check element for scrollable interface, if not found check parent until we get to root
  let scrollbox = null;

  while (elem.parentNode) {
    try {
      if ("scrollBoxObject" in elem && elem.scrollBoxObject) {
        scrollbox = elem.scrollBoxObject;
        break;
      }
      else if (elem.boxObject) {
        scrollbox = elem.boxObject.QueryInterface(Ci.nsIScrollBoxObject);
        break;
      }
    }
    catch (e) {
      // an exception is OK, we just don't want to propogate it
    }
    elem = elem.parentNode;
  }
  return scrollbox;
}

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
  /* used to stop everything if mouse leaves window on desktop */
  window.addEventListener("mouseout", this, true);

  /* these handle dragging of both chrome elements and content */
  window.addEventListener("mousedown", this, true);
  window.addEventListener("mouseup", this, true);
  window.addEventListener("mousemove", this, true);
  window.addEventListener("click", this, true);

  let stack = document.getElementById("browser-container");
  stack.addEventListener("DOMMouseScroll", this, true);

  let browserCanvas = document.getElementById("browser-canvas");
  browserCanvas.addEventListener("keydown", this, true);
  browserCanvas.addEventListener("keyup", this, true);

  let prefsvc = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch2);
  let allowKinetic = prefsvc.getBoolPref("browser.ui.panning.kinetic");

  this._modules.push(new ChromeInputModule(this, browserCanvas));
  this._modules.push(new ContentPanningModule(this, browserCanvas, allowKinetic));
  this._modules.push(new ContentClickingModule(this));
  this._modules.push(new ScrollwheelModule(this));
}

InputHandler.prototype = {
  _modules : [],
  _grabbed : null,
  _ignoreEvents: false,

  grab: function grab(obj) {
    this._grabbed = obj;

    for each(mod in this._modules) {
      if (mod != obj)
        mod.cancelPending();
    }
    // only send events to this object
    // call cancel on all modules
  },

  ungrab: function ungrab(obj) {
    this._grabbed = null;
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

    // relatedTarget should only be NULL if we move out of window
    // if so, ungrab and reset everything.  We don't always get
    // mouseout events if the mouse movement causes other window
    // activity, but this catches many of the cases
    if (aEvent.type == "mouseout" && !aEvent.relatedTarget) {
      this.grab(null);
      return;
    }

    if (this._grabbed) {
      this._grabbed.handleEvent(aEvent);
    }
    else {
      for each(mod in this._modules) {
        mod.handleEvent(aEvent);
        // if event got grabbed, don't pass to other handlers
        if (this._grabbed)
          break;
      }
    }
  }
};

/**
 * Drag Data is used by both chrome and content input modules
 */

function DragData(owner, dragRadius, dragStartTimeoutLength) {
  this._owner = owner;
  this._dragRadius = dragRadius;
  this._dragStartTimeoutLength = dragStartTimeoutLength;
  this.dragStartTimeout = -1;
  this.reset();
}

DragData.prototype = {
  reset: function reset() {
    this.dragging = false;
    this.sX = 0;
    this.sY = 0;
    this.lockedX = null;
    this.lockedY = null;

    this.clearDragStartTimeout();
  },

  setDragStart: function setDragStart(screenX, screenY) {
    this.sX = screenX;
    this.sY = screenY;
    this.dragStartTimeout = setTimeout(
      function(dragData, sX, sY) { dragData.clearDragStartTimeout(); dragData._owner._dragStart(sX, sY); },
      this._dragStartTimeoutLength,
      this, screenX, screenY);
  },

  clearDragStartTimeout: function clearDragStartTimeout() {
    if (this.dragStartTimeout != -1)
      clearTimeout(this.dragStartTimeout);
    this.dragStartTimeout = -1;
  },

  setLockedAxis: function setLockedAxis(sX, sY) {
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

    return [sX, sY];
  },

  lockMouseMove: function lockMouseMove(sX, sY) {
    if (this.lockedX !== null)
      sX = this.lockedX;
    else if (this.lockedY !== null)
      sY = this.lockedY;

    return [sX, sY];
  },

  /* returns true if we go ahead and start a drag */
  detectEarlyDrag: function detectEarlyDrag(sX, sY) {
    let dx = this.sX - sX;
    let dy = this.sY - sY;

    if (!this.dragging && this.dragStartTimeout != -1) {
      if ((dx*dx + dy*dy) > (this._dragRadius * this._dragRadius)) {
        this.clearDragStartTimeout();
        this._owner._dragStart(sX, sY);
        return true;
      }
    }
    return false;
  }
};


/**
 * Panning code for chrome elements
 */

function ChromeInputModule(owner, browserCanvas) {
  this._owner = owner;
  this._browserCanvas = browserCanvas;
  this._dragData = new DragData(this, 20, 200);
}

ChromeInputModule.prototype = {
  _owner: null,
  _ignoreNextClick: false,
  _dragData: null,
  _clickEvents : [],
  _targetScrollbox: null,

  handleEvent: function handleEvent(aEvent) {
    switch (aEvent.type) {
      case "mousedown":
        this._onMouseDown(aEvent);
        break;
      case "mousemove":
        this._onMouseMove(aEvent);
        break;
      case "mouseup":
        this._onMouseUp(aEvent);
        break;
      case "click":
        if (this._ignoreNextClick) {
          aEvent.stopPropagation();
          aEvent.preventDefault();
          this._ignoreNextClick = false;
        }
        break;
    }
  },

  /* If someone else grabs events ahead of us, cancel any pending
   * timeouts we may have.
   */
  cancelPending: function cancelPending() {
    this._dragData.reset();
    this._targetScrollbox = null;
  },

  _dragStart: function _dragStart(sX, sY) {
    let dragData = this._dragData;
    dragData.dragging = true;

    [sX, sY] = dragData.setLockedAxis(sX, sY);

    // grab all events until we stop the drag
    ws.dragStart(sX, sY);

    // prevent clicks from being sent once we start drag
    this._clickEvents = [];
  },

  _dragStop: function _dragStop(sX, sY) {
    let dragData = this._dragData;
    [sX, sY] = dragData.lockMouseMove(sX, sY);
    if (this._targetScrollbox)
      this._targetScrollbox.scrollBy(dragData.sX - sX, dragData.sY - sY);
    this._targetScrollbox = null;
  },

  _dragMove: function _dragMove(sX, sY) {
    let dragData = this._dragData;
    [sX, sY] = dragData.lockMouseMove(sX, sY);
    if (this._targetScrollbox)
      this._targetScrollbox.scrollBy(dragData.sX - sX, dragData.sY - sY);
    dragData.sX = sX;
    dragData.sY = sY;
  },

  _onMouseDown: function _onMouseDown(aEvent) {
    // exit early for events in the content area
    if (aEvent.target === this._browserCanvas) {
      return;
    }

    let dragData = this._dragData;

    this._targetScrollbox = getScrollboxFromElement(aEvent.target);
    if (!this._targetScrollbox)
      return;

    // absorb the event for the scrollable XUL element and make all future events grabbed too
    this._owner.grab(this);

    aEvent.stopPropagation();
    aEvent.preventDefault();

    dragData.setDragStart(aEvent.screenX, aEvent.screenY);

    // store away the event for possible sending later if a drag doesn't happen
    let clickEvent = document.createEvent("MouseEvent");
    clickEvent.initMouseEvent(aEvent.type, aEvent.bubbles, aEvent.cancelable,
                              aEvent.view, aEvent.detail,
                              aEvent.screenX, aEvent.screenY, aEvent.clientX, aEvent.clientY,
                              aEvent.ctrlKey, aEvent.altKey, aEvent.shiftKeyArg, aEvent.metaKeyArg,
                              aEvent.button, aEvent.relatedTarget);
    this._clickEvents.push({event: clickEvent, target: aEvent.target, time: Date.now()});
  },

  _onMouseUp: function _onMouseUp(aEvent) {
    // only process if original mousedown was on a scrollable element
    let dragData = this._dragData;
    if (!this._targetScrollbox)
      return;

    // keep an eye out for mouseups that didn't start with a mousedown
    if (!(this._clickEvents.length % 2)) {
      this._clickEvents = [];
    }
    else {
      let clickEvent = document.createEvent("MouseEvent");
      clickEvent.initMouseEvent(aEvent.type, aEvent.bubbles, aEvent.cancelable,
                                aEvent.view, aEvent.detail,
                                aEvent.screenX, aEvent.screenY, aEvent.clientX, aEvent.clientY,
                                aEvent.ctrlKey, aEvent.altKey, aEvent.shiftKeyArg, aEvent.metaKeyArg,
                                aEvent.button, aEvent.relatedTarget);
      this._clickEvents.push({event: clickEvent, target: aEvent.target, time: Date.now()});

      this._ignoreNextClick = true;
      this._sendSingleClick();
    }

    aEvent.stopPropagation();
    aEvent.preventDefault();

    if (dragData.dragging)
      this._dragStop(aEvent.screenX, aEvent.screenY);

    dragData.reset(); // be sure to reset the timer
    this._targetScrollbox = null;
    this._owner.ungrab(this);
  },

  _onMouseMove: function _onMouseMove(aEvent) {
    let dragData = this._dragData;

    // only process if original mousedown was on a scrollable element
    if (!this._targetScrollbox)
      return;

    aEvent.stopPropagation();
    aEvent.preventDefault();

    let sX = aEvent.screenX;
    let sY = aEvent.screenY;

    if (dragData.detectEarlyDrag(sX, sY))
      return;

    if (!dragData.dragging)
      return;

    [sX, sY] = dragData.lockMouseMove(sX, sY);
    this._dragMove(sX, sY);
  },


  // resend original events with our handler out of the loop
  _sendSingleClick: function _sendSingleClick() {
    this._owner.grab(this);
    this._owner.stopListening();

    // send original mouseDown/mouseUps again
    this._redispatchChromeMouseEvent(this._clickEvents[0].event);
    this._redispatchChromeMouseEvent(this._clickEvents[1].event);

    this._owner.startListening();
    this._owner.ungrab(this);

    this._clickEvents = [];
  },

  _redispatchChromeMouseEvent: function _redispatchChromeMouseEvent(aEvent) {
    if (!(aEvent instanceof MouseEvent)) {
      Cu.reportError("_redispatchChromeMouseEvent called with a non-mouse event");
      return;
    }

    var cwu = window.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);

    // Redispatch the mouse event, ignoring the root scroll frame
    cwu.sendMouseEvent(aEvent.type, aEvent.clientX, aEvent.clientY,
                       aEvent.button, aEvent.detail, 0, true);
  }
};

/**
 * Kinetic panning code for content
 */

function KineticData() {
  this.kineticHandle = -1;
  this.reset();
}

KineticData.prototype = {
  reset: function reset() {
    if (this.kineticHandle != -1) {
      window.clearInterval(this.kineticHandle);
      this.kineticHandle = -1;
    }

    this.kineticStepSize = 15;
    this.kineticDecelloration = 0.004;
    this.momentumBufferSize = 3;
    this.momentumBuffer = [];
    this.momentumBufferIndex = 0;
    this.lastTime = 0;
    this.kineticDuration = 0;
    this.kineticDirX = 0;
    this.kineticDirY = 0;
    this.kineticStep  = 0;
    this.kineticStartX  = 0;
    this.kineticStartY  = 0;
    this.kineticInitialVel = 0;
  }
};

function ContentPanningModule(owner, browserCanvas, useKinetic) {
  this._owner = owner;
  if (useKinetic !== undefined)
    this._useKinetic = useKinetic;
  this._browserCanvas = browserCanvas;
  this._dragData = new DragData(this, 20, 200);
  this._kineticData = new KineticData();
}

ContentPanningModule.prototype = {
  _owner: null,
  _dragData: null,

  _useKinetic: true,
  _kineticData: null,

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
      case "mouseup":
        this._onMouseUp(aEvent);
        break;
    }
  },

  /* If someone else grabs events ahead of us, cancel any pending
   * timeouts we may have.
   */
  cancelPending: function cancelPending() {
    let dragData = this._dragData;
    // stop scrolling, pass last coordinate we used
    this._endKinetic(dragData.sX, dragData.sY);
    dragData.reset();
  },

  _dragStart: function _dragStart(sX, sY) {
    let dragData = this._dragData;
    dragData.dragging = true;

    [sX, sY] = dragData.setLockedAxis(sX, sY);

    // grab all events until we stop the drag
    this._owner.grab(this);
    ws.dragStart(sX, sY);

    Browser.canvasBrowser.startPanning();

    // set the kinetic start time
    this._kineticData.lastTime = Date.now();
  },

  _dragStop: function _dragStop(sX, sY) {
    let dragData = this._dragData;

    this._owner.ungrab(this);

    [sX, sY] = dragData.lockMouseMove(sX, sY);

    if (this._useKinetic) {
      // start kinetic scrolling here for canvas only
      if (!this._startKinetic(sX, sY))
        this._endKinetic(sX, sY);
    }
    else {
      ws.dragStop();
    }

    // flush any paints that might be left so that our next pan will be fast
    Browser.canvasBrowser.endPanning();
  },

  _dragMove: function _dragMove(sX, sY) {
    let dragData = this._dragData;
    [sX, sY] = dragData.lockMouseMove(sX, sY);
    ws.dragMove(sX, sY);
    dragData.sX = sX;
    dragData.sY = sY;
  },

  _onMouseDown: function _onMouseDown(aEvent) {
    // if we're in the process of kineticly scrolling, stop and start over
    if (this._kineticData.kineticHandle != -1)
      this._endKinetic(aEvent.screenX, aEvent.screenY);

    this._dragData.setDragStart(aEvent.screenX, aEvent.screenY);
  },

  _onMouseUp: function _onMouseUp(aEvent) {
    let dragData = this._dragData;

    if (dragData.dragging)
      this._dragStop(aEvent.screenX, aEvent.screenY);

    dragData.reset(); // be sure to reset the timer
  },

  _onMouseMove: function _onMouseMove(aEvent) {
    // don't do anything if we're in the process of kineticly scrolling
    if (this._kineticData.kineticHandle != -1)
      return;

    let dragData = this._dragData;

    let sX = aEvent.screenX;
    let sY = aEvent.screenY;

    if (dragData.detectEarlyDrag(sX, sY))
      return;

    if (!dragData.dragging)
      return;

    this._dragMove(sX, sY);

    if (this._useKinetic) {
      // update our kinetic data
      let kineticData = this._kineticData;
      let t = Date.now();
      let dx = dragData.sX - sX;
      let dy = dragData.sY - sY;
      let dt = t - kineticData.lastTime;
      kineticData.lastTime = t;
      let momentumBuffer = { dx: -dx, dy: -dy, dt: dt };

      kineticData.momentumBuffer[kineticData.momentumBufferIndex] = momentumBuffer;
      kineticData.momentumBufferIndex++;
      kineticData.momentumBufferIndex %= kineticData.momentumBufferSize;
    }
  },

  _startKinetic: function _startKinetic(sX, sY) {
    let kineticData = this._kineticData;

    let dx = 0;
    let dy = 0;
    let dt = 0;
    if (kineticData.kineticInitialVel != 0)
      return true;

    if (!kineticData.momentumBuffer)
      return false;

    for (let i = 0; i < kineticData.momentumBufferSize; i++) {
      let me = kineticData.momentumBuffer[(kineticData.momentumBufferIndex + i) % kineticData.momentumBufferSize];
      if (!me)
        return false;

      dx += me.dx;
      dy += me.dy;
      dt += me.dt;
    }
    if (dt <= 0)
      return false;

    let dist = Math.sqrt(dx*dx+dy*dy);
    let vel  = dist/dt;
    if (vel < 1)
      return false;

    kineticData.kineticDirX = dx/dist;
    kineticData.kineticDirY = dy/dist;
    if (kineticData.kineticDirX > 0.9) {
      kineticData.kineticDirX = 1;
      kineticData.kineticDirY = 0;
    }
    else if (kineticData.kineticDirY < -0.9) {
      kineticData.kineticDirX = 0;
      kineticData.kineticDirY = -1;
    }
    else if (kineticData.kineticDirX < -0.9) {
      kineticData.kineticDirX = -1;
      kineticData.kineticDirY = 0;
    }
    else if (kineticData.kineticDirY > 0.9) {
      kineticData.kineticDirX = 0;
      kineticData.kineticDirY = 1;
    }

    kineticData.kineticDuration = vel/(2 * kineticData.kineticDecelloration);
    kineticData.kineticStep = 0;
    kineticData.kineticStartX =  sX;
    kineticData.kineticStartY =  sY;
    kineticData.kineticInitialVel = vel;
    kineticData.kineticHandle = window.setInterval(this._doKinetic, kineticData.kineticStepSize, this);
    return true;
  },

  _doKinetic: function _doKinetic(self) {
    let kineticData = self._kineticData;

    let t = kineticData.kineticStep * kineticData.kineticStepSize;
    kineticData.kineticStep++;
    if (t > kineticData.kineticDuration)
      t = kineticData.kineticDuration;
    let dist = kineticData.kineticInitialVel * t -
               kineticData.kineticDecelloration * t * t;
    let newX = Math.floor(kineticData.kineticDirX * dist + kineticData.kineticStartX);
    let newY = Math.floor(kineticData.kineticDirY * dist + kineticData.kineticStartY);

    self._dragMove(newX, newY);

    if(t >= kineticData.kineticDuration)
      self._endKinetic(newX, newY);
  },

  _endKinetic: function _endKinetic(sX, sY) {
    let kineticData = this._kineticData;

    ws.dragStop();
    this._owner.ungrab(this);
    this._dragData.reset();
    kineticData.reset();

    // Make sure that sidebars don't stay partially open
    // XXX this should live somewhere else
    let [leftVis,] = ws.getWidgetVisibility("tabs-container", false);
    let [rightVis,] = ws.getWidgetVisibility("browser-controls", false);
    if (leftVis != 0 && leftVis != 1) {
      let w = document.getElementById("tabs-container").getBoundingClientRect().width;
      if (leftVis >= 0.6666)
        ws.panBy(-w, 0, true);
      else
        ws.panBy(leftVis * w, 0, true);
    }
    else if (rightVis != 0 && rightVis != 1) {
      let w = document.getElementById("browser-controls").getBoundingClientRect().width;
      if (rightVis >= 0.6666)
        ws.panBy(w, 0, true);
      else
        ws.panBy(-rightVis * w, 0, true);
    }
  }
};

/**
 * Mouse click handlers
 */

function ContentClickingModule(owner) {
  this._owner = owner;
}


ContentClickingModule.prototype = {
  _clickTimeout : -1,
  _events : [],
  _zoomed : false,

  handleEvent: function handleEvent(aEvent) {
    // exit early for events outside displayed content area
    if (aEvent.target !== document.getElementById("browser-canvas"))
      return;

    switch (aEvent.type) {
      // UI panning events
      case "mousedown":
        this._events.push({event: aEvent, time: Date.now()});

        // we're waiting for a click
        if (this._clickTimeout != -1) {
          // if we just got another mousedown, don't send anything until we get another mousedown
          clearTimeout(this._clickTimeout);
          this.clickTimeout = -1;
        }
        break;
      case "mouseup":
        // keep an eye out for mouseups that didn't start with a mousedown
        if (!(this._events.length % 2)) {
          this._reset();
          break;
        }

        this._events.push({event: aEvent, time: Date.now()});

        if (this._clickTimeout == -1) {
          this._clickTimeout = setTimeout(function(self) { self._sendSingleClick(); }, 400, this);
        }
        else {
          clearTimeout(this._clickTimeout);
          this.clickTimeout = -1;
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
      clearTimeout(this._clickTimeout);
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
      if (!element)
        return null;

      // Find the nearest non-inline ancestor
      while (element.parentNode) {
        let display = window.getComputedStyle(element, "").getPropertyValue("display");
        let zoomable = /table/.test(display) || /block/.test(display);
        if (zoomable)
          break;

        element = element.parentNode;
      }
      return element;
    }

    let firstEvent = this._events[0].event;
    let zoomElement = optimalElementForPoint(firstEvent.clientX, firstEvent.clientY);

    if (zoomElement) {
      if (this._zoomed) {
        // zoom out
        this._zoomed = false;
        Browser.canvasBrowser.zoomFromElement(zoomElement);
      }
      else {
        // zoom in
        this._zoomed = true;
        Browser.canvasBrowser.zoomToElement(zoomElement);
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

function ScrollwheelModule(owner) {
  this._owner = owner;
}

ScrollwheelModule.prototype = {
  handleEvent: function handleEvent(aEvent) {
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
