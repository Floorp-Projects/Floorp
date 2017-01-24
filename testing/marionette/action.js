/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Task.jsm");

Cu.import("chrome://marionette/content/assert.js");
Cu.import("chrome://marionette/content/element.js");
Cu.import("chrome://marionette/content/error.js");
Cu.import("chrome://marionette/content/event.js");

this.EXPORTED_SYMBOLS = ["action"];

// TODO? With ES 2016 and Symbol you can make a safer approximation
// to an enum e.g. https://gist.github.com/xmlking/e86e4f15ec32b12c4689
/**
 * Implements WebDriver Actions API: a low-level interface for providing
 * virtualised device input to the web browser.
 */
this.action = {
  Pause: "pause",
  KeyDown: "keyDown",
  KeyUp: "keyUp",
  PointerDown: "pointerDown",
  PointerUp: "pointerUp",
  PointerMove: "pointerMove",
  PointerCancel: "pointerCancel",
};

const ACTIONS = {
  none: new Set([action.Pause]),
  key: new Set([action.Pause, action.KeyDown, action.KeyUp]),
  pointer: new Set([
    action.Pause,
    action.PointerDown,
    action.PointerUp,
    action.PointerMove,
    action.PointerCancel,
  ]),
};

/** Map from normalized key value to UI Events modifier key name */
const MODIFIER_NAME_LOOKUP = {
  "Alt": "alt",
  "Shift": "shift",
  "Control": "ctrl",
  "Meta": "meta",
};

/** Map from raw key (codepoint) to normalized key value */
const NORMALIZED_KEY_LOOKUP = {
  "\uE000": "Unidentified",
  "\uE001": "Cancel",
  "\uE002": "Help",
  "\uE003": "Backspace",
  "\uE004": "Tab",
  "\uE005": "Clear",
  "\uE006": "Return",
  "\uE007": "Enter",
  "\uE008": "Shift",
  "\uE009": "Control",
  "\uE00A": "Alt",
  "\uE00B": "Pause",
  "\uE00C": "Escape",
  "\uE00D": " ",
  "\uE00E": "PageUp",
  "\uE00F": "PageDown",
  "\uE010": "End",
  "\uE011": "Home",
  "\uE012": "ArrowLeft",
  "\uE013": "ArrowUp",
  "\uE014": "ArrowRight",
  "\uE015": "ArrowDown",
  "\uE016": "Insert",
  "\uE017": "Delete",
  "\uE018": ";",
  "\uE019": "=",
  "\uE01A": "0",
  "\uE01B": "1",
  "\uE01C": "2",
  "\uE01D": "3",
  "\uE01E": "4",
  "\uE01F": "5",
  "\uE020": "6",
  "\uE021": "7",
  "\uE022": "8",
  "\uE023": "9",
  "\uE024": "*",
  "\uE025": "+",
  "\uE026": ",",
  "\uE027": "-",
  "\uE028": ".",
  "\uE029": "/",
  "\uE031": "F1",
  "\uE032": "F2",
  "\uE033": "F3",
  "\uE034": "F4",
  "\uE035": "F5",
  "\uE036": "F6",
  "\uE037": "F7",
  "\uE038": "F8",
  "\uE039": "F9",
  "\uE03A": "F10",
  "\uE03B": "F11",
  "\uE03C": "F12",
  "\uE03D": "Meta",
  "\uE040": "ZenkakuHankaku",
  "\uE050": "Shift",
  "\uE051": "Control",
  "\uE052": "Alt",
  "\uE053": "Meta",
  "\uE054": "PageUp",
  "\uE055": "PageDown",
  "\uE056": "End",
  "\uE057": "Home",
  "\uE058": "ArrowLeft",
  "\uE059": "ArrowUp",
  "\uE05A": "ArrowRight",
  "\uE05B": "ArrowDown",
  "\uE05C": "Insert",
  "\uE05D": "Delete",
};

/** Map from raw key (codepoint) to key location */
const KEY_LOCATION_LOOKUP = {
  "\uE007": 1,
  "\uE008": 1,
  "\uE009": 1,
  "\uE00A": 1,
  "\uE01A": 3,
  "\uE01B": 3,
  "\uE01C": 3,
  "\uE01D": 3,
  "\uE01E": 3,
  "\uE01F": 3,
  "\uE020": 3,
  "\uE021": 3,
  "\uE022": 3,
  "\uE023": 3,
  "\uE024": 3,
  "\uE025": 3,
  "\uE026": 3,
  "\uE027": 3,
  "\uE028": 3,
  "\uE029": 3,
  "\uE03D": 1,
  "\uE050": 2,
  "\uE051": 2,
  "\uE052": 2,
  "\uE053": 2,
  "\uE054": 3,
  "\uE055": 3,
  "\uE056": 3,
  "\uE057": 3,
  "\uE058": 3,
  "\uE059": 3,
  "\uE05A": 3,
  "\uE05B": 3,
  "\uE05C": 3,
  "\uE05D": 3,
};

const KEY_CODE_LOOKUP = {
  "\uE00A": "AltLeft",
  "\uE052": "AltRight",
  "\uE015": "ArrowDown",
  "\uE012": "ArrowLeft",
  "\uE014": "ArrowRight",
  "\uE013": "ArrowUp",
  "`": "Backquote",
  "~": "Backquote",
  "\\": "Backslash",
  "|": "Backslash",
  "\uE003": "Backspace",
  "[": "BracketLeft",
  "{": "BracketLeft",
  "]": "BracketRight",
  "}": "BracketRight",
  ",": "Comma",
  "<": "Comma",
  "\uE009": "ControlLeft",
  "\uE051": "ControlRight",
  "\uE017": "Delete",
  ")": "Digit0",
  "0": "Digit0",
  "!": "Digit1",
  "1": "Digit1",
  "2": "Digit2",
  "@": "Digit2",
  "#": "Digit3",
  "3": "Digit3",
  "$": "Digit4",
  "4": "Digit4",
  "%": "Digit5",
  "5": "Digit5",
  "6": "Digit6",
  "^": "Digit6",
  "&": "Digit7",
  "7": "Digit7",
  "*": "Digit8",
  "8": "Digit8",
  "(": "Digit9",
  "9": "Digit9",
  "\uE010": "End",
  "\uE006": "Enter",
  "+": "Equal",
  "=": "Equal",
  "\uE00C": "Escape",
  "\uE031": "F1",
  "\uE03A": "F10",
  "\uE03B": "F11",
  "\uE03C": "F12",
  "\uE032": "F2",
  "\uE033": "F3",
  "\uE034": "F4",
  "\uE035": "F5",
  "\uE036": "F6",
  "\uE037": "F7",
  "\uE038": "F8",
  "\uE039": "F9",
  "\uE002": "Help",
  "\uE011": "Home",
  "\uE016": "Insert",
  "<": "IntlBackslash",
  ">": "IntlBackslash",
  "A": "KeyA",
  "a": "KeyA",
  "B": "KeyB",
  "b": "KeyB",
  "C": "KeyC",
  "c": "KeyC",
  "D": "KeyD",
  "d": "KeyD",
  "E": "KeyE",
  "e": "KeyE",
  "F": "KeyF",
  "f": "KeyF",
  "G": "KeyG",
  "g": "KeyG",
  "H": "KeyH",
  "h": "KeyH",
  "I": "KeyI",
  "i": "KeyI",
  "J": "KeyJ",
  "j": "KeyJ",
  "K": "KeyK",
  "k": "KeyK",
  "L": "KeyL",
  "l": "KeyL",
  "M": "KeyM",
  "m": "KeyM",
  "N": "KeyN",
  "n": "KeyN",
  "O": "KeyO",
  "o": "KeyO",
  "P": "KeyP",
  "p": "KeyP",
  "Q": "KeyQ",
  "q": "KeyQ",
  "R": "KeyR",
  "r": "KeyR",
  "S": "KeyS",
  "s": "KeyS",
  "T": "KeyT",
  "t": "KeyT",
  "U": "KeyU",
  "u": "KeyU",
  "V": "KeyV",
  "v": "KeyV",
  "W": "KeyW",
  "w": "KeyW",
  "X": "KeyX",
  "x": "KeyX",
  "Y": "KeyY",
  "y": "KeyY",
  "Z": "KeyZ",
  "z": "KeyZ",
  "-": "Minus",
  "_": "Minus",
  "\uE01A": "Numpad0",
  "\uE05C": "Numpad0",
  "\uE01B": "Numpad1",
  "\uE056": "Numpad1",
  "\uE01C": "Numpad2",
  "\uE05B": "Numpad2",
  "\uE01D": "Numpad3",
  "\uE055": "Numpad3",
  "\uE01E": "Numpad4",
  "\uE058": "Numpad4",
  "\uE01F": "Numpad5",
  "\uE020": "Numpad6",
  "\uE05A": "Numpad6",
  "\uE021": "Numpad7",
  "\uE057": "Numpad7",
  "\uE022": "Numpad8",
  "\uE059": "Numpad8",
  "\uE023": "Numpad9",
  "\uE054": "Numpad9",
  "\uE024": "NumpadAdd",
  "\uE026": "NumpadComma",
  "\uE028": "NumpadDecimal",
  "\uE05D": "NumpadDecimal",
  "\uE029": "NumpadDivide",
  "\uE007": "NumpadEnter",
  "\uE024": "NumpadMultiply",
  "\uE026": "NumpadSubtract",
  "\uE03D": "OSLeft",
  "\uE053": "OSRight",
  "\uE01E": "PageDown",
  "\uE01F": "PageUp",
  ".": "Period",
  ">": "Period",
  "\"": "Quote",
  "'": "Quote",
  ":": "Semicolon",
  ";": "Semicolon",
  "\uE008": "ShiftLeft",
  "\uE050": "ShiftRight",
  "/": "Slash",
  "?": "Slash",
  "\uE00D": "Space",
  "  ": "Space",
  "\uE004": "Tab",
};

/** Represents possible subtypes for a pointer input source. */
action.PointerType = {
  Mouse: "mouse",
  Pen: "pen",
  Touch: "touch",
};

/**
 * Look up a PointerType.
 *
 * @param {string} str
 *     Name of pointer type.
 *
 * @return {string}
 *     A pointer type for processing pointer parameters.
 *
 * @throws {InvalidArgumentError}
 *     If |str| is not a valid pointer type.
 */
action.PointerType.get = function (str) {
  let name = capitalize(str);
  if (!(name in this)) {
    throw new InvalidArgumentError(`Unknown pointerType: ${str}`);
  }
  return this[name];
};

/**
 * Input state associated with current session. This is a map between input ID and
 * the device state for that input source, with one entry for each active input source.
 *
 * Initialized in listener.js
 */
action.inputStateMap = undefined;

/**
 * List of |action.Action| associated with current session. Used to manage dispatching
 * events when resetting the state of the input sources. Reset operations are assumed
 * to be idempotent.
 *
 * Initialized in listener.js
 */
action.inputsToCancel = undefined;

/**
 * Represents device state for an input source.
 */
class InputState {
  constructor() {
    this.type = this.constructor.name.toLowerCase();
  }

  /**
   * Check equality of this InputState object with another.
   *
   * @para{?} other
   *     Object representing an input state.
   * @return {boolean}
   *     True if |this| has the same |type| as |other|.
   */
  is(other) {
    if (typeof other == "undefined") {
      return false;
    }
    return this.type === other.type;
  }

  toString() {
    return `[object ${this.constructor.name}InputState]`;
  }

  /**
   * @param {?} obj
   *     Object with property |type|, representing an action sequence or an
   *     action item.
   *
   * @return {action.InputState}
   *     An |action.InputState| object for the type of the |actionSequence|.
   *
   * @throws {InvalidArgumentError}
   *     If |actionSequence.type| is not valid.
   */
  static fromJson(obj) {
    let type = obj.type;
    if (!(type in ACTIONS)) {
      throw new InvalidArgumentError(`Unknown action type: ${type}`);
    }
    let name = type == "none" ? "Null" : capitalize(type);
    return new action.InputState[name]();
  }
}

/** Possible kinds of |InputState| for supported input sources. */
action.InputState = {};

/**
 * Input state associated with a keyboard-type device.
 */
action.InputState.Key = class extends InputState {
  constructor() {
    super();
    this.pressed = new Set();
    this.alt = false;
    this.shift = false;
    this.ctrl = false;
    this.meta = false;
  }

  /**
   * Update modifier state according to |key|.
   *
   * @param {string} key
   *     Normalized key value of a modifier key.
   * @param {boolean} value
   *     Value to set the modifier attribute to.
   *
   * @throws {InvalidArgumentError}
   *     If |key| is not a modifier.
   */
  setModState(key, value) {
    if (key in MODIFIER_NAME_LOOKUP) {
      this[MODIFIER_NAME_LOOKUP[key]] = value;
    } else {
      throw new InvalidArgumentError("Expected 'key' to be one of " +
          `${Object.keys(MODIFIER_NAME_LOOKUP)}; got: ${key}`);
    }
  }

  /**
   * Check whether |key| is pressed.
   *
   * @param {string} key
   *     Normalized key value.
   *
   * @return {boolean}
   *     True if |key| is in set of pressed keys.
   */
  isPressed(key) {
    return this.pressed.has(key);
  }

  /**
   * Add |key| to the set of pressed keys.
   *
   * @param {string} key
   *     Normalized key value.
   *
   * @return {boolean}
   *     True if |key| is in list of pressed keys.
   */
  press(key) {
    return this.pressed.add(key);
  }

  /**
   * Remove |key| from the set of pressed keys.
   *
   * @param {string} key
   *     Normalized key value.
   *
   * @return {boolean}
   *     True if |key| is removed successfully, false otherwise.
   */
  release(key) {
    return this.pressed.delete(key);
  }
};

/**
 * Input state not associated with a specific physical device.
 */
action.InputState.Null = class extends InputState {
  constructor() {
    super();
    this.type = "none";
  }
};

/**
 * Input state associated with a pointer-type input device.
 *
 * @param {string} subtype
 *     Kind of pointing device: mouse, pen, touch.
 * @param {boolean} primary
 *     Whether the pointing device is primary.
 */
action.InputState.Pointer = class extends InputState {
  constructor(subtype, primary) {
    super();
    this.pressed = new Set();
    this.subtype = subtype;
    this.primary = primary;
    this.x = 0;
    this.y = 0;
  }
};

/**
 * Repesents an action for dispatch. Used in |action.Chain| and |action.Sequence|.
 *
 * @param {string} id
 *     Input source ID.
 * @param {string} type
 *     Action type: none, key, pointer.
 * @param {string} subtype
 *     Action subtype: pause, keyUp, keyDown, pointerUp, pointerDown, pointerMove, pointerCancel.
 *
 * @throws {InvalidArgumentError}
 *      If any parameters are undefined.
 */
action.Action = class {
  constructor(id, type, subtype) {
    if ([id, type, subtype].includes(undefined)) {
      throw new InvalidArgumentError("Missing id, type or subtype");
    }
    for (let attr of [id, type, subtype]) {
      if (typeof attr != "string") {
        throw new InvalidArgumentError(`Expected string, got: ${attr}`);
      }
    }
    this.id = id;
    this.type = type;
    this.subtype = subtype;
  };

  toString() {
    return `[action ${this.type}]`;
  }

  /**
   * @param {?} actionSequence
   *     Object representing sequence of actions from one input source.
   * @param {?} actionItem
   *     Object representing a single action from |actionSequence|.
   *
   * @return {action.Action}
   *     An action that can be dispatched; corresponds to |actionItem|.
   *
   * @throws {InvalidArgumentError}
   *     If any |actionSequence| or |actionItem| attributes are invalid.
   * @throws {UnsupportedOperationError}
   *     If |actionItem.type| is |pointerCancel|.
   */
  static fromJson(actionSequence, actionItem) {
    let type = actionSequence.type;
    let id = actionSequence.id;
    let subtypes = ACTIONS[type];
    if (!subtypes) {
      throw new InvalidArgumentError("Unknown type: " + type);
    }
    let subtype = actionItem.type;
    if (!subtypes.has(subtype)) {
      throw new InvalidArgumentError(`Unknown subtype for ${type} action: ${subtype}`);
    }

    let item = new action.Action(id, type, subtype);
    if (type === "pointer") {
      action.processPointerAction(id,
          action.PointerParameters.fromJson(actionSequence.parameters), item);
    }

    switch (item.subtype) {
      case action.KeyUp:
      case action.KeyDown:
        let key = actionItem.value;
        // TODO countGraphemes
        // TODO key.value could be a single code point like "\uE012" (see rawKey)
        // or "grapheme cluster"
        if (typeof key != "string") {
          throw new InvalidArgumentError(
              "Expected 'value' to be a string that represents single code point " +
              "or grapheme cluster, got: " + key);
        }
        item.value = key;
        break;

      case action.PointerDown:
      case action.PointerUp:
        assert.positiveInteger(actionItem.button,
            error.pprint`Expected 'button' (${actionItem.button}) to be >= 0`);
        item.button = actionItem.button;
        break;

      case action.PointerMove:
        item.duration = actionItem.duration;
        if (typeof item.duration != "undefined"){
          assert.positiveInteger(item.duration,
              error.pprint`Expected 'duration' (${item.duration}) to be >= 0`);
        }
        if (typeof actionItem.element != "undefined" &&
            !element.isWebElementReference(actionItem.element)) {
          throw new InvalidArgumentError(
              "Expected 'actionItem.element' to be a web element reference, " +
              `got: ${actionItem.element}`);
        }
        item.element = actionItem.element;

        item.x = actionItem.x;
        if (typeof item.x != "undefined") {
          assert.positiveInteger(item.x, error.pprint`Expected 'x' (${item.x}) to be >= 0`);
        }
        item.y = actionItem.y;
        if (typeof item.y != "undefined") {
          assert.positiveInteger(item.y, error.pprint`Expected 'y' (${item.y}) to be >= 0`);
        }
        break;

      case action.PointerCancel:
        throw new UnsupportedOperationError();
        break;

      case action.Pause:
        item.duration = actionItem.duration;
        if (typeof item.duration != "undefined") {
          assert.positiveInteger(item.duration,
              error.pprint`Expected 'duration' (${item.duration}) to be >= 0`);
        }
        break;
    }

    return item;
  }
};

/**
 * Represents a series of ticks, specifying which actions to perform at each tick.
 */
action.Chain = class extends Array {
  toString() {
    return `[chain ${super.toString()}]`;
  }

  /**
   * @param {Array.<?>} actions
   *     Array of objects that each represent an action sequence.
   *
   * @return {action.Chain}
   *     Transpose of |actions| such that actions to be performed in a single tick
   *     are grouped together.
   *
   * @throws {InvalidArgumentError}
   *     If |actions| is not an Array.
   */
  static fromJson(actions) {
    assert.array(actions,
              error.pprint`Expected 'actions' to be an Array, got: ${actions}`);
    let actionsByTick = new action.Chain();
    //  TODO check that each actionSequence in actions refers to a different input ID
    for (let actionSequence of actions) {
      let inputSourceActions = action.Sequence.fromJson(actionSequence);
      for (let i = 0; i < inputSourceActions.length; i++) {
        // new tick
        if (actionsByTick.length < (i + 1)) {
          actionsByTick.push([]);
        }
        actionsByTick[i].push(inputSourceActions[i]);
      }
    }
    return actionsByTick;
  }
};

/**
 * Represents one input source action sequence; this is essentially an |Array.<action.Action>|.
 */
action.Sequence = class extends Array {
  toString() {
    return `[sequence ${super.toString()}]`;
  }

  /**
   * @param {?} actionSequence
   *     Object that represents a sequence action items for one input source.
   *
   * @return {action.Sequence}
   *     Sequence of actions that can be dispatched.
   *
   * @throws {InvalidArgumentError}
   *     If |actionSequence.id| is not a string or it's aleady mapped
   *     to an |action.InputState} incompatible with |actionSequence.type|.
   *     If |actionSequence.actions| is not an Array.
   */
  static fromJson(actionSequence) {
    // used here only to validate 'type' and InputState type
    let inputSourceState = InputState.fromJson(actionSequence);
    let id = actionSequence.id;
    if (typeof id == "undefined") {
      actionSequence.id = id = element.generateUUID();
    } else if (typeof id != "string") {
      throw new InvalidArgumentError(`Expected 'id' to be a string, got: ${id}`);
    }
    let actionItems = actionSequence.actions;
    if (!Array.isArray(actionItems)) {
      throw new InvalidArgumentError(
          `Expected 'actionSequence.actions' to be an Array, got: ${actionSequence.actions}`);
    }

    if (action.inputStateMap.has(id) && !action.inputStateMap.get(id).is(inputSourceState)) {
      throw new InvalidArgumentError(
          `Expected ${id} to be mapped to ${inputSourceState}, ` +
          `got: ${action.inputStateMap.get(id)}`);
    }
    let actions = new action.Sequence();
    for (let actionItem of actionItems) {
      actions.push(action.Action.fromJson(actionSequence, actionItem));
    }
    return actions;
  }
};

/**
 * Represents parameters in an action for a pointer input source.
 *
 * @param {string=} pointerType
 *     Type of pointing device. If the parameter is undefined, "mouse" is used.
 * @param {boolean=} primary
 *     Whether the input source is the primary pointing device.
 *     If the parameter is undefined, true is used.
 */
action.PointerParameters = class {
  constructor(pointerType = "mouse", primary = true) {
    this.pointerType = action.PointerType.get(pointerType);
    assert.boolean(primary);
    this.primary = primary;
  }

  toString() {
    return `[pointerParameters ${this.pointerType}, primary=${this.primary}]`;
  }

  /**
   * @param {?} parametersData
   *     Object that represents pointer parameters.
   *
   * @return {action.PointerParameters}
   *     Validated pointer paramters.
   */
  static fromJson(parametersData) {
    if (typeof parametersData == "undefined") {
      return new action.PointerParameters();
    } else {
      return new action.PointerParameters(parametersData.pointerType, parametersData.primary);
    }
  }
};

/**
 * Adds |pointerType| and |primary| attributes to Action |act|. Helper function
 * for |action.Action.fromJson|.
 *
 * @param {string} id
 *     Input source ID.
 * @param {action.PointerParams} pointerParams
 *     Input source pointer parameters.
 * @param {action.Action} act
 *     Action to be updated.
 *
 * @throws {InvalidArgumentError}
 *     If |id| is already mapped to an |action.InputState| that is
 *     not compatible with |act.subtype|.
 */
action.processPointerAction = function processPointerAction(id, pointerParams, act) {
  let subtype = act.subtype;
  if (action.inputStateMap.has(id) && action.inputStateMap.get(id).subtype !== subtype) {
    throw new InvalidArgumentError(
        `Expected 'id' ${id} to be mapped to InputState whose subtype is ` +
        `${action.inputStateMap.get(id).subtype}, got: ${subtype}`);
  }
  act.pointerType = pointerParams.pointerType;
  act.primary = pointerParams.primary;
};

/** Collect properties associated with KeyboardEvent */
action.Key = class {
  constructor(rawKey) {
    this.key = NORMALIZED_KEY_LOOKUP[rawKey] || rawKey;
    this.code =  KEY_CODE_LOOKUP[rawKey];
    this.location = KEY_LOCATION_LOOKUP[rawKey] || 0;
    this.altKey = false;
    this.shiftKey = false;
    this.ctrlKey = false;
    this.metaKey = false;
    this.repeat = false;
    this.isComposing = false;
    // Prevent keyCode from being guessed in event.js; we don't want to use it anyway.
    this.keyCode = 0;
  }

  update(inputState) {
    this.altKey = inputState.alt;
    this.shiftKey = inputState.shift;
    this.ctrlKey = inputState.ctrl;
    this.metaKey = inputState.meta;
  }
};

/**
 * Dispatch a chain of actions over |chain.length| ticks.
 *
 * This is done by creating a Promise for each tick that resolves once all the
 * Promises for individual tick-actions are resolved. The next tick's actions are
 * not dispatched until the Promise for the current tick is resolved.
 *
 * @param {action.Chain} chain
 *     Actions grouped by tick; each element in |chain| is a sequence of
 *     actions for one tick.
 * @param {element.Store} seenEls
 *     Element store.
 * @param {?} container
 *     Object with |frame| attribute of type |nsIDOMWindow|.
 *
 * @return {Promise}
 *     Promise for dispatching all actions in |chain|.
 */
action.dispatch = function(chain, seenEls, container) {
  let chainEvents = Task.spawn(function*() {
    for (let tickActions of chain) {
      yield action.dispatchTickActions(
        tickActions, action.computeTickDuration(tickActions), seenEls, container);
    }
  });
  return chainEvents;
};

/**
 * Dispatch sequence of actions for one tick.
 *
 * This creates a Promise for one tick that resolves once the Promise for each
 * tick-action is resolved, which takes at least |tickDuration| milliseconds.
 * The resolved set of events for each tick is followed by firing of pending DOM events.
 *
 * Note that the tick-actions are dispatched in order, but they may have different
 * durations and therefore may not end in the same order.
 *
 * @param {Array.<action.Action>} tickActions
 *     List of actions for one tick.
 * @param {number} tickDuration
 *     Duration in milliseconds of this tick.
 * @param {element.Store} seenEls
 *     Element store.
 * @param {?} container
 *     Object with |frame| attribute of type |nsIDOMWindow|.
 *
 * @return {Promise}
 *     Promise for dispatching all tick-actions and pending DOM events.
 */
action.dispatchTickActions = function(tickActions, tickDuration, seenEls, container) {
  let pendingEvents = tickActions.map(toEvents(tickDuration, seenEls, container));
  return Promise.all(pendingEvents).then(() => flushEvents(container));
};

/**
 * Compute tick duration in milliseconds for a collection of actions.
 *
 * @param {Array.<action.Action>} tickActions
 *     List of actions for one tick.
 *
 * @return {number}
 *     Longest action duration in |tickActions| if any, or 0.
 */
action.computeTickDuration = function(tickActions) {
  let max = 0;
  for (let a of tickActions) {
    let affectsWallClockTime = a.subtype == action.Pause ||
        (a.type == "pointer" && a.subtype == action.PointerMove);
    if (affectsWallClockTime && a.duration) {
      max = Math.max(a.duration, max);
    }
  }
  return max;
};

/**
 * Create a closure to use as a map from action definitions to Promise events.
 *
 * @param {number} tickDuration
 *     Duration in milliseconds of this tick.
 * @param {element.Store} seenEls
 *     Element store.
 * @param {?} container
 *     Object with |frame| attribute of type |nsIDOMWindow|.
 *
 * @return {function(action.Action): Promise}
 *     Function that takes an action and returns a Promise for dispatching
 *     the event that corresponds to that action.
 */
function toEvents(tickDuration, seenEls, container) {
  return function (a) {
    if (!action.inputStateMap.has(a.id)) {
      action.inputStateMap.set(a.id, InputState.fromJson(a));
    }
    let inputState = action.inputStateMap.get(a.id);
    switch (a.subtype) {
      case action.KeyUp:
        return dispatchKeyUp(a, inputState, container.frame);

      case action.KeyDown:
        return dispatchKeyDown(a, inputState, container.frame);

      case action.PointerDown:
      case action.PointerUp:
      case action.PointerMove:
      case action.PointerCancel:
        throw new UnsupportedOperationError();

      case action.Pause:
        return dispatchPause(a, tickDuration);
    }
  };
}

/**
 * Dispatch a keyDown action equivalent to pressing a key on a keyboard.
 *
 * @param {action.Action} a
 *     Action to dispatch.
 * @param {action.InputState} inputState
 *     Input state for this action's input source.
 * @param {nsIDOMWindow} win
 *     Current window.
 *
 * @return {Promise}
 *     Promise to dispatch at least a keydown event, and keypress if appropriate.
 */
function dispatchKeyDown(a, inputState, win) {
  return new Promise(resolve => {
    let keyEvent = new action.Key(a.value);
    keyEvent.repeat = inputState.isPressed(keyEvent.key);
    inputState.press(keyEvent.key);
    if (keyEvent.key in MODIFIER_NAME_LOOKUP) {
      inputState.setModState(keyEvent.key, true);
    }
    // Append a copy of |a| with keyUp subtype
    action.inputsToCancel.push(Object.assign({}, a, {subtype: action.KeyUp}));
    keyEvent.update(inputState);
    event.sendKeyDown(keyEvent.key, keyEvent, win);

    resolve();
  });
}

/**
 * Dispatch a keyUp action equivalent to releasing a key on a keyboard.
 *
 * @param {action.Action} a
 *     Action to dispatch.
 * @param {action.InputState} inputState
 *     Input state for this action's input source.
 * @param {nsIDOMWindow} win
 *     Current window.
 *
 * @return {Promise}
 *     Promise to dispatch a keyup event.
 */
function dispatchKeyUp(a, inputState, win) {
  return new Promise(resolve => {
    let keyEvent = new action.Key(a.value);
    if (!inputState.isPressed(keyEvent.key)) {
      resolve();
      return;
    }
    if (keyEvent.key in MODIFIER_NAME_LOOKUP) {
      inputState.setModState(keyEvent.key, false);
    }
    inputState.release(keyEvent.key);
    keyEvent.update(inputState);
    event.sendKeyUp(keyEvent.key, keyEvent, win);

    resolve();
  });
}

/**
 * Dispatch a pause action equivalent waiting for |a.duration| milliseconds, or a
 * default time interval of |tickDuration|.
 *
 * @param {action.Action} a
 *     Action to dispatch.
 * @param {number} tickDuration
 *     Duration in milliseconds of this tick.
 *
 * @return {Promise}
 *     Promise that is resolved after the specified time interval.
 */
function dispatchPause(a, tickDuration) {
  const TIMER = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  let duration = typeof a.duration == "undefined" ? tickDuration : a.duration;
  return new Promise(resolve =>
      TIMER.initWithCallback(resolve, duration, Ci.nsITimer.TYPE_ONE_SHOT)
  );
}

// helpers
/**
 * Force any pending DOM events to fire.
 *
 * @param {?} container
 *     Object with |frame| attribute of type |nsIDOMWindow|.
 *
 * @return {Promise}
 *     Promise to flush DOM events.
 */
function flushEvents(container) {
  return new Promise(resolve => container.frame.requestAnimationFrame(resolve));
}

function capitalize(str) {
  if (typeof str != "string") {
    throw new InvalidArgumentError(`Expected string, got: ${str}`);
  }
  return str.charAt(0).toUpperCase() + str.slice(1);
}
