/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Log.jsm");

Cu.import("chrome://marionette/content/element.js");
Cu.import("chrome://marionette/content/error.js");


this.EXPORTED_SYMBOLS = ["action"];

const logger = Log.repository.getLogger("Marionette");

// TODO? With ES 2016 and Symbol you can make a safer approximation
// to an enum e.g. https://gist.github.com/xmlking/e86e4f15ec32b12c4689
/**
 * Implements WebDriver Actions API: a low-level interfac for providing
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
 * @throws InvalidArgumentError
 *     If |str| is not a valid pointer type.
 */
action.PointerType.get = function(str) {
  let name = capitalize(str);
  if (!(name in this)) {
    throw new InvalidArgumentError(`Unknown pointerType: ${str}`);
  }
  return this[name];
};

/**
 * Input state associated with current session. This is a map between input ID and
 * the device state for that input source, with one entry for each active input source.
 */
action.inputStateMap = new Map();

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
   * @param {?} actionSequence
   *     Object representing an action sequence.
   *
   * @return {action.InputState}
   *     An |action.InputState| object for the type of the |actionSequence|.
   *
   * @throws InvalidArgumentError
   *     If |actionSequence.type| is not valid.
   */
  static fromJson(actionSequence) {
    let type = actionSequence.type;
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
 * @throws InvalidArgumentError
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
   *     Object representing a single action from |actionSequence|
   *
   * @return {action.Action}
   *     An action that can be dispatched; corresponds to |actionItem|.
   *
   * @throws InvalidArgumentError
   *     If any |actionSequence| or |actionItem| attributes are invalid.
   * @throws UnsupportedOperationError
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
        if (typeof key != "string" || (typeof key == "string" && key.length != 1)) {
          throw new InvalidArgumentError("Expected 'key' to be a single-character string, " +
                                         "got: " + key);
        }
        item.value = key;
        break;

      case action.PointerDown:
      case action.PointerUp:
        assertPositiveInteger(actionItem.button, "button");
        item.button = actionItem.button;
        break;

      case action.PointerMove:
        item.duration = actionItem.duration;
        if (typeof item.duration != "undefined"){
          assertPositiveInteger(item.duration, "duration");
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
          assertPositiveInteger(item.x, "x");
        }
        item.y = actionItem.y;
        if (typeof item.y != "undefined") {
          assertPositiveInteger(item.y, "y");
        }
        break;

      case action.PointerCancel:
        throw new UnsupportedOperationError();
        break;

      case action.Pause:
        item.duration = actionItem.duration;
        if (typeof item.duration != "undefined") {
          assertPositiveInteger(item.duration, "duration");
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
   * @param {Array<?>} actions
   *     Array of objects that each represent an action sequence.
   *
   * @return {action.Chain}
   *     Transpose of |actions| such that actions to be performed in a single tick
   *     are grouped together.
   *
   * @throws InvalidArgumentError
   *     If |actions| is not an Array.
   */
  static fromJson(actions) {
    if (!Array.isArray(actions)) {
      throw new InvalidArgumentError(`Expected 'actions' to be an Array, got: ${actions}`);
    }
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
 * Represents one input source action sequence; this is essentially an |Array<action.Action>|.
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
   * @throws InvalidArgumentError
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
 *     If the parameter is underfined, true is used.
 */
action.PointerParameters = class {
  constructor(pointerType = "mouse", primary = true) {
    this.pointerType = action.PointerType.get(pointerType);
    assertBoolean(primary, "primary");
    this.primary = primary;
  };

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
 * @throws InvalidArgumentError
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

// helpers
function assertPositiveInteger(value, name = undefined) {
  let suffix = name ? ` (${name})` : '';
  if (!Number.isInteger(value) || value < 0) {
    throw new InvalidArgumentError(`Expected integer >= 0${suffix}, got: ${value}`);
  }
}

function assertBoolean(value, name = undefined) {
  let suffix = name ? ` (${name})` : '';
  if (typeof(value) != "boolean") {
    throw new InvalidArgumentError(`Expected boolean${suffix}, got: ${value}`);
  }
}

function capitalize(str) {
  if (typeof str != "string") {
    throw new InvalidArgumentError(`Expected string, got: ${str}`);
  }
  return str.charAt(0).toUpperCase() + str.slice(1);
}
