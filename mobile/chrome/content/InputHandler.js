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
      else {
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

function dumpEvent(aEvent) {
  dump("{ ");

  for(var item in aEvent) {
    var value = aEvent[item];
	  dump(item + ": ");
    dump(value);
    dump(", ");
	}
  dump("}\n");
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
  let stack = document.getElementById("browser-container");
  stack.addEventListener("DOMMouseScroll", this, true);

  /* these handle dragging of both chrome elements and content */
  stack.addEventListener("mouseout", this, true);
  stack.addEventListener("mousedown", this, true);
  stack.addEventListener("mouseup", this, true);
  stack.addEventListener("mousemove", this, true);

  let content = document.getElementById("browser-canvas");
  content.addEventListener("keydown", this, true);
  content.addEventListener("keyup", this, true);

  let prefsvc = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch2);
  let allowKinetic = prefsvc.getBoolPref("browser.ui.panning.kinetic");

  this._modules.push(new ChromeInputModule(this));
  this._modules.push(new ContentPanningModule(this, allowKinetic));
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
    // only send events to this object
    // call cancel on all modules
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
 * Panning code for chrome elements
 */

function ChromeInputModule(owner, useKinetic) {
  this._owner = owner;
  if (useKinetic !== undefined)
    this._dragData.useKinetic = useKinetic;
}

ChromeInputModule.prototype = {
  _owner: null,

  _dragData: {
    dragging: false,
    sX: 0,
    sY: 0,
    dragStartTimeout: -1,
    targetScrollbox: null,

    reset: function reset() {
      this.dragging = false;
      this.sX = 0;
      this.sY = 0;
      if (this.dragStartTimeout != -1)
        clearTimeout(this.dragStartTimeout);
      this.dragStartTimeout = -1;
      this.targetScrollbox = null;
    }
  },

  _clickData: {
    _clickTimeout : -1,
    _events : [],
    reset: function reset() {
      if (this._clickTimeout != -1)
        clearTimeout(this._clickTimeout);
      this._clickTimeout = -1;
      this._events = [];
    },
  },


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
    }
  },

  /* If someone else grabs events ahead of us, cancel any pending
   * timeouts we may have.
   */
  cancelPending: function cancelPending() {
    this._dragData.reset();
  },

  _dragStart: function _dragStart(sX, sY) {
    let dragData = this._dragData;
    dragData.dragging = true;
    dragData.dragStartTimeout = -1;

    // grab all events until we stop the drag
    ws.dragStart(sX, sY);

    // prevent clicks from being sent once we start drag
    this._clickData.reset();
  },

  _dragStop: function _dragStop(sX, sY) {
    let dragData = this._dragData;

    if (dragData.targetScrollbox)
      dragData.targetScrollbox.scrollBy(dragData.sX - sX, dragData.sY - sY);
  },

  _dragMove: function _dragMove(sX, sY) {
    let dragData = this._dragData;
    if (dragData.targetScrollbox)
      dragData.targetScrollbox.scrollBy(dragData.sX - sX, dragData.sY - sY);
  },

  _onMouseDown: function _onMouseDown(aEvent) {
    // exit early for events in the content area
    if (aEvent.target === document.getElementById("browser-canvas")) {
      return;
    }

    let dragData = this._dragData;

    dragData.targetScrollbox = getScrollboxFromElement(aEvent.target);
    if (!dragData.targetScrollbox)
      return;

    // absorb the event for the scrollable XUL element and make all future events grabbed too
    this._owner.grab(this);

    aEvent.stopPropagation();
    aEvent.preventDefault();

    dragData.sX = aEvent.screenX;
    dragData.sY = aEvent.screenY;

    dragData.dragStartTimeout = setTimeout(function(self, sX, sY) { self._dragStart(sX, sY); },
                                  200, this, aEvent.screenX, aEvent.screenY);

    // store away the event for possible sending later if a drag doesn't happen
    let clickData = this._clickData;
    let clickEvent = document.createEvent("MouseEvent");
    clickEvent.initMouseEvent(aEvent.type, aEvent.bubbles, aEvent.cancelable,
                              aEvent.view, aEvent.detail,
                              aEvent.screenX, aEvent.screenY, aEvent.clientX, aEvent.clientY,
                              aEvent.ctrlKey, aEvent.altKey, aEvent.shiftKeyArg, aEvent.metaKeyArg,
                              aEvent.button, aEvent.relatedTarget);
    clickData._events.push({event: clickEvent, target: aEvent.target, time: Date.now()});

    // we're waiting for a click
    if (clickData._clickTimeout != -1) {
      // if we just got another mousedown, don't send anything until we get another mousedown
      clearTimeout(clickData._clickTimeout);
      clickData.clickTimeout = -1;
    }
  },

  _onMouseUp: function _onMouseUp(aEvent) {
    // only process if original mousedown was on a scrollable element
    let dragData = this._dragData;
    if (!dragData.targetScrollbox)
      return;

    let clickData = this._clickData;

    // keep an eye out for mouseups that didn't start with a mousedown
    if (!(clickData._events.length % 2)) {
      clickData.reset();
    }
    else {
      let clickEvent = document.createEvent("MouseEvent");
      clickEvent.initMouseEvent(aEvent.type, aEvent.bubbles, aEvent.cancelable,
                                aEvent.view, aEvent.detail,
                                aEvent.screenX, aEvent.screenY, aEvent.clientX, aEvent.clientY,
                                aEvent.ctrlKey, aEvent.altKey, aEvent.shiftKeyArg, aEvent.metaKeyArg,
                                aEvent.button, aEvent.relatedTarget);
      clickData._events.push({event: clickEvent, target: aEvent.target, time: Date.now()});

      if (clickData._clickTimeout == -1) {
        clickData._clickTimeout = setTimeout(function(self) { self._sendSingleClick(); }, 400, this);
      }
      else {
        clearTimeout(clickData._clickTimeout);
        // this._sendDoubleClick();
      }
    }

    aEvent.stopPropagation();
    aEvent.preventDefault();

    if (dragData.dragging)
      this._dragStop(aEvent.screenX, aEvent.screenY);

    dragData.reset(); // be sure to reset the timer
    this._owner.ungrab(this);
  },

  _onMouseMove: function _onMouseMove(aEvent) {
    let dragData = this._dragData;

    // only process if original mousedown was on a scrollable element
    if (!dragData.targetScrollbox)
      return;

    aEvent.stopPropagation();
    aEvent.preventDefault();

    let dx = dragData.sX - aEvent.screenX;
    let dy = dragData.sY - aEvent.screenY;

    if (!dragData.dragging && dragData.dragStartTimeout != -1) {
      if ((Math.abs(dx*dx) + Math.abs(dy*dy)) > 100) {
        clearTimeout(dragData.dragStartTimeout);
        this._dragStart(aEvent.screenX, aEvent.screenY);
      }
    }
    if (!dragData.dragging)
      return;

    this._dragMove(aEvent.screenX, aEvent.screenY);

    dragData.sX = aEvent.screenX;
    dragData.sY = aEvent.screenY;
  },


  // resend original events with our handler out of the loop
  _sendSingleClick: function _sendSingleClick() {
    let clickData = this._clickData;

    this._owner.grab(this);
    this._owner.stopListening();

    // send original mouseDown/mouseUps again
    this._redispatchChromeMouseEvent(clickData._events[0].event);
    this._redispatchChromeMouseEvent(clickData._events[1].event);

    this._owner.startListening();
    this._owner.ungrab(this);

    clickData.reset();
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

function ContentPanningModule(owner, useKinetic) {
  this._owner = owner;
  if (useKinetic !== undefined)
    this._dragData.useKinetic = useKinetic;
}

ContentPanningModule.prototype = {
  _owner: null,
  _dragData: {
    dragging: false,
    sX: 0,
    sY: 0,
    useKinetic: true,
    dragStartTimeout: -1,

    reset: function reset() {
      this.dragging = false;
      this.sX = 0;
      this.sY = 0;
      if (this.dragStartTimeout != -1)
        clearTimeout(this.dragStartTimeout);
      this.dragStartTimeout = -1;
    }
  },

  _kineticData: {
    // const
    kineticStepSize: 15,
    kineticDecelloration: 0.004,
    momentumBufferSize: 3,

    momentumBuffer: [],
    momentumBufferIndex: 0,
    lastTime: 0,
    kineticDuration: 0,
    kineticDirX: 0,
    kineticDirY: 0,
    kineticHandle : -1,
    kineticStep : 0,
    kineticStartX : 0,
    kineticStartY : 0,
    kineticInitialVel: 0,

    reset: function reset() {
      if (this.kineticHandle != -1) {
        window.clearInterval(this.kineticHandle);
        this.kineticHandle = -1;
      }

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
  },

  handleEvent: function handleEvent(aEvent) {
    // exit early for events outside displayed content area
    if (aEvent.target !== document.getElementById("browser-canvas"))
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
    this._dragData.reset();
    // XXX we should cancel kinetic here as well
  },

  _dragStart: function _dragStart(sX, sY) {
    let dragData = this._dragData;
    dragData.dragging = true;
    dragData.dragStartTimeout = -1;

    // grab all events until we stop the drag
    this._owner.grab(this);
    ws.dragStart(sX, sY);

    // set the kinetic start time
    this._kineticData.lastTime = Date.now();
  },

  _dragStop: function _dragStop(sX, sY) {
    let dragData = this._dragData;

    this._owner.ungrab(this);

    if (dragData.useKinetic) {
      // start kinetic scrolling here for canvas only
      if (!this._startKinetic(sX, sY))
        this._endKinetic(sX, sY);
    }
    else {
      ws.dragStop(sX, sY);
    }
  },

  _dragMove: function _dragMove(sX, sY) {
    let dragData = this._dragData;
    ws.dragMove(sX, sY);
  },

  _onMouseDown: function _onMouseDown(aEvent) {
    // if we're in the process of kineticly scrolling, stop and start over
    if (this.kineticHandle != -1)
      this._endKinetic(aEvent.screenX, aEvent.screenY);

    let dragData = this._dragData;

    dragData.sX = aEvent.screenX;
    dragData.sY = aEvent.screenY;

    dragData.dragStartTimeout = setTimeout(function(self, sX, sY) { self._dragStart(sX, sY); },
                                           200, this, aEvent.screenX, aEvent.screenY);
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

    let dx = dragData.sX - aEvent.screenX;
    let dy = dragData.sY - aEvent.screenY;

    if (!dragData.dragging && dragData.dragStartTimeout != -1) {
      if ((Math.abs(dx*dx) + Math.abs(dy*dy)) > 100) {
        clearTimeout(dragData.dragStartTimeout);
        this._dragStart(aEvent.screenX, aEvent.screenY);
      }
    }
    if (!dragData.dragging)
      return;

    this._dragMove(aEvent.screenX, aEvent.screenY);

    dragData.sX = aEvent.screenX;
    dragData.sY = aEvent.screenY;

    if (dragData.useKinetic) {
      // update our kinetic data
      let kineticData = this._kineticData;
      let t = Date.now();
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
    if (kineticData.kineticInitialVel)
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
    ws.dragStop(sX, sY);
    this._owner.ungrab(this);
    this._dragData.reset();
    this._kineticData.reset();

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
          this._sendDoubleClick();
        }
        break;
      case "mouseout":
        this._reset();
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
