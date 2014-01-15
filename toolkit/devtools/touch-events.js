/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let {CC, Cc, Ci, Cu, Cr} = require('chrome');

Cu.import('resource://gre/modules/Services.jsm');

let handlerCount = 0;

let orig_w3c_touch_events = Services.prefs.getIntPref('dom.w3c_touch_events.enabled');

let trackedWindows = new WeakMap();

// =================== Touch ====================
// Simulate touch events on desktop
function TouchEventHandler (window) {
  // Returns an already instanciated handler for this window
  let cached = trackedWindows.get(window);
  if (cached) {
    return cached;
  }

  let contextMenuTimeout = 0;


  let threshold = 25;
  try {
    threshold = Services.prefs.getIntPref('ui.dragThresholdX');
  } catch(e) {}

  let delay = 500;
  try {
    delay = Services.prefs.getIntPref('ui.click_hold_context_menus.delay');
  } catch(e) {}

  let TouchEventHandler = {
    enabled: false,
    events: ['mousedown', 'mousemove', 'mouseup'],
    start: function teh_start() {
      if (this.enabled)
        return false;
      this.enabled = true;
      let isReloadNeeded = Services.prefs.getIntPref('dom.w3c_touch_events.enabled') != 1;
      Services.prefs.setIntPref('dom.w3c_touch_events.enabled', 1);
      this.events.forEach((function(evt) {
        // Only listen trusted events to prevent messing with
        // event dispatched manually within content documents
        window.addEventListener(evt, this, true, false);
      }).bind(this));
      return isReloadNeeded;
    },
    stop: function teh_stop() {
      if (!this.enabled)
        return;
      this.enabled = false;
      Services.prefs.setIntPref('dom.w3c_touch_events.enabled', orig_w3c_touch_events);
      this.events.forEach((function(evt) {
        window.removeEventListener(evt, this, true);
      }).bind(this));
    },
    handleEvent: function teh_handleEvent(evt) {
      // Ignore all but real mouse event coming from physical mouse
      // (especially ignore mouse event being dispatched from a touch event)
      if (evt.button || evt.mozInputSource != Ci.nsIDOMMouseEvent.MOZ_SOURCE_MOUSE || evt.isSynthesized) {
        return;
      }

      // The gaia system window use an hybrid system even on the device which is
      // a mix of mouse/touch events. So let's not cancel *all* mouse events
      // if it is the current target.
      let content = this.getContent(evt.target);
      let isSystemWindow = content.location.toString().indexOf("system.gaiamobile.org") != -1;

      let eventTarget = this.target;
      let type = '';
      switch (evt.type) {
        case 'mousedown':
          this.target = evt.target;

          contextMenuTimeout =
            this.sendContextMenu(evt.target, evt.pageX, evt.pageY, delay);

          this.cancelClick = false;
          this.startX = evt.pageX;
          this.startY = evt.pageY;

          // Capture events so if a different window show up the events
          // won't be dispatched to something else.
          evt.target.setCapture(false);

          type = 'touchstart';
          break;

        case 'mousemove':
          if (!eventTarget)
            return;

          if (!this.cancelClick) {
            if (Math.abs(this.startX - evt.pageX) > threshold ||
                Math.abs(this.startY - evt.pageY) > threshold) {
              this.cancelClick = true;
              content.clearTimeout(contextMenuTimeout);
            }
          }

          type = 'touchmove';
          break;

        case 'mouseup':
          if (!eventTarget)
            return;
          this.target = null;

          content.clearTimeout(contextMenuTimeout);
          type = 'touchend';

          // Only register click listener after mouseup to ensure
          // catching only real user click. (Especially ignore click
          // being dispatched on form submit)
          if (evt.detail == 1) {
            window.addEventListener('click', this, true, false);
          }
          break;

        case 'click':
          // Mouse events has been cancelled so dispatch a sequence
          // of events to where touchend has been fired
          evt.preventDefault();
          evt.stopImmediatePropagation();

          window.removeEventListener('click', this, true, false);

          if (this.cancelClick)
            return;

          ignoreEvents = true;
          content.setTimeout(function dispatchMouseEvents(self) {
            self.fireMouseEvent('mousedown', evt);
            self.fireMouseEvent('mousemove', evt);
            self.fireMouseEvent('mouseup', evt);
            ignoreEvents = false;
         }, 0, this);

          return;
      }

      let target = eventTarget || this.target;
      if (target && type) {
        this.sendTouchEvent(evt, target, type);
      }

      if (!isSystemWindow) {
        evt.preventDefault();
        evt.stopImmediatePropagation();
      }
    },
    fireMouseEvent: function teh_fireMouseEvent(type, evt)  {
      let content = this.getContent(evt.target);
      var utils = content.QueryInterface(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsIDOMWindowUtils);
      utils.sendMouseEvent(type, evt.clientX, evt.clientY, 0, 1, 0, true, 0, Ci.nsIDOMMouseEvent.MOZ_SOURCE_TOUCH);
    },
    sendContextMenu: function teh_sendContextMenu(target, x, y, delay) {
      let doc = target.ownerDocument;
      let evt = doc.createEvent('MouseEvent');
      evt.initMouseEvent('contextmenu', true, true, doc.defaultView,
                         0, x, y, x, y, false, false, false, false,
                         0, null);

      let content = this.getContent(target);
      let timeout = content.setTimeout((function contextMenu() {
        target.dispatchEvent(evt);
        this.cancelClick = true;
      }).bind(this), delay);

      return timeout;
    },
    sendTouchEvent: function teh_sendTouchEvent(evt, target, name) {
      let document = target.ownerDocument;
      let content = this.getContent(target);

      let touchEvent = document.createEvent('touchevent');
      let point = document.createTouch(content, target, 0,
                                       evt.pageX, evt.pageY,
                                       evt.screenX, evt.screenY,
                                       evt.clientX, evt.clientY,
                                       1, 1, 0, 0);
      let touches = document.createTouchList(point);
      let targetTouches = touches;
      let changedTouches = touches;
      touchEvent.initTouchEvent(name, true, true, content, 0,
                                false, false, false, false,
                                touches, targetTouches, changedTouches);
      target.dispatchEvent(touchEvent);
      return touchEvent;
    },
    getContent: function teh_getContent(target) {
      let win = target.ownerDocument.defaultView;
      return win;
    }
  };
  trackedWindows.set(window, TouchEventHandler);

  return TouchEventHandler;
}

exports.TouchEventHandler = TouchEventHandler;
