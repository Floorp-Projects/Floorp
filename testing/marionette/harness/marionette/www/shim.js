/**
* mouse_event_shim.js: generate mouse events from touch events.
*
* This library listens for touch events and generates mousedown, mousemove
* mouseup, and click events to match them. It captures and dicards any
* real mouse events (non-synthetic events with isTrusted true) that are
* send by gecko so that there are not duplicates.
*
* This library does emit mouseover/mouseout and mouseenter/mouseleave
* events. You can turn them off by setting MouseEventShim.trackMouseMoves to
* false. This means that mousemove events will always have the same target
* as the mousedown even that began the series. You can also call
* MouseEventShim.setCapture() from a mousedown event handler to prevent
* mouse tracking until the next mouseup event.
*
* This library does not support multi-touch but should be sufficient
* to do drags based on mousedown/mousemove/mouseup events.
*
* This library does not emit dblclick events or contextmenu events
*/

'use strict';

(function() {
  // Make sure we don't run more than once
  if (MouseEventShim)
    return;

  // Bail if we're not on running on a platform that sends touch
  // events. We don't need the shim code for mouse events.
  try {
    document.createEvent('TouchEvent');
  } catch (e) {
    return;
  }

  var starttouch; // The Touch object that we started with
  var target; // The element the touch is currently over
  var emitclick; // Will we be sending a click event after mouseup?

  // Use capturing listeners to discard all mouse events from gecko
  window.addEventListener('mousedown', discardEvent, true);
  window.addEventListener('mouseup', discardEvent, true);
  window.addEventListener('mousemove', discardEvent, true);
  window.addEventListener('click', discardEvent, true);

  function discardEvent(e) {
    if (e.isTrusted) {
      e.stopImmediatePropagation(); // so it goes no further
      if (e.type === 'click')
        e.preventDefault(); // so it doesn't trigger a change event
    }
  }

  // Listen for touch events that bubble up to the window.
  // If other code has called stopPropagation on the touch events
  // then we'll never see them. Also, we'll honor the defaultPrevented
  // state of the event and will not generate synthetic mouse events
  window.addEventListener('touchstart', handleTouchStart);
  window.addEventListener('touchmove', handleTouchMove);
  window.addEventListener('touchend', handleTouchEnd);
  window.addEventListener('touchcancel', handleTouchEnd); // Same as touchend

  function handleTouchStart(e) {
    // If we're already handling a touch, ignore this one
    if (starttouch)
      return;

    // Ignore any event that has already been prevented
    if (e.defaultPrevented)
      return;

    // Sometimes an unknown gecko bug causes us to get a touchstart event
    // for an iframe target that we can't use because it is cross origin.
    // Don't start handling a touch in that case
    try {
      e.changedTouches[0].target.ownerDocument;
    }
    catch (e) {
      // Ignore the event if we can't see the properties of the target
      return;
    }

    // If there is more than one simultaneous touch, ignore all but the first
    starttouch = e.changedTouches[0];
    target = starttouch.target;
    emitclick = true;

    // Move to the position of the touch
    emitEvent('mousemove', target, starttouch);

    // Now send a synthetic mousedown
    var result = emitEvent('mousedown', target, starttouch);

    // If the mousedown was prevented, pass that on to the touch event.
    // And remember not to send a click event
    if (!result) {
      e.preventDefault();
      emitclick = false;
    }
  }

  function handleTouchEnd(e) {
    if (!starttouch)
      return;

    // End a MouseEventShim.setCapture() call
    if (MouseEventShim.capturing) {
      MouseEventShim.capturing = false;
      MouseEventShim.captureTarget = null;
    }

    for (var i = 0; i < e.changedTouches.length; i++) {
      var touch = e.changedTouches[i];
      // If the ended touch does not have the same id, skip it
      if (touch.identifier !== starttouch.identifier)
        continue;

      emitEvent('mouseup', target, touch);

      // If target is still the same element we started and the touch did not
      // move more than the threshold and if the user did not prevent
      // the mousedown, then send a click event, too.
      if (emitclick)
        emitEvent('click', starttouch.target, touch);

      starttouch = null;
      return;
    }
  }

  function handleTouchMove(e) {
    if (!starttouch)
      return;

    for (var i = 0; i < e.changedTouches.length; i++) {
      var touch = e.changedTouches[i];
      // If the ended touch does not have the same id, skip it
      if (touch.identifier !== starttouch.identifier)
        continue;

      // Don't send a mousemove if the touchmove was prevented
      if (e.defaultPrevented)
        return;

      // See if we've moved too much to emit a click event
      var dx = Math.abs(touch.screenX - starttouch.screenX);
      var dy = Math.abs(touch.screenY - starttouch.screenY);
      if (dx > MouseEventShim.dragThresholdX ||
          dy > MouseEventShim.dragThresholdY) {
        emitclick = false;
      }

      var tracking = MouseEventShim.trackMouseMoves &&
        !MouseEventShim.capturing;

      if (tracking) {
        // If the touch point moves, then the element it is over
        // may have changed as well. Note that calling elementFromPoint()
        // forces a layout if one is needed.
        // XXX: how expensive is it to do this on each touchmove?
        // Can we listen for (non-standard) touchleave events instead?
        var oldtarget = target;
        var newtarget = document.elementFromPoint(touch.clientX, touch.clientY);
        if (newtarget === null) {
          // this can happen as the touch is moving off of the screen, e.g.
          newtarget = oldtarget;
        }
        if (newtarget !== oldtarget) {
          leave(oldtarget, newtarget, touch); // mouseout, mouseleave
          target = newtarget;
        }
      }
      else if (MouseEventShim.captureTarget) {
        target = MouseEventShim.captureTarget;
      }

      emitEvent('mousemove', target, touch);

      if (tracking && newtarget !== oldtarget) {
        enter(newtarget, oldtarget, touch); // mouseover, mouseenter
      }
    }
  }

  // Return true if element a contains element b
  function contains(a, b) {
    return (a.compareDocumentPosition(b) & 16) !== 0;
  }

  // A touch has left oldtarget and entered newtarget
  // Send out all the events that are required
  function leave(oldtarget, newtarget, touch) {
    emitEvent('mouseout', oldtarget, touch, newtarget);

    // If the touch has actually left oldtarget (and has not just moved
    // into a child of oldtarget) send a mouseleave event. mouseleave
    // events don't bubble, so we have to repeat this up the hierarchy.
    for (var e = oldtarget; !contains(e, newtarget); e = e.parentNode) {
      emitEvent('mouseleave', e, touch, newtarget);
    }
  }

  // A touch has entered newtarget from oldtarget
  // Send out all the events that are required.
  function enter(newtarget, oldtarget, touch) {
    emitEvent('mouseover', newtarget, touch, oldtarget);

    // Emit non-bubbling mouseenter events if the touch actually entered
    // newtarget and wasn't already in some child of it
    for (var e = newtarget; !contains(e, oldtarget); e = e.parentNode) {
      emitEvent('mouseenter', e, touch, oldtarget);
    }
  }

  function emitEvent(type, target, touch, relatedTarget) {
    var synthetic = document.createEvent('MouseEvents');
    var bubbles = (type !== 'mouseenter' && type !== 'mouseleave');
    var count =
      (type === 'mousedown' || type === 'mouseup' || type === 'click') ? 1 : 0;

    synthetic.initMouseEvent(type,
                             bubbles, // canBubble
                             true, // cancelable
                             window,
                             count, // detail: click count
                             touch.screenX,
                             touch.screenY,
                             touch.clientX,
                             touch.clientY,
                             false, // ctrlKey: we don't have one
                             false, // altKey: we don't have one
                             false, // shiftKey: we don't have one
                             false, // metaKey: we don't have one
                             0, // we're simulating the left button
                             relatedTarget || null);

    try {
      return target.dispatchEvent(synthetic);
    }
    catch (e) {
      console.warn('Exception calling dispatchEvent', type, e);
      return true;
    }
  }
}());

var MouseEventShim = {
  // It is a known gecko bug that synthetic events have timestamps measured
  // in microseconds while regular events have timestamps measured in
  // milliseconds. This utility function returns a the timestamp converted
  // to milliseconds, if necessary.
  getEventTimestamp: function(e) {
    if (e.isTrusted) // XXX: Are real events always trusted?
      return e.timeStamp;
    else
      return e.timeStamp / 1000;
  },

  // Set this to false if you don't care about mouseover/out events
  // and don't want the target of mousemove events to follow the touch
  trackMouseMoves: true,

  // Call this function from a mousedown event handler if you want to guarantee
  // that the mousemove and mouseup events will go to the same element
  // as the mousedown even if they leave the bounds of the element. This is
  // like setting trackMouseMoves to false for just one drag. It is a
  // substitute for event.target.setCapture(true)
  setCapture: function(target) {
    this.capturing = true; // Will be set back to false on mouseup
    if (target)
      this.captureTarget = target;
  },

  capturing: false,

  // Keep these in sync with ui.dragThresholdX and ui.dragThresholdY prefs.
  // If a touch ever moves more than this many pixels from its starting point
  // then we will not synthesize a click event when the touch ends.
  dragThresholdX: 25,
  dragThresholdY: 25
};
