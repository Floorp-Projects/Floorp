/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint no-dupe-keys:off */
/* eslint-disable no-restricted-globals */

"use strict";

const EXPORTED_SYMBOLS = ["action"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  AppInfo: "chrome://remote/content/marionette/appinfo.js",
  assert: "chrome://remote/content/shared/webdriver/Assert.jsm",
  element: "chrome://remote/content/marionette/element.js",
  error: "chrome://remote/content/shared/webdriver/Errors.jsm",
  event: "chrome://remote/content/marionette/event.js",
  keyData: "chrome://remote/content/shared/webdriver/KeyData.jsm",
  Log: "chrome://remote/content/shared/Log.jsm",
  pprint: "chrome://remote/content/shared/Format.jsm",
  Sleep: "chrome://remote/content/marionette/sync.js",
});

XPCOMUtils.defineLazyGetter(lazy, "logger", () =>
  lazy.Log.get(lazy.Log.TYPES.MARIONETTE)
);

// TODO? With ES 2016 and Symbol you can make a safer approximation
// to an enum e.g. https://gist.github.com/xmlking/e86e4f15ec32b12c4689
/**
 * Implements WebDriver Actions API: a low-level interface for providing
 * virtualised device input to the web browser.
 *
 * Typical usage is to construct an action chain and then dispatch it:
 * const state = new action.State();
 * const chain = new action.Chain.fromJSON(state, protocolData);
 * await chain.dispatch(state, window);
 *
 * @namespace
 */
this.action = {};

/** Map from normalized key value to UI Events modifier key name */
const MODIFIER_NAME_LOOKUP = {
  Alt: "alt",
  Shift: "shift",
  Control: "ctrl",
  Meta: "meta",
};

/**
 * State associated with actions
 *
 * Typically each top-level browsing context in a session should have a single State object
 */
action.State = class {
  constructor(options = {}) {
    const { specCompatPointerOrigin = true } = options;

    /** Flag for WebDriver spec conforming pointer origin calculation. */
    this.specCompatPointerOrigin = specCompatPointerOrigin;

    /**
     * A map between input ID and the device state for that input
     * source, with one entry for each active input source.
     *
     * Maps string => InputSource
     */
    this.inputStateMap = new Map();

    /**
     * List of {@link Action} associated with current session.  Used to
     * manage dispatching events when resetting the state of the input sources.
     * Reset operations are assumed to be idempotent.
     */
    this.inputsToCancel = new TickActions();

    /**
     * Map between string input id and numeric pointer id
     */
    this.pointerIdMap = new Map();
  }

  toString() {
    return `[object ${this.constructor.name} ${JSON.stringify(this)}]`;
  }

  /**
   * Get the state for a given input source.
   *
   * @param {string} id Input source id.
   * @return {InputSource} Input source state.
   */
  getInputSource(id) {
    return this.inputStateMap.get(id);
  }

  /**
   * Find or add state for an input source. The caller should verify
   * that the returned state is the expected type.
   *
   * @param {string} id Input source id.
   * @param {InputSource} newInputSource Input source state.
   */
  getOrAddInputSource(id, newInputSource) {
    let inputSource = this.getInputSource(id);
    if (inputSource === undefined) {
      this.inputStateMap.set(id, newInputSource);
      inputSource = newInputSource;
    }
    return inputSource;
  }

  /**
   * Iterate over all input states of a given type
   *
   * @param {string} type Input source type name (e.g. "pointer").
   * @return {Iterator} Iterator over [id, input source].
   */
  *inputSourcesByType(type) {
    for (let [id, inputSource] of this.inputStateMap) {
      if (inputSource.type === type) {
        yield [id, inputSource];
      }
    }
  }

  /**
   * Get a numerical pointer id for a given pointer
   *
   * Pointer ids are positive integers. Mouse pointers are typically
   * ids 0 or 1. Non-mouse pointers never get assigned id < 2. Each
   * pointer gets a unique id.
   *
   * @param {string} id Pointer id.
   * @param {string} id Pointer type.
   * @return {number} Numerical pointer id.
   */
  getPointerId(id, type) {
    let pointerId = this.pointerIdMap.get(id);
    if (pointerId === undefined) {
      // Reserve pointer ids 0 and 1 for mouse pointers
      const idValues = Array.from(this.pointerIdMap.values());
      if (type === "mouse") {
        for (let mouseId of [0, 1]) {
          if (!idValues.includes(mouseId)) {
            pointerId = mouseId;
            break;
          }
        }
      }
      if (pointerId === undefined) {
        pointerId = Math.max(1, ...idValues) + 1;
      }
      this.pointerIdMap.set(id, pointerId);
    }
    return pointerId;
  }
};

/**
 * Device state for an input source.
 */
class InputSource {
  #id;
  static type = null;

  constructor(id) {
    this.#id = id;
    this.type = this.constructor.type;
  }

  toString() {
    return `[object ${this.constructor.name} id: ${this.#id} type: ${
      this.type
    }]`;
  }

  /**
   * @param {State} state Actions state.
   * @param {Sequence} actionSequence Actions for a specific input source.
   *
   * @return {InputSource}
   *     An {@link InputSource} object for the type of the
   *     {@link actionSequence}.
   *
   * @throws {InvalidArgumentError}
   *     If {@link actionSequence.type} is not valid.
   */
  static fromJSON(state, actionSequence) {
    const { id, type } = actionSequence;
    lazy.assert.string(
      id,
      lazy.pprint`Expected 'id' to be a string, got ${id}`
    );
    const cls = inputSourceTypes.get(type);
    if (cls === undefined) {
      throw new lazy.error.InvalidArgumentError(
        lazy.pprint`Unknown action type: ${type}`
      );
    }

    const sequenceInputSource = cls.fromJSON(state, actionSequence);
    const inputSource = state.getOrAddInputSource(id, sequenceInputSource);
    if (inputSource.type !== type) {
      throw new lazy.error.InvalidArgumentError(
        `Expected input source ${id} to be type ${inputSource.type}, ` +
          `got ${type}`
      );
    }
  }
}

/**
 * Input state not associated with a specific physical device.
 */
class NullInputSource extends InputSource {
  static type = "none";

  static fromJSON(state, actionSequence) {
    const { id } = actionSequence;
    return new this(id);
  }
}

/**
 * Input state associated with a keyboard-type device.
 */
class KeyInputSource extends InputSource {
  static type = "key";

  constructor(id) {
    super(id);
    this.pressed = new Set();
    this.alt = false;
    this.shift = false;
    this.ctrl = false;
    this.meta = false;
  }

  static fromJSON(state, actionSequence) {
    const { id } = actionSequence;
    return new this(id);
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
      throw new lazy.error.InvalidArgumentError(
        "Expected 'key' to be one of " +
          Object.keys(MODIFIER_NAME_LOOKUP) +
          lazy.pprint`, got ${key}`
      );
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
   *     True if |key| was present before removal, false otherwise.
   */
  release(key) {
    return this.pressed.delete(key);
  }
}

/**
 * Input state associated with a pointer-type device.
 */
class PointerInputSource extends InputSource {
  static type = "pointer";

  /**
   * @param {Pointer} pointer Object representing the specific pointer
   * type associated with this input source.
   */
  constructor(id, pointer) {
    super(id);
    this.pointer = pointer;
    this.x = 0;
    this.y = 0;
    this.pressed = new Set();
  }

  /**
   * Check whether |button| is pressed.
   *
   * @param {number} button
   *     Positive integer that refers to a mouse button.
   *
   * @return {boolean}
   *     True if |button| is in set of pressed buttons.
   */
  isPressed(button) {
    lazy.assert.positiveInteger(button);
    return this.pressed.has(button);
  }

  /**
   * Add |button| to the set of pressed keys.
   *
   * @param {number} button
   *     Positive integer that refers to a mouse button.
   *
   * @return {Set}
   *     Set of pressed buttons.
   */
  press(button) {
    lazy.assert.positiveInteger(button);
    this.pressed.add(button);
  }

  /**
   * Remove |button| from the set of pressed buttons.
   *
   * @param {number} button
   *     A positive integer that refers to a mouse button.
   *
   * @return {boolean}
   *     True if |button| was present before removals, false otherwise.
   */
  release(button) {
    lazy.assert.positiveInteger(button);
    return this.pressed.delete(button);
  }

  static fromJSON(state, actionSequence) {
    const { id, parameters } = actionSequence;

    const pointerType = parameters?.pointerType ?? "mouse";
    const pointerId = state.getPointerId(id, pointerType);
    const pointer = Pointer.fromJSON(pointerId, pointerType);
    return new this(id, pointer);
  }
}

const inputSourceTypes = new Map();
for (let cls of [NullInputSource, KeyInputSource, PointerInputSource]) {
  inputSourceTypes.set(cls.type, cls);
}

/**
 * Representation of a coordinate origin
 */
class Origin {
  /**
   * Viewport coordinates of the origin of this coordinate system.
   *
   * This is overridden in subclasses to provide a class-specific origin.
   *
   * @param {State} state - Actions state.
   * @param {InputSource} inputSource - State of current input device.
   * @param {WindowProxy} win - Current window global
   */
  getOriginCoordinates(state, inputSource, win) {
    throw new Error(
      `originCoordinates not defined for ${this.constructor.name}`
    );
  }

  /**
   * Convert [x, y] coordinates to viewport coordinates
   *
   * @param {State} state - Actions state
   * @param {InputSource} inputSource - State of the current input device
   * @param {Array<number>} coords - [x, y] coordinate of target relative to origin
   * @param {WindowProxy} win - Current window global
   */
  getTargetCoordinates(state, inputSource, coords, win) {
    const [x, y] = coords;
    const origin = this.getOriginCoordinates(state, inputSource, win);
    return [origin.x + x, origin.y + y];
  }

  /**
   * @param {Element|string=} origin - Type of orgin, one of "viewport", "pointer", element or undefined.
   *
   * @return {Origin} - An origin object representing the origin.
   *
   * @throws {InvalidArgumentError}
   *     If <code>origin</code> isn't a valid origin.
   */
  static fromJSON(origin) {
    if (origin === undefined || origin === "viewport") {
      return new ViewportOrigin();
    }
    if (origin === "pointer") {
      return new PointerOrigin();
    }
    if (lazy.element.isElement(origin)) {
      return new ElementOrigin(origin);
    }

    throw new lazy.error.InvalidArgumentError(
      `Expected 'origin' to be undefined, "viewport", "pointer", ` +
        lazy.pprint`or an element, got: ${origin}`
    );
  }
}

class ViewportOrigin extends Origin {
  getOriginCoordinates(state, inputSource, win) {
    return { x: 0, y: 0 };
  }
}

class PointerOrigin extends Origin {
  getOriginCoordinates(state, inputSource, win) {
    return { x: inputSource.x, y: inputSource.y };
  }
}

class ElementOrigin extends Origin {
  /**
   * @param {Element} element - The element providing the coordinate origin.
   */
  constructor(element) {
    super();
    this.element = element;
  }

  getOriginCoordinates(state, inputSource, win) {
    if (state.specCompatPointerOrigin) {
      const clientRects = this.element.getClientRects();
      // The spec doesn't handle this case; https://github.com/w3c/webdriver/issues/1642
      if (!clientRects.length) {
        throw new lazy.error.MoveTargetOutOfBoundsError(
          `Origin element is not displayed`
        );
      }
      return lazy.element.getInViewCentrePoint(clientRects[0], win);
    }
    return lazy.element.coordinates(this.element);
  }
}

/**
 * Repesents the behaviour of a single input source at a single
 * point in time.
 *
 * @param {string} id - Input source ID.
 */
class Action {
  /** Type of the input source associated with this action */
  static type = null;
  /** Type of action specific to the input source */
  static subtype = null;
  /** Whether this kind of action affects the overall duration of a tick */
  affectsWallClockTime = false;

  constructor(id) {
    this.id = id;
    this.type = this.constructor.type;
    this.subtype = this.constructor.subtype;
  }

  toString() {
    return `[${this.constructor.name} ${this.type}:${this.subtype}]`;
  }

  /**
   * Dispatch the action to the relevant window.
   *
   * This is overridden by subclasses to implement the type-specific
   * dispatch of the action.
   *
   * @param {State} state - Actions state.
   * @param {InputSource} inputSource - State of the current input device.
   * @param {number} tickDuration - Length of the current tick, in ms.
   * @param {WindowProxy} win - Current window global.
   * @return {Promise} - Promise that is resolved once the action is complete.
   */
  dispatch(state, inputSource, tickDuration, win) {
    throw new Error(
      `Action subclass ${this.constructor.name} must override dispatch()`
    );
  }

  /**
   * @param {string} type - Input source type.
   * @param {string} type - Input source id.
   * @param {Object} actionItem - Object representing a single action.
   *
   * @return {Action} - An action that can be dispatched.
   *
   * @throws {InvalidArgumentError}
   *     If any <code>actionSequence</code> or <code>actionItem</code>
   *     attributes are invalid.
   */
  static fromJSON(type, id, actionItem) {
    const subtype = actionItem.type;
    const subtypeMap = actionTypes.get(type);
    if (subtypeMap === undefined) {
      throw new lazy.error.InvalidArgumentError(`Unknown action type: ${type}`);
    }
    let cls = subtypeMap.get(subtype);
    // Non-device specific actions can happen for any action type
    if (cls === undefined) {
      cls = actionTypes.get("none").get(subtype);
    }
    if (cls === undefined) {
      throw new lazy.error.InvalidArgumentError(
        `Unknown subtype ${subtype} for type ${type}`
      );
    }
    return cls.fromJSON(id, actionItem);
  }
}

/**
 * Action not associated with a specific input device.
 */
class NullAction extends Action {
  static type = "none";
}

/**
 * Action that waits for a given duration.
 *
 * @param {string} id - Input source ID.
 * @param {Object} options - Named arguments.
 * @param {number} options.duration - Time to pause, in ms.
 */
class PauseAction extends NullAction {
  static subtype = "pause";
  affectsWallClockTime = true;

  constructor(id, options) {
    super(id);
    const { duration } = options;
    this.duration = duration;
  }

  /**
   * Dispatch pause action
   *
   * @param {State} state - Actions state.
   * @param {InputSource} inputSource - State of the current input device.
   * @param {number} tickDuration - Length of the current tick, in ms.
   * @param {WindowProxy} win - Current window global.
   * @return {Promise} - Promise that is resolved once the action is complete.
   */
  dispatch(state, inputSource, tickDuration, win) {
    const ms = this.duration ?? tickDuration;
    lazy.logger.trace(
      ` Dispatch ${this.constructor.name} with ${this.id} ${ms}`
    );
    return lazy.Sleep(ms);
  }

  static fromJSON(id, actionItem) {
    const { duration } = actionItem;
    if (duration !== undefined) {
      lazy.assert.positiveInteger(
        duration,
        lazy.pprint`Expected 'duration' (${duration}) to be >= 0`
      );
    }
    return new this(id, { duration });
  }
}

/**
 * Action associated with a keyboard input device
 *
 * @param {string} id - Input source ID.
 * @param {Object} options - Named arguments.
 * @param {string} options.value - Key character.
 */
class KeyAction extends Action {
  static type = "key";

  constructor(id, options) {
    super(id);
    const { value } = options;
    this.value = value;
  }

  getEventData(inputSource) {
    let value = this.value;
    if (inputSource.shift) {
      value = lazy.keyData.getShiftedKey(value);
    }
    return new KeyEventData(value);
  }

  static fromJSON(id, actionItem) {
    // TODO countGraphemes
    // TODO key.value could be a single code point like "\uE012"
    // (see rawKey) or "grapheme cluster"
    // https://bugzilla.mozilla.org/show_bug.cgi?id=1496323
    const value = actionItem.value;
    lazy.assert.string(
      value,
      "Expected 'value' to be a string that represents single code point " +
        lazy.pprint`or grapheme cluster, got ${value}`
    );
    return new this(id, { value });
  }
}

/**
 * Action equivalent to pressing a key on a keyboard.
 *
 * @param {string} id - Input source ID.
 * @param {string} value - Key character.
 */
class KeyDownAction extends KeyAction {
  static subtype = "keyDown";

  dispatch(state, inputSource, tickDuration, win) {
    lazy.logger.trace(
      `Dispatch ${this.constructor.name} with ${this.id} ${this.value}`
    );
    return new Promise(resolve => {
      const keyEvent = this.getEventData(inputSource);
      keyEvent.repeat = inputSource.isPressed(keyEvent.key);
      inputSource.press(keyEvent.key);
      if (keyEvent.key in MODIFIER_NAME_LOOKUP) {
        inputSource.setModState(keyEvent.key, true);
      }

      // Append a copy of |a| with keyUp subtype
      state.inputsToCancel.push(new KeyUpAction(this.id, this));
      keyEvent.update(state, inputSource);
      lazy.event.sendKeyDown(keyEvent, win);

      resolve();
    });
  }
}

/**
 * Action equivalent to releasing a key on a keyboard.
 *
 * @param {string} id - Input source ID.
 * @param {string} value - Key character.
 */
class KeyUpAction extends KeyAction {
  static subtype = "keyUp";

  dispatch(state, inputSource, tickDuration, win) {
    lazy.logger.trace(
      `Dispatch ${this.constructor.name} with ${this.id} ${this.value}`
    );
    return new Promise(resolve => {
      const keyEvent = this.getEventData(inputSource);
      if (!inputSource.isPressed(keyEvent.key)) {
        resolve();
        return;
      }
      if (keyEvent.key in MODIFIER_NAME_LOOKUP) {
        inputSource.setModState(keyEvent.key, false);
      }
      inputSource.release(keyEvent.key);
      keyEvent.update(state, inputSource);

      lazy.event.sendKeyUp(keyEvent, win);
      resolve();
    });
  }
}

/**
 * Action associated with a pointer input device
 *
 * @param {string} id - Input source ID.
 * @param {Object} options - Named arguments.
 * @param {number=} options.width - Pointer width in pixels.
 * @param {number=} options.height - Pointer height in pixels.
 * @param {number=} options.pressure - Pointer pressure.
 * @param {number=} options.tangentialPressure - Pointer tangential pressure.
 * @param {number=} options.tiltX - Pointer X tilt angle.
 * @param {number=} options.tiltX - Pointer Y tilt angle.
 * @param {number=} options.twist - Pointer twist angle.
 * @param {number=} options.altitudeAngle - Pointer altitude angle.
 * @param {number=} options.azimuthAngle - Pointer azimuth angle.
 */
class PointerAction extends Action {
  static type = "pointer";

  constructor(id, options) {
    super(id);
    const {
      width,
      height,
      pressure,
      tangentialPressure,
      tiltX,
      tiltY,
      twist,
      altitudeAngle,
      azimuthAngle,
    } = options;
    this.width = width;
    this.height = height;
    this.pressure = pressure;
    this.tangentialPressure = tangentialPressure;
    this.tiltX = tiltX;
    this.tiltY = tiltY;
    this.twist = twist;
    this.altitudeAngle = altitudeAngle;
    this.azimuthAngle = azimuthAngle;
  }

  /**
   * Validate properties common to all pointer types
   *
   * @param {Object} actionItem - Object representing a single action.
   */
  static validateCommon(actionItem) {
    const {
      width,
      height,
      pressure,
      tangentialPressure,
      tiltX,
      tiltY,
      twist,
      altitudeAngle,
      azimuthAngle,
    } = actionItem;
    if (width !== undefined) {
      lazy.assert.positiveInteger(
        width,
        lazy.pprint`Expected 'width' (${width}) to be >= 0`
      );
    }
    if (height !== undefined) {
      lazy.assert.positiveInteger(
        height,
        lazy.pprint`Expected 'height' (${height}) to be >= 0`
      );
    }
    if (pressure !== undefined) {
      lazy.assert.numberInRange(
        pressure,
        [0, 1],
        lazy.pprint`Expected 'pressure' (${pressure}) to be in range 0 to 1`
      );
    }
    if (tangentialPressure !== undefined) {
      lazy.assert.numberInRange(
        tangentialPressure,
        [-1, 1],
        lazy.pprint`Expected 'tangentialPressure' (${tangentialPressure}) to be in range -1 to 1`
      );
    }
    if (tiltX !== undefined) {
      lazy.assert.integerInRange(
        tiltX,
        [-90, 90],
        lazy.pprint`Expected 'tiltX' (${tiltX}) to be in range -90 to 90`
      );
    }
    if (tiltY !== undefined) {
      lazy.assert.integerInRange(
        tiltY,
        [-90, 90],
        lazy.pprint`Expected 'tiltY' (${tiltY}) to be in range -90 to 90`
      );
    }
    if (twist !== undefined) {
      lazy.assert.integerInRange(
        twist,
        [0, 359],
        lazy.pprint`Expected 'twist' (${twist}) to be in range 0 to 359`
      );
    }
    if (altitudeAngle !== undefined) {
      lazy.assert.numberInRange(
        altitudeAngle,
        [0, Math.PI / 2],
        lazy.pprint`Expected 'altitudeAngle' (${altitudeAngle}) to be in range 0 to ${Math.PI /
          2}`
      );
    }
    if (azimuthAngle !== undefined) {
      lazy.assert.numberInRange(
        azimuthAngle,
        [0, 2 * Math.PI],
        lazy.pprint`Expected 'azimuthAngle' (${azimuthAngle}) to be in range 0 to ${2 *
          Math.PI}`
      );
    }
    return {
      width,
      height,
      pressure,
      tangentialPressure,
      tiltX,
      tiltY,
      twist,
      altitudeAngle,
      azimuthAngle,
    };
  }
}

/**
 * Action associated with a pointer input device being depressed.
 *
 * @param {string} id - Input source ID.
 * @param {Object} options - Named arguments.
 * @param {number} options.button - Button being pressed. For devices without buttons (e.g. touch), this should be 0.
 * @param {number=} options.width - Pointer width in pixels.
 * @param {number=} options.height - Pointer height in pixels.
 * @param {number=} options.pressure - Pointer pressure.
 * @param {number=} options.tangentialPressure - Pointer tangential pressure.
 * @param {number=} options.tiltX - Pointer X tilt angle.
 * @param {number=} options.tiltX - Pointer Y tilt angle.
 * @param {number=} options.twist - Pointer twist angle.
 * @param {number=} options.altitudeAngle - Pointer altitude angle.
 * @param {number=} options.azimuthAngle - Pointer azimuth angle.
 */
class PointerDownAction extends PointerAction {
  static subtype = "pointerDown";

  constructor(id, options) {
    super(id, options);
    const { button } = options;
    this.button = button;
  }

  dispatch(state, inputSource, tickDuration, win) {
    lazy.logger.trace(
      `Dispatch ${this.constructor.name} ${inputSource.pointer.type} with id: ${this.id} button: ${this.button}`
    );
    return new Promise(resolve => {
      if (inputSource.isPressed(this.button)) {
        resolve();
        return;
      }

      inputSource.press(this.button);
      // Append a copy of |a| with pointerUp subtype
      state.inputsToCancel.push(new PointerUpAction(this.id, this));
      inputSource.pointer.pointerDown(state, inputSource, this, win);
      resolve();
    });
  }

  static fromJSON(id, actionItem) {
    const props = PointerAction.validateCommon(actionItem);
    const { button } = actionItem;
    lazy.assert.positiveInteger(
      button,
      lazy.pprint`Expected 'button' (${button}) to be >= 0`
    );
    props.button = button;
    return new this(id, props);
  }
}

/**
 * Action associated with a pointer input device being released.
 *
 * @param {string} id - Input source ID.
 * @param {Object} options - Named arguments.
 * @param {number} options.button - Button being released. For devices without buttons (e.g. touch), this should be 0.
 * @param {number=} options.width - Pointer width in pixels.
 * @param {number=} options.height - Pointer height in pixels.
 * @param {number=} options.pressure - Pointer pressure.
 * @param {number=} options.tangentialPressure - Pointer tangential pressure.
 * @param {number=} options.tiltX - Pointer X tilt angle.
 * @param {number=} options.tiltX - Pointer Y tilt angle.
 * @param {number=} options.twist - Pointer twist angle.
 * @param {number=} options.altitudeAngle - Pointer altitude angle.
 * @param {number=} options.azimuthAngle - Pointer azimuth angle.
 */
class PointerUpAction extends PointerAction {
  static subtype = "pointerUp";

  constructor(id, options) {
    super(id, options);
    const { button } = options;
    this.button = button;
  }

  dispatch(state, inputSource, tickDuration, win) {
    lazy.logger.trace(
      `Dispatch ${this.constructor.name} ${inputSource.pointer.type} with id: ${this.id} button: ${this.button}`
    );
    return new Promise(resolve => {
      if (!inputSource.isPressed(this.button)) {
        resolve();
        return;
      }

      inputSource.release(this.button);
      inputSource.pointer.pointerUp(state, inputSource, this, win);

      resolve();
    });
  }

  static fromJSON(id, actionItem) {
    const props = PointerAction.validateCommon(actionItem);
    const { button } = actionItem;
    lazy.assert.positiveInteger(
      button,
      lazy.pprint`Expected 'button' (${button}) to be >= 0`
    );
    props.button = button;
    return new this(id, props);
  }
}

/**
 * Action associated with a pointer input device being moved.
 *
 * @param {string} id - Input source ID.
 * @param {Object} options - Named arguments.
 * @param {number=} options.width - Pointer width in pixels.
 * @param {number=} options.height - Pointer height in pixels.
 * @param {number=} options.pressure - Pointer pressure.
 * @param {number=} options.tangentialPressure - Pointer tangential pressure.
 * @param {number=} options.tiltX - Pointer X tilt angle.
 * @param {number=} options.tiltX - Pointer Y tilt angle.
 * @param {number=} options.twist - Pointer twist angle.
 * @param {number=} options.altitudeAngle - Pointer altitude angle.
 * @param {number=} options.azimuthAngle - Pointer azimuth angle.
 * @param {number=} options.duration - Duration of move in ms.
 * @param {Origin} options.origin - Origin of target coordinates.
 * @param {number} options.x - X value of target coordinates.
 * @param {number} options.y - Y value of target coordinates.
 */
class PointerMoveAction extends PointerAction {
  static subtype = "pointerMove";
  affectsWallClockTime = true;

  constructor(id, options) {
    super(id, options);
    const { duration, origin, x, y } = options;
    this.duration = duration;
    this.origin = origin;
    this.x = x;
    this.y = y;
  }

  dispatch(state, inputSource, tickDuration, win) {
    lazy.logger.trace(
      `Dispatch ${this.constructor.name} ${inputSource.pointer.type} with id: ${this.id} x: ${this.x} y: ${this.y}`
    );
    const target = this.origin.getTargetCoordinates(
      state,
      inputSource,
      [this.x, this.y],
      win
    );

    assertInViewPort(target, win);

    return moveOverTime(
      [[inputSource.x, inputSource.y]],
      [target],
      this.duration ?? tickDuration,
      target => this.performOnePointerMove(state, inputSource, target, win)
    );
  }

  /**
   * Perform one part of a pointer move corresponding to a specific emitted event.
   *
   * @param {State} state - Actions state.
   * @param {InputSource} inputSource - State of the current input device.
   * @param {Array<Array<Number>>} targets - Array of [x, y] arrays
   * specifying the viewport coordinates to move to.
   * @param {WindowProxy} win - Current window global.
   */
  performOnePointerMove(state, inputSource, targets, win) {
    if (targets.length !== 1) {
      throw new Error(
        "PointerMoveAction.performOnePointerMove requires a single target"
      );
    }
    const target = targets[0];
    lazy.logger.trace(
      `PointerMoveAction.performOnePointerMove ${JSON.stringify(target)}`
    );
    if (target[0] == inputSource.x && target[1] == inputSource.y) {
      return;
    }

    inputSource.pointer.pointerMove(
      state,
      inputSource,
      this,
      target[0],
      target[1],
      win
    );

    inputSource.x = target[0];
    inputSource.y = target[1];
  }

  static fromJSON(id, actionItem) {
    const props = PointerAction.validateCommon(actionItem);
    const { duration, origin, x, y } = actionItem;
    if (duration !== undefined) {
      lazy.assert.positiveInteger(
        duration,
        lazy.pprint`Expected 'duration' (${duration}) to be >= 0`
      );
    }
    const originObject = Origin.fromJSON(origin);
    if (x !== undefined) {
      lazy.assert.integer(x, lazy.pprint`Expected 'x' (${x}) to be an Integer`);
    }
    if (y !== undefined) {
      lazy.assert.integer(y, lazy.pprint`Expected 'y' (${y}) to be an Integer`);
    }
    props.duration = duration;
    props.origin = originObject;
    props.x = x;
    props.y = y;
    return new this(id, props);
  }
}

/**
 * Split a transition from startCoord to targetCoord linearly over duration.
 *
 * startCoords and targetCoords are lists of [x,y] positions in some space
 * (e.g. screen position or scroll delta). This function will linearly
 * interoplate intermediate positions, sending out roughly one event
 * per frame to simulate moving between startCoord and targetCoord in
 * a time of tickDuration milliseconds. The callback function is
 * responsible for actually emitting the event, given the current
 * position in the coordinate space.
 *
 * @param {Array.<Array>} startCoords - Array of initial [x, y] coordinates
 * for each input source involved in the move.
 * @param {number} duration - Time in ms the move will take.
 * @param {Function} callback - Function that actually performs the
 * move. This takes a single parameter which is an array of [x, y]
 * coordinates corresponding to the move targets.
 */
async function moveOverTime(startCoords, targetCoords, duration, callback) {
  lazy.logger.trace(
    `moveOverTime start: ${startCoords} target: ${targetCoords} duration: ${duration}`
  );

  if (startCoords.length !== targetCoords.length) {
    throw new Error(
      "Expected equal number of start coordinates and target coordinates"
    );
  }

  if (
    !startCoords.every(item => item.length == 2) ||
    !targetCoords.every(item => item.length == 2)
  ) {
    throw new Error(
      "Expected start coordinates target coordinates to be Array of multiple [x,y] coordinates."
    );
  }

  if (duration === 0) {
    // move pointer to destination in one step
    callback(targetCoords);
    return;
  }

  const timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  // interval between pointermove increments in ms, based on common vsync
  const fps60 = 17;

  const distances = targetCoords.map((targetCoord, i) => {
    const startCoord = startCoords[i];
    return [targetCoord[0] - startCoord[0], targetCoord[1] - startCoord[1]];
  });
  const ONE_SHOT = Ci.nsITimer.TYPE_ONE_SHOT;
  const startTime = Date.now();
  const intermediatePointerEvents = (async () => {
    // wait |fps60| ms before performing first incremental pointer move
    await new Promise(resolveTimer =>
      timer.initWithCallback(resolveTimer, fps60, ONE_SHOT)
    );

    let durationRatio = Math.floor(Date.now() - startTime) / duration;
    const epsilon = fps60 / duration / 10;
    while (1 - durationRatio > epsilon) {
      const intermediateTargets = startCoords.map((startCoord, i) => {
        let distance = distances[i];
        return [
          Math.floor(durationRatio * distance[0] + startCoord[0]),
          Math.floor(durationRatio * distance[1] + startCoord[1]),
        ];
      });
      callback(intermediateTargets);
      // wait |fps60| ms before performing next pointer move
      await new Promise(resolveTimer =>
        timer.initWithCallback(resolveTimer, fps60, ONE_SHOT)
      );

      durationRatio = Math.floor(Date.now() - startTime) / duration;
    }
  })();

  await intermediatePointerEvents;

  // perform last pointer move after all incremental moves are resolved and
  // durationRatio is close enough to 1
  callback(targetCoords);
}

const actionTypes = new Map();
for (let cls of [
  KeyDownAction,
  KeyUpAction,
  PauseAction,
  PointerDownAction,
  PointerUpAction,
  PointerMoveAction,
]) {
  if (!actionTypes.has(cls.type)) {
    actionTypes.set(cls.type, new Map());
  }
  actionTypes.get(cls.type).set(cls.subtype, cls);
}

/**
 * Implementation of the behaviour of a specific type of pointer
 */
class Pointer {
  /** Type of pointer */
  static type = null;

  constructor(id) {
    this.id = id;
  }

  /**
   * Implementation of depressing the pointer.
   *
   * @param {State} state - Actions state.
   * @param {InputSource} inputSource - State of the current input device.
   * @param {Action} action - The Action object invoking the pointer
   * @param {WindowProxy} win - Current window global.
   */
  pointerDown(state, inputSource, action, win) {
    throw new Error(
      `Unimplemented pointerDown for pointerType ${this.constructor.type}`
    );
  }

  /**
   * Implementation of releasing the pointer.
   *
   * @param {State} state - Actions state.
   * @param {InputSource} inputSource - State of the current input device.
   * @param {Action} action - The Action object invoking the pointer
   * @param {WindowProxy} win - Current window global.
   */
  pointerUp(state, inputSource, action, win) {
    throw new Error(
      `Unimplemented pointerUp for pointerType ${this.constructor.type}`
    );
  }

  /**
   * Implementation of moving the pointer.
   *
   * @param {State} state - Actions state.
   * @param {InputSource} inputSource - State of the current input device.
   * @param {number} targetX - Target X coordinate of the pointer move
   * @param {number} targetY - Target Y coordinate of the pointer move
   * @param {WindowProxy} win - Current window global.
   */
  pointerMove(state, inputSource, targetX, targetY, win) {
    throw new Error(
      `Unimplemented pointerMove for pointerType ${this.constructor.type}`
    );
  }

  /**
   * @param {number} pointerId - Numeric pointer id.
   * @param {string} pointerType - Pointer type.
   * @return {Pointer} - The pointer class for {@link pointerType}
   *
   * @throws {InvalidArgumentError} - If {@link pointerType} is not a valid pointer type.
   */
  static fromJSON(pointerId, pointerType) {
    const cls = pointerTypes.get(pointerType);
    if (cls === undefined) {
      throw new lazy.error.InvalidArgumentError(
        `Unknown pointerType: ${pointerType}`
      );
    }
    return new cls(pointerId);
  }
}

/**
 * Implementation of mouse pointer behaviour
 */
class MousePointer extends Pointer {
  static type = "mouse";

  pointerDown(state, inputSource, action, win) {
    const mouseEvent = new MouseEventData("mousedown", {
      button: action.button,
    });
    mouseEvent.update(state, inputSource);
    if (mouseEvent.ctrlKey) {
      if (lazy.AppInfo.isMac) {
        mouseEvent.button = 2;
        lazy.event.DoubleClickTracker.resetClick();
      }
    } else if (lazy.event.DoubleClickTracker.isClicked()) {
      mouseEvent.clickCount = 2;
    }
    lazy.event.synthesizeMouseAtPoint(
      inputSource.x,
      inputSource.y,
      mouseEvent,
      win
    );
    if (
      lazy.event.MouseButton.isSecondary(mouseEvent.button) ||
      (mouseEvent.ctrlKey && lazy.AppInfo.isMac)
    ) {
      const contextMenuEvent = { ...mouseEvent, type: "contextmenu" };
      lazy.event.synthesizeMouseAtPoint(
        inputSource.x,
        inputSource.y,
        contextMenuEvent,
        win
      );
    }
  }

  pointerUp(state, inputSource, action, win) {
    const mouseEvent = new MouseEventData("mouseup", {
      button: action.button,
    });
    mouseEvent.update(state, inputSource);
    if (lazy.event.DoubleClickTracker.isClicked()) {
      mouseEvent.clickCount = 2;
    }
    lazy.event.synthesizeMouseAtPoint(
      inputSource.x,
      inputSource.y,
      mouseEvent,
      win
    );
  }

  pointerMove(state, inputSource, action, targetX, targetY, win) {
    const mouseEvent = new MouseEventData("mousemove");
    mouseEvent.update(state, inputSource);
    lazy.event.synthesizeMouseAtPoint(targetX, targetY, mouseEvent, win);
  }
}

const pointerTypes = new Map();
for (let cls of [MousePointer]) {
  pointerTypes.set(cls.type, cls);
}

/**
 * Represents a series of ticks, specifying which actions to perform at
 * each tick.
 */
action.Chain = class extends Array {
  toString() {
    return `[chain ${super.toString()}]`;
  }

  /**
   * Dispatch the action chain to the relevant window.
   *
   * @param {State} state - Actions state.
   * @param {WindowProxy} win - Current window global.
   * @return {Promise} - Promise that is resolved once the action
   * chain is complete.
   */
  dispatch(state, win) {
    let i = 1;
    const chainEvents = (async () => {
      for (let tickActions of this) {
        lazy.logger.trace(`Dispatching tick ${i++}/${this.length}`);
        await tickActions.dispatch(state, win);
      }
    })();
    return chainEvents;
  }

  /**
   * @param {Array.<Object>} actions - Array of objects that each
   * represent an action sequence.
   * @return {action.Chain} - Object that allows dispatching a chain
   * of actions.
   * @throws {InvalidArgumentError} - If actions doesn't correspond to
   * a valid action chain.
   */
  static fromJSON(state, actions) {
    lazy.assert.array(
      actions,
      lazy.pprint`Expected 'actions' to be an array, got ${actions}`
    );

    const actionsByTick = new this();
    for (let actionSequence of actions) {
      const inputSourceActions = Sequence.fromJSON(state, actionSequence);
      for (let i = 0; i < inputSourceActions.length; i++) {
        // new tick
        if (actionsByTick.length < i + 1) {
          actionsByTick.push(new TickActions());
        }
        actionsByTick[i].push(inputSourceActions[i]);
      }
    }
    return actionsByTick;
  }
};

/**
 * Represents the action for each input device to perform in a single tick.
 */
class TickActions extends Array {
  /**
   * Tick duration in milliseconds.
   *
   * @return {number} - Longest action duration in |tickActions| if any, or 0.
   */
  getDuration() {
    let max = 0;
    for (let action of this) {
      if (action.affectsWallClockTime && action.duration) {
        max = Math.max(action.duration, max);
      }
    }
    return max;
  }

  /**
   * Dispatch sequence of actions for this tick.
   *
   * This creates a Promise for one tick that resolves once the Promise
   * for each tick-action is resolved, which takes at least |tickDuration|
   * milliseconds.  The resolved set of events for each tick is followed by
   * firing of pending DOM events.
   *
   * Note that the tick-actions are dispatched in order, but they may have
   * different durations and therefore may not end in the same order.
   *
   * @param {State} state - Actions state.
   * @param {WindowProxy} win - Current window global.
   * @return {Promise} - Promise that resolves when tick is complete.
   */
  dispatch(state, win) {
    const tickDuration = this.getDuration();
    const pendingEvents = this.map(action => {
      const inputSource = state.getInputSource(action.id);
      return action.dispatch(state, inputSource, tickDuration, win);
    });
    return Promise.all(pendingEvents);
  }
}

/**
 * Represents one input source action sequence; this is essentially an
 * |Array.<Action>|.
 *
 * This is a temporary object only used when constructing an {@link
 * action.Chain}.
 */
class Sequence extends Array {
  toString() {
    return `[sequence ${super.toString()}]`;
  }

  /**
   * @param {State} state - Actions state.
   * @param {Object} actionSequence
   *     Protocol representation of the actions for a specific input source.
   * @return {Array.<Array>} - Array of [InputSource?,Action|TouchActionGroup]
   */
  static fromJSON(state, actionSequence) {
    // used here to validate 'type' in addition to InputSource type below
    const { id, type, actions } = actionSequence;

    // type and id get validated in InputSource.fromJSON
    lazy.assert.array(
      actions,
      "Expected 'actionSequence.actions' to be an array, " +
        lazy.pprint`got ${actionSequence.actions}`
    );

    // This sets the input state in the global state map, if it's new
    InputSource.fromJSON(state, actionSequence);

    const sequence = new this();
    for (let actionItem of actions) {
      sequence.push(Action.fromJSON(type, id, actionItem));
    }

    return sequence;
  }
}

/**
 * Representation of an input event
 */
class InputEventData {
  constructor() {
    this.altKey = false;
    this.shiftKey = false;
    this.ctrlKey = false;
    this.metaKey = false;
  }

  /**
   * Update the input data based on global and input state
   *
   * @param {State} state - Actions state.
   * @param {InputSource} inputSource - State of the current input device.
   */
  update(state, inputSource) {}

  toString() {
    return `${this.constructor.name} ${JSON.stringify(this)}`;
  }
}

/**
 * Representation of a key input event
 *
 * @param {string} rawKey - Key value.
 */
class KeyEventData extends InputEventData {
  constructor(rawKey) {
    super();
    const { key, code, location, printable } = lazy.keyData.getData(rawKey);
    this.key = key;
    this.code = code;
    this.location = location;
    this.printable = printable;
    this.repeat = false;
    // keyCode will be computed by event.sendKeyDown
  }

  update(state, inputSource) {
    this.altKey = inputSource.alt;
    this.shiftKey = inputSource.shift;
    this.ctrlKey = inputSource.ctrl;
    this.metaKey = inputSource.meta;
  }
}

/**
 * Representation of a pointer input event
 *
 * @param {string} type - Event type.
 */
class PointerEventData extends InputEventData {
  constructor(type) {
    super();
    this.type = type;
    this.buttons = 0;
  }

  update(state, inputSource) {
    // set modifier properties based on whether any corresponding keys are
    // pressed on any key input source
    for (let [, otherInputSource] of state.inputSourcesByType("key")) {
      this.altKey = otherInputSource.alt || this.altKey;
      this.ctrlKey = otherInputSource.ctrl || this.ctrlKey;
      this.metaKey = otherInputSource.meta || this.metaKey;
      this.shiftKey = otherInputSource.shift || this.shiftKey;
    }
    let allButtons = Array.from(inputSource.pressed);
    this.buttons = allButtons.reduce((a, i) => a + Math.pow(2, i), 0);
  }
}

/** Collect properties associated with mouse pointer events */
class MouseEventData extends PointerEventData {
  constructor(type, options = {}) {
    super(type);
    const { button = 0 } = options;
    lazy.assert.positiveInteger(button);
    this.button = button;
    this.buttons = 0;
  }

  update(state, inputSource) {
    super.update(state, inputSource);
    this.id = inputSource.pointer.id;
  }
}

// helpers

/**
 * Assert that target is in the viewport of win.
 *
 * @param {Array.<number>} target - [x, y] coordinates of target
 * relative to viewport.
 * @param {WindowProxy} win - target window.
 * @throws {MoveTargetOutOfBoundsError} - If target is outside the
 * viewport.
 */
function assertInViewPort(target, win) {
  const [x, y] = target;
  lazy.assert.number(x, `Expected x to be finite number`);
  lazy.assert.number(y, `Expected y to be finite number`);
  // Viewport includes scrollbars if rendered.
  if (x < 0 || y < 0 || x > win.innerWidth || y > win.innerHeight) {
    throw new lazy.error.MoveTargetOutOfBoundsError(
      `(${x}, ${y}) is out of bounds of viewport ` +
        `width (${win.innerWidth}) ` +
        `and height (${win.innerHeight})`
    );
  }
}
