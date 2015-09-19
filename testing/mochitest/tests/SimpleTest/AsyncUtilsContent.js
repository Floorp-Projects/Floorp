/*
 * This code is used for handling synthesizeMouse in a content process.
 * Generally it just delegates to EventUtils.js.
 */

// Set up a dummy environment so that EventUtils works. We need to be careful to
// pass a window object into each EventUtils method we call rather than having
// it rely on the |window| global.
var EventUtils = {};
EventUtils.window = {};
EventUtils.parent = EventUtils.window;
EventUtils._EU_Ci = Components.interfaces;
EventUtils._EU_Cc = Components.classes;
// EventUtils' `sendChar` function relies on the navigator to synthetize events.
EventUtils.navigator = content.document.defaultView.navigator;
EventUtils.KeyboardEvent = content.document.defaultView.KeyboardEvent;

Services.scriptloader.loadSubScript("chrome://mochikit/content/tests/SimpleTest/EventUtils.js", EventUtils);

addMessageListener("Test:SynthesizeMouse", (message) => {
  let data = message.data;
  let target = data.target;
  if (typeof target == "string") {
    target = content.document.querySelector(target);
  }
  else {
    target = message.objects.object;
  }

  let left = data.x;
  let top = data.y;
  if (target) {
    let rect = target.getBoundingClientRect();
    left += rect.left;
    top += rect.top;

    if (data.event.centered) {
      left += rect.width / 2;
      top += rect.height / 2;
    }
  }

  let result = EventUtils.synthesizeMouseAtPoint(left, top, data.event, content);
  sendAsyncMessage("Test:SynthesizeMouseDone", { defaultPrevented: result });
});

addMessageListener("Test:SendChar", message => {
  let result = EventUtils.sendChar(message.data.char, content);
  sendAsyncMessage("Test:SendCharDone", { sendCharResult: result });
});
