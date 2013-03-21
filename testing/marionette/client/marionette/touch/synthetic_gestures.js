// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

//
// SyntheticGestures.js: test utilities for generating gesture events.
//
// This module defines a SyntheticGestures object whose properties are
// utility functions that generate sequences of synthetic events. The event
// sequences are intended to be recognized as gestures by GestureDetector.js
// so this module is useful for writing tests of UI code that uses
// GestureDetector.js.
//
// SyntheticGestures.tap(): emit touch events to generate a tap gesture
//   on the specified target element.  GestureDetector.js should emit
//   a tap event in response.  SyntheticGesture.mousetap() is the same
//   but synthesizes mouse events instead.
//
// SyntheticGestures.dbltap(): emit touch events to generate a double
//   tap gesture on the specified target element.  GestureDetector.js
//   should emit two tap events and a dbltap event in response.
//   SyntheticGesture.mousedbltap() is the same but synthesizes mouse
//   events instead of touch events.
//
// SyntheticGestures.swipe(): emit touch events for a basic one finger
//   touch, move, lift geture over a specifed target, with given start
//   and end coordinates and gesture duration. GestureDetector.js should
//   emit a series of pan events and a final swipe event.
//   SyntheticGestures.mouseswipe() is similar but uses mouse events
//   instead of touch events.
//
// SyntheticGestures.hold(): emit touch events for a touch/hold/move/release
//   gesture. GestureDetector should emit a holdstart/holdmove/holdend
//   sequence of events in response.  SyntheticGesture.mousehold() is
//   similar but uses mouse events instead.
//
// SyntheticGestures.pinch(): emit touch events for a 2-finger pinch
//   open or pinch close gesture with given staring points for each
//   finger and a given scale factor. GestureDetector.js should
//   respond with a sequence of transform events culminating in one
//   with something very close to the specified scale factor.
//   Two-finger gestures can't be simulated with a mouse, so there is no
//   mouse-only version of this gesture, and the pinch() function will
//   fail on platforms that do not support touch events.
//
// The coordinates passed to these functions are relative to the
// coordinate system of the target element.  If you pass numbers, they
// are interpreted as pixels relative to upper-left corner of the
// target.  If you pass a string, then it is parsed to a number. If a
// string-valued coordinate ends with '%', then it is interpreted as a
// percent of the target element's width or height, otherwise it is
// interpreted as a number of pixels. For the swipe(), mouseswipe(),
// hold(), and mousehold() functions, if the second two coordinates
// are strings that begin with "+" or "-" then they are interpreted
// relative to the first two coordinates. These relative values can be
// specifed as percentages or as pixels.
//
var SyntheticGestures = (function() {
  // Send move events about this often
  var EVENT_INTERVAL = 30;  // milliseconds

  // The current array of all pending touches
  var touches = [];
  // For assigning unique ids to all touches
  var nextTouchId = 1000;

  // Emit a touch event of the specified type, using touch as the sole
  // member of the changedTouches property
  function emitTouchEvent(type, touch) {
    var target = touch.target;
    var doc = target.ownerDocument;
    var win = doc.defaultView;

    // Create the three lists of touches

    // All touches in the same document
    var documentTouches = doc.createTouchList(touches.filter(function(t) {
      return t.target.ownerDocument === doc;
    }));
    // All touches on the same target
    var targetTouches = doc.createTouchList(touches.filter(function(t) {
      return t.target === target;
    }));
    // Just this one touch that changed
    var changedTouches = doc.createTouchList(touch);

    // Create the event object
    var event = document.createEvent('TouchEvent');
    event.initTouchEvent(type,
                         true, // bubles
                         true, // cancellable
                         win,
                         0,    // detail
                         false, false, false, false, // no modifier keys
                         documentTouches,
                         targetTouches,
                         changedTouches);

    // Now dispatch it
    target.dispatchEvent(event);
  }

  //
  // Dispatch a touchstart, touchmove+, touchend sequence of events to
  // simulate a single-finger touch of the specified duration.
  //
  // The xt and yt arguments are functions that must return x and y
  // coordinates of the touch at any time t, 0 <= t <= duration.
  // Alternatively, xt and yt can be two-element arrays [from,to], and
  // the mouse coordinates will be interpolated between those
  // coordinates.  The then argument is an optional callback that will
  // be invoked after the touchend event is sent. In either case, the
  // coordinates should be window coordinates, relative to the viewport
  //
  // touch() can be called multiple times concurrently to simulate
  // 2 and 3 finger gestures.
  //
  // This is a low-level function that the higher-level touch gesture
  // utilities are built on. Most testing code will not need to call it.
  //
  function touch(target, duration, xt, yt, then) {
    var doc = target.ownerDocument;
    var win = doc.defaultView;
    var touchId = nextTouchId++;

    var x = xt;
    if (typeof xt !== 'function') {
      x = function(t) { return xt[0] + t / duration * (xt[1] - xt[0]); };
    }

    var y = yt;
    if (typeof yt !== 'function') {
      y = function(t) { return yt[0] + t / duration * (yt[1] - yt[0]); };
    }

    // viewport coordinates
    var clientX = Math.round(x(0)), clientY = Math.round(y(0));

    // document coordinates
    var pageX = clientX + win.pageXOffset,
        pageY = clientY + win.pageYOffset;

    // screen coordinates
    var screenX = clientX + win.mozInnerScreenX,
        screenY = clientY + win.mozInnerScreenY;

    // Remember the coordinates
    var lastX = clientX, lastY = clientY;

    // Create the touch object we'll be
    var touch = doc.createTouch(win, target, touchId,
                                pageX, pageY,
                                screenX, screenY,
                                clientX, clientY);

    // Add this new touch to the list of touches
    touches.push(touch);

    // Send the start event
    emitTouchEvent('touchstart', touch);

    var startTime = Date.now();

    setTimeout(nextEvent, EVENT_INTERVAL);

    function nextEvent() {
      // Figure out if this is the last of the touchmove events
      var time = Date.now();
      var dt = time - startTime;
      var last = dt + EVENT_INTERVAL / 2 > duration;

      // Find our touch object in the touches[] array.
      // Note that its index may have changed since we pushed it
      var touchIndex = touches.indexOf(touch);

      // If this is the last move event, make sure we move all the way
      if (last)
        dt = duration;

      // New coordinates of the touch
      clientX = Math.round(x(dt));
      clientY = Math.round(y(dt));

      // If we've moved, send a move event
      if (clientX !== lastX || clientY !== lastY) { // If we moved
        lastX = clientX;
        lastY = clientY;
        pageX = clientX + win.pageXOffset;
        pageY = clientY + win.pageYOffset;
        screenX = clientX + win.mozInnerScreenX;
        screenY = clientY + win.mozInnerScreenY;

        // Since we moved, we've got to create a new Touch object
        // with the new coordinates
        touch = doc.createTouch(win, target, touchId,
                                pageX, pageY,
                                screenX, screenY,
                                clientX, clientY);

        // Replace the old touch object with the new one
        touches[touchIndex] = touch;

        // And send the touchmove event
        emitTouchEvent('touchmove', touch);
      }

      // If that was the last move, send the touchend event
      // and call the callback
      if (last) {
        touches.splice(touchIndex, 1);
        emitTouchEvent('touchend', touch);
        if (then)
          setTimeout(then, 0);
      }
      // Otherwise, schedule the next event
      else {
        setTimeout(nextEvent, EVENT_INTERVAL);
      }
    }
  }

  function coordinates(target, x0, y0, x1, y1) {
    var coords = {};
    var box = target.getBoundingClientRect();

    var tx0 = typeof x0;
    var ty0 = typeof y0;
    var tx1 = typeof x1;
    var ty1 = typeof y1;

    function percent(s, x) {
      s = s.trim();
      var f = parseFloat(s);
      if (s[s.length - 1] === '%')
        f = f * x / 100;
      return f;
    }

    function relative(s, x) {
      var factor;
      if (s[0] === '+')
        factor = 1;
      else
        factor = -1;
      return factor * percent(s.substring(1), x);
    }

    if (tx0 === 'number')
      coords.x0 = box.left + x0;
    else if (tx0 === 'string')
      coords.x0 = box.left + percent(x0, box.width);

    if (tx1 === 'number')
      coords.x1 = box.left + x1;
    else if (tx1 === 'string') {
      x1 = x1.trim();
      if (x1[0] === '+' || x1[0] === '-')
        coords.x1 = coords.x0 + relative(x1, box.width);
      else
        coords.x1 = box.left + percent(x1, box.width);
    }

    if (ty0 === 'number')
      coords.y0 = box.top + y0;
    else if (ty0 === 'string')
      coords.y0 = box.top + percent(y0, box.height);

    if (ty1 === 'number')
      coords.y1 = box.top + y1;
    else if (ty1 === 'string') {
      y1 = y1.trim();
      if (y1[0] === '+' || y1[0] === '-')
        coords.y1 = coords.y0 + relative(y1, box.height);
      else
        coords.y1 = box.top + percent(y1, box.height);
    }

    return coords;
  }



  // Dispatch touchstart and touchend events over the specified target
  // and then invoke the then() callback.
  // x and y are the relative to the viewport.
  // If not specified then the center of the target is used. t is the
  // optional amount of time between touchstart and touchend event.
  function tap(target, then, x, y, t, sendAll) {
    if (!SyntheticGestures.touchSupported || !target.ownerDocument.createTouch) {
      console.warn('tap: touch events not supported; using mouse instead');
      return mousetap(target, then, x, y, t, true);
    }

    if (x == null)
      x = '50%';
    if (y == null)
      y = '50%';

    var c = coordinates(target, x, y);

    if (sendAll) {
      touch(target, t || 50, [c.x0, c.x0], [c.y0, c.y0],  function() {
        mousetap(target, then, x, y, t, true);
      });
    }
    else {
      touch(target, t || 50, [c.x0, c.x0], [c.y0, c.y0], then);
    }
  }

  // Dispatch a dbltap gesture. The arguments are like those to tap()
  // except that interval is the time between taps rather than the time between
  // touchstart and touchend
  function dbltap(target, then, x, y, interval) {
    if (!SyntheticGestures.touchSupported || !target.ownerDocument.createTouch) {
      console.warn('dbltap: touch events not supported; using mouse instead');
      return mousedbltap(target, then, x, y, interval);
    }

    if (x == null)
      x = '50%';
    if (y == null)
      y = '50%';
    var c = coordinates(target, x, y);

    touch(target, 25, [c.x0, c.x0], [c.y0, c.y0], function() {
      // When the first tap is done, start a timer for interval ms.
      setTimeout(function() {
        // After interval ms, send the second tap
        touch(target, 25, [c.x0, c.x0], [c.y0, c.y0], then);
      }, interval || 50);
    });
  }

  // Swipe smoothly from (x1, y1) to (x2, y2) over duration ms
  // then invoke the then() callback.
  function swipe(target, x1, y1, x2, y2, duration, then) {
    if (!SyntheticGestures.touchSupported) {
      console.warn('swipe: touch events not supported; using mouse instead');
      return mouseswipe(target, x1, y1, x2, y2, duration, then);
    }

    var c = coordinates(target, x1, y1, x2, y2);
    touch(target, duration || 200, [c.x0, c.x1], [c.y0, c.y1], then);
  }

  // Begin a touch at x1,y1 and hold it for holdtime ms,
  // then move smoothly to x2,y2 over movetime ms, and then invoke then().
  function hold(target, holdtime, x1, y1, x2, y2, movetime, then) {
    if (!SyntheticGestures.touchSupported || !target.ownerDocument.createTouch) {
      console.warn('hold: touch events not supported; using mouse instead');
      return mousehold(target, holdtime, x1, y1, x2, y2, movetime, then);
    }

    if (!movetime)
      movetime = 200;

    var c = coordinates(target, x1, y1, x2, y2);

    touch(target, holdtime + movetime,
          function(t) { // x coordinate a function of t
            if (t < holdtime)
              return c.x0;
            else
              return c.x0 + (t - holdtime) / movetime * (c.x1 - c.x0);
          },
          function(t) { // y coordinate a function of t
            if (t < holdtime)
              return c.y0;
            else
              return c.y0 + (t - holdtime) / movetime * (c.y1 - c.y0);
          },
          then);
  }

  // Begin touches at (x1,y1) and (x2,y2) and then move them smoothly toward
  // or away from each other by the specified scale factor over duration ms.
  // Finally, invoke the then() callback
  function pinch(target, x1, y1, x2, y2, scale, duration, then) {
    if (!SyntheticGestures.touchSupported) {
      console.error('pinch: touch events not supported on this platform');
      return;
    }

    var c1 = coordinates(target, x1, y1);
    var c2 = coordinates(target, x2, y2);
    x1 = c1.x0;
    y1 = c1.y0;
    x2 = c2.x0;
    y2 = c2.y0;

    var xmid = (x1 + x2) / 2;
    var ymid = (y1 + y2) / 2;

    var newx1 = Math.round(xmid + (x1 - xmid) * scale);
    var newy1 = Math.round(ymid + (y1 - ymid) * scale);
    var newx2 = Math.round(xmid + (x2 - xmid) * scale);
    var newy2 = Math.round(ymid + (y2 - ymid) * scale);

    // Emit two concurrent series of touch events.a
    // The first one is simple:
    touch(target, duration, [x1, newx1], [y1, newy1]);

    // The second touch moves twice as fast and then holds still and
    // lasts for an extra 200ms to ensure that both moves complete
    // before either finger lifts up.  Hopefully this means we'll get
    // the full scale effect
    touch(target, duration + 200,
          function(t) {
            if (t < duration / 2)
              return x2 + t * 2 / duration * (newx2 - x2);
            else
              return newx2;
          },
          function(t) {
            if (t < duration / 2)
              return y2 + t * 2 / duration * (newy2 - y2);
            else
              return newy2;
          },
          then);
  }

  //
  // Dispatch a mousedown, mousemove+, mouseup sequence of events over
  // duration ms to simulate a click-drag motion of the mouse or trackpad.
  //
  // The xt and yt arguments are [x0,x1], [y0,y1] or are functions
  // of t that specify the mouse coordinates at time t just as for the
  // touch() function.
  //
  // Unlike the touch() function, this drag() function takes a document
  // argument instead of an element argument, and always determines the
  // target of its events using document.elementFromPoint().  Callers
  // must ensure that xt and yt specify an initial point inside the
  // desired target element.
  //
  // This is a low-level function that the higher-level mouse gesture
  // utilities are built on. Most testing code will not need to call it.
  //
  function drag(doc, duration, xt, yt, then, detail, button, sendClick) {
    var win = doc.defaultView;
    detail = detail || 1;
    button = button || 0;

    var x = xt;
    if (typeof xt !== 'function') {
      x = function(t) { return xt[0] + t / duration * (xt[1] - xt[0]); };
    }

    var y = yt;
    if (typeof yt !== 'function') {
      y = function(t) { return yt[0] + t / duration * (yt[1] - yt[0]); };
    }

    // viewport coordinates
    var clientX = Math.round(x(0)), clientY = Math.round(y(0));

    // Remember the coordinates
    var lastX = clientX, lastY = clientY;

    // Send the initial mousedown event
    mouseEvent('mousedown', clientX, clientY);

    // Now send a sequence of mousemove events followed by mouse up
    var startTime = Date.now();
    setTimeout(nextEvent, EVENT_INTERVAL);

    // Send a mouse event of the specified type to whatever element is
    // at the specified coordinates in the viewport.
    function mouseEvent(type, clientX, clientY) {
      // Figure out what element the mouse would be over at (x,y)
      var target = doc.elementFromPoint(clientX, clientY);
      // Create an event
      var mousedown = doc.createEvent('MouseEvent');
      // Initialize it
      mousedown.initMouseEvent(type,
                               true, true,    // bubbles, cancellable
                               win, detail,   // window, click count
                               clientX + win.mozInnerScreenX,
                               clientY + win.mozInnerScreenY,
                               clientX, clientY,
                               false, false, false, false, // keyboard modifiers
                               button, null); // mouse button, related target
      // And dispatch it on the target element
      target.dispatchEvent(mousedown);
    }

    function nextEvent() {
      // Figure out if we've sent all of the mousemove events
      var time = Date.now();
      var dt = time - startTime;

      // Is this the last move event we'll be sending?
      var last = dt + EVENT_INTERVAL / 2 > duration;

      // If so make sure we move all the way
      if (last)
        dt = duration;

      // New coordinates of the touch
      clientX = Math.round(x(dt));
      clientY = Math.round(y(dt));

      // If we moved, send a move event
      if (clientX !== lastX || clientY !== lastY) { // If we moved
        lastX = clientX;
        lastY = clientY;
        mouseEvent('mousemove', clientX, clientY);
      }

      // If this was the last move, send a mouse up and call the callback
      // Otherwise, schedule the next move event
      if (last) {
        mouseEvent('mouseup', lastX, lastY);
        if (sendClick) {
          mouseEvent('click', clientX, clientY);
        }
        if (then) {
          setTimeout(then, 0);
        }
      }
      else {
        setTimeout(nextEvent, EVENT_INTERVAL);
      }
    }
  }

  // Send a mousedown/mouseup pair
  // XXX: will the browser automatically follow this with a click event?
  function mousetap(target, then, x, y, t, sendClick) {
    if (x == null)
      x = '50%';
    if (y == null)
      y = '50%';
    var c = coordinates(target, x, y);

    drag(target.ownerDocument, t || 50, [c.x0, c.x0], [c.y0, c.y0], then, null, null, sendClick);
  }

  // Dispatch a dbltap gesture. The arguments are like those to tap()
  // except that interval is the time between taps rather than the time between
  // touchstart and touchend
  function mousedbltap(target, then, x, y, interval) {
    if (x == null)
      x = '50%';
    if (y == null)
      y = '50%';
    var c = coordinates(target, x, y);

    drag(target.ownerDocument, 25, [c.x0, c.x0], [c.y0, c.y0], function() {
      // When the first tap is done, start a timer for interval ms.
      setTimeout(function() {
        // After interval ms, send the second tap with the click count set to 2.
        drag(target.ownerDocument, 25, [c.x0, c.x0], [c.y0, c.y0], then, 2);
      }, interval || 50);
    });
  }

  // Swipe smoothly with the mouse from (x1, y1) to (x2, y2) over duration ms
  // then invoke the then() callback.
  function mouseswipe(target, x1, y1, x2, y2, duration, then) {
    var c = coordinates(target, x1, y1, x2, y2);
    drag(target.ownerDocument, duration || 200,
         [c.x0, c.x1], [c.y0, c.y1], then);
  }

  // Mousedown at x1,y1 and hold it for holdtime ms,
  // then move smoothly to x2,y2 over movetime ms, then mouse up,
  // and then invoke then().
  function mousehold(target, holdtime, x1, y1, x2, y2, movetime, then) {
    if (!movetime)
      movetime = 200;

    var c = coordinates(target, x1, y1, x2, y2);

    drag(target.ownerDocument, holdtime + movetime,
          function(t) { // x coordinate a function of t
            if (t < holdtime)
              return c.x0;
            else
              return c.x0 + (t - holdtime) / movetime * (c.x1 - c.x0);
          },
          function(t) { // y coordinate a function of t
            if (t < holdtime)
              return c.y0;
            else
              return c.y0 + (t - holdtime) / movetime * (c.y1 - c.y0);
          },
          then);
  }

  return {
    touchSupported: true,
    tap: tap,
    mousetap: mousetap,
    dbltap: dbltap,
    mousedbltap: mousedbltap,
    swipe: swipe,
    mouseswipe: mouseswipe,
    hold: hold,
    mousehold: mousehold,
    pinch: pinch // There is no mouse-based alternative to this
  };
}());
