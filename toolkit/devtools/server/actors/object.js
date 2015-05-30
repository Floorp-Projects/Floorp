/* -*- indent-tabs-mode: nil; js-indent-level: 2; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cu, Ci } = require("chrome");
const { GeneratedLocation } = require("devtools/server/actors/common");
const { DebuggerServer } = require("devtools/server/main")
const DevToolsUtils = require("devtools/toolkit/DevToolsUtils");
const { dbg_assert } = DevToolsUtils;
const PromiseDebugging = require("PromiseDebugging");

const TYPED_ARRAY_CLASSES = ["Uint8Array", "Uint8ClampedArray", "Uint16Array",
      "Uint32Array", "Int8Array", "Int16Array", "Int32Array", "Float32Array",
      "Float64Array"];

// Number of items to preview in objects, arrays, maps, sets, lists,
// collections, etc.
const OBJECT_PREVIEW_MAX_ITEMS = 10;

/**
 * Creates an actor for the specified object.
 *
 * @param obj Debugger.Object
 *        The debuggee object.
 * @param hooks Object
 *        A collection of abstract methods that are implemented by the caller.
 *        ObjectActor requires the following functions to be implemented by
 *        the caller:
 *          - createValueGrip
 *              Creates a value grip for the given object
 *          - sources
 *              TabSources getter that manages the sources of a thread
 *          - createEnvironmentActor
 *              Creates and return an environment actor
 *          - getGripDepth
 *              An actor's grip depth getter
 *          - incrementGripDepth
 *              Increment the actor's grip depth
 *          - decrementGripDepth
 *              Decrement the actor's grip depth
 */
function ObjectActor(obj, {
  createValueGrip,
  sources,
  createEnvironmentActor,
  getGripDepth,
  incrementGripDepth,
  decrementGripDepth
}) {
  dbg_assert(!obj.optimizedOut,
    "Should not create object actors for optimized out values!");
  this.obj = obj;
  this.hooks = {
    createValueGrip,
    sources,
    createEnvironmentActor,
    getGripDepth,
    incrementGripDepth,
    decrementGripDepth
  };
  this.iterators = new Set();
}

ObjectActor.prototype = {
  actorPrefix: "obj",

  /**
   * Returns a grip for this actor for returning in a protocol message.
   */
  grip: function () {
    this.hooks.incrementGripDepth();

    let g = {
      "type": "object",
      "class": this.obj.class,
      "actor": this.actorID,
      "extensible": this.obj.isExtensible(),
      "frozen": this.obj.isFrozen(),
      "sealed": this.obj.isSealed()
    };

    if (this.obj.class != "DeadObject") {
      // Expose internal Promise state.
      if (this.obj.class == "Promise") {
        const { state, value, reason } = getPromiseState(this.obj);
        g.promiseState = { state };
        if (state == "fulfilled") {
          g.promiseState.value = this.hooks.createValueGrip(value);
        } else if (state == "rejected") {
          g.promiseState.reason = this.hooks.createValueGrip(reason);
        }
      }

      // FF40+: Allow to know how many properties an object has
      // to lazily display them when there is a bunch.
      // Throws on some MouseEvent object in tests.
      try {
        // Bug 1163520: Assert on internal functions
        if (this.obj.class != "Function") {
          g.ownPropertyLength = this.obj.getOwnPropertyNames().length;
        }
      } catch(e) {}

      let raw = this.obj.unsafeDereference();

      // If Cu is not defined, we are running on a worker thread, where xrays
      // don't exist.
      if (Cu) {
        raw = Cu.unwaiveXrays(raw);
      }

      if (!DevToolsUtils.isSafeJSObject(raw)) {
        raw = null;
      }

      let previewers = DebuggerServer.ObjectActorPreviewers[this.obj.class] ||
                       DebuggerServer.ObjectActorPreviewers.Object;
      for (let fn of previewers) {
        try {
          if (fn(this, g, raw)) {
            break;
          }
        } catch (e) {
          DevToolsUtils.reportException("ObjectActor.prototype.grip previewer function", e);
        }
      }
    }

    this.hooks.decrementGripDepth();
    return g;
  },

  /**
   * Releases this actor from the pool.
   */
  release: function () {
    if (this.registeredPool.objectActors) {
      this.registeredPool.objectActors.delete(this.obj);
    }
    this.iterators.forEach(actor => this.registeredPool.removeActor(actor));
    this.iterators.clear();
    this.registeredPool.removeActor(this);
  },

  /**
   * Handle a protocol request to provide the definition site of this function
   * object.
   *
   * @param aRequest object
   *        The protocol request object.
   */
  onDefinitionSite: function OA_onDefinitionSite(aRequest) {
    if (this.obj.class != "Function") {
      return {
        from: this.actorID,
        error: "objectNotFunction",
        message: this.actorID + " is not a function."
      };
    }

    if (!this.obj.script) {
      return {
        from: this.actorID,
        error: "noScript",
        message: this.actorID + " has no Debugger.Script"
      };
    }

    return this.hooks.sources().getOriginalLocation(new GeneratedLocation(
      this.hooks.sources().createNonSourceMappedActor(this.obj.script.source),
      this.obj.script.startLine,
      0 // TODO bug 901138: use Debugger.Script.prototype.startColumn
    )).then((originalLocation) => {
      return {
        source: originalLocation.originalSourceActor.form(),
        line: originalLocation.originalLine,
        column: originalLocation.originalColumn
      };
    });
  },

  /**
   * Handle a protocol request to provide the names of the properties defined on
   * the object and not its prototype.
   *
   * @param aRequest object
   *        The protocol request object.
   */
  onOwnPropertyNames: function (aRequest) {
    return { from: this.actorID,
             ownPropertyNames: this.obj.getOwnPropertyNames() };
  },

  /**
   * Creates an actor to iterate over an object property names and values.
   * See PropertyIteratorActor constructor for more info about options param.
   *
   * @param aRequest object
   *        The protocol request object.
   */
  onEnumProperties: function (aRequest) {
    let actor = new PropertyIteratorActor(this, aRequest.options);
    this.registeredPool.addActor(actor);
    this.iterators.add(actor);
    return { iterator: actor.grip() };
  },

  /**
   * Handle a protocol request to provide the prototype and own properties of
   * the object.
   *
   * @param aRequest object
   *        The protocol request object.
   */
  onPrototypeAndProperties: function (aRequest) {
    let ownProperties = Object.create(null);
    let names;
    try {
      names = this.obj.getOwnPropertyNames();
    } catch (ex) {
      // The above can throw if this.obj points to a dead object.
      // TODO: we should use Cu.isDeadWrapper() - see bug 885800.
      return { from: this.actorID,
               prototype: this.hooks.createValueGrip(null),
               ownProperties: ownProperties,
               safeGetterValues: Object.create(null) };
    }
    for (let name of names) {
      ownProperties[name] = this._propertyDescriptor(name);
    }
    return { from: this.actorID,
             prototype: this.hooks.createValueGrip(this.obj.proto),
             ownProperties: ownProperties,
             safeGetterValues: this._findSafeGetterValues(names) };
  },

  /**
   * Find the safe getter values for the current Debugger.Object, |this.obj|.
   *
   * @private
   * @param array aOwnProperties
   *        The array that holds the list of known ownProperties names for
   *        |this.obj|.
   * @param number [aLimit=0]
   *        Optional limit of getter values to find.
   * @return object
   *         An object that maps property names to safe getter descriptors as
   *         defined by the remote debugging protocol.
   */
  _findSafeGetterValues: function (aOwnProperties, aLimit = 0)
  {
    let safeGetterValues = Object.create(null);
    let obj = this.obj;
    let level = 0, i = 0;

    while (obj) {
      let getters = this._findSafeGetters(obj);
      for (let name of getters) {
        // Avoid overwriting properties from prototypes closer to this.obj. Also
        // avoid providing safeGetterValues from prototypes if property |name|
        // is already defined as an own property.
        if (name in safeGetterValues ||
            (obj != this.obj && aOwnProperties.indexOf(name) !== -1)) {
          continue;
        }

        // Ignore __proto__ on Object.prototye.
        if (!obj.proto && name == "__proto__") {
          continue;
        }

        let desc = null, getter = null;
        try {
          desc = obj.getOwnPropertyDescriptor(name);
          getter = desc.get;
        } catch (ex) {
          // The above can throw if the cache becomes stale.
        }
        if (!getter) {
          obj._safeGetters = null;
          continue;
        }

        let result = getter.call(this.obj);
        if (result && !("throw" in result)) {
          let getterValue = undefined;
          if ("return" in result) {
            getterValue = result.return;
          } else if ("yield" in result) {
            getterValue = result.yield;
          }
          // WebIDL attributes specified with the LenientThis extended attribute
          // return undefined and should be ignored.
          if (getterValue !== undefined) {
            safeGetterValues[name] = {
              getterValue: this.hooks.createValueGrip(getterValue),
              getterPrototypeLevel: level,
              enumerable: desc.enumerable,
              writable: level == 0 ? desc.writable : true,
            };
            if (aLimit && ++i == aLimit) {
              break;
            }
          }
        }
      }
      if (aLimit && i == aLimit) {
        break;
      }

      obj = obj.proto;
      level++;
    }

    return safeGetterValues;
  },

  /**
   * Find the safe getters for a given Debugger.Object. Safe getters are native
   * getters which are safe to execute.
   *
   * @private
   * @param Debugger.Object aObject
   *        The Debugger.Object where you want to find safe getters.
   * @return Set
   *         A Set of names of safe getters. This result is cached for each
   *         Debugger.Object.
   */
  _findSafeGetters: function (aObject)
  {
    if (aObject._safeGetters) {
      return aObject._safeGetters;
    }

    let getters = new Set();
    let names = [];
    try {
      names = aObject.getOwnPropertyNames()
    } catch (ex) {
      // Calling getOwnPropertyNames() on some wrapped native prototypes is not
      // allowed: "cannot modify properties of a WrappedNative". See bug 952093.
    }

    for (let name of names) {
      let desc = null;
      try {
        desc = aObject.getOwnPropertyDescriptor(name);
      } catch (e) {
        // Calling getOwnPropertyDescriptor on wrapped native prototypes is not
        // allowed (bug 560072).
      }
      if (!desc || desc.value !== undefined || !("get" in desc)) {
        continue;
      }

      if (DevToolsUtils.hasSafeGetter(desc)) {
        getters.add(name);
      }
    }

    aObject._safeGetters = getters;
    return getters;
  },

  /**
   * Handle a protocol request to provide the prototype of the object.
   *
   * @param aRequest object
   *        The protocol request object.
   */
  onPrototype: function (aRequest) {
    return { from: this.actorID,
             prototype: this.hooks.createValueGrip(this.obj.proto) };
  },

  /**
   * Handle a protocol request to provide the property descriptor of the
   * object's specified property.
   *
   * @param aRequest object
   *        The protocol request object.
   */
  onProperty: function (aRequest) {
    if (!aRequest.name) {
      return { error: "missingParameter",
               message: "no property name was specified" };
    }

    return { from: this.actorID,
             descriptor: this._propertyDescriptor(aRequest.name) };
  },

  /**
   * Handle a protocol request to provide the display string for the object.
   *
   * @param aRequest object
   *        The protocol request object.
   */
  onDisplayString: function (aRequest) {
    const string = stringify(this.obj);
    return { from: this.actorID,
             displayString: this.hooks.createValueGrip(string) };
  },

  /**
   * A helper method that creates a property descriptor for the provided object,
   * properly formatted for sending in a protocol response.
   *
   * @private
   * @param string aName
   *        The property that the descriptor is generated for.
   * @param boolean [aOnlyEnumerable]
   *        Optional: true if you want a descriptor only for an enumerable
   *        property, false otherwise.
   * @return object|undefined
   *         The property descriptor, or undefined if this is not an enumerable
   *         property and aOnlyEnumerable=true.
   */
  _propertyDescriptor: function (aName, aOnlyEnumerable) {
    let desc;
    try {
      desc = this.obj.getOwnPropertyDescriptor(aName);
    } catch (e) {
      // Calling getOwnPropertyDescriptor on wrapped native prototypes is not
      // allowed (bug 560072). Inform the user with a bogus, but hopefully
      // explanatory, descriptor.
      return {
        configurable: false,
        writable: false,
        enumerable: false,
        value: e.name
      };
    }

    if (!desc || aOnlyEnumerable && !desc.enumerable) {
      return undefined;
    }

    let retval = {
      configurable: desc.configurable,
      enumerable: desc.enumerable
    };

    if ("value" in desc) {
      retval.writable = desc.writable;
      retval.value = this.hooks.createValueGrip(desc.value);
    } else {
      if ("get" in desc) {
        retval.get = this.hooks.createValueGrip(desc.get);
      }
      if ("set" in desc) {
        retval.set = this.hooks.createValueGrip(desc.set);
      }
    }
    return retval;
  },

  /**
   * Handle a protocol request to provide the source code of a function.
   *
   * @param aRequest object
   *        The protocol request object.
   */
  onDecompile: function (aRequest) {
    if (this.obj.class !== "Function") {
      return { error: "objectNotFunction",
               message: "decompile request is only valid for object grips " +
                        "with a 'Function' class." };
    }

    return { from: this.actorID,
             decompiledCode: this.obj.decompile(!!aRequest.pretty) };
  },

  /**
   * Handle a protocol request to provide the parameters of a function.
   *
   * @param aRequest object
   *        The protocol request object.
   */
  onParameterNames: function (aRequest) {
    if (this.obj.class !== "Function") {
      return { error: "objectNotFunction",
               message: "'parameterNames' request is only valid for object " +
                        "grips with a 'Function' class." };
    }

    return { parameterNames: this.obj.parameterNames };
  },

  /**
   * Handle a protocol request to release a thread-lifetime grip.
   *
   * @param aRequest object
   *        The protocol request object.
   */
  onRelease: function (aRequest) {
    this.release();
    return {};
  },

  /**
   * Handle a protocol request to provide the lexical scope of a function.
   *
   * @param aRequest object
   *        The protocol request object.
   */
  onScope: function (aRequest) {
    if (this.obj.class !== "Function") {
      return { error: "objectNotFunction",
               message: "scope request is only valid for object grips with a" +
                        " 'Function' class." };
    }

    let envActor = this.hooks.createEnvironmentActor(this.obj.environment,
                                                     this.registeredPool);
    if (!envActor) {
      return { error: "notDebuggee",
               message: "cannot access the environment of this function." };
    }

    return { from: this.actorID, scope: envActor.form() };
  }
};

ObjectActor.prototype.requestTypes = {
  "definitionSite": ObjectActor.prototype.onDefinitionSite,
  "parameterNames": ObjectActor.prototype.onParameterNames,
  "prototypeAndProperties": ObjectActor.prototype.onPrototypeAndProperties,
  "enumProperties": ObjectActor.prototype.onEnumProperties,
  "prototype": ObjectActor.prototype.onPrototype,
  "property": ObjectActor.prototype.onProperty,
  "displayString": ObjectActor.prototype.onDisplayString,
  "ownPropertyNames": ObjectActor.prototype.onOwnPropertyNames,
  "decompile": ObjectActor.prototype.onDecompile,
  "release": ObjectActor.prototype.onRelease,
  "scope": ObjectActor.prototype.onScope,
};

/**
 * Creates an actor to iterate over an object's property names and values.
 *
 * @param objectActor ObjectActor
 *        The object actor.
 * @param options Object
 *        A dictionary object with various boolean attributes:
 *        - ignoreSafeGetters Boolean
 *          If true, do not iterate over safe getters.
 *        - ignoreIndexedProperties Boolean
 *          If true, filters out Array items.
 *          e.g. properties names between `0` and `object.length`.
 *        - ignoreNonIndexedProperties Boolean
 *          If true, filters out items that aren't array items
 *          e.g. properties names that are not a number between `0`
 *          and `object.length`.
 *        - sort Boolean
 *          If true, the iterator will sort the properties by name
 *          before dispatching them.
 *        - query String
 *          If non-empty, will filter the properties by names containing
 *          this query string. The match is not case-sensitive.
 */
function PropertyIteratorActor(objectActor, options){
  this.objectActor = objectActor;

  let ownProperties = Object.create(null);
  let names = [];
  try {
    names = this.objectActor.obj.getOwnPropertyNames();
  } catch (ex) {}

  let safeGetterValues = {};
  let safeGetterNames = [];
  if (!options.ignoreSafeGetters) {
    // Merge the safe getter values into the existing properties list.
    safeGetterValues = this.objectActor._findSafeGetterValues(names);
    safeGetterNames = Object.keys(safeGetterValues);
    for (let name of safeGetterNames) {
      if (names.indexOf(name) === -1) {
        names.push(name);
      }
    }
  }

  if (options.ignoreIndexedProperties || options.ignoreNonIndexedProperties) {
    let length = DevToolsUtils.getProperty(this.objectActor.obj, "length");
    if (typeof(length) !== "number") {
      // Pseudo arrays are flagged as ArrayLike if they have
      // subsequent indexed properties without having any length attribute.
      length = 0;
      for (let key of names) {
        if (isNaN(key) || key != length++) {
          break;
        }
      }
    }

    if (options.ignoreIndexedProperties) {
      names = names.filter(i => {
        // Use parseFloat in order to reject floats...
        // (parseInt converts floats to integer)
        // (Number(str) converts spaces to 0)
        i = parseFloat(i);
        return !Number.isInteger(i) || i < 0 || i >= length;
      });
    }

    if (options.ignoreNonIndexedProperties) {
      names = names.filter(i => {
        i = parseFloat(i);
        return Number.isInteger(i) && i >= 0 && i < length;
      });
    }
  }

  if (options.query) {
    let { query } = options;
    query = query.toLowerCase();
    names = names.filter(name => {
      return name.toLowerCase().includes(query);
    });
  }

  if (options.sort) {
    names.sort();
  }

  // Now build the descriptor list
  for (let name of names) {
    let desc = this.objectActor._propertyDescriptor(name);
    if (!desc) {
      desc = safeGetterValues[name];
    }
    else if (name in safeGetterValues) {
      // Merge the safe getter values into the existing properties list.
      let { getterValue, getterPrototypeLevel } = safeGetterValues[name];
      desc.getterValue = getterValue;
      desc.getterPrototypeLevel = getterPrototypeLevel;
    }
    ownProperties[name] = desc;
  }

  this.names = names;
  this.ownProperties = ownProperties;
}

PropertyIteratorActor.prototype = {
  actorPrefix: "propertyIterator",

  grip: function() {
    return {
      type: "propertyIterator",
      actor: this.actorID,
      count: this.names.length
    };
  },

  names: function({ indexes }) {
    let list = [];
    for (let idx of indexes) {
      list.push(this.names[idx]);
    }
    return {
      names: list
    };
  },

  slice: function({ start, count }) {
    let names = this.names.slice(start, start + count);
    let props = Object.create(null);
    for (let name of names) {
      props[name] = this.ownProperties[name];
    }
    return {
      ownProperties: props
    };
  },

  all: function() {
    return {
      ownProperties: this.ownProperties
    };
  }
};

PropertyIteratorActor.prototype.requestTypes = {
  "names": PropertyIteratorActor.prototype.names,
  "slice": PropertyIteratorActor.prototype.slice,
  "all": PropertyIteratorActor.prototype.all,
};

/**
 * Functions for adding information to ObjectActor grips for the purpose of
 * having customized output. This object holds arrays mapped by
 * Debugger.Object.prototype.class.
 *
 * In each array you can add functions that take two
 * arguments:
 *   - the ObjectActor instance to make a preview for,
 *   - the grip object being prepared for the client,
 *   - the raw JS object after calling Debugger.Object.unsafeDereference(). This
 *   argument is only provided if the object is safe for reading properties and
 *   executing methods. See DevToolsUtils.isSafeJSObject().
 *
 * Functions must return false if they cannot provide preview
 * information for the debugger object, or true otherwise.
 */
DebuggerServer.ObjectActorPreviewers = {
  String: [function({obj, hooks}, aGrip) {
    let result = genericObjectPreviewer("String", String, obj, hooks);
    let length = DevToolsUtils.getProperty(obj, "length");

    if (!result || typeof length != "number") {
      return false;
    }

    aGrip.preview = {
      kind: "ArrayLike",
      length: length
    };

    if (hooks.getGripDepth() > 1) {
      return true;
    }

    let items = aGrip.preview.items = [];

    const max = Math.min(result.value.length, OBJECT_PREVIEW_MAX_ITEMS);
    for (let i = 0; i < max; i++) {
      let value = hooks.createValueGrip(result.value[i]);
      items.push(value);
    }

    return true;
  }],

  Boolean: [function({obj, hooks}, aGrip) {
    let result = genericObjectPreviewer("Boolean", Boolean, obj, hooks);
    if (result) {
      aGrip.preview = result;
      return true;
    }

    return false;
  }],

  Number: [function({obj, hooks}, aGrip) {
    let result = genericObjectPreviewer("Number", Number, obj, hooks);
    if (result) {
      aGrip.preview = result;
      return true;
    }

    return false;
  }],

  Function: [function({obj, hooks}, aGrip) {
    if (obj.name) {
      aGrip.name = obj.name;
    }

    if (obj.displayName) {
      aGrip.displayName = obj.displayName.substr(0, 500);
    }

    if (obj.parameterNames) {
      aGrip.parameterNames = obj.parameterNames;
    }

    // Check if the developer has added a de-facto standard displayName
    // property for us to use.
    let userDisplayName;
    try {
      userDisplayName = obj.getOwnPropertyDescriptor("displayName");
    } catch (e) {
      // Calling getOwnPropertyDescriptor with displayName might throw
      // with "permission denied" errors for some functions.
      dumpn(e);
    }

    if (userDisplayName && typeof userDisplayName.value == "string" &&
        userDisplayName.value) {
      aGrip.userDisplayName = hooks.createValueGrip(userDisplayName.value);
    }

    return true;
  }],

  RegExp: [function({obj, hooks}, aGrip) {
    // Avoid having any special preview for the RegExp.prototype itself.
    if (!obj.proto || obj.proto.class != "RegExp") {
      return false;
    }

    let str = RegExp.prototype.toString.call(obj.unsafeDereference());
    aGrip.displayString = hooks.createValueGrip(str);
    return true;
  }],

  Date: [function({obj, hooks}, aGrip) {
    let time = Date.prototype.getTime.call(obj.unsafeDereference());

    aGrip.preview = {
      timestamp: hooks.createValueGrip(time),
    };
    return true;
  }],

  Array: [function({obj, hooks}, aGrip) {
    let length = DevToolsUtils.getProperty(obj, "length");
    if (typeof length != "number") {
      return false;
    }

    aGrip.preview = {
      kind: "ArrayLike",
      length: length,
    };

    if (hooks.getGripDepth() > 1) {
      return true;
    }

    let raw = obj.unsafeDereference();
    let items = aGrip.preview.items = [];

    for (let i = 0; i < length; ++i) {
      // Array Xrays filter out various possibly-unsafe properties (like
      // functions, and claim that the value is undefined instead. This
      // is generally the right thing for privileged code accessing untrusted
      // objects, but quite confusing for Object previews. So we manually
      // override this protection by waiving Xrays on the array, and re-applying
      // Xrays on any indexed value props that we pull off of it.
      let desc = Object.getOwnPropertyDescriptor(Cu.waiveXrays(raw), i);
      if (desc && !desc.get && !desc.set) {
        let value = Cu.unwaiveXrays(desc.value);
        value = makeDebuggeeValueIfNeeded(obj, value);
        items.push(hooks.createValueGrip(value));
      } else {
        items.push(null);
      }

      if (items.length == OBJECT_PREVIEW_MAX_ITEMS) {
        break;
      }
    }

    return true;
  }], // Array

  Set: [function({obj, hooks}, aGrip) {
    let size = DevToolsUtils.getProperty(obj, "size");
    if (typeof size != "number") {
      return false;
    }

    aGrip.preview = {
      kind: "ArrayLike",
      length: size,
    };

    // Avoid recursive object grips.
    if (hooks.getGripDepth() > 1) {
      return true;
    }

    let raw = obj.unsafeDereference();
    let items = aGrip.preview.items = [];
    // We currently lack XrayWrappers for Set, so when we iterate over
    // the values, the temporary iterator objects get created in the target
    // compartment. However, we _do_ have Xrays to Object now, so we end up
    // Xraying those temporary objects, and filtering access to |it.value|
    // based on whether or not it's Xrayable and/or callable, which breaks
    // the for/of iteration.
    //
    // This code is designed to handle untrusted objects, so we can safely
    // waive Xrays on the iterable, and relying on the Debugger machinery to
    // make sure we handle the resulting objects carefully.
    for (let item of Cu.waiveXrays(Set.prototype.values.call(raw))) {
      item = Cu.unwaiveXrays(item);
      item = makeDebuggeeValueIfNeeded(obj, item);
      items.push(hooks.createValueGrip(item));
      if (items.length == OBJECT_PREVIEW_MAX_ITEMS) {
        break;
      }
    }

    return true;
  }], // Set

  Map: [function({obj, hooks}, aGrip) {
    let size = DevToolsUtils.getProperty(obj, "size");
    if (typeof size != "number") {
      return false;
    }

    aGrip.preview = {
      kind: "MapLike",
      size: size,
    };

    if (hooks.getGripDepth() > 1) {
      return true;
    }

    let raw = obj.unsafeDereference();
    let entries = aGrip.preview.entries = [];
    // Iterating over a Map via .entries goes through various intermediate
    // objects - an Iterator object, then a 2-element Array object, then the
    // actual values we care about. We don't have Xrays to Iterator objects,
    // so we get Opaque wrappers for them. And even though we have Xrays to
    // Arrays, the semantics often deny access to the entires based on the
    // nature of the values. So we need waive Xrays for the iterator object
    // and the tupes, and then re-apply them on the underlying values until
    // we fix bug 1023984.
    //
    // Even then though, we might want to continue waiving Xrays here for the
    // same reason we do so for Arrays above - this filtering behavior is likely
    // to be more confusing than beneficial in the case of Object previews.
    for (let keyValuePair of Cu.waiveXrays(Map.prototype.entries.call(raw))) {
      let key = Cu.unwaiveXrays(keyValuePair[0]);
      let value = Cu.unwaiveXrays(keyValuePair[1]);
      key = makeDebuggeeValueIfNeeded(obj, key);
      value = makeDebuggeeValueIfNeeded(obj, value);
      entries.push([hooks.createValueGrip(key),
                    hooks.createValueGrip(value)]);
      if (entries.length == OBJECT_PREVIEW_MAX_ITEMS) {
        break;
      }
    }

    return true;
  }], // Map

  DOMStringMap: [function({obj, hooks}, aGrip, aRawObj) {
    if (!aRawObj) {
      return false;
    }

    let keys = obj.getOwnPropertyNames();
    aGrip.preview = {
      kind: "MapLike",
      size: keys.length,
    };

    if (hooks.getGripDepth() > 1) {
      return true;
    }

    let entries = aGrip.preview.entries = [];
    for (let key of keys) {
      let value = makeDebuggeeValueIfNeeded(obj, aRawObj[key]);
      entries.push([key, hooks.createValueGrip(value)]);
      if (entries.length == OBJECT_PREVIEW_MAX_ITEMS) {
        break;
      }
    }

    return true;
  }], // DOMStringMap

}; // DebuggerServer.ObjectActorPreviewers

/**
 * Generic previewer for "simple" classes like String, Number and Boolean.
 *
 * @param string aClassName
 *        Class name to expect.
 * @param object aClass
 *        The class to expect, eg. String. The valueOf() method of the class is
 *        invoked on the given object.
 * @param Debugger.Object aObj
 *        The debugger object we need to preview.
 * @param object aHooks
 *        The thread actor to use to create a value grip.
 * @return object|null
 *         An object with one property, "value", which holds the value grip that
 *         represents the given object. Null is returned if we cant preview the
 *         object.
 */
function genericObjectPreviewer(aClassName, aClass, aObj, aHooks) {
  if (!aObj.proto || aObj.proto.class != aClassName) {
    return null;
  }

  let raw = aObj.unsafeDereference();
  let v = null;
  try {
    v = aClass.prototype.valueOf.call(raw);
  } catch (ex) {
    // valueOf() can throw if the raw JS object is "misbehaved".
    return null;
  }

  if (v !== null) {
    v = aHooks.createValueGrip(makeDebuggeeValueIfNeeded(aObj, v));
    return { value: v };
  }

  return null;
}

// Preview functions that do not rely on the object class.
DebuggerServer.ObjectActorPreviewers.Object = [
  function TypedArray({obj, hooks}, aGrip) {
    if (TYPED_ARRAY_CLASSES.indexOf(obj.class) == -1) {
      return false;
    }

    let length = DevToolsUtils.getProperty(obj, "length");
    if (typeof length != "number") {
      return false;
    }

    aGrip.preview = {
      kind: "ArrayLike",
      length: length,
    };

    if (hooks.getGripDepth() > 1) {
      return true;
    }

    let raw = obj.unsafeDereference();
    let global = Cu.getGlobalForObject(DebuggerServer);
    let classProto = global[obj.class].prototype;
    // The Xray machinery for TypedArrays denies indexed access on the grounds
    // that it's slow, and advises callers to do a structured clone instead.
    let safeView = Cu.cloneInto(classProto.subarray.call(raw, 0, OBJECT_PREVIEW_MAX_ITEMS), global);
    let items = aGrip.preview.items = [];
    for (let i = 0; i < safeView.length; i++) {
      items.push(safeView[i]);
    }

    return true;
  },

  function Error({obj, hooks}, aGrip) {
    switch (obj.class) {
      case "Error":
      case "EvalError":
      case "RangeError":
      case "ReferenceError":
      case "SyntaxError":
      case "TypeError":
      case "URIError":
        let name = DevToolsUtils.getProperty(obj, "name");
        let msg = DevToolsUtils.getProperty(obj, "message");
        let stack = DevToolsUtils.getProperty(obj, "stack");
        let fileName = DevToolsUtils.getProperty(obj, "fileName");
        let lineNumber = DevToolsUtils.getProperty(obj, "lineNumber");
        let columnNumber = DevToolsUtils.getProperty(obj, "columnNumber");
        aGrip.preview = {
          kind: "Error",
          name: hooks.createValueGrip(name),
          message: hooks.createValueGrip(msg),
          stack: hooks.createValueGrip(stack),
          fileName: hooks.createValueGrip(fileName),
          lineNumber: hooks.createValueGrip(lineNumber),
          columnNumber: hooks.createValueGrip(columnNumber),
        };
        return true;
      default:
        return false;
    }
  },

  function CSSMediaRule({obj, hooks}, aGrip, aRawObj) {
    if (isWorker || !aRawObj || !(aRawObj instanceof Ci.nsIDOMCSSMediaRule)) {
      return false;
    }
    aGrip.preview = {
      kind: "ObjectWithText",
      text: hooks.createValueGrip(aRawObj.conditionText),
    };
    return true;
  },

  function CSSStyleRule({obj, hooks}, aGrip, aRawObj) {
    if (isWorker || !aRawObj || !(aRawObj instanceof Ci.nsIDOMCSSStyleRule)) {
      return false;
    }
    aGrip.preview = {
      kind: "ObjectWithText",
      text: hooks.createValueGrip(aRawObj.selectorText),
    };
    return true;
  },

  function ObjectWithURL({obj, hooks}, aGrip, aRawObj) {
    if (isWorker || !aRawObj || !(aRawObj instanceof Ci.nsIDOMCSSImportRule ||
                                  aRawObj instanceof Ci.nsIDOMCSSStyleSheet ||
                                  aRawObj instanceof Ci.nsIDOMLocation ||
                                  aRawObj instanceof Ci.nsIDOMWindow)) {
      return false;
    }

    let url;
    if (aRawObj instanceof Ci.nsIDOMWindow && aRawObj.location) {
      url = aRawObj.location.href;
    } else if (aRawObj.href) {
      url = aRawObj.href;
    } else {
      return false;
    }

    aGrip.preview = {
      kind: "ObjectWithURL",
      url: hooks.createValueGrip(url),
    };

    return true;
  },

  function ArrayLike({obj, hooks}, aGrip, aRawObj) {
    if (isWorker || !aRawObj ||
        obj.class != "DOMStringList" &&
        obj.class != "DOMTokenList" &&
        !(aRawObj instanceof Ci.nsIDOMMozNamedAttrMap ||
          aRawObj instanceof Ci.nsIDOMCSSRuleList ||
          aRawObj instanceof Ci.nsIDOMCSSValueList ||
          aRawObj instanceof Ci.nsIDOMFileList ||
          aRawObj instanceof Ci.nsIDOMFontFaceList ||
          aRawObj instanceof Ci.nsIDOMMediaList ||
          aRawObj instanceof Ci.nsIDOMNodeList ||
          aRawObj instanceof Ci.nsIDOMStyleSheetList)) {
      return false;
    }

    if (typeof aRawObj.length != "number") {
      return false;
    }

    aGrip.preview = {
      kind: "ArrayLike",
      length: aRawObj.length,
    };

    if (hooks.getGripDepth() > 1) {
      return true;
    }

    let items = aGrip.preview.items = [];

    for (let i = 0; i < aRawObj.length &&
                    items.length < OBJECT_PREVIEW_MAX_ITEMS; i++) {
      let value = makeDebuggeeValueIfNeeded(obj, aRawObj[i]);
      items.push(hooks.createValueGrip(value));
    }

    return true;
  }, // ArrayLike

  function CSSStyleDeclaration({obj, hooks}, aGrip, aRawObj) {
    if (isWorker || !aRawObj || !(aRawObj instanceof Ci.nsIDOMCSSStyleDeclaration)) {
      return false;
    }

    aGrip.preview = {
      kind: "MapLike",
      size: aRawObj.length,
    };

    let entries = aGrip.preview.entries = [];

    for (let i = 0; i < OBJECT_PREVIEW_MAX_ITEMS &&
                    i < aRawObj.length; i++) {
      let prop = aRawObj[i];
      let value = aRawObj.getPropertyValue(prop);
      entries.push([prop, hooks.createValueGrip(value)]);
    }

    return true;
  },

  function DOMNode({obj, hooks}, aGrip, aRawObj) {
    if (isWorker || obj.class == "Object" || !aRawObj ||
        !(aRawObj instanceof Ci.nsIDOMNode)) {
      return false;
    }

    let preview = aGrip.preview = {
      kind: "DOMNode",
      nodeType: aRawObj.nodeType,
      nodeName: aRawObj.nodeName,
    };

    if (aRawObj instanceof Ci.nsIDOMDocument && aRawObj.location) {
      preview.location = hooks.createValueGrip(aRawObj.location.href);
    } else if (aRawObj instanceof Ci.nsIDOMDocumentFragment) {
      preview.childNodesLength = aRawObj.childNodes.length;

      if (hooks.getGripDepth() < 2) {
        preview.childNodes = [];
        for (let node of aRawObj.childNodes) {
          let actor = hooks.createValueGrip(obj.makeDebuggeeValue(node));
          preview.childNodes.push(actor);
          if (preview.childNodes.length == OBJECT_PREVIEW_MAX_ITEMS) {
            break;
          }
        }
      }
    } else if (aRawObj instanceof Ci.nsIDOMElement) {
      // Add preview for DOM element attributes.
      if (aRawObj instanceof Ci.nsIDOMHTMLElement) {
        preview.nodeName = preview.nodeName.toLowerCase();
      }

      let i = 0;
      preview.attributes = {};
      preview.attributesLength = aRawObj.attributes.length;
      for (let attr of aRawObj.attributes) {
        preview.attributes[attr.nodeName] = hooks.createValueGrip(attr.value);
        if (++i == OBJECT_PREVIEW_MAX_ITEMS) {
          break;
        }
      }
    } else if (aRawObj instanceof Ci.nsIDOMAttr) {
      preview.value = hooks.createValueGrip(aRawObj.value);
    } else if (aRawObj instanceof Ci.nsIDOMText ||
               aRawObj instanceof Ci.nsIDOMComment) {
      preview.textContent = hooks.createValueGrip(aRawObj.textContent);
    }

    return true;
  }, // DOMNode

  function DOMEvent({obj, hooks}, aGrip, aRawObj) {
    if (isWorker || !aRawObj || !(aRawObj instanceof Ci.nsIDOMEvent)) {
      return false;
    }

    let preview = aGrip.preview = {
      kind: "DOMEvent",
      type: aRawObj.type,
      properties: Object.create(null),
    };

    if (hooks.getGripDepth() < 2) {
      let target = obj.makeDebuggeeValue(aRawObj.target);
      preview.target = hooks.createValueGrip(target);
    }

    let props = [];
    if (aRawObj instanceof Ci.nsIDOMMouseEvent) {
      props.push("buttons", "clientX", "clientY", "layerX", "layerY");
    } else if (aRawObj instanceof Ci.nsIDOMKeyEvent) {
      let modifiers = [];
      if (aRawObj.altKey) {
        modifiers.push("Alt");
      }
      if (aRawObj.ctrlKey) {
        modifiers.push("Control");
      }
      if (aRawObj.metaKey) {
        modifiers.push("Meta");
      }
      if (aRawObj.shiftKey) {
        modifiers.push("Shift");
      }
      preview.eventKind = "key";
      preview.modifiers = modifiers;

      props.push("key", "charCode", "keyCode");
    } else if (aRawObj instanceof Ci.nsIDOMTransitionEvent) {
      props.push("propertyName", "pseudoElement");
    } else if (aRawObj instanceof Ci.nsIDOMAnimationEvent) {
      props.push("animationName", "pseudoElement");
    } else if (aRawObj instanceof Ci.nsIDOMClipboardEvent) {
      props.push("clipboardData");
    }

    // Add event-specific properties.
    for (let prop of props) {
      let value = aRawObj[prop];
      if (value && (typeof value == "object" || typeof value == "function")) {
        // Skip properties pointing to objects.
        if (hooks.getGripDepth() > 1) {
          continue;
        }
        value = obj.makeDebuggeeValue(value);
      }
      preview.properties[prop] = hooks.createValueGrip(value);
    }

    // Add any properties we find on the event object.
    if (!props.length) {
      let i = 0;
      for (let prop in aRawObj) {
        let value = aRawObj[prop];
        if (prop == "target" || prop == "type" || value === null ||
            typeof value == "function") {
          continue;
        }
        if (value && typeof value == "object") {
          if (hooks.getGripDepth() > 1) {
            continue;
          }
          value = obj.makeDebuggeeValue(value);
        }
        preview.properties[prop] = hooks.createValueGrip(value);
        if (++i == OBJECT_PREVIEW_MAX_ITEMS) {
          break;
        }
      }
    }

    return true;
  }, // DOMEvent

  function DOMException({obj, hooks}, aGrip, aRawObj) {
    if (isWorker || !aRawObj || !(aRawObj instanceof Ci.nsIDOMDOMException)) {
      return false;
    }

    aGrip.preview = {
      kind: "DOMException",
      name: hooks.createValueGrip(aRawObj.name),
      message: hooks.createValueGrip(aRawObj.message),
      code: hooks.createValueGrip(aRawObj.code),
      result: hooks.createValueGrip(aRawObj.result),
      filename: hooks.createValueGrip(aRawObj.filename),
      lineNumber: hooks.createValueGrip(aRawObj.lineNumber),
      columnNumber: hooks.createValueGrip(aRawObj.columnNumber),
    };

    return true;
  },

  function PseudoArray({obj, hooks}, aGrip, aRawObj) {
    let length = 0;

    // Making sure all keys are numbers from 0 to length-1
    let keys = obj.getOwnPropertyNames();
    if (keys.length == 0) {
      return false;
    }
    for (let key of keys) {
      if (isNaN(key) || key != length++) {
        return false;
      }
    }

    aGrip.preview = {
      kind: "ArrayLike",
      length: length,
    };

    // Avoid recursive object grips.
    if (hooks.getGripDepth() > 1) {
      return true;
    }

    let items = aGrip.preview.items = [];

    let i = 0;
    for (let key of keys) {
      if (aRawObj.hasOwnProperty(key) && i++ < OBJECT_PREVIEW_MAX_ITEMS) {
        let value = makeDebuggeeValueIfNeeded(obj, aRawObj[key]);
        items.push(hooks.createValueGrip(value));
      }
    }

    return true;
  }, // PseudoArray

  function GenericObject(aObjectActor, aGrip) {
    let {obj, hooks} = aObjectActor;
    if (aGrip.preview || aGrip.displayString || hooks.getGripDepth() > 1) {
      return false;
    }

    let i = 0, names = [];
    let preview = aGrip.preview = {
      kind: "Object",
      ownProperties: Object.create(null),
    };

    try {
      names = obj.getOwnPropertyNames();
    } catch (ex) {
      // Calling getOwnPropertyNames() on some wrapped native prototypes is not
      // allowed: "cannot modify properties of a WrappedNative". See bug 952093.
    }

    preview.ownPropertiesLength = names.length;

    for (let name of names) {
      let desc = aObjectActor._propertyDescriptor(name, true);
      if (!desc) {
        continue;
      }

      preview.ownProperties[name] = desc;
      if (++i == OBJECT_PREVIEW_MAX_ITEMS) {
        break;
      }
    }

    if (i < OBJECT_PREVIEW_MAX_ITEMS) {
      preview.safeGetterValues = aObjectActor.
                                 _findSafeGetterValues(Object.keys(preview.ownProperties),
                                                       OBJECT_PREVIEW_MAX_ITEMS - i);
    }

    return true;
  }, // GenericObject
]; // DebuggerServer.ObjectActorPreviewers.Object

/**
 * Call PromiseDebugging.getState on this Debugger.Object's referent and wrap
 * the resulting `value` or `reason` properties in a Debugger.Object instance.
 *
 * See dom/webidl/PromiseDebugging.webidl
 *
 * @returns Object
 *          An object of one of the following forms:
 *          - { state: "pending" }
 *          - { state: "fulfilled", value }
 *          - { state: "rejected", reason }
 */
function getPromiseState(obj) {
  if (obj.class != "Promise") {
    throw new Error(
      "Can't call `getPromiseState` on `Debugger.Object`s that don't " +
      "refer to Promise objects.");
  }

  const state = PromiseDebugging.getState(obj.unsafeDereference());
  return {
    state: state.state,
    value: obj.makeDebuggeeValue(state.value),
    reason: obj.makeDebuggeeValue(state.reason)
  };
};

/**
 * Determine if a given value is non-primitive.
 *
 * @param Any aValue
 *        The value to test.
 * @return Boolean
 *         Whether the value is non-primitive.
 */
function isObject(aValue) {
  const type = typeof aValue;
  return type == "object" ? aValue !== null : type == "function";
}

/**
 * Create a function that can safely stringify Debugger.Objects of a given
 * builtin type.
 *
 * @param Function aCtor
 *        The builtin class constructor.
 * @return Function
 *         The stringifier for the class.
 */
function createBuiltinStringifier(aCtor) {
  return aObj => aCtor.prototype.toString.call(aObj.unsafeDereference());
}

/**
 * Stringify a Debugger.Object-wrapped Error instance.
 *
 * @param Debugger.Object aObj
 *        The object to stringify.
 * @return String
 *         The stringification of the object.
 */
function errorStringify(aObj) {
  let name = DevToolsUtils.getProperty(aObj, "name");
  if (name === "" || name === undefined) {
    name = aObj.class;
  } else if (isObject(name)) {
    name = stringify(name);
  }

  let message = DevToolsUtils.getProperty(aObj, "message");
  if (isObject(message)) {
    message = stringify(message);
  }

  if (message === "" || message === undefined) {
    return name;
  }
  return name + ": " + message;
}

/**
 * Stringify a Debugger.Object based on its class.
 *
 * @param Debugger.Object aObj
 *        The object to stringify.
 * @return String
 *         The stringification for the object.
 */
function stringify(aObj) {
  if (aObj.class == "DeadObject") {
    const error = new Error("Dead object encountered.");
    DevToolsUtils.reportException("stringify", error);
    return "<dead object>";
  }

  const stringifier = stringifiers[aObj.class] || stringifiers.Object;

  try {
    return stringifier(aObj);
  } catch (e) {
    DevToolsUtils.reportException("stringify", e);
    return "<failed to stringify object>";
  }
}

// Used to prevent infinite recursion when an array is found inside itself.
let seen = null;

let stringifiers = {
  Error: errorStringify,
  EvalError: errorStringify,
  RangeError: errorStringify,
  ReferenceError: errorStringify,
  SyntaxError: errorStringify,
  TypeError: errorStringify,
  URIError: errorStringify,
  Boolean: createBuiltinStringifier(Boolean),
  Function: createBuiltinStringifier(Function),
  Number: createBuiltinStringifier(Number),
  RegExp: createBuiltinStringifier(RegExp),
  String: createBuiltinStringifier(String),
  Object: obj => "[object " + obj.class + "]",
  Array: obj => {
    // If we're at the top level then we need to create the Set for tracking
    // previously stringified arrays.
    const topLevel = !seen;
    if (topLevel) {
      seen = new Set();
    } else if (seen.has(obj)) {
      return "";
    }

    seen.add(obj);

    const len = DevToolsUtils.getProperty(obj, "length");
    let string = "";

    // The following check is only required because the debuggee could possibly
    // be a Proxy and return any value. For normal objects, array.length is
    // always a non-negative integer.
    if (typeof len == "number" && len > 0) {
      for (let i = 0; i < len; i++) {
        const desc = obj.getOwnPropertyDescriptor(i);
        if (desc) {
          const { value } = desc;
          if (value != null) {
            string += isObject(value) ? stringify(value) : value;
          }
        }

        if (i < len - 1) {
          string += ",";
        }
      }
    }

    if (topLevel) {
      seen = null;
    }

    return string;
  },
  DOMException: obj => {
    const message = DevToolsUtils.getProperty(obj, "message") || "<no message>";
    const result = (+DevToolsUtils.getProperty(obj, "result")).toString(16);
    const code = DevToolsUtils.getProperty(obj, "code");
    const name = DevToolsUtils.getProperty(obj, "name") || "<unknown>";

    return '[Exception... "' + message + '" ' +
           'code: "' + code +'" ' +
           'nsresult: "0x' + result + ' (' + name + ')"]';
  },
  Promise: obj => {
    const { state, value, reason } = getPromiseState(obj);
    let statePreview = state;
    if (state != "pending") {
      const settledValue = state === "fulfilled" ? value : reason;
      statePreview += ": " + (typeof settledValue === "object" && settledValue !== null
                                ? stringify(settledValue)
                                : settledValue);
    }
    return "Promise (" + statePreview + ")";
  },
};

/**
 * Make a debuggee value for the given object, if needed. Primitive values
 * are left the same.
 *
 * Use case: you have a raw JS object (after unsafe dereference) and you want to
 * send it to the client. In that case you need to use an ObjectActor which
 * requires a debuggee value. The Debugger.Object.prototype.makeDebuggeeValue()
 * method works only for JS objects and functions.
 *
 * @param Debugger.Object obj
 * @param any value
 * @return object
 */
function makeDebuggeeValueIfNeeded(obj, value) {
  if (value && (typeof value == "object" || typeof value == "function")) {
    return obj.makeDebuggeeValue(value);
  }
  return value;
}

exports.ObjectActor = ObjectActor;
exports.PropertyIteratorActor = PropertyIteratorActor;

