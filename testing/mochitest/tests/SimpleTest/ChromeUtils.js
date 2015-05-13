/**
 * ChromeUtils.js is a set of mochitest utilities that are used to 
 * synthesize events in the browser.  These are only used by 
 * mochitest-chrome and browser-chrome tests.  Originally these functions were in 
 * EventUtils.js, but when porting to specialPowers, we didn't want
 * to move unnecessary functions.
 *
 */

const EventUtils = {};
const scriptLoader = Components.classes["@mozilla.org/moz/jssubscript-loader;1"].
                   getService(Components.interfaces.mozIJSSubScriptLoader);
scriptLoader.loadSubScript("chrome://mochikit/content/tests/SimpleTest/EventUtils.js", EventUtils);

/**
 * Synthesize a query text content event.
 *
 * @param aOffset  The character offset.  0 means the first character in the
 *                 selection root.
 * @param aLength  The length of getting text.  If the length is too long,
 *                 the extra length is ignored.
 * @param aWindow  Optional (If null, current |window| will be used)
 * @return         An nsIQueryContentEventResult object.  If this failed,
 *                 the result might be null.
 */
function synthesizeQueryTextContent(aOffset, aLength, aWindow)
{
  var utils = _getDOMWindowUtils(aWindow);
  if (!utils) {
    return nullptr;
  }
  return utils.sendQueryContentEvent(utils.QUERY_TEXT_CONTENT,
                                     aOffset, aLength, 0, 0,
                                     QUERY_CONTENT_FLAG_USE_NATIVE_LINE_BREAK);
}

/**
 * Synthesize a query text rect event.
 *
 * @param aOffset  The character offset.  0 means the first character in the
 *                 selection root.
 * @param aLength  The length of the text.  If the length is too long,
 *                 the extra length is ignored.
 * @param aWindow  Optional (If null, current |window| will be used)
 * @return         An nsIQueryContentEventResult object.  If this failed,
 *                 the result might be null.
 */
function synthesizeQueryTextRect(aOffset, aLength, aWindow)
{
  var utils = _getDOMWindowUtils(aWindow);
  if (!utils) {
    return nullptr;
  }
  return utils.sendQueryContentEvent(utils.QUERY_TEXT_RECT,
                                     aOffset, aLength, 0, 0,
                                     QUERY_CONTENT_FLAG_USE_NATIVE_LINE_BREAK);
}

/**
 * Synthesize a query editor rect event.
 *
 * @param aWindow  Optional (If null, current |window| will be used)
 * @return         An nsIQueryContentEventResult object.  If this failed,
 *                 the result might be null.
 */
function synthesizeQueryEditorRect(aWindow)
{
  var utils = _getDOMWindowUtils(aWindow);
  if (!utils) {
    return nullptr;
  }
  return utils.sendQueryContentEvent(utils.QUERY_EDITOR_RECT, 0, 0, 0, 0,
                                     QUERY_CONTENT_FLAG_USE_NATIVE_LINE_BREAK);
}

/**
 * Synthesize a character at point event.
 *
 * @param aX, aY   The offset in the client area of the DOM window.
 * @param aWindow  Optional (If null, current |window| will be used)
 * @return         An nsIQueryContentEventResult object.  If this failed,
 *                 the result might be null.
 */
function synthesizeCharAtPoint(aX, aY, aWindow)
{
  var utils = _getDOMWindowUtils(aWindow);
  if (!utils) {
    return nullptr;
  }
  return utils.sendQueryContentEvent(utils.QUERY_CHARACTER_AT_POINT,
                                     0, 0, aX, aY,
                                     QUERY_CONTENT_FLAG_USE_NATIVE_LINE_BREAK);
}

/**
 * Emulate a dragstart event.
 *  element - element to fire the dragstart event on
 *  expectedDragData - the data you expect the data transfer to contain afterwards
 *                      This data is in the format:
 *                         [ [ {type: value, data: value, test: function}, ... ], ... ]
 *                     can be null
 *  aWindow - optional; defaults to the current window object.
 *  x - optional; initial x coordinate
 *  y - optional; initial y coordinate
 * Returns null if data matches.
 * Returns the event.dataTransfer if data does not match
 *
 * eqTest is an optional function if comparison can't be done with x == y;
 *   function (actualData, expectedData) {return boolean}
 *   @param actualData from dataTransfer
 *   @param expectedData from expectedDragData
 * see bug 462172 for example of use
 *
 */
function synthesizeDragStart(element, expectedDragData, aWindow, x, y)
{
  if (!aWindow)
    aWindow = window;
  x = x || 2;
  y = y || 2;
  const step = 9;

  var result = "trapDrag was not called";
  var trapDrag = function(event) {
    try {
      var dataTransfer = event.dataTransfer;
      result = null;
      if (!dataTransfer)
        throw "no dataTransfer";
      if (expectedDragData == null ||
          dataTransfer.mozItemCount != expectedDragData.length)
        throw dataTransfer;
      for (var i = 0; i < dataTransfer.mozItemCount; i++) {
        var dtTypes = dataTransfer.mozTypesAt(i);
        if (dtTypes.length != expectedDragData[i].length)
          throw dataTransfer;
        for (var j = 0; j < dtTypes.length; j++) {
          if (dtTypes[j] != expectedDragData[i][j].type)
            throw dataTransfer;
          var dtData = dataTransfer.mozGetDataAt(dtTypes[j],i);
          if (expectedDragData[i][j].eqTest) {
            if (!expectedDragData[i][j].eqTest(dtData, expectedDragData[i][j].data))
              throw dataTransfer;
          }
          else if (expectedDragData[i][j].data != dtData)
            throw dataTransfer;
        }
      }
    } catch(ex) {
      result = ex;
    }
    event.preventDefault();
    event.stopPropagation();
  }
  aWindow.addEventListener("dragstart", trapDrag, false);
  EventUtils.synthesizeMouse(element, x, y, { type: "mousedown" }, aWindow);
  x += step; y += step;
  EventUtils.synthesizeMouse(element, x, y, { type: "mousemove" }, aWindow);
  x += step; y += step;
  EventUtils.synthesizeMouse(element, x, y, { type: "mousemove" }, aWindow);
  aWindow.removeEventListener("dragstart", trapDrag, false);
  EventUtils.synthesizeMouse(element, x, y, { type: "mouseup" }, aWindow);
  return result;
}

/**
 * INTERNAL USE ONLY
 * Create an event object to pass to EventUtils.sendDragEvent.
 *
 * @param aType          The string represents drag event type.
 * @param aDestElement   The element to fire the drag event, used to calculate
 *                       screenX/Y and clientX/Y.
 * @param aDestWindow    Optional; Defaults to the current window object.
 * @param aDataTransfer  dataTransfer for current drag session.
 * @param aDragEvent     The object contains properties to override the event
 *                       object
 * @return               An object to pass to EventUtils.sendDragEvent.
 */
function createDragEventObject(aType, aDestElement, aDestWindow, aDataTransfer,
                               aDragEvent)
{
  var destRect = aDestElement.getBoundingClientRect();
  var destClientX = destRect.left + destRect.width / 2;
  var destClientY = destRect.top + destRect.height / 2;
  var destScreenX = aDestWindow.mozInnerScreenX + destClientX;
  var destScreenY = aDestWindow.mozInnerScreenY + destClientY;
  if ("clientX" in aDragEvent && !("screenX" in aDragEvent)) {
    aDragEvent.screenX = aDestWindow.mozInnerScreenX + aDragEvent.clientX;
  }
  if ("clientY" in aDragEvent && !("screenY" in aDragEvent)) {
    aDragEvent.screenY = aDestWindow.mozInnerScreenY + aDragEvent.clientY;
  }
  return Object.assign({ type: aType,
                         screenX: destScreenX, screenY: destScreenY,
                         clientX: destClientX, clientY: destClientY,
                         dataTransfer: aDataTransfer }, aDragEvent);
}

/**
 * Emulate a event sequence of dragstart, dragenter, and dragover.
 *
 * @param aSrcElement   The element to use to start the drag.
 * @param aDestElement  The element to fire the dragover, dragenter events
 * @param aDragData     The data to supply for the data transfer.
 *                      This data is in the format:
 *                        [ [ {type: value, data: value}, ...], ... ]
 *                      Pass null to avoid modifying dataTransfer.
 * @param aDropEffect   The drop effect to set during the dragstart event, or
 *                      'move' if null.
 * @param aWindow       Optional; Defaults to the current window object.
 * @param aDestWindow   Optional; Defaults to aWindow.
 *                      Used when aDestElement is in a different window than
 *                      aSrcElement.
 * @param aDragEvent    Optional; Defaults to empty object. Overwrites an object
 *                      passed to EventUtils.sendDragEvent.
 * @return              A two element array, where the first element is the
 *                      value returned from EventUtils.sendDragEvent for
 *                      dragover event, and the second element is the
 *                      dataTransfer for the current drag session.
 */
function synthesizeDragOver(aSrcElement, aDestElement, aDragData, aDropEffect, aWindow, aDestWindow, aDragEvent={})
{
  if (!aWindow)
    aWindow = window;
  if (!aDestWindow)
    aDestWindow = aWindow;

  var dataTransfer;
  var trapDrag = function(event) {
    dataTransfer = event.dataTransfer;
    if (aDragData) {
      for (var i = 0; i < aDragData.length; i++) {
        var item = aDragData[i];
        for (var j = 0; j < item.length; j++) {
          dataTransfer.mozSetDataAt(item[j].type, item[j].data, i);
        }
      }
    }
    dataTransfer.dropEffect = aDropEffect || "move";
    event.preventDefault();
  };

  // need to use real mouse action
  aWindow.addEventListener("dragstart", trapDrag, true);
  EventUtils.synthesizeMouseAtCenter(aSrcElement, { type: "mousedown" }, aWindow);

  var rect = aSrcElement.getBoundingClientRect();
  var x = rect.width / 2;
  var y = rect.height / 2;
  EventUtils.synthesizeMouse(aSrcElement, x, y, { type: "mousemove" }, aWindow);
  EventUtils.synthesizeMouse(aSrcElement, x+10, y+10, { type: "mousemove" }, aWindow);
  aWindow.removeEventListener("dragstart", trapDrag, true);

  var event = createDragEventObject("dragenter", aDestElement, aDestWindow,
                                    dataTransfer, aDragEvent);
  EventUtils.sendDragEvent(event, aDestElement, aDestWindow);

  event = createDragEventObject("dragover", aDestElement, aDestWindow,
                                dataTransfer, aDragEvent);
  var result = EventUtils.sendDragEvent(event, aDestElement, aDestWindow);

  return [result, dataTransfer];
}

/**
 * Emulate the drop event and mouseup event.
 * This should be called after synthesizeDragOver.
 *
 * @param aResult        The first element of the array returned from
 *                       synthesizeDragOver.
 * @param aDataTransfer  The second element of the array returned from
 *                       synthesizeDragOver.
 * @param aDestElement   The element to fire the drop event.
 * @param aDestWindow    Optional; Defaults to the current window object.
 * @param aDragEvent     Optional; Defaults to empty object. Overwrites an
 *                       object passed to EventUtils.sendDragEvent.
 * @return               "none" if aResult is true,
 *                       aDataTransfer.dropEffect otherwise.
 */
function synthesizeDropAfterDragOver(aResult, aDataTransfer, aDestElement, aDestWindow, aDragEvent={})
{
  if (!aDestWindow)
    aDestWindow = window;

  var effect = aDataTransfer.dropEffect;
  var event;

  if (aResult) {
    effect = "none";
  } else if (effect != "none") {
    event = createDragEventObject("drop", aDestElement, aDestWindow,
                                  aDataTransfer, aDragEvent);
    EventUtils.sendDragEvent(event, aDestElement, aDestWindow);
  }

  EventUtils.synthesizeMouseAtCenter(aDestElement, { type: "mouseup" }, aDestWindow);

  return effect;
}

/**
 * Emulate a drag and drop by emulating a dragstart and firing events dragenter,
 * dragover, and drop.
 *
 * @param aSrcElement   The element to use to start the drag.
 * @param aDestElement  The element to fire the dragover, dragenter events
 * @param aDragData     The data to supply for the data transfer.
 *                      This data is in the format:
 *                        [ [ {type: value, data: value}, ...], ... ]
 *                      Pass null to avoid modifying dataTransfer.
 * @param aDropEffect   The drop effect to set during the dragstart event, or
 *                      'move' if null.
 * @param aWindow       Optional; Defaults to the current window object.
 * @param aDestWindow   Optional; Defaults to aWindow.
 *                      Used when aDestElement is in a different window than
 *                      aSrcElement.
 * @param aDragEvent    Optional; Defaults to empty object. Overwrites an object
 *                      passed to EventUtils.sendDragEvent.
 * @return              The drop effect that was desired.
 */
function synthesizeDrop(aSrcElement, aDestElement, aDragData, aDropEffect, aWindow, aDestWindow, aDragEvent={})
{
  if (!aWindow)
    aWindow = window;
  if (!aDestWindow)
    aDestWindow = aWindow;

  var ds = Components.classes["@mozilla.org/widget/dragservice;1"].
           getService(Components.interfaces.nsIDragService);

  ds.startDragSession();

  try {
    var [result, dataTransfer] = synthesizeDragOver(aSrcElement, aDestElement,
                                                    aDragData, aDropEffect,
                                                    aWindow, aDestWindow,
                                                    aDragEvent);
    return synthesizeDropAfterDragOver(result, dataTransfer, aDestElement,
                                       aDestWindow, aDragEvent);
  } finally {
    ds.endDragSession(true);
  }
}

var PluginUtils =
{
  withTestPlugin : function(callback)
  {
    if (typeof Components == "undefined")
    {
      todo(false, "Not a Mozilla-based browser");
      return false;
    }

    var ph = Components.classes["@mozilla.org/plugin/host;1"]
                       .getService(Components.interfaces.nsIPluginHost);
    var tags = ph.getPluginTags();

    // Find the test plugin
    for (var i = 0; i < tags.length; i++)
    {
      if (tags[i].name == "Test Plug-in")
      {
        callback(tags[i]);
        return true;
      }
    }
    todo(false, "Need a test plugin on this platform");
    return false;
  }
};
