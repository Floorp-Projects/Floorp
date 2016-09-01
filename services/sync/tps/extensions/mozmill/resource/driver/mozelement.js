/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["Elem", "Selector", "ID", "Link", "XPath", "Name", "Lookup",
                        "MozMillElement", "MozMillCheckBox", "MozMillRadio", "MozMillDropList",
                        "MozMillTextBox", "subclasses"
                       ];

const NAMESPACE_XUL = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;

var EventUtils = {};  Cu.import('resource://mozmill/stdlib/EventUtils.js', EventUtils);

var assertions = {};  Cu.import('resource://mozmill/modules/assertions.js', assertions);
var broker = {};      Cu.import('resource://mozmill/driver/msgbroker.js', broker);
var elementslib = {}; Cu.import('resource://mozmill/driver/elementslib.js', elementslib);
var utils = {};       Cu.import('resource://mozmill/stdlib/utils.js', utils);

var assert = new assertions.Assert();

// A list of all the subclasses available.  Shared modules can push their own subclasses onto this list
var subclasses = [MozMillCheckBox, MozMillRadio, MozMillDropList, MozMillTextBox];

/**
 * createInstance()
 *
 * Returns an new instance of a MozMillElement
 * The type of the element is automatically determined
 */
function createInstance(locatorType, locator, elem, document) {
  var args = { "document": document, "element": elem };

  // If we already have an element lets determine the best MozMillElement type
  if (elem) {
    for (var i = 0; i < subclasses.length; ++i) {
      if (subclasses[i].isType(elem)) {
        return new subclasses[i](locatorType, locator, args);
      }
    }
  }

  // By default we create a base MozMillElement
  if (MozMillElement.isType(elem)) {
    return new MozMillElement(locatorType, locator, args);
  }

  throw new Error("Unsupported element type " + locatorType + ": " + locator);
}

var Elem = function (node) {
  return createInstance("Elem", node, node);
};

var Selector = function (document, selector, index) {
  return createInstance("Selector", selector, elementslib.Selector(document, selector, index), document);
};

var ID = function (document, nodeID) {
  return createInstance("ID", nodeID, elementslib.ID(document, nodeID), document);
};

var Link = function (document, linkName) {
  return createInstance("Link", linkName, elementslib.Link(document, linkName), document);
};

var XPath = function (document, expr) {
  return createInstance("XPath", expr, elementslib.XPath(document, expr), document);
};

var Name = function (document, nName) {
  return createInstance("Name", nName, elementslib.Name(document, nName), document);
};

var Lookup = function (document, expression) {
  var elem = createInstance("Lookup", expression, elementslib.Lookup(document, expression), document);

  // Bug 864268 - Expose the expression property to maintain backwards compatibility
  elem.expression = elem._locator;

  return elem;
};

/**
 * MozMillElement
 * The base class for all mozmill elements
 */
function MozMillElement(locatorType, locator, args) {
  args = args || {};
  this._locatorType = locatorType;
  this._locator = locator;
  this._element = args["element"];
  this._owner = args["owner"];

  this._document = this._element ? this._element.ownerDocument : args["document"];
  this._defaultView = this._document ? this._document.defaultView : null;

  // Used to maintain backwards compatibility with controller.js
  this.isElement = true;
}

// Static method that returns true if node is of this element type
MozMillElement.isType = function (node) {
  return true;
};

// This getter is the magic behind lazy loading (note distinction between _element and element)
MozMillElement.prototype.__defineGetter__("element", function () {
  // If the document is invalid (e.g. reload of the page), invalidate the cached
  // element and update the document cache
  if (this._defaultView && this._defaultView.document !== this._document) {
    this._document = this._defaultView.document;
    this._element = undefined;
  }

  if (this._element == undefined) {
    if (elementslib[this._locatorType]) {
      this._element = elementslib[this._locatorType](this._document, this._locator);
    } else if (this._locatorType == "Elem") {
      this._element = this._locator;
    } else {
      throw new Error("Unknown locator type: " + this._locatorType);
    }
  }

  return this._element;
});

/**
 * Drag an element to the specified offset on another element, firing mouse and
 * drag events. Adapted from EventUtils.js synthesizeDrop()
 *
 * By default it will drag the source element over the destination's element
 * center with a "move" dropEffect.
 *
 * @param {MozElement} aElement
 *        Destination element over which the drop occurs
 * @param {Number} [aOffsetX=aElement.width/2]
 *        Relative x offset for dropping on aElement
 * @param {Number} [aOffsetY=aElement.height/2]
 *        Relative y offset for dropping on aElement
 * @param {DOMWindow} [aSourceWindow=this.element.ownerDocument.defaultView]
 *        Custom source Window to be used.
 * @param {DOMWindow} [aDestWindow=aElement.getNode().ownerDocument.defaultView]
 *        Custom destination Window to be used.
 * @param {String} [aDropEffect="move"]
 *        Possible values: copy, move, link, none
 * @param {Object[]} [aDragData]
 *        An array holding custom drag data to be used during the drag event
 *        Format: [{ type: "text/plain", "Text to drag"}, ...]
 *
 * @returns {String} the captured dropEffect
 */
MozMillElement.prototype.dragToElement = function(aElement, aOffsetX, aOffsetY,
                                                  aSourceWindow, aDestWindow,
                                                  aDropEffect, aDragData) {
  if (!this.element) {
    throw new Error("Could not find element " + this.getInfo());
  }
  if (!aElement) {
    throw new Error("Missing destination element");
  }

  var srcNode = this.element;
  var destNode = aElement.getNode();
  var srcWindow = aSourceWindow ||
                  (srcNode.ownerDocument ? srcNode.ownerDocument.defaultView
                                         : srcNode);
  var destWindow = aDestWindow ||
                  (destNode.ownerDocument ? destNode.ownerDocument.defaultView
                                          : destNode);

  var srcRect = srcNode.getBoundingClientRect();
  var srcCoords = {
    x: srcRect.width / 2,
    y: srcRect.height / 2
  };
  var destRect = destNode.getBoundingClientRect();
  var destCoords = {
    x: (!aOffsetX || isNaN(aOffsetX)) ? (destRect.width / 2) : aOffsetX,
    y: (!aOffsetY || isNaN(aOffsetY)) ? (destRect.height / 2) : aOffsetY
  };

  var windowUtils = destWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                              .getInterface(Ci.nsIDOMWindowUtils);
  var ds = Cc["@mozilla.org/widget/dragservice;1"].getService(Ci.nsIDragService);

  var dataTransfer;
  var trapDrag = function (event) {
    srcWindow.removeEventListener("dragstart", trapDrag, true);
    dataTransfer = event.dataTransfer;

    if (!aDragData) {
      return;
    }

    for (var i = 0; i < aDragData.length; i++) {
      var item = aDragData[i];
      for (var j = 0; j < item.length; j++) {
        dataTransfer.mozSetDataAt(item[j].type, item[j].data, i);
      }
    }

    dataTransfer.dropEffect = aDropEffect || "move";
    event.preventDefault();
    event.stopPropagation();
  }

  ds.startDragSession();

  try {
    srcWindow.addEventListener("dragstart", trapDrag, true);
    EventUtils.synthesizeMouse(srcNode, srcCoords.x, srcCoords.y,
                               { type: "mousedown" }, srcWindow);
    EventUtils.synthesizeMouse(destNode, destCoords.x, destCoords.y,
                               { type: "mousemove" }, destWindow);

    var event = destWindow.document.createEvent("DragEvent");
    event.initDragEvent("dragenter", true, true, destWindow, 0, 0, 0, 0, 0,
                        false, false, false, false, 0, null, dataTransfer);
    event.initDragEvent("dragover", true, true, destWindow, 0, 0, 0, 0, 0,
                        false, false, false, false, 0, null, dataTransfer);
    event.initDragEvent("drop", true, true, destWindow, 0, 0, 0, 0, 0,
                        false, false, false, false, 0, null, dataTransfer);
    windowUtils.dispatchDOMEventViaPresShell(destNode, event, true);

    EventUtils.synthesizeMouse(destNode, destCoords.x, destCoords.y,
                               { type: "mouseup" }, destWindow);

    return dataTransfer.dropEffect;
  } finally {
    ds.endDragSession(true);
  }

};

// Returns the actual wrapped DOM node
MozMillElement.prototype.getNode = function () {
  return this.element;
};

MozMillElement.prototype.getInfo = function () {
  return this._locatorType + ": " + this._locator;
};

/**
 * Sometimes an element which once existed will no longer exist in the DOM
 * This function re-searches for the element
 */
MozMillElement.prototype.exists = function () {
  this._element = undefined;
  if (this.element) {
    return true;
  }

  return false;
};

/**
 * Synthesize a keypress event on the given element
 *
 * @param {string} aKey
 *        Key to use for synthesizing the keypress event. It can be a simple
 *        character like "k" or a string like "VK_ESCAPE" for command keys
 * @param {object} aModifiers
 *        Information about the modifier keys to send
 *        Elements: accelKey   - Hold down the accelerator key (ctrl/meta)
 *                               [optional - default: false]
 *                  altKey     - Hold down the alt key
 *                              [optional - default: false]
 *                  ctrlKey    - Hold down the ctrl key
 *                               [optional - default: false]
 *                  metaKey    - Hold down the meta key (command key on Mac)
 *                               [optional - default: false]
 *                  shiftKey   - Hold down the shift key
 *                               [optional - default: false]
 * @param {object} aExpectedEvent
 *        Information about the expected event to occur
 *        Elements: target     - Element which should receive the event
 *                               [optional - default: current element]
 *                  type       - Type of the expected key event
 */
MozMillElement.prototype.keypress = function (aKey, aModifiers, aExpectedEvent) {
  if (!this.element) {
    throw new Error("Could not find element " + this.getInfo());
  }

  var win = this.element.ownerDocument ? this.element.ownerDocument.defaultView
                                       : this.element;
  this.element.focus();

  if (aExpectedEvent) {
    if (!aExpectedEvent.type) {
      throw new Error(arguments.callee.name + ": Expected event type not specified");
    }

    var target = aExpectedEvent.target ? aExpectedEvent.target.getNode()
                                       : this.element;
    EventUtils.synthesizeKeyExpectEvent(aKey, aModifiers || {}, target, aExpectedEvent.type,
                                        "MozMillElement.keypress()", win);
  } else {
    EventUtils.synthesizeKey(aKey, aModifiers || {}, win);
  }

  broker.pass({'function':'MozMillElement.keypress()'});

  return true;
};


/**
 * Synthesize a general mouse event on the given element
 *
 * @param {number} aOffsetX
 *        Relative x offset in the elements bounds to click on
 * @param {number} aOffsetY
 *        Relative y offset in the elements bounds to click on
 * @param {object} aEvent
 *        Information about the event to send
 *        Elements: accelKey   - Hold down the accelerator key (ctrl/meta)
 *                               [optional - default: false]
 *                  altKey     - Hold down the alt key
 *                               [optional - default: false]
 *                  button     - Mouse button to use
 *                               [optional - default: 0]
 *                  clickCount - Number of counts to click
 *                               [optional - default: 1]
 *                  ctrlKey    - Hold down the ctrl key
 *                               [optional - default: false]
 *                  metaKey    - Hold down the meta key (command key on Mac)
 *                               [optional - default: false]
 *                  shiftKey   - Hold down the shift key
 *                               [optional - default: false]
 *                  type       - Type of the mouse event ('click', 'mousedown',
 *                               'mouseup', 'mouseover', 'mouseout')
 *                               [optional - default: 'mousedown' + 'mouseup']
 * @param {object} aExpectedEvent
 *        Information about the expected event to occur
 *        Elements: target     - Element which should receive the event
 *                               [optional - default: current element]
 *                  type       - Type of the expected mouse event
 */
MozMillElement.prototype.mouseEvent = function (aOffsetX, aOffsetY, aEvent, aExpectedEvent) {
  if (!this.element) {
    throw new Error(arguments.callee.name + ": could not find element " + this.getInfo());
  }

  if ("document" in this.element) {
    throw new Error("A window cannot be a target for mouse events.");
  }

  var rect = this.element.getBoundingClientRect();

  if (!aOffsetX || isNaN(aOffsetX)) {
    aOffsetX = rect.width / 2;
  }

  if (!aOffsetY || isNaN(aOffsetY)) {
    aOffsetY = rect.height / 2;
  }

  // Scroll element into view otherwise the click will fail
  if ("scrollIntoView" in this.element)
    this.element.scrollIntoView();

  if (aExpectedEvent) {
    // The expected event type has to be set
    if (!aExpectedEvent.type) {
      throw new Error(arguments.callee.name + ": Expected event type not specified");
    }

    // If no target has been specified use the specified element
    var target = aExpectedEvent.target ? aExpectedEvent.target.getNode()
                                       : this.element;
    if (!target) {
      throw new Error(arguments.callee.name + ": could not find element " +
                      aExpectedEvent.target.getInfo());
    }

    EventUtils.synthesizeMouseExpectEvent(this.element, aOffsetX, aOffsetY, aEvent,
                                          target, aExpectedEvent.type,
                                          "MozMillElement.mouseEvent()",
                                          this.element.ownerDocument.defaultView);
  } else {
    EventUtils.synthesizeMouse(this.element, aOffsetX, aOffsetY, aEvent,
                               this.element.ownerDocument.defaultView);
  }

  // Bug 555347
  // We don't know why this sleep is necessary but more investigation is needed
  // before it can be removed
  utils.sleep(0);

  return true;
};

/**
 * Synthesize a mouse click event on the given element
 */
MozMillElement.prototype.click = function (aOffsetX, aOffsetY, aExpectedEvent) {
  // Handle menu items differently
  if (this.element && this.element.tagName == "menuitem") {
    this.element.click();
  } else {
    this.mouseEvent(aOffsetX, aOffsetY, {}, aExpectedEvent);
  }

  broker.pass({'function':'MozMillElement.click()'});

  return true;
};

/**
 * Synthesize a double click on the given element
 */
MozMillElement.prototype.doubleClick = function (aOffsetX, aOffsetY, aExpectedEvent) {
  this.mouseEvent(aOffsetX, aOffsetY, {clickCount: 2}, aExpectedEvent);

  broker.pass({'function':'MozMillElement.doubleClick()'});

  return true;
};

/**
 * Synthesize a mouse down event on the given element
 */
MozMillElement.prototype.mouseDown = function (aButton, aOffsetX, aOffsetY, aExpectedEvent) {
  this.mouseEvent(aOffsetX, aOffsetY, {button: aButton, type: "mousedown"}, aExpectedEvent);

  broker.pass({'function':'MozMillElement.mouseDown()'});

  return true;
};

/**
 * Synthesize a mouse out event on the given element
 */
MozMillElement.prototype.mouseOut = function (aButton, aOffsetX, aOffsetY, aExpectedEvent) {
  this.mouseEvent(aOffsetX, aOffsetY, {button: aButton, type: "mouseout"}, aExpectedEvent);

  broker.pass({'function':'MozMillElement.mouseOut()'});

  return true;
};

/**
 * Synthesize a mouse over event on the given element
 */
MozMillElement.prototype.mouseOver = function (aButton, aOffsetX, aOffsetY, aExpectedEvent) {
  this.mouseEvent(aOffsetX, aOffsetY, {button: aButton, type: "mouseover"}, aExpectedEvent);

  broker.pass({'function':'MozMillElement.mouseOver()'});

  return true;
};

/**
 * Synthesize a mouse up event on the given element
 */
MozMillElement.prototype.mouseUp = function (aButton, aOffsetX, aOffsetY, aExpectedEvent) {
  this.mouseEvent(aOffsetX, aOffsetY, {button: aButton, type: "mouseup"}, aExpectedEvent);

  broker.pass({'function':'MozMillElement.mouseUp()'});

  return true;
};

/**
 * Synthesize a mouse middle click event on the given element
 */
MozMillElement.prototype.middleClick = function (aOffsetX, aOffsetY, aExpectedEvent) {
  this.mouseEvent(aOffsetX, aOffsetY, {button: 1}, aExpectedEvent);

  broker.pass({'function':'MozMillElement.middleClick()'});

  return true;
};

/**
 * Synthesize a mouse right click event on the given element
 */
MozMillElement.prototype.rightClick = function (aOffsetX, aOffsetY, aExpectedEvent) {
  this.mouseEvent(aOffsetX, aOffsetY, {type : "contextmenu", button: 2 }, aExpectedEvent);

  broker.pass({'function':'MozMillElement.rightClick()'});

  return true;
};

/**
 * Synthesize a general touch event on the given element
 *
 * @param {Number} [aOffsetX=aElement.width / 2]
 *        Relative x offset in the elements bounds to click on
 * @param {Number} [aOffsetY=aElement.height / 2]
 *        Relative y offset in the elements bounds to click on
 * @param {Object} [aEvent]
 *        Information about the event to send
 * @param {Boolean} [aEvent.altKey=false]
 *        A Boolean value indicating whether or not the alt key was down when
 *        the touch event was fired
 * @param {Number} [aEvent.angle=0]
 *        The angle (in degrees) that the ellipse described by rx and
 *        ry must be rotated, clockwise, to most accurately cover the area
 *        of contact between the user and the surface.
 * @param {Touch[]} [aEvent.changedTouches]
 *        A TouchList of all the Touch objects representing individual points of
 *        contact whose states changed between the previous touch event and
 *        this one
 * @param {Boolean} [aEvent.ctrlKey]
 *        A Boolean value indicating whether or not the control key was down
 *        when the touch event was fired
 * @param {Number} [aEvent.force=1]
 *        The amount of pressure being applied to the surface by the user, as a
 *        float between 0.0 (no pressure) and 1.0 (maximum pressure)
 * @param {Number} [aEvent.id=0]
 *        A unique identifier for this Touch object. A given touch (say, by a
 *        finger) will have the same identifier for the duration of its movement
 *        around the surface. This lets you ensure that you're tracking the same
 *        touch all the time
 * @param {Boolean} [aEvent.metaKey]
 *        A Boolean value indicating whether or not the meta key was down when
 *        the touch event was fired.
 * @param {Number} [aEvent.rx=1]
 *        The X radius of the ellipse that most closely circumscribes the area
 *        of contact with the screen.
 * @param {Number} [aEvent.ry=1]
 *        The Y radius of the ellipse that most closely circumscribes the area
 *        of contact with the screen.
 * @param {Boolean} [aEvent.shiftKey]
 *        A Boolean value indicating whether or not the shift key was down when
 *        the touch event was fired
 * @param {Touch[]} [aEvent.targetTouches]
 *        A TouchList of all the Touch objects that are both currently in
 *        contact with the touch surface and were also started on the same
 *        element that is the target of the event
 * @param {Touch[]} [aEvent.touches]
 *        A TouchList of all the Touch objects representing all current points
 *        of contact with the surface, regardless of target or changed status
 * @param {Number} [aEvent.type=*|touchstart|touchend|touchmove|touchenter|touchleave|touchcancel]
 *        The type of touch event that occurred
 * @param {Element} [aEvent.target]
 *        The target of the touches associated with this event. This target
 *        corresponds to the target of all the touches in the targetTouches
 *        attribute, but note that other touches in this event may have a
 *        different target. To be careful, you should use the target associated
 *        with individual touches
 */
MozMillElement.prototype.touchEvent = function (aOffsetX, aOffsetY, aEvent) {
  if (!this.element) {
    throw new Error(arguments.callee.name + ": could not find element " + this.getInfo());
  }

  if ("document" in this.element) {
    throw new Error("A window cannot be a target for touch events.");
  }

  var rect = this.element.getBoundingClientRect();

  if (!aOffsetX || isNaN(aOffsetX)) {
    aOffsetX = rect.width / 2;
  }

  if (!aOffsetY || isNaN(aOffsetY)) {
    aOffsetY = rect.height / 2;
  }

  // Scroll element into view otherwise the click will fail
  if ("scrollIntoView" in this.element) {
    this.element.scrollIntoView();
  }

  EventUtils.synthesizeTouch(this.element, aOffsetX, aOffsetY, aEvent,
                             this.element.ownerDocument.defaultView);

  return true;
};

/**
 * Synthesize a touch tap event on the given element
 *
 * @param {Number} [aOffsetX=aElement.width / 2]
 *        Left offset in px where the event is triggered
 * @param {Number} [aOffsetY=aElement.height / 2]
 *        Top offset in px where the event is triggered
 * @param {Object} [aExpectedEvent]
 *        Information about the expected event to occur
 * @param {MozMillElement} [aExpectedEvent.target=this.element]
 *        Element which should receive the event
 * @param {MozMillElement} [aExpectedEvent.type]
 *        Type of the expected mouse event
 */
MozMillElement.prototype.tap = function (aOffsetX, aOffsetY, aExpectedEvent) {
  this.mouseEvent(aOffsetX, aOffsetY, {
    clickCount: 1,
    inputSource: Ci.nsIDOMMouseEvent.MOZ_SOURCE_TOUCH
  }, aExpectedEvent);

  broker.pass({'function':'MozMillElement.tap()'});

  return true;
};

/**
 * Synthesize a double tap on the given element
 *
 * @param {Number} [aOffsetX=aElement.width / 2]
 *        Left offset in px where the event is triggered
 * @param {Number} [aOffsetY=aElement.height / 2]
 *        Top offset in px where the event is triggered
 * @param {Object} [aExpectedEvent]
 *        Information about the expected event to occur
 * @param {MozMillElement} [aExpectedEvent.target=this.element]
 *        Element which should receive the event
 * @param {MozMillElement} [aExpectedEvent.type]
 *        Type of the expected mouse event
 */
MozMillElement.prototype.doubleTap = function (aOffsetX, aOffsetY, aExpectedEvent) {
  this.mouseEvent(aOffsetX, aOffsetY, {
    clickCount: 2,
    inputSource: Ci.nsIDOMMouseEvent.MOZ_SOURCE_TOUCH
  }, aExpectedEvent);

  broker.pass({'function':'MozMillElement.doubleTap()'});

  return true;
};

/**
 * Synthesize a long press
 *
 * @param {Number} aOffsetX
 *        Left offset in px where the event is triggered
 * @param {Number} aOffsetY
 *        Top offset in px where the event is triggered
 * @param {Number} [aTime=1000]
 *        Duration of the "press" event in ms
 */
MozMillElement.prototype.longPress = function (aOffsetX, aOffsetY, aTime) {
  var time = aTime || 1000;

  this.touchStart(aOffsetX, aOffsetY);
  utils.sleep(time);
  this.touchEnd(aOffsetX, aOffsetY);

  broker.pass({'function':'MozMillElement.longPress()'});

  return true;
};

/**
 * Synthesize a touch & drag event on the given element
 *
 * @param {Number} aOffsetX1
 *        Left offset of the start position
 * @param {Number} aOffsetY1
 *        Top offset of the start position
 * @param {Number} aOffsetX2
 *        Left offset of the end position
 * @param {Number} aOffsetY2
 *        Top offset of the end position
 */
MozMillElement.prototype.touchDrag = function (aOffsetX1, aOffsetY1, aOffsetX2, aOffsetY2) {
  this.touchStart(aOffsetX1, aOffsetY1);
  this.touchMove(aOffsetX2, aOffsetY2);
  this.touchEnd(aOffsetX2, aOffsetY2);

  broker.pass({'function':'MozMillElement.move()'});

  return true;
};

/**
 * Synthesize a press / touchstart event on the given element
 *
 * @param {Number} aOffsetX
 *        Left offset where the event is triggered
 * @param {Number} aOffsetY
 *        Top offset where the event is triggered
 */
MozMillElement.prototype.touchStart = function (aOffsetX, aOffsetY) {
  this.touchEvent(aOffsetX, aOffsetY, { type: "touchstart" });

  broker.pass({'function':'MozMillElement.touchStart()'});

  return true;
};

/**
 * Synthesize a release / touchend event on the given element
 *
 * @param {Number} aOffsetX
 *        Left offset where the event is triggered
 * @param {Number} aOffsetY
 *        Top offset where the event is triggered
 */
MozMillElement.prototype.touchEnd = function (aOffsetX, aOffsetY) {
  this.touchEvent(aOffsetX, aOffsetY, { type: "touchend" });

  broker.pass({'function':'MozMillElement.touchEnd()'});

  return true;
};

/**
 * Synthesize a touchMove event on the given element
 *
 * @param {Number} aOffsetX
 *        Left offset where the event is triggered
 * @param {Number} aOffsetY
 *        Top offset where the event is triggered
 */
MozMillElement.prototype.touchMove = function (aOffsetX, aOffsetY) {
  this.touchEvent(aOffsetX, aOffsetY, { type: "touchmove" });

  broker.pass({'function':'MozMillElement.touchMove()'});

  return true;
};

MozMillElement.prototype.waitForElement = function (timeout, interval) {
  var elem = this;

  assert.waitFor(function () {
    return elem.exists();
  }, "Element.waitForElement(): Element '" + this.getInfo() +
     "' has been found", timeout, interval);

  broker.pass({'function':'MozMillElement.waitForElement()'});
};

MozMillElement.prototype.waitForElementNotPresent = function (timeout, interval) {
  var elem = this;

  assert.waitFor(function () {
    return !elem.exists();
  }, "Element.waitForElementNotPresent(): Element '" + this.getInfo() +
     "' has not been found", timeout, interval);

  broker.pass({'function':'MozMillElement.waitForElementNotPresent()'});
};

MozMillElement.prototype.waitThenClick = function (timeout, interval,
                                                   aOffsetX, aOffsetY, aExpectedEvent) {
  this.waitForElement(timeout, interval);
  this.click(aOffsetX, aOffsetY, aExpectedEvent);
};

/**
 * Waits for the element to be available in the DOM, then trigger a tap event
 *
 * @param {Number} [aTimeout=5000]
 *        Time to wait for the element to be available
 * @param {Number} [aInterval=100]
 *        Interval to check for availability
 * @param {Number} [aOffsetX=aElement.width / 2]
 *        Left offset where the event is triggered
 * @param {Number} [aOffsetY=aElement.height / 2]
 *        Top offset where the event is triggered
 * @param {Object} [aExpectedEvent]
 *        Information about the expected event to occur
 * @param {MozMillElement} [aExpectedEvent.target=this.element]
 *        Element which should receive the event
 * @param {MozMillElement} [aExpectedEvent.type]
 *        Type of the expected mouse event
 */
MozMillElement.prototype.waitThenTap = function (aTimeout, aInterval,
                                                 aOffsetX, aOffsetY, aExpectedEvent) {
  this.waitForElement(aTimeout, aInterval);
  this.tap(aOffsetX, aOffsetY, aExpectedEvent);
};

// Dispatches an HTMLEvent
MozMillElement.prototype.dispatchEvent = function (eventType, canBubble, modifiers) {
  canBubble = canBubble || true;
  modifiers = modifiers || { };

  let document = 'ownerDocument' in this.element ? this.element.ownerDocument
                                                 : this.element.document;

  let evt = document.createEvent('HTMLEvents');
  evt.shiftKey = modifiers["shift"];
  evt.metaKey = modifiers["meta"];
  evt.altKey = modifiers["alt"];
  evt.ctrlKey = modifiers["ctrl"];
  evt.initEvent(eventType, canBubble, true);

  this.element.dispatchEvent(evt);
};


/**
 * MozMillCheckBox, which inherits from MozMillElement
 */
function MozMillCheckBox(locatorType, locator, args) {
  MozMillElement.call(this, locatorType, locator, args);
}


MozMillCheckBox.prototype = Object.create(MozMillElement.prototype, {
  check : {
    /**
     * Enable/Disable a checkbox depending on the target state
     *
     * @param {boolean} state State to set
     * @return {boolean} Success state
     */
    value : function MMCB_check(state) {
      var result = false;

      if (!this.element) {
        throw new Error("could not find element " + this.getInfo());
      }

      // If we have a XUL element, unwrap its XPCNativeWrapper
      if (this.element.namespaceURI == NAMESPACE_XUL) {
        this.element = utils.unwrapNode(this.element);
      }

      state = (typeof(state) == "boolean") ? state : false;
      if (state != this.element.checked) {
        this.click();
        var element = this.element;

        assert.waitFor(function () {
          return element.checked == state;
        }, "CheckBox.check(): Checkbox " + this.getInfo() + " could not be checked/unchecked", 500);

        result = true;
      }

      broker.pass({'function':'MozMillCheckBox.check(' + this.getInfo() +
                   ', state: ' + state + ')'});

      return result;
    }
  }
});


/**
 * Returns true if node is of type MozMillCheckBox
 *
 * @static
 * @param {DOMNode} node Node to check for its type
 * @return {boolean} True if node is of type checkbox
 */
MozMillCheckBox.isType = function MMCB_isType(node) {
  return ((node.localName.toLowerCase() == "input" && node.getAttribute("type") == "checkbox") ||
    (node.localName.toLowerCase() == 'toolbarbutton' && node.getAttribute('type') == 'checkbox') ||
    (node.localName.toLowerCase() == 'checkbox'));
};


/**
 * MozMillRadio, which inherits from MozMillElement
 */
function MozMillRadio(locatorType, locator, args) {
  MozMillElement.call(this, locatorType, locator, args);
}


MozMillRadio.prototype = Object.create(MozMillElement.prototype, {
  select : {
    /**
     * Select the given radio button
     *
     * @param {number} [index=0]
     *        Specifies which radio button in the group to select (only
     *        applicable to radiogroup elements)
     * @return {boolean} Success state
     */
    value : function MMR_select(index) {
      if (!this.element) {
        throw new Error("could not find element " + this.getInfo());
      }

      if (this.element.localName.toLowerCase() == "radiogroup") {
        var element = this.element.getElementsByTagName("radio")[index || 0];
        new MozMillRadio("Elem", element).click();
      } else {
        var element = this.element;
        this.click();
      }

      assert.waitFor(function () {
        // If we have a XUL element, unwrap its XPCNativeWrapper
        if (element.namespaceURI == NAMESPACE_XUL) {
          element = utils.unwrapNode(element);
          return element.selected == true;
        }

        return element.checked == true;
      }, "Radio.select(): Radio button " + this.getInfo() + " has been selected", 500);

      broker.pass({'function':'MozMillRadio.select(' + this.getInfo() + ')'});

      return true;
    }
  }
});


/**
 * Returns true if node is of type MozMillRadio
 *
 * @static
 * @param {DOMNode} node Node to check for its type
 * @return {boolean} True if node is of type radio
 */
MozMillRadio.isType = function MMR_isType(node) {
  return ((node.localName.toLowerCase() == 'input' && node.getAttribute('type') == 'radio') ||
    (node.localName.toLowerCase() == 'toolbarbutton' && node.getAttribute('type') == 'radio') ||
    (node.localName.toLowerCase() == 'radio') ||
    (node.localName.toLowerCase() == 'radiogroup'));
};


/**
 * MozMillDropList, which inherits from MozMillElement
 */
function MozMillDropList(locatorType, locator, args) {
  MozMillElement.call(this, locatorType, locator, args);
}


MozMillDropList.prototype = Object.create(MozMillElement.prototype, {
  select : {
    /**
     * Select the specified option and trigger the relevant events of the element
     * @return {boolean}
     */
    value : function MMDL_select(index, option, value) {
      if (!this.element){
        throw new Error("Could not find element " + this.getInfo());
      }

      //if we have a select drop down
      if (this.element.localName.toLowerCase() == "select"){
        var item = null;

        // The selected item should be set via its index
        if (index != undefined) {
          // Resetting a menulist has to be handled separately
          if (index == -1) {
            this.dispatchEvent('focus', false);
            this.element.selectedIndex = index;
            this.dispatchEvent('change', true);

            broker.pass({'function':'MozMillDropList.select()'});

            return true;
          } else {
            item = this.element.options.item(index);
          }
        } else {
          for (var i = 0; i < this.element.options.length; i++) {
            var entry = this.element.options.item(i);
            if (option != undefined && entry.innerHTML == option ||
              value != undefined && entry.value == value) {
              item = entry;
              break;
            }
          }
        }

        // Click the item
        try {
          // EventUtils.synthesizeMouse doesn't work.
          this.dispatchEvent('focus', false);
          item.selected = true;
          this.dispatchEvent('change', true);

          var self = this;
          var selected = index || option || value;
          assert.waitFor(function () {
            switch (selected) {
              case index:
                return selected === self.element.selectedIndex;
                break;
              case option:
                return selected === item.label;
                break;
              case value:
                return selected === item.value;
                break;
            }
          }, "DropList.select(): The correct item has been selected");

          broker.pass({'function':'MozMillDropList.select()'});

          return true;
        } catch (e) {
          throw new Error("No item selected for element " + this.getInfo());
        }
      }
      //if we have a xul menupopup select accordingly
      else if (this.element.namespaceURI.toLowerCase() == NAMESPACE_XUL) {
        var ownerDoc = this.element.ownerDocument;
        // Unwrap the XUL element's XPCNativeWrapper
        this.element = utils.unwrapNode(this.element);
        // Get the list of menuitems
        var menuitems = this.element.
                        getElementsByTagNameNS(NAMESPACE_XUL, "menupopup")[0].
                        getElementsByTagNameNS(NAMESPACE_XUL, "menuitem");

        var item = null;

        if (index != undefined) {
          if (index == -1) {
            this.dispatchEvent('focus', false);
            this.element.boxObject.activeChild = null;
            this.dispatchEvent('change', true);

            broker.pass({'function':'MozMillDropList.select()'});

            return true;
          } else {
            item = menuitems[index];
          }
        } else {
          for (var i = 0; i < menuitems.length; i++) {
            var entry = menuitems[i];
            if (option != undefined && entry.label == option ||
              value != undefined && entry.value == value) {
              item = entry;
              break;
            }
          }
        }

        // Click the item
        try {
          item.click();

          var self = this;
          var selected = index || option || value;
          assert.waitFor(function () {
            switch (selected) {
              case index:
                return selected === self.element.selectedIndex;
                break;
              case option:
                return selected === self.element.label;
                break;
              case value:
                return selected === self.element.value;
                break;
            }
          }, "DropList.select(): The correct item has been selected");

          broker.pass({'function':'MozMillDropList.select()'});

          return true;
        } catch (e) {
          throw new Error('No item selected for element ' + this.getInfo());
        }
      }
    }
  }
});


/**
 * Returns true if node is of type MozMillDropList
 *
 * @static
 * @param {DOMNode} node Node to check for its type
 * @return {boolean} True if node is of type dropdown list
 */
MozMillDropList.isType = function MMR_isType(node) {
  return ((node.localName.toLowerCase() == 'toolbarbutton' &&
    (node.getAttribute('type') == 'menu' || node.getAttribute('type') == 'menu-button')) ||
    (node.localName.toLowerCase() == 'menu') ||
    (node.localName.toLowerCase() == 'menulist') ||
    (node.localName.toLowerCase() == 'select' ));
};


/**
 * MozMillTextBox, which inherits from MozMillElement
 */
function MozMillTextBox(locatorType, locator, args) {
  MozMillElement.call(this, locatorType, locator, args);
}


MozMillTextBox.prototype = Object.create(MozMillElement.prototype, {
  sendKeys : {
    /**
     * Synthesize keypress events for each character on the given element
     *
     * @param {string} aText
     *        The text to send as single keypress events
     * @param {object} aModifiers
     *        Information about the modifier keys to send
     *        Elements: accelKey   - Hold down the accelerator key (ctrl/meta)
     *                               [optional - default: false]
     *                  altKey     - Hold down the alt key
     *                              [optional - default: false]
     *                  ctrlKey    - Hold down the ctrl key
     *                               [optional - default: false]
     *                  metaKey    - Hold down the meta key (command key on Mac)
     *                               [optional - default: false]
     *                  shiftKey   - Hold down the shift key
     *                               [optional - default: false]
     * @param {object} aExpectedEvent
     *        Information about the expected event to occur
     *        Elements: target     - Element which should receive the event
     *                               [optional - default: current element]
     *                  type       - Type of the expected key event
     * @return {boolean} Success state
     */
    value : function MMTB_sendKeys(aText, aModifiers, aExpectedEvent) {
      if (!this.element) {
        throw new Error("could not find element " + this.getInfo());
      }

      var element = this.element;
      Array.forEach(aText, function (letter) {
        var win = element.ownerDocument ? element.ownerDocument.defaultView
          : element;
        element.focus();

        if (aExpectedEvent) {
          if (!aExpectedEvent.type) {
            throw new Error(arguments.callee.name + ": Expected event type not specified");
          }

          var target = aExpectedEvent.target ? aExpectedEvent.target.getNode()
            : element;
          EventUtils.synthesizeKeyExpectEvent(letter, aModifiers || {}, target,
            aExpectedEvent.type,
            "MozMillTextBox.sendKeys()", win);
        } else {
          EventUtils.synthesizeKey(letter, aModifiers || {}, win);
        }
      });

      broker.pass({'function':'MozMillTextBox.type()'});

      return true;
    }
  }
});


/**
 * Returns true if node is of type MozMillTextBox
 *
 * @static
 * @param {DOMNode} node Node to check for its type
 * @return {boolean} True if node is of type textbox
 */
MozMillTextBox.isType = function MMR_isType(node) {
  return ((node.localName.toLowerCase() == 'input' &&
    (node.getAttribute('type') == 'text' || node.getAttribute('type') == 'search')) ||
    (node.localName.toLowerCase() == 'textarea') ||
    (node.localName.toLowerCase() == 'textbox'));
};
