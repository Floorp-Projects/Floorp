// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
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
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <stuart@mozilla.com>
 *   Brad Lassey <blassey@mozilla.com>
 *   Mark Finkle <mfinkle@mozilla.com>
 *   Gavin Sharp <gavin.sharp@gmail.com>
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
  let stack = document.getElementById("browser-container");
  stack.addEventListener("DOMMouseScroll", this, true);

  let content = document.getElementById("canvas");
  content.addEventListener("mouseout", this, true);
  content.addEventListener("mousedown", this, true);
  content.addEventListener("mouseup", this, true);
  content.addEventListener("mousemove", this, true);
  content.addEventListener("keydown", this, true);
  content.addEventListener("keyup", this, true);

  let prefsvc = Components.classes["@mozilla.org/preferences-service;1"].
    getService(Components.interfaces.nsIPrefBranch2);
  let allowKinetic = prefsvc.getBoolPref("browser.ui.panning.kinetic");
  //let allowKinetic = false;

  if (allowKinetic)
    this._modules.push(new KineticPanningModule(this));
  else
    this._modules.push(new PanningModule(this));

  this._modules.push(new ClickingModule(this));
  this._modules.push(new ScrollwheelModule(this));
}

InputHandler.prototype = {
  _modules : [],
  _grabbed : null,

  grab: function(obj) {
    //dump("grabbing\n");
    this._grabbed = obj;

    for each(mod in this._modules) {
      if (mod != obj)
        mod.cancelPending();
    }
    // only send events to this object
    // call cancel on all modules
  },

  ungrab: function(obj) {
    //dump("unggrabbing\n");
    this._grabbed = null;
    // only send events to this object
    // call cancel on all modules
  },

  handleEvent: function (aEvent) {
    if (this._grabbed) {
      this._grabbed.handleEvent(aEvent);
    } else {
      for each(mod in this._modules)
        mod.handleEvent(aEvent);
    }
  }
};


/**
 * Kinetic panning code
 */

function KineticPanningModule(owner) {
  this._owner = owner;
}

KineticPanningModule.prototype = {
  _owner: null,
  _dragData: {
    dragging: false,
    sX: 0,
    sY: 0,
    dragStartTimeout: -1,

    reset: function() {
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

    reset: function() {
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

  handleEvent: function(aEvent) {
    switch (aEvent.type) {
      case "mousedown":
        return this._onMouseDown(aEvent);
        break;
      case "mousemove":
        return this._onMouseMove(aEvent);
      case "mouseout":
      case "mouseup":
        return this._onMouseUp(aEvent);
    }
  },

  /* If someone else grabs events ahead of us, cancel any pending
   * timeouts we may have.
   */
  cancelPending: function() {
    this._dragData.reset();
    // XXX we should cancel kinetic here as well
    //dump("canceling drag\n");
  },

  _dragStart: function(sX, sY) {
    this._dragData.dragging = true;
    this._dragData.dragStartTimeout = -1;

    // grab all events until we stop the drag
    this._owner.grab(this);

    ws.dragStart(sX, sY);

    // set the kinetic start time
    this._kineticData.lastTime = Date.now();
  },

  _dragStop: function(sX, sY) {
    // start kinetic scrolling here.
    if (!this._startKinetic(sX, sY)) {
      this._endKinetic(sX, sY);
    }
  },

  _dragMove: function(sX, sY) {
    ws.dragMove(sX, sY);
  },

  _onMouseDown: function(aEvent) {
    // if we're in the process of kineticly scrolling, stop and start over
    if (this.kineticHandle != -1)
      this._endKinetic(aEvent.screenX, aEvent.screenY);

    let dragData = this._dragData;

    dragData.sX = aEvent.screenX;
    dragData.sY = aEvent.screenY;

    dragData.dragStartTimeout = setTimeout(function(self, sX, sY) { self._dragStart(sX, sY) },
                                           200, this, aEvent.screenX, aEvent.screenY);
  },
  _onMouseUp: function(aEvent) {
    let dragData = this._dragData;

    if (dragData.dragging)
      this._dragStop(aEvent.screenX, aEvent.screenY);
    else
      this._dragData.reset(); // be sure to reset the timer
  },

  _onMouseMove: function(aEvent) {
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

    // update our kinetic data
    let kineticData = this._kineticData;
    let t = Date.now();
    let dt = t - kineticData.lastTime;
    kineticData.lastTime = t;
    let momentumBuffer = { dx: -dx, dy: -dy, dt: dt }

    kineticData.momentumBuffer[kineticData.momentumBufferIndex] = momentumBuffer;
    kineticData.momentumBufferIndex++;
    kineticData.momentumBufferIndex %= kineticData.momentumBufferSize;
  },

  _startKinetic: function(sX, sY) {
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
    } else if (kineticData.kineticDirY < -0.9) {
      kineticData.kineticDirX = 0;
      kineticData.kineticDirY = -1;
    } else if (kineticData.kineticDirX < -0.9) {
      kineticData.kineticDirX = -1;
      kineticData.kineticDirY = 0;
    } else if (kineticData.kineticDirY > 0.9) {
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
  
  _doKinetic: function(self) {
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

  _endKinetic: function(sX, sY) {
    ws.dragStop(sX, sY);
    this._owner.ungrab(this);
    this._dragData.reset();
    this._kineticData.reset();
  },
};


/**
 * Regular non-kinetic panning code
 */

function PanningModule(owner) {
  this._owner = owner;
}

PanningModule.prototype = {
  _owner: null,
  _dragData: {
    dragging: false,
    sX: 0,
    sY: 0,
    dragStartTimeout: -1,

    reset: function() {
      this.dragging = false;
      this.sX = 0;
      this.sY = 0;
      if (this.dragStartTimeout != -1)
        clearTimeout(this.dragStartTimeout);
      this.dragStartTimeout = -1;
    }
  },

  handleEvent: function(aEvent) {
    switch (aEvent.type) {
      case "mousedown":
        return this._onMouseDown(aEvent);
        break;
      case "mousemove":
        return this._onMouseMove(aEvent);
      case "mouseout":
      case "mouseup":
        return this._onMouseUp(aEvent);
    }
  },

  /* If someone else grabs events ahead of us, cancel any pending
   * timeouts we may have.
   */
  cancelPending: function() {
    this._dragData.reset();
    //dump("canceling drag\n");
  },

  _dragStart: function(sX, sY) {
    //dump("starting drag\n");
    this._dragData.dragging = true;
    this._dragData.dragStartTimeout = -1;

    // grab all events until we stop the drag
    this._owner.grab(this);

    ws.dragStart(sX, sY);
  },

  _dragStop: function(sX, sY) {
    //dump("ending drag\n");
    this._dragData.reset();

    ws.dragStop(sX, sY);

    this._owner.ungrab(this);
  },

  _dragMove: function(sX, sY) {
    //dump("moving drag" + sX + " " + sY + "\n");
    ws.dragMove(sX, sY);
  },

  _onMouseDown: function(aEvent) {
    let dragData = this._dragData;

    dragData.sX = aEvent.screenX;
    dragData.sY = aEvent.screenY;

    dragData.dragStartTimeout = setTimeout(function(self, sX, sY) { self._dragStart(sX, sY) },
                                           200, this, aEvent.screenX, aEvent.screenY);
  },

  _onMouseUp: function(aEvent) {
    let dragData = this._dragData;

    if (dragData.dragging)
      this._dragStop(aEvent.screenX, aEvent.screenY);
    else
      this._dragData.reset(); // be sure to reset the timer
  },

  _onMouseMove: function(aEvent) {
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
  }
};

/**
 * Mouse click handlers
 */

function ClickingModule(owner) {
  this._owner = owner;
}


ClickingModule.prototype = {
  _clickTimeout : -1,
  _events : [],
  _zoomed : false,

  handleEvent: function (aEvent) {
    switch (aEvent.type) {
      // UI panning events
      case "mousedown":
        //dump("mousedown\n");
        this._events.push({event: aEvent, time: Date.now()});

        // we're waiting for a click
        if (this._clickTimeout != -1) {
          // if we just got another mousedown, don't send anything until we get another mousedown
          clearTimeout(this._clickTimeout);
          this.clickTimeout = -1
        }
        break;
      case "mouseup":
        // keep an eye out for mouseups that didn't start with a mousedown
        if (!(this._events.length % 2)) {
          this._reset();
          break;
        }
        
        //dump("mouseup\n");
        this._events.push({event: aEvent, time: Date.now()});

        if (this._clickTimeout == -1) {
          this._clickTimeout = setTimeout(function(self) { self._sendSingleClick() }, 400, this);
        } else {
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
  cancelPending: function() {
    //dump("canceling click\n");

    this._reset();
  },

  _reset: function() {
    if (this._clickTimeout != -1)
      clearTimeout(this._clickTimeout);
    this._clickTimeout = -1;

    this._events = [];
  },

  _sendSingleClick: function() {
    this._owner.grab(this);
    this._redispatchMouseEvent(this._events[0].event);
    this._redispatchMouseEvent(this._events[1].event);
    this._owner.ungrab(this);

    this._reset();
  },

  _sendDoubleClick: function() {
    this._owner.grab(this);

    // XXX disable zooming until it works properly.

    function optimalElementForPoint(cX, cY) {
      var element = Browser.content.elementFromPoint(cX, cY);
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
	Browser.content.zoomFromElement(zoomElement);
      } else {
	// zoom in
	this._zoomed = true;
	Browser.content.zoomToElement(zoomElement);
      }

    }

    this._owner.ungrab(this);

    this._reset();
  },


  _redispatchMouseEvent: function(aEvent, aType) {
    if (!(aEvent instanceof MouseEvent)) {
      Components.utils.reportError("_redispatchMouseEvent called with a non-mouse event");
      return;
    }

    var [x, y] = Browser.content._clientToContentCoords(aEvent.clientX, aEvent.clientY);
    //dump("sending mouse event to: " + x + " " + y + "\n");

    var cwin = Browser.currentBrowser.contentWindow;
    var cwu = cwin.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                  .getInterface(Components.interfaces.nsIDOMWindowUtils);

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
  handleEvent: function (aEvent) {
    switch (aEvent.type) {
      // UI panning events
      case "DOMMouseScroll":
        this._owner.grab(this);
        this.deckbrowser.zoom(aEvent.detail);
        this._owner.ungrab(this);
        break;
    }
  },

  /* If someone else grabs events ahead of us, cancel any pending
   * timeouts we may have.
   */
  cancelPending: function() {
  }
};

