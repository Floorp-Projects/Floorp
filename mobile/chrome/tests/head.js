/*=============================================================================
  Common Helpers functions
=============================================================================*/
function waitFor(callback, test, timeout) {
  if (test()) {
    callback();
    return;
  }

  timeout = timeout || Date.now();
  if (Date.now() - timeout > 1000)
    throw "waitFor timeout";
  setTimeout(waitFor, 50, callback, test, timeout);
};

function makeURI(spec) {
  return gIOService.newURI(spec, null, null);
};

EventUtils.synthesizeMouseForContent = function synthesizeMouseForContent(aElement, aOffsetX, aOffsetY, aEvent, aWindow) {
  let container = document.getElementById("tile-container");
  let rect = container.getBoundingClientRect();

  EventUtils.synthesizeMouse(aElement, rect.left + aOffsetX, rect.top + aOffsetY, aEvent, aWindow);
};
