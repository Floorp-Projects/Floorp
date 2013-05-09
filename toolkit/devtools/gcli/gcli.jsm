/*
 * Copyright 2012, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

"use strict";

/**
 * DO NOT MODIFY THIS FILE DIRECTLY.
 * This file is generated from separate files stored in the GCLI project.
 * Please modify the files there and use the import script so the 2 projects
 * are kept in sync.
 * For more information, ask Joe Walker <jwalker@mozilla.com>
 */

this.EXPORTED_SYMBOLS = [ "gcli" ];

var define = Components.utils.import("resource://gre/modules/devtools/Require.jsm", {}).define;
var require = Components.utils.import("resource://gre/modules/devtools/Require.jsm", {}).require;
var console = Components.utils.import("resource://gre/modules/devtools/Console.jsm", {}).console;
var setTimeout = Components.utils.import("resource://gre/modules/Timer.jsm", {}).setTimeout;
var clearTimeout = Components.utils.import("resource://gre/modules/Timer.jsm", {}).clearTimeout;
var Node = Components.interfaces.nsIDOMNode;
var HTMLElement = Components.interfaces.nsIDOMHTMLElement;

/*
 * Copyright 2012, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

var mozl10n = {};

(function(aMozl10n) {

  'use strict';

  var temp = {};
  Components.utils.import("resource://gre/modules/Services.jsm", temp);

  var stringBundle;
  try {
    stringBundle = temp.Services.strings.createBundle(
            "chrome://browser/locale/devtools/gclicommands.properties");
  }
  catch (ex) {
    console.error("Using string fallbacks");
    stringBundle = {
      GetStringFromName: function(name) { return name; },
      formatStringFromName: function(name) { return name; }
    };
  }

  /**
   * Lookup a string in the GCLI string bundle
   * @param name The name to lookup
   * @return The looked up name
   */
  aMozl10n.lookup = function(name) {
    try {
      return stringBundle.GetStringFromName(name);
    }
    catch (ex) {
      throw new Error("Failure in lookup('" + name + "')");
    }
  };

  /**
   * Lookup a string in the GCLI string bundle
   * @param name The name to lookup
   * @param swaps An array of swaps. See stringBundle.formatStringFromName
   * @return The looked up name
   */
  aMozl10n.lookupFormat = function(name, swaps) {
    try {
      return stringBundle.formatStringFromName(name, swaps, swaps.length);
    }
    catch (ex) {
      throw new Error("Failure in lookupFormat('" + name + "')");
    }
  };

})(mozl10n);

define('gcli/index', ['require', 'exports', 'module' , 'gcli/types/basic', 'gcli/types/selection', 'gcli/types/command', 'gcli/types/javascript', 'gcli/types/node', 'gcli/types/resource', 'gcli/types/setting', 'gcli/settings', 'gcli/ui/intro', 'gcli/ui/focus', 'gcli/ui/fields/basic', 'gcli/ui/fields/javascript', 'gcli/ui/fields/selection', 'gcli/commands/connect', 'gcli/commands/context', 'gcli/commands/help', 'gcli/commands/pref', 'gcli/canon', 'gcli/converters', 'gcli/ui/ffdisplay'], function(require, exports, module) {

  'use strict';

  // Internal startup process. Not exported
  // The basic/selection are depended on by others so they must come first
  require('gcli/types/basic').startup();
  require('gcli/types/selection').startup();

  require('gcli/types/command').startup();
  require('gcli/types/javascript').startup();
  require('gcli/types/node').startup();
  require('gcli/types/resource').startup();
  require('gcli/types/setting').startup();

  require('gcli/settings').startup();
  require('gcli/ui/intro').startup();
  require('gcli/ui/focus').startup();
  require('gcli/ui/fields/basic').startup();
  require('gcli/ui/fields/javascript').startup();
  require('gcli/ui/fields/selection').startup();

  require('gcli/commands/connect').startup();
  require('gcli/commands/context').startup();
  require('gcli/commands/help').startup();
  require('gcli/commands/pref').startup();

  var Cc = Components.classes;
  var Ci = Components.interfaces;
  var prefSvc = "@mozilla.org/preferences-service;1";
  var prefService = Cc[prefSvc].getService(Ci.nsIPrefService);
  var prefBranch = prefService.getBranch(null).QueryInterface(Ci.nsIPrefBranch2);

  // The API for use by command authors
  exports.addCommand = require('gcli/canon').addCommand;
  exports.removeCommand = require('gcli/canon').removeCommand;
  exports.addConverter = require('gcli/converters').addConverter;
  exports.removeConverter = require('gcli/converters').removeConverter;
  exports.lookup = mozl10n.lookup;
  exports.lookupFormat = mozl10n.lookupFormat;

  /**
   * This code is internal and subject to change without notice.
   * createView() for Firefox requires an options object with the following
   * members:
   * - contentDocument: From the window of the attached tab
   * - chromeDocument: GCLITerm.document
   * - environment.hudId: GCLITerm.hudId
   * - jsEnvironment.globalObject: 'window'
   * - jsEnvironment.evalFunction: 'eval' in a sandbox
   * - inputElement: GCLITerm.inputNode
   * - completeElement: GCLITerm.completeNode
   * - hintElement: GCLITerm.hintNode
   * - inputBackgroundElement: GCLITerm.inputStack
   */
  exports.createDisplay = function(opts) {
    var FFDisplay = require('gcli/ui/ffdisplay').FFDisplay;
    return new FFDisplay(opts);
  };

  exports.hiddenByChromePref = function() {
    return !prefBranch.prefHasUserValue("devtools.chrome.enabled");
  };

});
/*
 * Copyright 2012, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

define('gcli/types/basic', ['require', 'exports', 'module' , 'util/promise', 'util/util', 'util/l10n', 'gcli/types', 'gcli/types/selection', 'gcli/argument'], function(require, exports, module) {

'use strict';

var Promise = require('util/promise');
var util = require('util/util');
var l10n = require('util/l10n');
var types = require('gcli/types');
var Type = require('gcli/types').Type;
var Status = require('gcli/types').Status;
var Conversion = require('gcli/types').Conversion;
var ArrayConversion = require('gcli/types').ArrayConversion;
var SelectionType = require('gcli/types/selection').SelectionType;

var BlankArgument = require('gcli/argument').BlankArgument;
var ArrayArgument = require('gcli/argument').ArrayArgument;


/**
 * Registration and de-registration.
 */
exports.startup = function() {
  types.addType(StringType);
  types.addType(NumberType);
  types.addType(BooleanType);
  types.addType(BlankType);
  types.addType(DelegateType);
  types.addType(ArrayType);
};

exports.shutdown = function() {
  types.removeType(StringType);
  types.removeType(NumberType);
  types.removeType(BooleanType);
  types.removeType(BlankType);
  types.removeType(DelegateType);
  types.removeType(ArrayType);
};


/**
 * 'string' the most basic string type that doesn't need to convert
 */
function StringType(typeSpec) {
}

StringType.prototype = Object.create(Type.prototype);

StringType.prototype.stringify = function(value, context) {
  if (value == null) {
    return '';
  }
  return value.toString();
};

StringType.prototype.parse = function(arg, context) {
  if (arg.text == null || arg.text === '') {
    return Promise.resolve(new Conversion(undefined, arg, Status.INCOMPLETE, ''));
  }
  return Promise.resolve(new Conversion(arg.text, arg));
};

StringType.prototype.name = 'string';

exports.StringType = StringType;


/**
 * We distinguish between integers and floats with the _allowFloat flag.
 */
function NumberType(typeSpec) {
  // Default to integer values
  this._allowFloat = !!typeSpec.allowFloat;

  if (typeSpec) {
    this._min = typeSpec.min;
    this._max = typeSpec.max;
    this._step = typeSpec.step || 1;

    if (!this._allowFloat &&
        (this._isFloat(this._min) ||
         this._isFloat(this._max) ||
         this._isFloat(this._step))) {
      throw new Error('allowFloat is false, but non-integer values given in type spec');
    }
  }
  else {
    this._step = 1;
  }
}

NumberType.prototype = Object.create(Type.prototype);

NumberType.prototype.stringify = function(value, context) {
  if (value == null) {
    return '';
  }
  return '' + value;
};

NumberType.prototype.getMin = function() {
  if (this._min) {
    if (typeof this._min === 'function') {
      return this._min();
    }
    if (typeof this._min === 'number') {
      return this._min;
    }
  }
  return undefined;
};

NumberType.prototype.getMax = function() {
  if (this._max) {
    if (typeof this._max === 'function') {
      return this._max();
    }
    if (typeof this._max === 'number') {
      return this._max;
    }
  }
  return undefined;
};

NumberType.prototype.parse = function(arg, context) {
  if (arg.text.replace(/^\s*-?/, '').length === 0) {
    return Promise.resolve(new Conversion(undefined, arg, Status.INCOMPLETE, ''));
  }

  if (!this._allowFloat && (arg.text.indexOf('.') !== -1)) {
    var message = l10n.lookupFormat('typesNumberNotInt2', [ arg.text ]);
    return Promise.resolve(new Conversion(undefined, arg, Status.ERROR, message));
  }

  var value;
  if (this._allowFloat) {
    value = parseFloat(arg.text);
  }
  else {
    value = parseInt(arg.text, 10);
  }

  if (isNaN(value)) {
    var message = l10n.lookupFormat('typesNumberNan', [ arg.text ]);
    return Promise.resolve(new Conversion(undefined, arg, Status.ERROR, message));
  }

  var max = this.getMax();
  if (max != null && value > max) {
    var message = l10n.lookupFormat('typesNumberMax', [ value, max ]);
    return Promise.resolve(new Conversion(undefined, arg, Status.ERROR, message));
  }

  var min = this.getMin();
  if (min != null && value < min) {
    var message = l10n.lookupFormat('typesNumberMin', [ value, min ]);
    return Promise.resolve(new Conversion(undefined, arg, Status.ERROR, message));
  }

  return Promise.resolve(new Conversion(value, arg));
};

NumberType.prototype.decrement = function(value, context) {
  if (typeof value !== 'number' || isNaN(value)) {
    return this.getMax() || 1;
  }
  var newValue = value - this._step;
  // Snap to the nearest incremental of the step
  newValue = Math.ceil(newValue / this._step) * this._step;
  return this._boundsCheck(newValue);
};

NumberType.prototype.increment = function(value, context) {
  if (typeof value !== 'number' || isNaN(value)) {
    var min = this.getMin();
    return min != null ? min : 0;
  }
  var newValue = value + this._step;
  // Snap to the nearest incremental of the step
  newValue = Math.floor(newValue / this._step) * this._step;
  if (this.getMax() == null) {
    return newValue;
  }
  return this._boundsCheck(newValue);
};

/**
 * Return the input value so long as it is within the max/min bounds. If it is
 * lower than the minimum, return the minimum. If it is bigger than the maximum
 * then return the maximum.
 */
NumberType.prototype._boundsCheck = function(value) {
  var min = this.getMin();
  if (min != null && value < min) {
    return min;
  }
  var max = this.getMax();
  if (max != null && value > max) {
    return max;
  }
  return value;
};

/**
 * Return true if the given value is a finite number and not an integer, else
 * return false.
 */
NumberType.prototype._isFloat = function(value) {
  return ((typeof value === 'number') && isFinite(value) && (value % 1 !== 0));
};

NumberType.prototype.name = 'number';

exports.NumberType = NumberType;


/**
 * true/false values
 */
function BooleanType(typeSpec) {
}

BooleanType.prototype = Object.create(SelectionType.prototype);

BooleanType.prototype.lookup = [
  { name: 'false', value: false },
  { name: 'true', value: true }
];

BooleanType.prototype.parse = function(arg, context) {
  if (arg.type === 'TrueNamedArgument') {
    return Promise.resolve(new Conversion(true, arg));
  }
  if (arg.type === 'FalseNamedArgument') {
    return Promise.resolve(new Conversion(false, arg));
  }
  return SelectionType.prototype.parse.call(this, arg, context);
};

BooleanType.prototype.stringify = function(value, context) {
  if (value == null) {
    return '';
  }
  return '' + value;
};

BooleanType.prototype.getBlank = function() {
  return new Conversion(false, new BlankArgument(), Status.VALID, '',
                        Promise.resolve(this.lookup));
};

BooleanType.prototype.name = 'boolean';

exports.BooleanType = BooleanType;


/**
 * A type for "we don't know right now, but hope to soon".
 */
function DelegateType(typeSpec) {
  if (typeof typeSpec.delegateType !== 'function') {
    throw new Error('Instances of DelegateType need typeSpec.delegateType to be a function that returns a type');
  }
  Object.keys(typeSpec).forEach(function(key) {
    this[key] = typeSpec[key];
  }, this);
}

/**
 * Child types should implement this method to return an instance of the type
 * that should be used. If no type is available, or some sort of temporary
 * placeholder is required, BlankType can be used.
 * @param context An ExecutionContext to allow access to information about the
 * current state of the command line, or null if one is not available. A
 * context is available for stringify, parse*, [in|de]crement and getType
 * functions but not for isImportant
 */
DelegateType.prototype.delegateType = function(context) {
  throw new Error('Not implemented');
};

DelegateType.prototype = Object.create(Type.prototype);

DelegateType.prototype.stringify = function(value, context) {
  return this.delegateType(context).stringify(value, context);
};

DelegateType.prototype.parse = function(arg, context) {
  return this.delegateType(context).parse(arg, context);
};

DelegateType.prototype.decrement = function(value, context) {
  var delegated = this.delegateType(context);
  return (delegated.decrement ? delegated.decrement(value, context) : undefined);
};

DelegateType.prototype.increment = function(value, context) {
  var delegated = this.delegateType(context);
  return (delegated.increment ? delegated.increment(value, context) : undefined);
};

DelegateType.prototype.getType = function(context) {
  return this.delegateType(context);
};

Object.defineProperty(DelegateType.prototype, 'isImportant', {
  get: function() {
    return this.delegateType().isImportant;
  },
  enumerable: true
});

/**
 * DelegateType is designed to be inherited from, so DelegateField needs a way
 * to check if something works like a delegate without using 'name'
 */
DelegateType.prototype.isDelegate = true;

DelegateType.prototype.name = 'delegate';

exports.DelegateType = DelegateType;


/**
 * 'blank' is a type for use with DelegateType when we don't know yet.
 * It should not be used anywhere else.
 */
function BlankType(typeSpec) {
}

BlankType.prototype = Object.create(Type.prototype);

BlankType.prototype.stringify = function(value, context) {
  return '';
};

BlankType.prototype.parse = function(arg, context) {
  return Promise.resolve(new Conversion(undefined, arg));
};

BlankType.prototype.name = 'blank';

exports.BlankType = BlankType;


/**
 * A set of objects of the same type
 */
function ArrayType(typeSpec) {
  if (!typeSpec.subtype) {
    console.error('Array.typeSpec is missing subtype. Assuming string.' +
        JSON.stringify(typeSpec));
    typeSpec.subtype = 'string';
  }

  Object.keys(typeSpec).forEach(function(key) {
    this[key] = typeSpec[key];
  }, this);
  this.subtype = types.createType(this.subtype);
}

ArrayType.prototype = Object.create(Type.prototype);

ArrayType.prototype.stringify = function(values, context) {
  if (values == null) {
    return '';
  }
  // BUG 664204: Check for strings with spaces and add quotes
  return values.join(' ');
};

ArrayType.prototype.parse = function(arg, context) {
  if (arg.type !== 'ArrayArgument') {
    console.error('non ArrayArgument to ArrayType.parse', arg);
    throw new Error('non ArrayArgument to ArrayType.parse');
  }

  // Parse an argument to a conversion
  // Hack alert. ArrayConversion needs to be able to answer questions about
  // the status of individual conversions in addition to the overall state.
  // |subArg.conversion| allows us to do that easily.
  var subArgParse = function(subArg) {
    return this.subtype.parse(subArg, context).then(function(conversion) {
      subArg.conversion = conversion;
      return conversion;
    }.bind(this));
  }.bind(this);

  var conversionPromises = arg.getArguments().map(subArgParse);
  return util.all(conversionPromises).then(function(conversions) {
    return new ArrayConversion(conversions, arg);
  });
};

ArrayType.prototype.getBlank = function(values) {
  return new ArrayConversion([], new ArrayArgument());
};

ArrayType.prototype.name = 'array';

exports.ArrayType = ArrayType;


});
/*
 * Copyright 2012, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

define('util/promise', ['require', 'exports', 'module' ], function(require, exports, module) {

  'use strict';

  var imported = {};
  Components.utils.import("resource://gre/modules/commonjs/sdk/core/promise.js",
                          imported);

  exports.defer = imported.Promise.defer;
  exports.resolve = imported.Promise.resolve;
  exports.reject = imported.Promise.reject;

});
/*
 * Copyright 2012, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

define('util/util', ['require', 'exports', 'module' , 'util/promise'], function(require, exports, module) {

'use strict';

/*
 * A number of DOM manipulation and event handling utilities.
 */

//------------------------------------------------------------------------------

var eventDebug = false;

/**
 * Patch up broken console API from node
 */
if (eventDebug) {
  if (console.group == null) {
    console.group = function() { console.log(arguments); };
  }
  if (console.groupEnd == null) {
    console.groupEnd = function() { console.log(arguments); };
  }
}

/**
 * Useful way to create a name for a handler, used in createEvent()
 */
function nameFunction(handler) {
  var scope = handler.scope ? handler.scope.constructor.name + '.' : '';
  var name = handler.func.name;
  if (name) {
    return scope + name;
  }
  for (var prop in handler.scope) {
    if (handler.scope[prop] === handler.func) {
      return scope + prop;
    }
  }
  return scope + handler.func;
}

/**
 * Create an event.
 * For use as follows:
 *
 *   function Hat() {
 *     this.putOn = createEvent('Hat.putOn');
 *     ...
 *   }
 *   Hat.prototype.adorn = function(person) {
 *     this.putOn({ hat: hat, person: person });
 *     ...
 *   }
 *
 *   var hat = new Hat();
 *   hat.putOn.add(function(ev) {
 *     console.log('The hat ', ev.hat, ' has is worn by ', ev.person);
 *   }, scope);
 *
 * @param name Optional name to help with debugging
 */
exports.createEvent = function(name) {
  var handlers = [];
  var holdFire = false;
  var heldEvents = [];
  var eventCombiner = undefined;

  /**
   * This is how the event is triggered.
   * @param ev The event object to be passed to the event listeners
   */
  var event = function(ev) {
    if (holdFire) {
      heldEvents.push(ev);
      if (eventDebug) {
        console.log('Held fire: ' + name, ev);
      }
      return;
    }

    if (eventDebug) {
      console.group('Fire: ' + name + ' to ' + handlers.length + ' listeners', ev);
    }

    // Use for rather than forEach because it step debugs better, which is
    // important for debugging events
    for (var i = 0; i < handlers.length; i++) {
      var handler = handlers[i];
      if (eventDebug) {
        console.log(nameFunction(handler));
      }
      handler.func.call(handler.scope, ev);
    }

    if (eventDebug) {
      console.groupEnd();
    }
  };

  /**
   * Add a new handler function
   * @param func The function to call when this event is triggered
   * @param scope Optional 'this' object for the function call
   */
  event.add = function(func, scope) {
    if (eventDebug) {
      console.log('Adding listener to ' + name);
    }

    handlers.push({ func: func, scope: scope });
  };

  /**
   * Remove a handler function added through add(). Both func and scope must
   * be strict equals (===) the values used in the call to add()
   * @param func The function to call when this event is triggered
   * @param scope Optional 'this' object for the function call
   */
  event.remove = function(func, scope) {
    if (eventDebug) {
      console.log('Removing listener from ' + name);
    }

    var found = false;
    handlers = handlers.filter(function(test) {
      var match = (test.func === func && test.scope === scope);
      if (match) {
        found = true;
      }
      return !match;
    });
    if (!found) {
      console.warn('Handler not found. Attached to ' + name);
    }
  };

  /**
   * Remove all handlers.
   * Reset the state of this event back to it's post create state
   */
  event.removeAll = function() {
    handlers = [];
  };

  /**
   * Temporarily prevent this event from firing.
   * @see resumeFire(ev)
   */
  event.holdFire = function() {
    if (eventDebug) {
      console.group('Holding fire: ' + name);
    }

    holdFire = true;
  };

  /**
   * Resume firing events.
   * If there are heldEvents, then we fire one event to cover them all. If an
   * event combining function has been provided then we use that to combine the
   * events. Otherwise the last held event is used.
   * @see holdFire()
   */
  event.resumeFire = function() {
    if (eventDebug) {
      console.groupEnd('Resume fire: ' + name);
    }

    if (holdFire !== true) {
      throw new Error('Event not held: ' + name);
    }

    holdFire = false;
    if (heldEvents.length === 0) {
      return;
    }

    if (heldEvents.length === 1) {
      event(heldEvents[0]);
    }
    else {
      var first = heldEvents[0];
      var last = heldEvents[heldEvents.length - 1];
      if (eventCombiner) {
        event(eventCombiner(first, last, heldEvents));
      }
      else {
        event(last);
      }
    }

    heldEvents = [];
  };

  /**
   * When resumeFire has a number of events to combine, by default it just
   * picks the last, however you can provide an eventCombiner which returns a
   * combined event.
   * eventCombiners will be passed 3 parameters:
   * - first The first event to be held
   * - last The last event to be held
   * - all An array containing all the held events
   * The return value from an eventCombiner is expected to be an event object
   */
  Object.defineProperty(event, 'eventCombiner', {
    set: function(newEventCombiner) {
      if (typeof newEventCombiner !== 'function') {
        throw new Error('eventCombiner is not a function');
      }
      eventCombiner = newEventCombiner;
    },

    enumerable: true
  });

  return event;
};

//------------------------------------------------------------------------------

var Promise = require('util/promise');

/**
 * Implementation of 'promised', while we wait for bug 790195 to be fixed.
 * @see Consuming promises in https://addons.mozilla.org/en-US/developers/docs/sdk/latest/modules/sdk/core/promise.html
 * @see https://bugzilla.mozilla.org/show_bug.cgi?id=790195
 * @see https://github.com/mozilla/addon-sdk/blob/master/packages/api-utils/lib/promise.js#L179
 */
exports.promised = (function() {
  // Note: Define shortcuts and utility functions here in order to avoid
  // slower property accesses and unnecessary closure creations on each
  // call of this popular function.

  var call = Function.call;
  var concat = Array.prototype.concat;

  // Utility function that does following:
  // execute([ f, self, args...]) => f.apply(self, args)
  function execute(args) { return call.apply(call, args); }

  // Utility function that takes promise of `a` array and maybe promise `b`
  // as arguments and returns promise for `a.concat(b)`.
  function promisedConcat(promises, unknown) {
    return promises.then(function(values) {
      return Promise.resolve(unknown).then(function(value) {
        return values.concat([ value ]);
      });
    });
  }

  return function promised(f, prototype) {
    /**
    Returns a wrapped `f`, which when called returns a promise that resolves to
    `f(...)` passing all the given arguments to it, which by the way may be
    promises. Optionally second `prototype` argument may be provided to be used
    a prototype for a returned promise.

    ## Example

    var promise = promised(Array)(1, promise(2), promise(3))
    promise.then(console.log) // => [ 1, 2, 3 ]
    **/

    return function promised() {
      // create array of [ f, this, args... ]
      return concat.apply([ f, this ], arguments).
          // reduce it via `promisedConcat` to get promised array of fulfillments
          reduce(promisedConcat, Promise.resolve([], prototype)).
          // finally map that to promise of `f.apply(this, args...)`
          then(execute);
    };
  };
})();

/**
 * Convert an array of promises to a single promise, which is resolved (with an
 * array containing resolved values) only when all the component promises are
 * resolved.
 */
exports.all = exports.promised(Array);

/**
 * Utility to convert a resolved promise to a concrete value.
 * Warning: This is something of an experiment. The alternative of mixing
 * concrete/promise return values could be better.
 */
exports.synchronize = function(promise) {
  if (promise == null || typeof promise.then !== 'function') {
    return promise;
  }
  var failure = undefined;
  var reply = undefined;
  var onDone = function(value) {
    failure = false;
    reply = value;
  };
  var onError = function (value) {
    failure = true;
    reply = value;
  };
  promise.then(onDone, onError);
  if (failure === undefined) {
    throw new Error('non synchronizable promise');
  }
  if (failure) {
    throw reply;
  }
  return reply;
};

/**
 * promiseMap is roughly like Array.map except that the action is taken to be
 * something that completes asynchronously, returning a promise.
 * @param array An array of objects to enumerate
 * @param action A function to call for each member of the array
 * @param scope Optional object to use as 'this' for the function calls
 * @return A promise which is resolved (with an array of resolution values)
 * when all the array members have been passed to the action function, and
 * rejected as soon as any of the action function calls fails 
 */
exports.promiseEach = function(array, action, scope) {
  if (array.length === 0) {
    return Promise.resolve([]);
  }

  var deferred = Promise.defer();

  var callNext = function(index) {
    var replies = [];
    var promiseReply = action.call(scope, array[index]);
    Promise.resolve(promiseReply).then(function(reply) {
      replies[index] = reply;

      var nextIndex = index + 1;
      if (nextIndex >= array.length) {
        deferred.resolve(replies);
      }
      else {
        callNext(nextIndex);
      }
    }).then(null, function(ex) {
      deferred.reject(ex);
    });
  };

  callNext(0);
  return deferred.promise;
};

/**
 * Catching errors from promises isn't as simple as:
 *   promise.then(handler, console.error);
 * for a number of reasons:
 * - chrome's console doesn't have bound functions (why?)
 * - we don't get stack traces out from console.error(ex);
 */
exports.errorHandler = function(ex) {
  if (ex instanceof Error) {
    // V8 weirdly includes the exception message in the stack
    if (ex.stack.indexOf(ex.message) !== -1) {
      console.error(ex.stack);
    }
    else {
      console.error('' + ex);
      console.error(ex.stack);
    }
  }
  else {
    console.error(ex);
  }
};


//------------------------------------------------------------------------------

/**
 * XHTML namespace
 */
exports.NS_XHTML = 'http://www.w3.org/1999/xhtml';

/**
 * XUL namespace
 */
exports.NS_XUL = 'http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul';

/**
 * Create an HTML or XHTML element depending on whether the document is HTML
 * or XML based. Where HTML/XHTML elements are distinguished by whether they
 * are created using doc.createElementNS('http://www.w3.org/1999/xhtml', tag)
 * or doc.createElement(tag)
 * If you want to create a XUL element then you don't have a problem knowing
 * what namespace you want.
 * @param doc The document in which to create the element
 * @param tag The name of the tag to create
 * @returns The created element
 */
exports.createElement = function(doc, tag) {
  if (exports.isXmlDocument(doc)) {
    return doc.createElementNS(exports.NS_XHTML, tag);
  }
  else {
    return doc.createElement(tag);
  }
};

/**
 * Remove all the child nodes from this node
 * @param elem The element that should have it's children removed
 */
exports.clearElement = function(elem) {
  while (elem.hasChildNodes()) {
    elem.removeChild(elem.firstChild);
  }
};

var isAllWhitespace = /^\s*$/;

/**
 * Iterate over the children of a node looking for TextNodes that have only
 * whitespace content and remove them.
 * This utility is helpful when you have a template which contains whitespace
 * so it looks nice, but where the whitespace interferes with the rendering of
 * the page
 * @param elem The element which should have blank whitespace trimmed
 * @param deep Should this node removal include child elements
 */
exports.removeWhitespace = function(elem, deep) {
  var i = 0;
  while (i < elem.childNodes.length) {
    var child = elem.childNodes.item(i);
    if (child.nodeType === 3 /*Node.TEXT_NODE*/ &&
        isAllWhitespace.test(child.textContent)) {
      elem.removeChild(child);
    }
    else {
      if (deep && child.nodeType === 1 /*Node.ELEMENT_NODE*/) {
        exports.removeWhitespace(child, deep);
      }
      i++;
    }
  }
};

/**
 * Create a style element in the document head, and add the given CSS text to
 * it.
 * @param cssText The CSS declarations to append
 * @param doc The document element to work from
 * @param id Optional id to assign to the created style tag. If the id already
 * exists on the document, we do not add the CSS again.
 */
exports.importCss = function(cssText, doc, id) {
  if (!cssText) {
    return undefined;
  }

  doc = doc || document;

  if (!id) {
    id = 'hash-' + hash(cssText);
  }

  var found = doc.getElementById(id);
  if (found) {
    if (found.tagName.toLowerCase() !== 'style') {
      console.error('Warning: importCss passed id=' + id +
              ', but that pre-exists (and isn\'t a style tag)');
    }
    return found;
  }

  var style = exports.createElement(doc, 'style');
  style.id = id;
  style.appendChild(doc.createTextNode(cssText));

  var head = doc.getElementsByTagName('head')[0] || doc.documentElement;
  head.appendChild(style);

  return style;
};

/**
 * Simple hash function which happens to match Java's |String.hashCode()|
 * Done like this because I we don't need crypto-security, but do need speed,
 * and I don't want to spend a long time working on it.
 * @see http://werxltd.com/wp/2010/05/13/javascript-implementation-of-javas-string-hashcode-method/
 */
function hash(str) {
  var hash = 0;
  if (str.length == 0) {
    return hash;
  }
  for (var i = 0; i < str.length; i++) {
    var character = str.charCodeAt(i);
    hash = ((hash << 5) - hash) + character;
    hash = hash & hash; // Convert to 32bit integer
  }
  return hash;
}

/**
 * Shortcut for clearElement/createTextNode/appendChild to make up for the lack
 * of standards around textContent/innerText
 */
exports.setTextContent = function(elem, text) {
  exports.clearElement(elem);
  var child = elem.ownerDocument.createTextNode(text);
  elem.appendChild(child);
};

/**
 * There are problems with innerHTML on XML documents, so we need to do a dance
 * using document.createRange().createContextualFragment() when in XML mode
 */
exports.setContents = function(elem, contents) {
  if (typeof HTMLElement !== 'undefined' && contents instanceof HTMLElement) {
    exports.clearElement(elem);
    elem.appendChild(contents);
    return;
  }

  if ('innerHTML' in elem) {
    elem.innerHTML = contents;
  }
  else {
    try {
      var ns = elem.ownerDocument.documentElement.namespaceURI;
      if (!ns) {
        ns = exports.NS_XHTML;
      }
      exports.clearElement(elem);
      contents = '<div xmlns="' + ns + '">' + contents + '</div>';
      var range = elem.ownerDocument.createRange();
      var child = range.createContextualFragment(contents).firstChild;
      while (child.hasChildNodes()) {
        elem.appendChild(child.firstChild);
      }
    }
    catch (ex) {
      console.error('Bad XHTML', ex);
      console.trace();
      throw ex;
    }
  }
};

/**
 * Utility to find elements with href attributes and add a target=_blank
 * attribute to make sure that opened links will open in a new window.
 */
exports.linksToNewTab = function(element) {
  var links = element.ownerDocument.querySelectorAll('*[href]');
  for (var i = 0; i < links.length; i++) {
    links[i].setAttribute('target', '_blank');
  }
  return element;
};

/**
 * Load some HTML into the given document and return a DOM element.
 * This utility assumes that the html has a single root (other than whitespace)
 */
exports.toDom = function(document, html) {
  var div = exports.createElement(document, 'div');
  exports.setContents(div, html);
  return div.children[0];
};

/**
 * How to detect if we're in an XML document.
 * In a Mozilla we check that document.xmlVersion = null, however in Chrome
 * we use document.contentType = undefined.
 * @param doc The document element to work from (defaulted to the global
 * 'document' if missing
 */
exports.isXmlDocument = function(doc) {
  doc = doc || document;
  // Best test for Firefox
  if (doc.contentType && doc.contentType != 'text/html') {
    return true;
  }
  // Best test for Chrome
  if (doc.xmlVersion != null) {
    return true;
  }
  return false;
};

/**
 * Find the position of [element] in [nodeList].
 * @returns an index of the match, or -1 if there is no match
 */
function positionInNodeList(element, nodeList) {
  for (var i = 0; i < nodeList.length; i++) {
    if (element === nodeList[i]) {
      return i;
    }
  }
  return -1;
}

/**
 * Find a unique CSS selector for a given element
 * @returns a string such that ele.ownerDocument.querySelector(reply) === ele
 * and ele.ownerDocument.querySelectorAll(reply).length === 1
 */
exports.findCssSelector = function(ele) {
  var document = ele.ownerDocument;
  if (ele.id && document.getElementById(ele.id) === ele) {
    return '#' + ele.id;
  }

  // Inherently unique by tag name
  var tagName = ele.tagName.toLowerCase();
  if (tagName === 'html') {
    return 'html';
  }
  if (tagName === 'head') {
    return 'head';
  }
  if (tagName === 'body') {
    return 'body';
  }

  if (ele.parentNode == null) {
    console.log('danger: ' + tagName);
  }

  // We might be able to find a unique class name
  var selector, index, matches;
  if (ele.classList.length > 0) {
    for (var i = 0; i < ele.classList.length; i++) {
      // Is this className unique by itself?
      selector = '.' + ele.classList.item(i);
      matches = document.querySelectorAll(selector);
      if (matches.length === 1) {
        return selector;
      }
      // Maybe it's unique with a tag name?
      selector = tagName + selector;
      matches = document.querySelectorAll(selector);
      if (matches.length === 1) {
        return selector;
      }
      // Maybe it's unique using a tag name and nth-child
      index = positionInNodeList(ele, ele.parentNode.children) + 1;
      selector = selector + ':nth-child(' + index + ')';
      matches = document.querySelectorAll(selector);
      if (matches.length === 1) {
        return selector;
      }
    }
  }

  // So we can be unique w.r.t. our parent, and use recursion
  index = positionInNodeList(ele, ele.parentNode.children) + 1;
  selector = exports.findCssSelector(ele.parentNode) + ' > ' +
          tagName + ':nth-child(' + index + ')';

  return selector;
};

/**
 * Work out the path for images.
 */
exports.createUrlLookup = function(callingModule) {
  return function imageUrl(path) {
    try {
      return require('text!gcli/ui/' + path);
    }
    catch (ex) {
      // Under node/unamd callingModule is provided by node. This code isn't
      // the right answer but it's enough to pass all the unit tests and get
      // test coverage information, which is all we actually care about here.
      if (callingModule.filename) {
        return callingModule.filename + path;
      }

      var filename = callingModule.id.split('/').pop() + '.js';

      if (callingModule.uri.substr(-filename.length) !== filename) {
        console.error('Can\'t work out path from module.uri/module.id');
        return path;
      }

      if (callingModule.uri) {
        var end = callingModule.uri.length - filename.length - 1;
        return callingModule.uri.substr(0, end) + '/' + path;
      }

      return filename + '/' + path;
    }
  };
};


//------------------------------------------------------------------------------

/**
 * Keyboard handling is a mess. http://unixpapa.com/js/key.html
 * It would be good to use DOM L3 Keyboard events,
 * http://www.w3.org/TR/2010/WD-DOM-Level-3-Events-20100907/#events-keyboardevents
 * however only Webkit supports them, and there isn't a shim on Monernizr:
 * https://github.com/Modernizr/Modernizr/wiki/HTML5-Cross-browser-Polyfills
 * and when the code that uses this KeyEvent was written, nothing was clear,
 * so instead, we're using this unmodern shim:
 * http://stackoverflow.com/questions/5681146/chrome-10-keyevent-or-something-similar-to-firefoxs-keyevent
 * See BUG 664991: GCLI's keyboard handling should be updated to use DOM-L3
 * https://bugzilla.mozilla.org/show_bug.cgi?id=664991
 */
if (typeof 'KeyEvent' === 'undefined') {
  exports.KeyEvent = this.KeyEvent;
}
else {
  exports.KeyEvent = {
    DOM_VK_CANCEL: 3,
    DOM_VK_HELP: 6,
    DOM_VK_BACK_SPACE: 8,
    DOM_VK_TAB: 9,
    DOM_VK_CLEAR: 12,
    DOM_VK_RETURN: 13,
    DOM_VK_ENTER: 14,
    DOM_VK_SHIFT: 16,
    DOM_VK_CONTROL: 17,
    DOM_VK_ALT: 18,
    DOM_VK_PAUSE: 19,
    DOM_VK_CAPS_LOCK: 20,
    DOM_VK_ESCAPE: 27,
    DOM_VK_SPACE: 32,
    DOM_VK_PAGE_UP: 33,
    DOM_VK_PAGE_DOWN: 34,
    DOM_VK_END: 35,
    DOM_VK_HOME: 36,
    DOM_VK_LEFT: 37,
    DOM_VK_UP: 38,
    DOM_VK_RIGHT: 39,
    DOM_VK_DOWN: 40,
    DOM_VK_PRINTSCREEN: 44,
    DOM_VK_INSERT: 45,
    DOM_VK_DELETE: 46,
    DOM_VK_0: 48,
    DOM_VK_1: 49,
    DOM_VK_2: 50,
    DOM_VK_3: 51,
    DOM_VK_4: 52,
    DOM_VK_5: 53,
    DOM_VK_6: 54,
    DOM_VK_7: 55,
    DOM_VK_8: 56,
    DOM_VK_9: 57,
    DOM_VK_SEMICOLON: 59,
    DOM_VK_EQUALS: 61,
    DOM_VK_A: 65,
    DOM_VK_B: 66,
    DOM_VK_C: 67,
    DOM_VK_D: 68,
    DOM_VK_E: 69,
    DOM_VK_F: 70,
    DOM_VK_G: 71,
    DOM_VK_H: 72,
    DOM_VK_I: 73,
    DOM_VK_J: 74,
    DOM_VK_K: 75,
    DOM_VK_L: 76,
    DOM_VK_M: 77,
    DOM_VK_N: 78,
    DOM_VK_O: 79,
    DOM_VK_P: 80,
    DOM_VK_Q: 81,
    DOM_VK_R: 82,
    DOM_VK_S: 83,
    DOM_VK_T: 84,
    DOM_VK_U: 85,
    DOM_VK_V: 86,
    DOM_VK_W: 87,
    DOM_VK_X: 88,
    DOM_VK_Y: 89,
    DOM_VK_Z: 90,
    DOM_VK_CONTEXT_MENU: 93,
    DOM_VK_NUMPAD0: 96,
    DOM_VK_NUMPAD1: 97,
    DOM_VK_NUMPAD2: 98,
    DOM_VK_NUMPAD3: 99,
    DOM_VK_NUMPAD4: 100,
    DOM_VK_NUMPAD5: 101,
    DOM_VK_NUMPAD6: 102,
    DOM_VK_NUMPAD7: 103,
    DOM_VK_NUMPAD8: 104,
    DOM_VK_NUMPAD9: 105,
    DOM_VK_MULTIPLY: 106,
    DOM_VK_ADD: 107,
    DOM_VK_SEPARATOR: 108,
    DOM_VK_SUBTRACT: 109,
    DOM_VK_DECIMAL: 110,
    DOM_VK_DIVIDE: 111,
    DOM_VK_F1: 112,
    DOM_VK_F2: 113,
    DOM_VK_F3: 114,
    DOM_VK_F4: 115,
    DOM_VK_F5: 116,
    DOM_VK_F6: 117,
    DOM_VK_F7: 118,
    DOM_VK_F8: 119,
    DOM_VK_F9: 120,
    DOM_VK_F10: 121,
    DOM_VK_F11: 122,
    DOM_VK_F12: 123,
    DOM_VK_F13: 124,
    DOM_VK_F14: 125,
    DOM_VK_F15: 126,
    DOM_VK_F16: 127,
    DOM_VK_F17: 128,
    DOM_VK_F18: 129,
    DOM_VK_F19: 130,
    DOM_VK_F20: 131,
    DOM_VK_F21: 132,
    DOM_VK_F22: 133,
    DOM_VK_F23: 134,
    DOM_VK_F24: 135,
    DOM_VK_NUM_LOCK: 144,
    DOM_VK_SCROLL_LOCK: 145,
    DOM_VK_COMMA: 188,
    DOM_VK_PERIOD: 190,
    DOM_VK_SLASH: 191,
    DOM_VK_BACK_QUOTE: 192,
    DOM_VK_OPEN_BRACKET: 219,
    DOM_VK_BACK_SLASH: 220,
    DOM_VK_CLOSE_BRACKET: 221,
    DOM_VK_QUOTE: 222,
    DOM_VK_META: 224
  };
}


});
/*
 * Copyright 2012, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

define('util/l10n', ['require', 'exports', 'module' ], function(require, exports, module) {

'use strict';

Components.utils.import('resource://gre/modules/XPCOMUtils.jsm');
Components.utils.import('resource://gre/modules/Services.jsm');

var imports = {};
XPCOMUtils.defineLazyGetter(imports, 'stringBundle', function () {
  return Services.strings.createBundle('chrome://browser/locale/devtools/gcli.properties');
});

/*
 * Not supported when embedded - we're doing things the Mozilla way not the
 * require.js way.
 */
exports.registerStringsSource = function(modulePath) {
  throw new Error('registerStringsSource is not available in mozilla');
};

exports.unregisterStringsSource = function(modulePath) {
  throw new Error('unregisterStringsSource is not available in mozilla');
};

exports.lookupSwap = function(key, swaps) {
  throw new Error('lookupSwap is not available in mozilla');
};

exports.lookupPlural = function(key, ord, swaps) {
  throw new Error('lookupPlural is not available in mozilla');
};

exports.getPreferredLocales = function() {
  return [ 'root' ];
};

/** @see lookup() in lib/util/l10n.js */
exports.lookup = function(key) {
  try {
    // Our memory leak hunter walks reachable objects trying to work out what
    // type of thing they are using object.constructor.name. If that causes
    // problems then we can avoid the unknown-key-exception with the following:
    /*
    if (key === 'constructor') {
      return { name: 'l10n-mem-leak-defeat' };
    }
    */

    return imports.stringBundle.GetStringFromName(key);
  }
  catch (ex) {
    console.error('Failed to lookup ', key, ex);
    return key;
  }
};

/** @see propertyLookup in lib/util/l10n.js */
exports.propertyLookup = Proxy.create({
  get: function(rcvr, name) {
    return exports.lookup(name);
  }
});

/** @see lookupFormat in lib/util/l10n.js */
exports.lookupFormat = function(key, swaps) {
  try {
    return imports.stringBundle.formatStringFromName(key, swaps, swaps.length);
  }
  catch (ex) {
    console.error('Failed to format ', key, ex);
    return key;
  }
};


});
/*
 * Copyright 2012, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

define('gcli/types', ['require', 'exports', 'module' , 'util/util', 'util/promise', 'gcli/argument'], function(require, exports, module) {

'use strict';

var util = require('util/util');
var Promise = require('util/promise');
var Argument = require('gcli/argument').Argument;
var BlankArgument = require('gcli/argument').BlankArgument;


/**
 * Some types can detect validity, that is to say they can distinguish between
 * valid and invalid values.
 * We might want to change these constants to be numbers for better performance
 */
var Status = {
  /**
   * The conversion process worked without any problem, and the value is
   * valid. There are a number of failure states, so the best way to check
   * for failure is (x !== Status.VALID)
   */
  VALID: {
    toString: function() { return 'VALID'; },
    valueOf: function() { return 0; }
  },

  /**
   * A conversion process failed, however it was noted that the string
   * provided to 'parse()' could be VALID by the addition of more characters,
   * so the typing may not be actually incorrect yet, just unfinished.
   * @see Status.ERROR
   */
  INCOMPLETE: {
    toString: function() { return 'INCOMPLETE'; },
    valueOf: function() { return 1; }
  },

  /**
   * The conversion process did not work, the value should be null and a
   * reason for failure should have been provided. In addition some
   * completion values may be available.
   * @see Status.INCOMPLETE
   */
  ERROR: {
    toString: function() { return 'ERROR'; },
    valueOf: function() { return 2; }
  },

  /**
   * A combined status is the worser of the provided statuses. The statuses
   * can be provided either as a set of arguments or a single array
   */
  combine: function() {
    var combined = Status.VALID;
    for (var i = 0; i < arguments.length; i++) {
      var status = arguments[i];
      if (Array.isArray(status)) {
        status = Status.combine.apply(null, status);
      }
      if (status > combined) {
        combined = status;
      }
    }
    return combined;
  }
};

exports.Status = Status;


/**
 * The type.parse() method converts an Argument into a value, Conversion is
 * a wrapper to that value.
 * Conversion is needed to collect a number of properties related to that
 * conversion in one place, i.e. to handle errors and provide traceability.
 * @param value The result of the conversion
 * @param arg The data from which the conversion was made
 * @param status See the Status values [VALID|INCOMPLETE|ERROR] defined above.
 * The default status is Status.VALID.
 * @param message If status=ERROR, there should be a message to describe the
 * error. A message is not needed unless for other statuses, but could be
 * present for any status including VALID (in the case where we want to note a
 * warning, for example).
 * See BUG 664676: GCLI conversion error messages should be localized
 * @param predictions If status=INCOMPLETE, there could be predictions as to
 * the options available to complete the input.
 * We generally expect there to be about 7 predictions (to match human list
 * comprehension ability) however it is valid to provide up to about 20,
 * or less. It is the job of the predictor to decide a smart cut-off.
 * For example if there are 4 very good matches and 4 very poor ones,
 * probably only the 4 very good matches should be presented.
 * The predictions are presented either as an array of prediction objects or as
 * a function which returns this array when called with no parameters.
 * Each prediction object has the following shape:
 *     {
 *       name: '...',     // textual completion. i.e. what the cli uses
 *       value: { ... },  // value behind the textual completion
 *       incomplete: true // this completion is only partial (optional)
 *     }
 * The 'incomplete' property could be used to denote a valid completion which
 * could have sub-values (e.g. for tree navigation).
 */
function Conversion(value, arg, status, message, predictions) {
  // The result of the conversion process. Will be null if status != VALID
  this.value = value;

  // Allow us to trace where this Conversion came from
  this.arg = arg;
  if (arg == null) {
    throw new Error('Missing arg');
  }

  if (predictions != null) {
    var toCheck = typeof predictions === 'function' ? predictions() : predictions;
    if (typeof toCheck.then !== 'function') {
      throw new Error('predictions is not a promise');
    }
    toCheck.then(function(value) {
      if (!Array.isArray(value)) {
        throw new Error('prediction resolves to non array');
      }
    }, util.errorHandler);
  }

  this._status = status || Status.VALID;
  this.message = message;
  this.predictions = predictions;
}

/**
 * Ensure that all arguments that are part of this conversion know what they
 * are assigned to.
 * @param assignment The Assignment (param/conversion link) to inform the
 * argument about.
 */
Conversion.prototype.assign = function(assignment) {
  this.arg.assign(assignment);
};

/**
 * Work out if there is information provided in the contained argument.
 */
Conversion.prototype.isDataProvided = function() {
  return this.arg.type !== 'BlankArgument';
};

/**
 * 2 conversions are equal if and only if their args are equal (argEquals) and
 * their values are equal (valueEquals).
 * @param that The conversion object to compare against.
 */
Conversion.prototype.equals = function(that) {
  if (this === that) {
    return true;
  }
  if (that == null) {
    return false;
  }
  return this.valueEquals(that) && this.argEquals(that);
};

/**
 * Check that the value in this conversion is strict equal to the value in the
 * provided conversion.
 * @param that The conversion to compare values with
 */
Conversion.prototype.valueEquals = function(that) {
  return this.value === that.value;
};

/**
 * Check that the argument in this conversion is equal to the value in the
 * provided conversion as defined by the argument (i.e. arg.equals).
 * @param that The conversion to compare arguments with
 */
Conversion.prototype.argEquals = function(that) {
  return that == null ? false : this.arg.equals(that.arg);
};

/**
 * Accessor for the status of this conversion
 */
Conversion.prototype.getStatus = function(arg) {
  return this._status;
};

/**
 * Defined by the toString() value provided by the argument
 */
Conversion.prototype.toString = function() {
  return this.arg.toString();
};

/**
 * If status === INCOMPLETE, then we may be able to provide predictions as to
 * how the argument can be completed.
 * @return An array of items, or a promise of an array of items, where each
 * item is an object with the following properties:
 * - name (mandatory): Displayed to the user, and typed in. No whitespace
 * - description (optional): Short string for display in a tool-tip
 * - manual (optional): Longer description which details usage
 * - incomplete (optional): Indicates that the prediction if used should not
 *   be considered necessarily sufficient, which typically will mean that the
 *   UI should not append a space to the completion
 * - value (optional): If a value property is present, this will be used as the
 *   value of the conversion, otherwise the item itself will be used.
 */
Conversion.prototype.getPredictions = function() {
  if (typeof this.predictions === 'function') {
    return this.predictions();
  }
  return Promise.resolve(this.predictions || []);
};

/**
 * Return a promise of an index constrained by the available predictions.
 * i.e. (index % predicitons.length)
 */
Conversion.prototype.constrainPredictionIndex = function(index) {
  if (index == null) {
    return Promise.resolve();
  }

  return this.getPredictions().then(function(value) {
    if (value.length === 0) {
      return undefined;
    }

    index = index % value.length;
    if (index < 0) {
      index = value.length + index;
    }
    return index;
  }.bind(this));
};

/**
 * Constant to allow everyone to agree on the maximum number of predictions
 * that should be provided. We actually display 1 less than this number.
 */
Conversion.maxPredictions = 11;

exports.Conversion = Conversion;


/**
 * ArrayConversion is a special Conversion, needed because arrays are converted
 * member by member rather then as a whole, which means we can track the
 * conversion if individual array elements. So an ArrayConversion acts like a
 * normal Conversion (which is needed as Assignment requires a Conversion) but
 * it can also be devolved into a set of Conversions for each array member.
 */
function ArrayConversion(conversions, arg) {
  this.arg = arg;
  this.conversions = conversions;
  this.value = conversions.map(function(conversion) {
    return conversion.value;
  }, this);

  this._status = Status.combine(conversions.map(function(conversion) {
    return conversion.getStatus();
  }));

  // This message is just for reporting errors like "not enough values"
  // rather that for problems with individual values.
  this.message = '';

  // Predictions are generally provided by individual values
  this.predictions = [];
}

ArrayConversion.prototype = Object.create(Conversion.prototype);

ArrayConversion.prototype.assign = function(assignment) {
  this.conversions.forEach(function(conversion) {
    conversion.assign(assignment);
  }, this);
  this.assignment = assignment;
};

ArrayConversion.prototype.getStatus = function(arg) {
  if (arg && arg.conversion) {
    return arg.conversion.getStatus();
  }
  return this._status;
};

ArrayConversion.prototype.isDataProvided = function() {
  return this.conversions.length > 0;
};

ArrayConversion.prototype.valueEquals = function(that) {
  if (!(that instanceof ArrayConversion)) {
    throw new Error('Can\'t compare values with non ArrayConversion');
  }

  if (this.value === that.value) {
    return true;
  }

  if (this.value.length !== that.value.length) {
    return false;
  }

  for (var i = 0; i < this.conversions.length; i++) {
    if (!this.conversions[i].valueEquals(that.conversions[i])) {
      return false;
    }
  }

  return true;
};

ArrayConversion.prototype.toString = function() {
  return '[ ' + this.conversions.map(function(conversion) {
    return conversion.toString();
  }, this).join(', ') + ' ]';
};

exports.ArrayConversion = ArrayConversion;


/**
 * Most of our types are 'static' e.g. there is only one type of 'string',
 * however some types like 'selection' and 'delegate' are customizable.
 * The basic Type type isn't useful, but does provide documentation about what
 * types do.
 */
function Type() {
}

/**
 * Convert the given <tt>value</tt> to a string representation.
 * Where possible, there should be round-tripping between values and their
 * string representations.
 * @param value The object to convert into a string
 * @param context An ExecutionContext to allow basic Requisition access
 */
Type.prototype.stringify = function(value, context) {
  throw new Error('Not implemented');
};

/**
 * Convert the given <tt>arg</tt> to an instance of this type.
 * Where possible, there should be round-tripping between values and their
 * string representations.
 * @param arg An instance of <tt>Argument</tt> to convert.
 * @param context An ExecutionContext to allow basic Requisition access
 * @return Conversion
 */
Type.prototype.parse = function(arg, context) {
  throw new Error('Not implemented');
};

/**
 * A convenience method for times when you don't have an argument to parse
 * but instead have a string.
 * @see #parse(arg)
 */
Type.prototype.parseString = function(str, context) {
  return this.parse(new Argument(str), context);
};

/**
 * The plug-in system, and other things need to know what this type is
 * called. The name alone is not enough to fully specify a type. Types like
 * 'selection' and 'delegate' need extra data, however this function returns
 * only the name, not the extra data.
 */
Type.prototype.name = undefined;

/**
 * If there is some concept of a higher value, return it,
 * otherwise return undefined.
 */
Type.prototype.increment = function(value, context) {
  return undefined;
};

/**
 * If there is some concept of a lower value, return it,
 * otherwise return undefined.
 */
Type.prototype.decrement = function(value, context) {
  return undefined;
};

/**
 * The 'blank value' of most types is 'undefined', but there are exceptions;
 * This allows types to specify a better conversion from empty string than
 * 'undefined'.
 * 2 known examples of this are boolean -> false and array -> []
 */
Type.prototype.getBlank = function() {
  return new Conversion(undefined, new BlankArgument(), Status.INCOMPLETE, '');
};

/**
 * This is something of a hack for the benefit of DelegateType which needs to
 * be able to lie about it's type for fields to accept it as one of their own.
 * Sub-types can ignore this unless they're DelegateType.
 * @param context An ExecutionContext to allow basic Requisition access
 */
Type.prototype.getType = function(context) {
  return this;
};

exports.Type = Type;

/**
 * Private registry of types
 * Invariant: types[name] = type.name
 */
var registeredTypes = {};

exports.getTypeNames = function() {
  return Object.keys(registeredTypes);
};

/**
 * Add a new type to the list available to the system.
 * You can pass 2 things to this function - either an instance of Type, in
 * which case we return this instance when #getType() is called with a 'name'
 * that matches type.name.
 * Also you can pass in a constructor (i.e. function) in which case when
 * #getType() is called with a 'name' that matches Type.prototype.name we will
 * pass the typeSpec into this constructor.
 */
exports.addType = function(type) {
  if (typeof type === 'object') {
    if (!type.name) {
      throw new Error('All registered types must have a name');
    }

    if (type instanceof Type) {
      registeredTypes[type.name] = type;
    }
    else {
      if (!type.parent) {
        throw new Error('\'parent\' property required for object declarations');
      }
      var name = type.name;
      var parent = type.parent;
      type.name = parent;
      delete type.parent;

      registeredTypes[name] = exports.createType(type);

      type.name = name;
      type.parent = parent;
    }
  }
  else if (typeof type === 'function') {
    if (!type.prototype.name) {
      throw new Error('All registered types must have a name');
    }
    registeredTypes[type.prototype.name] = type;
  }
  else {
    throw new Error('Unknown type: ' + type);
  }
};

/**
 * Remove a type from the list available to the system
 */
exports.removeType = function(type) {
  delete registeredTypes[type.name];
};

/**
 * Find a type, previously registered using #addType()
 */
exports.createType = function(typeSpec) {
  if (typeof typeSpec === 'string') {
    typeSpec = { name: typeSpec };
  }

  if (typeof typeSpec !== 'object') {
    throw new Error('Can\'t extract type from ' + typeSpec);
  }

  if (!typeSpec.name) {
    throw new Error('Missing \'name\' member to typeSpec');
  }

  var newType;
  var type = registeredTypes[typeSpec.name];

  if (!type) {
    console.error('Known types: ' + Object.keys(registeredTypes).join(', '));
    throw new Error('Unknown type: \'' + typeSpec.name + '\'');
  }

  if (typeof type === 'function') {
    newType = new type(typeSpec);
  }
  else {
    // Shallow clone 'type'
    newType = {};
    for (var key in type) {
      newType[key] = type[key];
    }

    // Copy the properties of typeSpec onto the new type
    for (var key in typeSpec) {
      newType[key] = typeSpec[key];
    }

    if (typeof newType.constructor === 'function') {
      newType.constructor();
    }
  }

  return newType;
};


});
/*
 * Copyright 2012, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

define('gcli/argument', ['require', 'exports', 'module' ], function(require, exports, module) {

'use strict';

/**
 * Thinking out loud here:
 * Arguments are an area where we could probably refactor things a bit better.
 * The split process in Requisition creates a set of Arguments, which are then
 * assigned. The assign process sometimes converts them into subtypes of
 * Argument. We might consider that what gets assigned is _always_ one of the
 * subtypes (or actually a different type hierarchy entirely) and that we
 * don't manipulate the prefix/text/suffix but just use the 'subtypes' as
 * filters which present a view of the underlying original Argument.
 */

/**
 * We record where in the input string an argument comes so we can report
 * errors against those string positions.
 * @param text The string (trimmed) that contains the argument
 * @param prefix Knowledge of quotation marks and whitespace used prior to the
 * text in the input string allows us to re-generate the original input from
 * the arguments.
 * @param suffix Any quotation marks and whitespace used after the text.
 * Whitespace is normally placed in the prefix to the succeeding argument, but
 * can be used here when this is the last argument.
 * @constructor
 */
function Argument(text, prefix, suffix) {
  if (text === undefined) {
    this.text = '';
    this.prefix = '';
    this.suffix = '';
  }
  else {
    this.text = text;
    this.prefix = prefix !== undefined ? prefix : '';
    this.suffix = suffix !== undefined ? suffix : '';
  }
}

Argument.prototype.type = 'Argument';

/**
 * Return the result of merging these arguments.
 * case and some of the arguments are in quotation marks?
 */
Argument.prototype.merge = function(following) {
  // Is it possible that this gets called when we're merging arguments
  // for the single string?
  return new Argument(
    this.text + this.suffix + following.prefix + following.text,
    this.prefix, following.suffix);
};

/**
 * Returns a new Argument like this one but with various items changed.
 * @param options Values to use in creating a new Argument.
 * Warning: some implementations of beget make additions to the options
 * argument. You should be aware of this in the unlikely event that you want to
 * reuse 'options' arguments.
 * Properties:
 * - text: The new text value
 * - prefixSpace: Should the prefix be altered to begin with a space?
 * - prefixPostSpace: Should the prefix be altered to end with a space?
 * - suffixSpace: Should the suffix be altered to end with a space?
 * - type: Constructor to use in creating new instances. Default: Argument
 * - dontQuote: Should we avoid adding prefix/suffix quotes when the text value
 *   has a space? Needed when we're completing a sub-command.
 */
Argument.prototype.beget = function(options) {
  var text = this.text;
  var prefix = this.prefix;
  var suffix = this.suffix;

  if (options.text != null) {
    text = options.text;

    // We need to add quotes when the replacement string has spaces or is empty
    if (!options.dontQuote) {
      var needsQuote = text.indexOf(' ') >= 0 || text.length == 0;
      var hasQuote = /['"]$/.test(prefix);
      if (needsQuote && !hasQuote) {
        prefix = prefix + '\'';
        suffix = '\'' + suffix;
      }
    }
  }

  if (options.prefixSpace && prefix.charAt(0) !== ' ') {
    prefix = ' ' + prefix;
  }

  if (options.prefixPostSpace && prefix.charAt(prefix.length - 1) !== ' ') {
    prefix = prefix + ' ';
  }

  if (options.suffixSpace && suffix.charAt(suffix.length - 1) !== ' ') {
    suffix = suffix + ' ';
  }

  if (text === this.text && suffix === this.suffix && prefix === this.prefix) {
    return this;
  }

  var type = options.type || Argument;
  return new type(text, prefix, suffix);
};

/**
 * We need to keep track of which assignment we've been assigned to
 */
Argument.prototype.assign = function(assignment) {
  this.assignment = assignment;
};

/**
 * Sub-classes of Argument are collections of arguments, getArgs() gets access
 * to the members of the collection in order to do things like re-create input
 * command lines. For the simple Argument case it's just an array containing
 * only this.
 */
Argument.prototype.getArgs = function() {
  return [ this ];
};

/**
 * We define equals to mean all arg properties are strict equals.
 * Used by Conversion.argEquals and Conversion.equals and ultimately
 * Assignment.equals to avoid reporting a change event when a new conversion
 * is assigned.
 */
Argument.prototype.equals = function(that) {
  if (this === that) {
    return true;
  }
  if (that == null || !(that instanceof Argument)) {
    return false;
  }

  return this.text === that.text &&
       this.prefix === that.prefix && this.suffix === that.suffix;
};

/**
 * Helper when we're putting arguments back together
 */
Argument.prototype.toString = function() {
  // BUG 664207: We should re-escape escaped characters
  // But can we do that reliably?
  return this.prefix + this.text + this.suffix;
};

/**
 * Merge an array of arguments into a single argument.
 * All Arguments in the array are expected to have the same emitter
 */
Argument.merge = function(argArray, start, end) {
  start = (start === undefined) ? 0 : start;
  end = (end === undefined) ? argArray.length : end;

  var joined;
  for (var i = start; i < end; i++) {
    var arg = argArray[i];
    if (!joined) {
      joined = arg;
    }
    else {
      joined = joined.merge(arg);
    }
  }
  return joined;
};

/**
 * For test/debug use only. The output from this function is subject to wanton
 * random change without notice, and should not be relied upon to even exist
 * at some later date.
 */
Object.defineProperty(Argument.prototype, '_summaryJson', {
  get: function() {
    var assignStatus = this.assignment == null ?
            'null' :
            this.assignment.param.name;
    return '<' + this.prefix + ':' + this.text + ':' + this.suffix + '>' +
        ' (a=' + assignStatus + ',' + ' t=' + this.type + ')';
  },
  enumerable: true
});

exports.Argument = Argument;


/**
 * BlankArgument is a marker that the argument wasn't typed but is there to
 * fill a slot. Assignments begin with their arg set to a BlankArgument.
 */
function BlankArgument() {
  this.text = '';
  this.prefix = '';
  this.suffix = '';
}

BlankArgument.prototype = Object.create(Argument.prototype);

BlankArgument.prototype.type = 'BlankArgument';

exports.BlankArgument = BlankArgument;


/**
 * ScriptArgument is a marker that the argument is designed to be Javascript.
 * It also implements the special rules that spaces after the { or before the
 * } are part of the pre/suffix rather than the content, and that they are
 * never 'blank' so they can be used by Requisition._split() and not raise an
 * ERROR status due to being blank.
 */
function ScriptArgument(text, prefix, suffix) {
  this.text = text !== undefined ? text : '';
  this.prefix = prefix !== undefined ? prefix : '';
  this.suffix = suffix !== undefined ? suffix : '';

  ScriptArgument._moveSpaces(this);
}

ScriptArgument.prototype = Object.create(Argument.prototype);

ScriptArgument.prototype.type = 'ScriptArgument';

/**
 * Private/Dangerous: Alters a ScriptArgument to move the spaces at the start
 * or end of the 'text' into the prefix/suffix. With a string, " a " is 3 chars
 * long, but with a ScriptArgument, { a } is only one char long.
 * Arguments are generally supposed to be immutable, so this method should only
 * be called on a ScriptArgument that isn't exposed to the outside world yet.
 */
ScriptArgument._moveSpaces = function(arg) {
  while (arg.text.charAt(0) === ' ') {
    arg.prefix = arg.prefix + ' ';
    arg.text = arg.text.substring(1);
  }

  while (arg.text.charAt(arg.text.length - 1) === ' ') {
    arg.suffix = ' ' + arg.suffix;
    arg.text = arg.text.slice(0, -1);
  }
};

/**
 * As Argument.beget that implements the space rule documented in the ctor.
 */
ScriptArgument.prototype.beget = function(options) {
  options.type = ScriptArgument;
  var begotten = Argument.prototype.beget.call(this, options);
  ScriptArgument._moveSpaces(begotten);
  return begotten;
};

exports.ScriptArgument = ScriptArgument;


/**
 * Commands like 'echo' with a single string argument, and used with the
 * special format like: 'echo a b c' effectively have a number of arguments
 * merged together.
 */
function MergedArgument(args, start, end) {
  if (!Array.isArray(args)) {
    throw new Error('args is not an array of Arguments');
  }

  if (start === undefined) {
    this.args = args;
  }
  else {
    this.args = args.slice(start, end);
  }

  var arg = Argument.merge(this.args);
  this.text = arg.text;
  this.prefix = arg.prefix;
  this.suffix = arg.suffix;
}

MergedArgument.prototype = Object.create(Argument.prototype);

MergedArgument.prototype.type = 'MergedArgument';

/**
 * Keep track of which assignment we've been assigned to, and allow the
 * original args to do the same.
 */
MergedArgument.prototype.assign = function(assignment) {
  this.args.forEach(function(arg) {
    arg.assign(assignment);
  }, this);

  this.assignment = assignment;
};

MergedArgument.prototype.getArgs = function() {
  return this.args;
};

MergedArgument.prototype.equals = function(that) {
  if (this === that) {
    return true;
  }
  if (that == null || !(that instanceof MergedArgument)) {
    return false;
  }

  // We might need to add a check that args is the same here

  return this.text === that.text &&
       this.prefix === that.prefix && this.suffix === that.suffix;
};

exports.MergedArgument = MergedArgument;


/**
 * TrueNamedArguments are for when we have an argument like --verbose which
 * has a boolean value, and thus the opposite of '--verbose' is ''.
 */
function TrueNamedArgument(arg) {
  this.arg = arg;
  this.text = arg.text;
  this.prefix = arg.prefix;
  this.suffix = arg.suffix;
}

TrueNamedArgument.prototype = Object.create(Argument.prototype);

TrueNamedArgument.prototype.type = 'TrueNamedArgument';

TrueNamedArgument.prototype.assign = function(assignment) {
  if (this.arg) {
    this.arg.assign(assignment);
  }
  this.assignment = assignment;
};

TrueNamedArgument.prototype.getArgs = function() {
  return [ this.arg ];
};

TrueNamedArgument.prototype.equals = function(that) {
  if (this === that) {
    return true;
  }
  if (that == null || !(that instanceof TrueNamedArgument)) {
    return false;
  }

  return this.text === that.text &&
       this.prefix === that.prefix && this.suffix === that.suffix;
};

/**
 * As Argument.beget that rebuilds nameArg and valueArg
 */
TrueNamedArgument.prototype.beget = function(options) {
  if (options.text) {
    console.error('Can\'t change text of a TrueNamedArgument', this, options);
  }

  options.type = TrueNamedArgument;
  var begotten = Argument.prototype.beget.call(this, options);
  begotten.arg = new Argument(begotten.text, begotten.prefix, begotten.suffix);
  return begotten;
};

exports.TrueNamedArgument = TrueNamedArgument;


/**
 * FalseNamedArguments are for when we don't have an argument like --verbose
 * which has a boolean value, and thus the opposite of '' is '--verbose'.
 */
function FalseNamedArgument() {
  this.text = '';
  this.prefix = '';
  this.suffix = '';
}

FalseNamedArgument.prototype = Object.create(Argument.prototype);

FalseNamedArgument.prototype.type = 'FalseNamedArgument';

FalseNamedArgument.prototype.getArgs = function() {
  return [ ];
};

FalseNamedArgument.prototype.equals = function(that) {
  if (this === that) {
    return true;
  }
  if (that == null || !(that instanceof FalseNamedArgument)) {
    return false;
  }

  return this.text === that.text &&
       this.prefix === that.prefix && this.suffix === that.suffix;
};

exports.FalseNamedArgument = FalseNamedArgument;


/**
 * A named argument is for cases where we have input in one of the following
 * formats:
 * <ul>
 * <li>--param value
 * <li>-p value
 * </ul>
 * We model this as a normal argument but with a long prefix.
 *
 * There are 2 ways to construct a NamedArgument. One using 2 Arguments which
 * are taken to be the argument for the name (e.g. '--param') and one for the
 * value to assign to that parameter.
 * Alternatively, you can pass in the text/prefix/suffix values in the same
 * way as an Argument is constructed. If you do this then you are expected to
 * assign to nameArg and valueArg before exposing the new NamedArgument.
 */
function NamedArgument() {
  if (typeof arguments[0] === 'string') {
    this.nameArg = null;
    this.valueArg = null;
    this.text = arguments[0];
    this.prefix = arguments[1];
    this.suffix = arguments[2];
  }
  else if (arguments[1] == null) {
    this.nameArg = arguments[0];
    this.valueArg = null;
    this.text = '';
    this.prefix = this.nameArg.toString();
    this.suffix = '';
  }
  else {
    this.nameArg = arguments[0];
    this.valueArg = arguments[1];
    this.text = this.valueArg.text;
    this.prefix = this.nameArg.toString() + this.valueArg.prefix;
    this.suffix = this.valueArg.suffix;
  }
}

NamedArgument.prototype = Object.create(Argument.prototype);

NamedArgument.prototype.type = 'NamedArgument';

NamedArgument.prototype.assign = function(assignment) {
  this.nameArg.assign(assignment);
  if (this.valueArg != null) {
    this.valueArg.assign(assignment);
  }
  this.assignment = assignment;
};

NamedArgument.prototype.getArgs = function() {
  return this.valueArg ? [ this.nameArg, this.valueArg ] : [ this.nameArg ];
};

NamedArgument.prototype.equals = function(that) {
  if (this === that) {
    return true;
  }
  if (that == null) {
    return false;
  }

  if (!(that instanceof NamedArgument)) {
    return false;
  }

  // We might need to add a check that nameArg and valueArg are the same

  return this.text === that.text &&
       this.prefix === that.prefix && this.suffix === that.suffix;
};

/**
 * As Argument.beget that rebuilds nameArg and valueArg
 */
NamedArgument.prototype.beget = function(options) {
  options.type = NamedArgument;
  var begotten = Argument.prototype.beget.call(this, options);

  // Cut the prefix into |whitespace|non-whitespace|whitespace+quote so we can
  // rebuild nameArg and valueArg from the parts
  var matches = /^([\s]*)([^\s]*)([\s]*['"]?)$/.exec(begotten.prefix);

  if (this.valueArg == null && begotten.text === '') {
    begotten.nameArg = new Argument(matches[2], matches[1], matches[3]);
    begotten.valueArg = null;
  }
  else {
    begotten.nameArg = new Argument(matches[2], matches[1], '');
    begotten.valueArg = new Argument(begotten.text, matches[3], begotten.suffix);
  }

  return begotten;
};

exports.NamedArgument = NamedArgument;


/**
 * An argument the groups together a number of plain arguments together so they
 * can be jointly assigned to a single array parameter
 */
function ArrayArgument() {
  this.args = [];
}

ArrayArgument.prototype = Object.create(Argument.prototype);

ArrayArgument.prototype.type = 'ArrayArgument';

ArrayArgument.prototype.addArgument = function(arg) {
  this.args.push(arg);
};

ArrayArgument.prototype.addArguments = function(args) {
  Array.prototype.push.apply(this.args, args);
};

ArrayArgument.prototype.getArguments = function() {
  return this.args;
};

ArrayArgument.prototype.assign = function(assignment) {
  this.args.forEach(function(arg) {
    arg.assign(assignment);
  }, this);

  this.assignment = assignment;
};

ArrayArgument.prototype.getArgs = function() {
  return this.args;
};

ArrayArgument.prototype.equals = function(that) {
  if (this === that) {
    return true;
  }
  if (that == null) {
    return false;
  }

  if (!(that.type === 'ArrayArgument')) {
    return false;
  }

  if (this.args.length !== that.args.length) {
    return false;
  }

  for (var i = 0; i < this.args.length; i++) {
    if (!this.args[i].equals(that.args[i])) {
      return false;
    }
  }

  return true;
};

/**
 * Helper when we're putting arguments back together
 */
ArrayArgument.prototype.toString = function() {
  return '{' + this.args.map(function(arg) {
    return arg.toString();
  }, this).join(',') + '}';
};

exports.ArrayArgument = ArrayArgument;


});
/*
 * Copyright 2012, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

define('gcli/types/selection', ['require', 'exports', 'module' , 'util/promise', 'util/util', 'util/l10n', 'gcli/types', 'gcli/types/spell', 'gcli/argument'], function(require, exports, module) {

'use strict';

var Promise = require('util/promise');
var util = require('util/util');
var l10n = require('util/l10n');
var types = require('gcli/types');
var Type = require('gcli/types').Type;
var Status = require('gcli/types').Status;
var Conversion = require('gcli/types').Conversion;
var spell = require('gcli/types/spell');
var BlankArgument = require('gcli/argument').BlankArgument;


/**
 * Registration and de-registration.
 */
exports.startup = function() {
  types.addType(SelectionType);
};

exports.shutdown = function() {
  types.removeType(SelectionType);
};


/**
 * A selection allows the user to pick a value from known set of options.
 * An option is made up of a name (which is what the user types) and a value
 * (which is passed to exec)
 * @param typeSpec Object containing properties that describe how this
 * selection functions. Properties include:
 * - lookup: An array of objects, one for each option, which contain name and
 *   value properties. lookup can be a function which returns this array
 * - data: An array of strings - alternative to 'lookup' where the valid values
 *   are strings. i.e. there is no mapping between what is typed and the value
 *   that is used by the program
 * - stringifyProperty: Conversion from value to string is generally a process
 *   of looking through all the valid options for a matching value, and using
 *   the associated name. However the name maybe available directly from the
 *   value using a property lookup. Setting 'stringifyProperty' allows
 *   SelectionType to take this shortcut.
 * - cacheable: If lookup is a function, then we normally assume that
 *   the values fetched can change. Setting 'cacheable:true' enables internal
 *   caching.
 * - neverForceAsync: It's useful for testing purposes to be able to force all
 *   selection types to be asynchronous. This flag prevents that happening for
 *   types that are fundamentally synchronous.
 */
function SelectionType(typeSpec) {
  if (typeSpec) {
    Object.keys(typeSpec).forEach(function(key) {
      this[key] = typeSpec[key];
    }, this);
  }
}

SelectionType.prototype = Object.create(Type.prototype);

SelectionType.prototype.stringify = function(value, context) {
  if (value == null) {
    return '';
  }
  if (this.stringifyProperty != null) {
    return value[this.stringifyProperty];
  }

  try {
    var name = null;
    var lookup = util.synchronize(this.getLookup());
    lookup.some(function(item) {
      if (item.value === value) {
        name = item.name;
        return true;
      }
      return false;
    }, this);
    return name;
  }
  catch (ex) {
    // Types really need to ensure stringify can happen synchronously
    // which means using stringifyProperty if getLookup is asynchronous, but
    // if this fails we need a bailout ...
    return value.toString();
  }
};

/**
 * If typeSpec contained cacheable:true then calls to parse() work on cached
 * data. clearCache() enables the cache to be cleared.
 */
SelectionType.prototype.clearCache = function() {
  delete this._cachedLookup;
};

/**
 * There are several ways to get selection data. This unifies them into one
 * single function.
 * @return An array of objects with name and value properties.
 */
SelectionType.prototype.getLookup = function() {
  if (this._cachedLookup != null) {
    return this._cachedLookup;
  }

  var reply;
  if (this.lookup == null) {
    reply = resolve(this.data, this.neverForceAsync).then(dataToLookup);
  }
  else {
    var lookup = (typeof this.lookup === 'function') ?
            this.lookup.bind(this) :
            this.lookup;

    reply = resolve(lookup, this.neverForceAsync);
  }

  if (this.cacheable && !forceAsync) {
    this._cachedLookup = reply;
  }

  return reply;
};

var forceAsync = false;

/**
 * Both 'lookup' and 'data' properties (see docs on SelectionType constructor)
 * in addition to being real data can be a function or a promise, or even a
 * function which returns a promise of real data, etc. This takes a thing and
 * returns a promise of actual values.
 */
function resolve(thing, neverForceAsync) {
  if (forceAsync && !neverForceAsync) {
    var deferred = Promise.defer();
    setTimeout(function() {
      Promise.resolve(thing).then(function(resolved) {
        if (typeof resolved === 'function') {
          resolved = resolve(resolved(), neverForceAsync);
        }

        deferred.resolve(resolved);
      });
    }, 500);
    return deferred.promise;
  }

  return Promise.resolve(thing).then(function(resolved) {
    if (typeof resolved === 'function') {
      return resolve(resolved(), neverForceAsync);
    }
    return resolved;
  });
}

/**
 * Selection can be provided with either a lookup object (in the 'lookup'
 * property) or an array of strings (in the 'data' property). Internally we
 * always use lookup, so we need a way to convert a 'data' array to a lookup.
 */
function dataToLookup(data) {
  if (!Array.isArray(data)) {
    throw new Error('SelectionType has no lookup or data');
  }

  return data.map(function(option) {
    return { name: option, value: option };
  }, this);
};

/**
 * Return a list of possible completions for the given arg.
 * @param arg The initial input to match
 * @return A trimmed array of string:value pairs
 */
SelectionType.prototype._findPredictions = function(arg) {
  return Promise.resolve(this.getLookup()).then(function(lookup) {
    var predictions = [];
    var i, option;
    var maxPredictions = Conversion.maxPredictions;
    var match = arg.text.toLowerCase();

    // If the arg has a suffix then we're kind of 'done'. Only an exact match
    // will do.
    if (arg.suffix.length > 0) {
      for (i = 0; i < lookup.length && predictions.length < maxPredictions; i++) {
        option = lookup[i];
        if (option.name === arg.text) {
          this._addToPredictions(predictions, option, arg);
        }
      }

      return predictions;
    }

    // Cache lower case versions of all the option names
    for (i = 0; i < lookup.length; i++) {
      option = lookup[i];
      if (option._gcliLowerName == null) {
        option._gcliLowerName = option.name.toLowerCase();
      }
    }

    // Exact hidden matches. If 'hidden: true' then we only allow exact matches
    // All the tests after here check that !option.value.hidden
    for (i = 0; i < lookup.length && predictions.length < maxPredictions; i++) {
      option = lookup[i];
      if (option.name === arg.text) {
        this._addToPredictions(predictions, option, arg);
      }
    }

    // Start with prefix matching
    for (i = 0; i < lookup.length && predictions.length < maxPredictions; i++) {
      option = lookup[i];
      if (option._gcliLowerName.indexOf(match) === 0 && !option.value.hidden) {
        if (predictions.indexOf(option) === -1) {
          this._addToPredictions(predictions, option, arg);
        }
      }
    }

    // Try infix matching if we get less half max matched
    if (predictions.length < (maxPredictions / 2)) {
      for (i = 0; i < lookup.length && predictions.length < maxPredictions; i++) {
        option = lookup[i];
        if (option._gcliLowerName.indexOf(match) !== -1 && !option.value.hidden) {
          if (predictions.indexOf(option) === -1) {
            this._addToPredictions(predictions, option, arg);
          }
        }
      }
    }

    // Try fuzzy matching if we don't get a prefix match
    if (predictions.length === 0) {
      var names = [];
      lookup.forEach(function(opt) {
        if (!opt.value.hidden) {
          names.push(opt.name);
        }
      });
      var corrected = spell.correct(match, names);
      if (corrected) {
        lookup.forEach(function(opt) {
          if (opt.name === corrected) {
            predictions.push(opt);
          }
        }, this);
      }
    }

    return predictions;
  }.bind(this));
};

/**
 * Add an option to our list of predicted options.
 * We abstract out this portion of _findPredictions() because CommandType needs
 * to make an extra check before actually adding which SelectionType does not
 * need to make.
 */
SelectionType.prototype._addToPredictions = function(predictions, option, arg) {
  predictions.push(option);
};

SelectionType.prototype.parse = function(arg, context) {
  return this._findPredictions(arg).then(function(predictions) {
    if (predictions.length === 0) {
      var msg = l10n.lookupFormat('typesSelectionNomatch', [ arg.text ]);
      return new Conversion(undefined, arg, Status.ERROR, msg,
                            Promise.resolve(predictions));
    }

    if (predictions[0].name === arg.text) {
      var value = predictions[0].value;
      return new Conversion(value, arg, Status.VALID, '',
                            Promise.resolve(predictions));
    }

    return new Conversion(undefined, arg, Status.INCOMPLETE, '',
                          Promise.resolve(predictions));
  }.bind(this));
};

SelectionType.prototype.getBlank = function() {
  var predictFunc = function() {
    return Promise.resolve(this.getLookup()).then(function(lookup) {
      return lookup.filter(function(option) {
        return !option.value.hidden;
      }).slice(0, Conversion.maxPredictions - 1);
    });
  }.bind(this);

  return new Conversion(undefined, new BlankArgument(), Status.INCOMPLETE, '',
                        predictFunc);
};

/**
 * For selections, up is down and black is white. It's like this, given a list
 * [ a, b, c, d ], it's natural to think that it starts at the top and that
 * going up the list, moves towards 'a'. However 'a' has the lowest index, so
 * for SelectionType, up is down and down is up.
 * Sorry.
 */
SelectionType.prototype.decrement = function(value, context) {
  var lookup = util.synchronize(this.getLookup());
  var index = this._findValue(lookup, value);
  if (index === -1) {
    index = 0;
  }
  index++;
  if (index >= lookup.length) {
    index = 0;
  }
  return lookup[index].value;
};

/**
 * See note on SelectionType.decrement()
 */
SelectionType.prototype.increment = function(value, context) {
  var lookup = util.synchronize(this.getLookup());
  var index = this._findValue(lookup, value);
  if (index === -1) {
    // For an increment operation when there is nothing to start from, we
    // want to start from the top, i.e. index 0, so the value before we
    // 'increment' (see note above) must be 1.
    index = 1;
  }
  index--;
  if (index < 0) {
    index = lookup.length - 1;
  }
  return lookup[index].value;
};

/**
 * Walk through an array of { name:.., value:... } objects looking for a
 * matching value (using strict equality), returning the matched index (or -1
 * if not found).
 * @param lookup Array of objects with name/value properties to search through
 * @param value The value to search for
 * @return The index at which the match was found, or -1 if no match was found
 */
SelectionType.prototype._findValue = function(lookup, value) {
  var index = -1;
  for (var i = 0; i < lookup.length; i++) {
    var pair = lookup[i];
    if (pair.value === value) {
      index = i;
      break;
    }
  }
  return index;
};

/**
 * SelectionType is designed to be inherited from, so SelectionField needs a way
 * to check if something works like a selection without using 'name'
 */
SelectionType.prototype.isSelection = true;

SelectionType.prototype.name = 'selection';

exports.SelectionType = SelectionType;


});
/*
 * Copyright 2012, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

define('gcli/types/spell', ['require', 'exports', 'module' ], function(require, exports, module) {

'use strict';

/*
 * A spell-checker based on Damerau-Levenshtein distance.
 */

var INSERTION_COST = 1;
var DELETION_COST = 1;
var SWAP_COST = 1;
var SUBSTITUTION_COST = 2;
var MAX_EDIT_DISTANCE = 4;

/**
 * Compute Damerau-Levenshtein Distance
 * @see http://en.wikipedia.org/wiki/Damerau%E2%80%93Levenshtein_distance
 */
function damerauLevenshteinDistance(wordi, wordj) {
  var wordiLen = wordi.length;
  var wordjLen = wordj.length;

  // We only need to store three rows of our dynamic programming matrix.
  // (Without swap, it would have been two.)
  var row0 = new Array(wordiLen+1);
  var row1 = new Array(wordiLen+1);
  var row2 = new Array(wordiLen+1);
  var tmp;

  var i, j;

  // The distance between the empty string and a string of size i is the cost
  // of i insertions.
  for (i = 0; i <= wordiLen; i++) {
    row1[i] = i * INSERTION_COST;
  }

  // Row-by-row, we're computing the edit distance between substrings wordi[0..i]
  // and wordj[0..j].
  for (j = 1; j <= wordjLen; j++)
  {
    // Edit distance between wordi[0..0] and wordj[0..j] is the cost of j
    // insertions.
    row0[0] = j * INSERTION_COST;

    for (i = 1; i <= wordiLen; i++) {
      // Handle deletion, insertion and substitution: we can reach each cell
      // from three other cells corresponding to those three operations. We
      // want the minimum cost.
      row0[i] = Math.min(
          row0[i-1] + DELETION_COST,
          row1[i] + INSERTION_COST,
          row1[i-1] + (wordi[i-1] === wordj[j-1] ? 0 : SUBSTITUTION_COST));
      // We handle swap too, eg. distance between help and hlep should be 1. If
      // we find such a swap, there's a chance to update row0[1] to be lower.
      if (i > 1 && j > 1 && wordi[i-1] === wordj[j-2] && wordj[j-1] === wordi[i-2]) {
        row0[i] = Math.min(row0[i], row2[i-2] + SWAP_COST);
      }
    }

    tmp = row2;
    row2 = row1;
    row1 = row0;
    row0 = tmp;
  }

  return row1[wordiLen];
}

/**
 * A function that returns the correction for the specified word.
 */
exports.correct = function(word, names) {
  if (names.length === 0) {
    return undefined;
  }

  var distance = {};
  var sortedCandidates;

  names.forEach(function(candidate) {
    distance[candidate] = damerauLevenshteinDistance(word, candidate);
  });

  sortedCandidates = names.sort(function(worda, wordb) {
    if (distance[worda] !== distance[wordb]) {
      return distance[worda] - distance[wordb];
    }
    else {
      // if the score is the same, always return the first string
      // in the lexicographical order
      return worda < wordb;
    }
  });

  if (distance[sortedCandidates[0]] <= MAX_EDIT_DISTANCE) {
    return sortedCandidates[0];
  }
  else {
    return undefined;
  }
};


});
/*
 * Copyright 2012, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

define('gcli/types/command', ['require', 'exports', 'module' , 'util/promise', 'util/l10n', 'gcli/canon', 'gcli/types', 'gcli/types/selection'], function(require, exports, module) {

'use strict';

var Promise = require('util/promise');
var l10n = require('util/l10n');
var canon = require('gcli/canon');
var types = require('gcli/types');
var SelectionType = require('gcli/types/selection').SelectionType;
var Status = require('gcli/types').Status;
var Conversion = require('gcli/types').Conversion;


/**
 * Registration and de-registration.
 */
exports.startup = function() {
  types.addType(CommandType);
  types.addType(ParamType);
};

exports.shutdown = function() {
  types.removeType(CommandType);
  types.removeType(ParamType);
};


/**
 * Select from the available commands.
 * This is very similar to a SelectionType, however the level of hackery in
 * SelectionType to make it handle Commands correctly was to high, so we
 * simplified.
 * If you are making changes to this code, you should check there too.
 */
function ParamType(typeSpec) {
  this.requisition = typeSpec.requisition;
  this.isIncompleteName = typeSpec.isIncompleteName;
  this.stringifyProperty = 'name';
  this.neverForceAsync = true;
}

ParamType.prototype = Object.create(SelectionType.prototype);

ParamType.prototype.name = 'param';

ParamType.prototype.lookup = function() {
  var displayedParams = [];
  var command = this.requisition.commandAssignment.value;
  if (command != null) {
    command.params.forEach(function(param) {
      var arg = this.requisition.getAssignment(param.name).arg;
      if (!param.isPositionalAllowed && arg.type === "BlankArgument") {
        displayedParams.push({ name: '--' + param.name, value: param });
      }
    }, this);
  }
  return displayedParams;
};

ParamType.prototype.parse = function(arg, context) {
  if (this.isIncompleteName) {
    return SelectionType.prototype.parse.call(this, arg, context);
  }
  else {
    var message = l10n.lookup('cliUnusedArg');
    return Promise.resolve(new Conversion(undefined, arg, Status.ERROR, message));
  }
};


/**
 * Select from the available commands.
 * This is very similar to a SelectionType, however the level of hackery in
 * SelectionType to make it handle Commands correctly was to high, so we
 * simplified.
 * If you are making changes to this code, you should check there too.
 */
function CommandType() {
  this.stringifyProperty = 'name';
  this.neverForceAsync = true;
}

CommandType.prototype = Object.create(SelectionType.prototype);

CommandType.prototype.name = 'command';

CommandType.prototype.lookup = function() {
  var commands = canon.getCommands();
  commands.sort(function(c1, c2) {
    return c1.name.localeCompare(c2.name);
  });
  return commands.map(function(command) {
    return { name: command.name, value: command };
  }, this);
};

/**
 * Add an option to our list of predicted options
 */
CommandType.prototype._addToPredictions = function(predictions, option, arg) {
  // The command type needs to exclude sub-commands when the CLI
  // is blank, but include them when we're filtering. This hack
  // excludes matches when the filter text is '' and when the
  // name includes a space.
  if (arg.text.length !== 0 || option.name.indexOf(' ') === -1) {
    predictions.push(option);
  }
};

CommandType.prototype.parse = function(arg, context) {
  // Especially at startup, predictions live over the time that things change
  // so we provide a completion function rather than completion values
  var predictFunc = function() {
    return this._findPredictions(arg);
  }.bind(this);

  return this._findPredictions(arg).then(function(predictions) {
    if (predictions.length === 0) {
      var msg = l10n.lookupFormat('typesSelectionNomatch', [ arg.text ]);
      return new Conversion(undefined, arg, Status.ERROR, msg, predictFunc);
    }

    var command = predictions[0].value;

    if (predictions.length === 1) {
      // Is it an exact match of an executable command,
      // or just the only possibility?
      if (command.name === arg.text && typeof command.exec === 'function') {
        return new Conversion(command, arg, Status.VALID, '');
      }

      return new Conversion(undefined, arg, Status.INCOMPLETE, '', predictFunc);
    }

    // It's valid if the text matches, even if there are several options
    if (predictions[0].name === arg.text) {
      return new Conversion(command, arg, Status.VALID, '', predictFunc);
    }

    return new Conversion(undefined, arg, Status.INCOMPLETE, '', predictFunc);
  }.bind(this));
};


});
/*
 * Copyright 2012, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

define('gcli/canon', ['require', 'exports', 'module' , 'util/promise', 'util/util', 'util/l10n', 'gcli/types'], function(require, exports, module) {

'use strict';

var Promise = require('util/promise');
var util = require('util/util');
var l10n = require('util/l10n');

var types = require('gcli/types');
var Status = require('gcli/types').Status;

/**
 * Implement the localization algorithm for any documentation objects (i.e.
 * description and manual) in a command.
 * @param data The data assigned to a description or manual property
 * @param onUndefined If data == null, should we return the data untouched or
 * lookup a 'we don't know' key in it's place.
 */
function lookup(data, onUndefined) {
  if (data == null) {
    if (onUndefined) {
      return l10n.lookup(onUndefined);
    }

    return data;
  }

  if (typeof data === 'string') {
    return data;
  }

  if (typeof data === 'object') {
    if (data.key) {
      return l10n.lookup(data.key);
    }

    var locales = l10n.getPreferredLocales();
    var translated;
    locales.some(function(locale) {
      translated = data[locale];
      return translated != null;
    });
    if (translated != null) {
      return translated;
    }

    console.error('Can\'t find locale in descriptions: ' +
            'locales=' + JSON.stringify(locales) + ', ' +
            'description=' + JSON.stringify(data));
    return '(No description)';
  }

  return l10n.lookup(onUndefined);
}


/**
 * The command object is mostly just setup around a commandSpec (as passed to
 * #addCommand()).
 */
function Command(commandSpec) {
  Object.keys(commandSpec).forEach(function(key) {
    this[key] = commandSpec[key];
  }, this);

  if (!this.name) {
    throw new Error('All registered commands must have a name');
  }

  if (this.params == null) {
    this.params = [];
  }
  if (!Array.isArray(this.params)) {
    throw new Error('command.params must be an array in ' + this.name);
  }

  this.hasNamedParameters = false;
  this.description = 'description' in this ? this.description : undefined;
  this.description = lookup(this.description, 'canonDescNone');
  this.manual = 'manual' in this ? this.manual : undefined;
  this.manual = lookup(this.manual);

  // At this point this.params has nested param groups. We want to flatten it
  // out and replace the param object literals with Parameter objects
  var paramSpecs = this.params;
  this.params = [];

  // Track if the user is trying to mix default params and param groups.
  // All the non-grouped parameters must come before all the param groups
  // because non-grouped parameters can be assigned positionally, so their
  // index is important. We don't want 'holes' in the order caused by
  // parameter groups.
  var usingGroups = false;

  // In theory this could easily be made recursive, so param groups could
  // contain nested param groups. Current thinking is that the added
  // complexity for the UI probably isn't worth it, so this implementation
  // prevents nesting.
  paramSpecs.forEach(function(spec) {
    if (!spec.group) {
      var param = new Parameter(spec, this, null);
      this.params.push(param);

      if (!param.isPositionalAllowed) {
        this.hasNamedParameters = true;
      }

      if (usingGroups && param.groupName == null) {
        throw new Error('Parameters can\'t come after param groups.' +
                        ' Ignoring ' + this.name + '/' + spec.name);
      }

      if (param.groupName != null) {
        usingGroups = true;
      }
    }
    else {
      spec.params.forEach(function(ispec) {
        var param = new Parameter(ispec, this, spec.group);
        this.params.push(param);

        if (!param.isPositionalAllowed) {
          this.hasNamedParameters = true;
        }
      }, this);

      usingGroups = true;
    }
  }, this);
}

exports.Command = Command;


/**
 * A wrapper for a paramSpec so we can sort out shortened versions names for
 * option switches
 */
function Parameter(paramSpec, command, groupName) {
  this.command = command || { name: 'unnamed' };
  this.paramSpec = paramSpec;
  this.name = this.paramSpec.name;
  this.type = this.paramSpec.type;

  this.groupName = groupName;
  if (this.groupName != null) {
    if (this.paramSpec.option != null) {
      throw new Error('Can\'t have a "option" property in a nested parameter');
    }
  }
  else {
    if (this.paramSpec.option != null) {
      this.groupName = this.paramSpec.option === true ?
              l10n.lookup('canonDefaultGroupName') :
              '' + this.paramSpec.option;
    }
  }

  if (!this.name) {
    throw new Error('In ' + this.command.name +
                    ': all params must have a name');
  }

  var typeSpec = this.type;
  this.type = types.createType(typeSpec);
  if (this.type == null) {
    console.error('Known types: ' + types.getTypeNames().join(', '));
    throw new Error('In ' + this.command.name + '/' + this.name +
                    ': can\'t find type for: ' + JSON.stringify(typeSpec));
  }

  // boolean parameters have an implicit defaultValue:false, which should
  // not be changed. See the docs.
  if (this.type.name === 'boolean' &&
      this.paramSpec.defaultValue !== undefined) {
    throw new Error('In ' + this.command.name + '/' + this.name +
                    ': boolean parameters can not have a defaultValue.' +
                    ' Ignoring');
  }

  // Check the defaultValue for validity.
  // Both undefined and null get a pass on this test. undefined is used when
  // there is no defaultValue, and null is used when the parameter is
  // optional, neither are required to parse and stringify.
  if (this._defaultValue != null) {
    try {
      // Passing null in for a context is bound to get us into trouble some day
      // in which case we'll need to mock one up in some way
      var context = null;
      var defaultText = this.type.stringify(this.paramSpec.defaultValue, context);
      var parsed = this.type.parseString(defaultText, context);
      parsed.then(function(defaultConversion) {
        if (defaultConversion.getStatus() !== Status.VALID) {
          console.error('In ' + this.command.name + '/' + this.name +
                        ': Error round tripping defaultValue. status = ' +
                        defaultConversion.getStatus());
        }
      }.bind(this), util.errorHandler);
    }
    catch (ex) {
      throw new Error('In ' + this.command.name + '/' + this.name + ': ' + ex);
    }
  }

  // All parameters that can only be set via a named parameter must have a
  // non-undefined default value
  if (!this.isPositionalAllowed && this.paramSpec.defaultValue === undefined &&
      this.type.getBlank == null && !(this.type.name === 'boolean')) {
    throw new Error('In ' + this.command.name + '/' + this.name +
                    ': Missing defaultValue for optional parameter.');
  }
}

/**
 * type.getBlank can be expensive, so we delay execution where we can
 */
Object.defineProperty(Parameter.prototype, 'defaultValue', {
  get: function() {
    if (!('_defaultValue' in this)) {
      this._defaultValue = (this.paramSpec.defaultValue !== undefined) ?
          this.paramSpec.defaultValue :
          this.type.getBlank().value;
    }

    return this._defaultValue;
  },
  enumerable : true
});

/**
 * Does the given name uniquely identify this param (among the other params
 * in this command)
 * @param name The name to check
 */
Parameter.prototype.isKnownAs = function(name) {
  if (name === '--' + this.name) {
    return true;
  }
  return false;
};

/**
 * Read the default value for this parameter either from the parameter itself
 * (if this function has been over-ridden) or from the type, or from calling
 * parseString on an empty string
 */
Parameter.prototype.getBlank = function() {
  return this.type.getBlank();
};

/**
 * Resolve the manual for this parameter, by looking in the paramSpec
 * and doing a l10n lookup
 */
Object.defineProperty(Parameter.prototype, 'manual', {
  get: function() {
    return lookup(this.paramSpec.manual || undefined);
  },
  enumerable: true
});

/**
 * Resolve the description for this parameter, by looking in the paramSpec
 * and doing a l10n lookup
 */
Object.defineProperty(Parameter.prototype, 'description', {
  get: function() {
    return lookup(this.paramSpec.description || undefined, 'canonDescNone');
  },
  enumerable: true
});

/**
 * Is the user required to enter data for this parameter? (i.e. has
 * defaultValue been set to something other than undefined)
 */
Object.defineProperty(Parameter.prototype, 'isDataRequired', {
  get: function() {
    return this.defaultValue === undefined;
  },
  enumerable: true
});

/**
 * Reflect the paramSpec 'hidden' property (dynamically so it can change)
 */
Object.defineProperty(Parameter.prototype, 'hidden', {
  get: function() {
    return this.paramSpec.hidden;
  },
  enumerable: true
});

/**
 * Are we allowed to assign data to this parameter using positional
 * parameters?
 */
Object.defineProperty(Parameter.prototype, 'isPositionalAllowed', {
  get: function() {
    return this.groupName == null;
  },
  enumerable: true
});

exports.Parameter = Parameter;


/**
 * A canon is a store for a list of commands
 */
function Canon() {
  // A lookup hash of our registered commands
  this._commands = {};
  // A sorted list of command names, we regularly want them in order, so pre-sort
  this._commandNames = [];
  // A lookup of the original commandSpecs by command name
  this._commandSpecs = {};

  // Enable people to be notified of changes to the list of commands
  this.onCanonChange = util.createEvent('canon.onCanonChange');
}

/**
 * Add a command to the canon of known commands.
 * This function is exposed to the outside world (via gcli/index). It is
 * documented in docs/index.md for all the world to see.
 * @param commandSpec The command and its metadata.
 * @return The new command
 */
Canon.prototype.addCommand = function(commandSpec) {
  if (this._commands[commandSpec.name] != null) {
    // Roughly canon.removeCommand() without the event call, which we do later
    delete this._commands[commandSpec.name];
    this._commandNames = this._commandNames.filter(function(test) {
      return test !== commandSpec.name;
    });
  }

  var command = new Command(commandSpec);
  this._commands[commandSpec.name] = command;
  this._commandNames.push(commandSpec.name);
  this._commandNames.sort();

  this._commandSpecs[commandSpec.name] = commandSpec;

  this.onCanonChange();
  return command;
};

/**
 * Remove an individual command. The opposite of #addCommand().
 * Removing a non-existent command is a no-op.
 * @param commandOrName Either a command name or the command itself.
 * @return true if a command was removed, false otherwise.
 */
Canon.prototype.removeCommand = function(commandOrName) {
  var name = typeof commandOrName === 'string' ?
          commandOrName :
          commandOrName.name;

  if (!this._commands[name]) {
    return false;
  }

  // See start of canon.addCommand if changing this code
  delete this._commands[name];
  delete this._commandSpecs[name];
  this._commandNames = this._commandNames.filter(function(test) {
    return test !== name;
  });

  this.onCanonChange();
  return true;
};

/**
 * Retrieve a command by name
 * @param name The name of the command to retrieve
 */
Canon.prototype.getCommand = function(name) {
  // '|| undefined' is to silence 'reference to undefined property' warnings
  return this._commands[name] || undefined;
};

/**
 * Get an array of all the registered commands.
 */
Canon.prototype.getCommands = function() {
  return Object.keys(this._commands).map(function(name) {
    return this._commands[name];
  }, this);
};

/**
 * Get an array containing the names of the registered commands.
 */
Canon.prototype.getCommandNames = function() {
  return this._commandNames.slice(0);
};

/**
 * Get access to the stored commandMetaDatas (i.e. before they were made into
 * instances of Command/Parameters) so we can remote them.
 */
Canon.prototype.getCommandSpecs = function() {
  var specs = {};

  Object.keys(this._commandSpecs).forEach(function(name) {
    var spec = this._commandSpecs[name];
    if (spec.exec == null) {
      spec.isParent = true;
    }
    specs[name] = spec;
  }.bind(this));

  return specs;
};

/**
 * Add a set of commands that are executed somewhere else.
 * @param prefix The name prefix that we assign to all command names
 * @param commandSpecs Presumably as obtained from getCommandSpecs on remote
 * @param remoter Function to call on exec of a new remote command. This is
 * defined just like an exec function (i.e. that takes args/context as params
 * and returns a promise) with one extra feature, that the context includes a
 * 'commandName' property that contains the original command name.
 * @param to URL-like string that describes where the commands are executed.
 * This is to complete the parent command description.
 */
Canon.prototype.addProxyCommands = function(prefix, commandSpecs, remoter, to) {
  var names = Object.keys(commandSpecs);

  if (this._commands[prefix] != null) {
    throw new Error(l10n.lookupFormat('canonProxyExists', [ prefix ]));
  }

  // We need to add the parent command so all the commands from the other
  // system have a parent
  this.addCommand({
    name: prefix,
    isProxy: true,
    description: l10n.lookupFormat('canonProxyDesc', [ to ]),
    manual: l10n.lookupFormat('canonProxyManual', [ to ])
  });

  names.forEach(function(name) {
    var commandSpec = commandSpecs[name];

    if (!commandSpec.isParent) {
      commandSpec.exec = function(args, context) {
        context.commandName = name;
        return remoter(args, context);
      }.bind(this);
    }

    commandSpec.name = prefix + ' ' + commandSpec.name;
    commandSpec.isProxy = true;
    this.addCommand(commandSpec);
  }.bind(this));
};

/**
 * Add a set of commands that are executed somewhere else.
 * @param prefix The name prefix that we assign to all command names
 * @param commandSpecs Presumably as obtained from getCommandSpecs on remote
 * @param remoter Function to call on exec of a new remote command. This is
 * defined just like an exec function (i.e. that takes args/context as params
 * and returns a promise) with one extra feature, that the context includes a
 * 'commandName' property that contains the original command name.
 * @param to URL-like string that describes where the commands are executed.
 * This is to complete the parent command description.
 */
Canon.prototype.removeProxyCommands = function(prefix) {
  var toRemove = [];
  Object.keys(this._commandSpecs).forEach(function(name) {
    if (name.indexOf(prefix) === 0) {
      toRemove.push(name);
    }
  }.bind(this));

  var removed = [];
  toRemove.forEach(function(name) {
    var command = this.getCommand(name);
    if (command.isProxy) {
      this.removeCommand(name);
      removed.push(name);
    }
    else {
      console.error('Skipping removal of \'' + name +
                    '\' because it is not a proxy command.');
    }
  }.bind(this));

  return removed;
};

var canon = new Canon();

exports.Canon = Canon;
exports.addCommand = canon.addCommand.bind(canon);
exports.removeCommand = canon.removeCommand.bind(canon);
exports.onCanonChange = canon.onCanonChange;
exports.getCommands = canon.getCommands.bind(canon);
exports.getCommand = canon.getCommand.bind(canon);
exports.getCommandNames = canon.getCommandNames.bind(canon);
exports.getCommandSpecs = canon.getCommandSpecs.bind(canon);
exports.addProxyCommands = canon.addProxyCommands.bind(canon);
exports.removeProxyCommands = canon.removeProxyCommands.bind(canon);

/**
 * CommandOutputManager stores the output objects generated by executed
 * commands.
 *
 * CommandOutputManager is exposed to the the outside world and could (but
 * shouldn't) be used before gcli.startup() has been called.
 * This could should be defensive to that where possible, and we should
 * certainly document if the use of it or similar will fail if used too soon.
 */
function CommandOutputManager() {
  this.onOutput = util.createEvent('CommandOutputManager.onOutput');
}

exports.CommandOutputManager = CommandOutputManager;


});
/*
 * Copyright 2012, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

define('gcli/types/javascript', ['require', 'exports', 'module' , 'util/promise', 'util/l10n', 'gcli/types'], function(require, exports, module) {

'use strict';

var Promise = require('util/promise');
var l10n = require('util/l10n');
var types = require('gcli/types');

var Conversion = types.Conversion;
var Type = types.Type;
var Status = types.Status;


/**
 * Registration and de-registration.
 */
exports.startup = function() {
  types.addType(JavascriptType);
};

exports.shutdown = function() {
  types.removeType(JavascriptType);
};

/**
 * The object against which we complete, which is usually 'window' if it exists
 * but could be something else in non-web-content environments.
 */
var globalObject = undefined;
if (typeof window !== 'undefined') {
  globalObject = window;
}

/**
 * Setter for the object against which JavaScript completions happen
 */
exports.setGlobalObject = function(obj) {
  globalObject = obj;
};

/**
 * Getter for the object against which JavaScript completions happen, for use
 * in testing
 */
exports.getGlobalObject = function() {
  return globalObject;
};

/**
 * Remove registration of object against which JavaScript completions happen
 */
exports.unsetGlobalObject = function() {
  globalObject = undefined;
};


/**
 * 'javascript' handles scripted input
 */
function JavascriptType(typeSpec) {
}

JavascriptType.prototype = Object.create(Type.prototype);

JavascriptType.prototype.stringify = function(value, context) {
  if (value == null) {
    return '';
  }
  return value;
};

/**
 * When sorting out completions, there is no point in displaying millions of
 * matches - this the number of matches that we aim for
 */
JavascriptType.MAX_COMPLETION_MATCHES = 10;

JavascriptType.prototype.parse = function(arg, context) {
  var typed = arg.text;
  var scope = globalObject;

  // No input is undefined
  if (typed === '') {
    return Promise.resolve(new Conversion(undefined, arg, Status.INCOMPLETE));
  }
  // Just accept numbers
  if (!isNaN(parseFloat(typed)) && isFinite(typed)) {
    return Promise.resolve(new Conversion(typed, arg));
  }
  // Just accept constants like true/false/null/etc
  if (typed.trim().match(/(null|undefined|NaN|Infinity|true|false)/)) {
    return Promise.resolve(new Conversion(typed, arg));
  }

  // Analyze the input text and find the beginning of the last part that
  // should be completed.
  var beginning = this._findCompletionBeginning(typed);

  // There was an error analyzing the string.
  if (beginning.err) {
    return Promise.resolve(new Conversion(typed, arg, Status.ERROR, beginning.err));
  }

  // If the current state is ParseState.COMPLEX, then we can't do completion.
  // so bail out now
  if (beginning.state === ParseState.COMPLEX) {
    return Promise.resolve(new Conversion(typed, arg));
  }

  // If the current state is not ParseState.NORMAL, then we are inside of a
  // string which means that no completion is possible.
  if (beginning.state !== ParseState.NORMAL) {
    return Promise.resolve(new Conversion(typed, arg, Status.INCOMPLETE, ''));
  }

  var completionPart = typed.substring(beginning.startPos);
  var properties = completionPart.split('.');
  var matchProp;
  var prop = undefined;

  if (properties.length > 1) {
    matchProp = properties.pop().trimLeft();
    for (var i = 0; i < properties.length; i++) {
      prop = properties[i].trim();

      // We can't complete on null.foo, so bail out
      if (scope == null) {
        return Promise.resolve(new Conversion(typed, arg, Status.ERROR,
                                        l10n.lookup('jstypeParseScope')));
      }

      if (prop === '') {
        return Promise.resolve(new Conversion(typed, arg, Status.INCOMPLETE, ''));
      }

      // Check if prop is a getter function on 'scope'. Functions can change
      // other stuff so we can't execute them to get the next object. Stop here.
      if (this._isSafeProperty(scope, prop)) {
        return Promise.resolve(new Conversion(typed, arg));
      }

      try {
        scope = scope[prop];
      }
      catch (ex) {
        // It would be nice to be able to report this error in some way but
        // as it can happen just when someone types '{sessionStorage.', it
        // almost doesn't really count as an error, so we ignore it
        return Promise.resolve(new Conversion(typed, arg, Status.VALID, ''));
      }
    }
  }
  else {
    matchProp = properties[0].trimLeft();
  }

  // If the reason we just stopped adjusting the scope was a non-simple string,
  // then we're not sure if the input is valid or invalid, so accept it
  if (prop && !prop.match(/^[0-9A-Za-z]*$/)) {
    return Promise.resolve(new Conversion(typed, arg));
  }

  // However if the prop was a simple string, it is an error
  if (scope == null) {
    var message = l10n.lookupFormat('jstypeParseMissing', [ prop ]);
    return Promise.resolve(new Conversion(typed, arg, Status.ERROR, message));
  }

  // If the thing we're looking for isn't a simple string, then we're not going
  // to find it, but we're not sure if it's valid or invalid, so accept it
  if (!matchProp.match(/^[0-9A-Za-z]*$/)) {
    return Promise.resolve(new Conversion(typed, arg));
  }

  // Skip Iterators and Generators.
  if (this._isIteratorOrGenerator(scope)) {
    return Promise.resolve(new Conversion(typed, arg));
  }

  var matchLen = matchProp.length;
  var prefix = matchLen === 0 ? typed : typed.slice(0, -matchLen);
  var status = Status.INCOMPLETE;
  var message = '';

  // We really want an array of matches (for sorting) but it's easier to
  // detect existing members if we're using a map initially
  var matches = {};

  // We only display a maximum of MAX_COMPLETION_MATCHES, so there is no point
  // in digging up the prototype chain for matches that we're never going to
  // use. Initially look for matches directly on the object itself and then
  // look up the chain to find more
  var distUpPrototypeChain = 0;
  var root = scope;
  try {
    while (root != null &&
        Object.keys(matches).length < JavascriptType.MAX_COMPLETION_MATCHES) {

      Object.keys(root).forEach(function(property) {
        // Only add matching properties. Also, as we're walking up the
        // prototype chain, properties on 'higher' prototypes don't override
        // similarly named properties lower down
        if (property.indexOf(matchProp) === 0 && !(property in matches)) {
          matches[property] = {
            prop: property,
            distUpPrototypeChain: distUpPrototypeChain
          };
        }
      });

      distUpPrototypeChain++;
      root = Object.getPrototypeOf(root);
    }
  }
  catch (ex) {
    return Promise.resolve(new Conversion(typed, arg, Status.INCOMPLETE, ''));
  }

  // Convert to an array for sorting, and while we're at it, note if we got
  // an exact match so we know that this input is valid
  matches = Object.keys(matches).map(function(property) {
    if (property === matchProp) {
      status = Status.VALID;
    }
    return matches[property];
  });

  // The sort keys are:
  // - Being on the object itself, not in the prototype chain
  // - The lack of existence of a vendor prefix
  // - The name
  matches.sort(function(m1, m2) {
    if (m1.distUpPrototypeChain !== m2.distUpPrototypeChain) {
      return m1.distUpPrototypeChain - m2.distUpPrototypeChain;
    }
    // Push all vendor prefixes to the bottom of the list
    return isVendorPrefixed(m1.prop) ?
      (isVendorPrefixed(m2.prop) ? m1.prop.localeCompare(m2.prop) : 1) :
      (isVendorPrefixed(m2.prop) ? -1 : m1.prop.localeCompare(m2.prop));
  });

  // Trim to size. There is a bug for doing a better job of finding matches
  // (bug 682694), but in the mean time there is a performance problem
  // associated with creating a large number of DOM nodes that few people will
  // ever read, so trim ...
  if (matches.length > JavascriptType.MAX_COMPLETION_MATCHES) {
    matches = matches.slice(0, JavascriptType.MAX_COMPLETION_MATCHES - 1);
  }

  // Decorate the matches with:
  // - a description
  // - a value (for the menu) and,
  // - an incomplete flag which reports if we should assume that the user isn't
  //   going to carry on the JS expression with this input so far
  var predictions = matches.map(function(match) {
    var description;
    var incomplete = true;

    if (this._isSafeProperty(scope, match.prop)) {
      description = '(property getter)';
    }
    else {
      try {
        var value = scope[match.prop];

        if (typeof value === 'function') {
          description = '(function)';
        }
        else if (typeof value === 'boolean' || typeof value === 'number') {
          description = '= ' + value;
          incomplete = false;
        }
        else if (typeof value === 'string') {
          if (value.length > 40) {
            value = value.substring(0, 37) + '…';
          }
          description = '= \'' + value + '\'';
          incomplete = false;
        }
        else {
          description = '(' + typeof value + ')';
        }
      }
      catch (ex) {
        description = '(' + l10n.lookup('jstypeParseError') + ')';
      }
    }

    return {
      name: prefix + match.prop,
      value: {
        name: prefix + match.prop,
        description: description
      },
      description: description,
      incomplete: incomplete
    };
  }, this);

  if (predictions.length === 0) {
    status = Status.ERROR;
    message = l10n.lookupFormat('jstypeParseMissing', [ matchProp ]);
  }

  // If the match is the only one possible, and its VALID, predict nothing
  if (predictions.length === 1 && status === Status.VALID) {
    predictions = [];
  }

  return Promise.resolve(new Conversion(typed, arg, status, message,
                                  Promise.resolve(predictions)));
};

/**
 * Does the given property have a prefix that indicates that it is vendor
 * specific?
 */
function isVendorPrefixed(name) {
  return name.indexOf('moz') === 0 ||
         name.indexOf('webkit') === 0 ||
         name.indexOf('ms') === 0;
}

/**
 * Constants used in return value of _findCompletionBeginning()
 */
var ParseState = {
  /**
   * We have simple input like window.foo, without any punctuation that makes
   * completion prediction be confusing or wrong
   */
  NORMAL: 0,

  /**
   * The cursor is in some Javascript that makes completion hard to predict,
   * like console.log(
   */
  COMPLEX: 1,

  /**
   * The cursor is inside single quotes (')
   */
  QUOTE: 2,

  /**
   * The cursor is inside single quotes (")
   */
  DQUOTE: 3
};

var OPEN_BODY = '{[('.split('');
var CLOSE_BODY = '}])'.split('');
var OPEN_CLOSE_BODY = {
  '{': '}',
  '[': ']',
  '(': ')'
};

/**
 * How we distinguish between simple and complex JS input. We attempt
 * completion against simple JS.
 */
var simpleChars = /[a-zA-Z0-9.]/;

/**
 * Analyzes a given string to find the last statement that is interesting for
 * later completion.
 * @param text A string to analyze
 * @return If there was an error in the string detected, then a object like
 *   { err: 'ErrorMesssage' }
 * is returned, otherwise a object like
 *   {
 *     state: ParseState.NORMAL|ParseState.QUOTE|ParseState.DQUOTE,
 *     startPos: index of where the last statement begins
 *   }
 */
JavascriptType.prototype._findCompletionBeginning = function(text) {
  var bodyStack = [];

  var state = ParseState.NORMAL;
  var start = 0;
  var c;
  var complex = false;

  for (var i = 0; i < text.length; i++) {
    c = text[i];
    if (!simpleChars.test(c)) {
      complex = true;
    }

    switch (state) {
      // Normal JS state.
      case ParseState.NORMAL:
        if (c === '"') {
          state = ParseState.DQUOTE;
        }
        else if (c === '\'') {
          state = ParseState.QUOTE;
        }
        else if (c === ';') {
          start = i + 1;
        }
        else if (c === ' ') {
          start = i + 1;
        }
        else if (OPEN_BODY.indexOf(c) != -1) {
          bodyStack.push({
            token: c,
            start: start
          });
          start = i + 1;
        }
        else if (CLOSE_BODY.indexOf(c) != -1) {
          var last = bodyStack.pop();
          if (!last || OPEN_CLOSE_BODY[last.token] != c) {
            return { err: l10n.lookup('jstypeBeginSyntax') };
          }
          if (c === '}') {
            start = i + 1;
          }
          else {
            start = last.start;
          }
        }
        break;

      // Double quote state > " <
      case ParseState.DQUOTE:
        if (c === '\\') {
          i ++;
        }
        else if (c === '\n') {
          return { err: l10n.lookup('jstypeBeginUnterm') };
        }
        else if (c === '"') {
          state = ParseState.NORMAL;
        }
        break;

      // Single quote state > ' <
      case ParseState.QUOTE:
        if (c === '\\') {
          i ++;
        }
        else if (c === '\n') {
          return { err: l10n.lookup('jstypeBeginUnterm') };
        }
        else if (c === '\'') {
          state = ParseState.NORMAL;
        }
        break;
    }
  }

  if (state === ParseState.NORMAL && complex) {
    state = ParseState.COMPLEX;
  }

  return {
    state: state,
    startPos: start
  };
};

/**
 * Return true if the passed object is either an iterator or a generator, and
 * false otherwise
 * @param obj The object to check
 */
JavascriptType.prototype._isIteratorOrGenerator = function(obj) {
  if (obj === null) {
    return false;
  }

  if (typeof aObject === 'object') {
    if (typeof obj.__iterator__ === 'function' ||
        obj.constructor && obj.constructor.name === 'Iterator') {
      return true;
    }

    try {
      var str = obj.toString();
      if (typeof obj.next === 'function' &&
          str.indexOf('[object Generator') === 0) {
        return true;
      }
    }
    catch (ex) {
      // window.history.next throws in the typeof check above.
      return false;
    }
  }

  return false;
};

/**
 * Would calling 'scope[prop]' cause the invocation of a non-native (i.e. user
 * defined) function property?
 * Since calling functions can have side effects, it's only safe to do that if
 * explicitly requested, rather than because we're trying things out for the
 * purposes of completion.
 */
JavascriptType.prototype._isSafeProperty = function(scope, prop) {
  if (typeof scope !== 'object') {
    return false;
  }

  // Walk up the prototype chain of 'scope' looking for a property descriptor
  // for 'prop'
  var propDesc = undefined;
  while (scope) {
    try {
      propDesc = Object.getOwnPropertyDescriptor(scope, prop);
      if (propDesc) {
        break;
      }
    }
    catch (ex) {
      // Native getters throw here. See bug 520882.
      if (ex.name === 'NS_ERROR_XPC_BAD_CONVERT_JS' ||
          ex.name === 'NS_ERROR_XPC_BAD_OP_ON_WN_PROTO') {
        return false;
      }
      return true;
    }
    scope = Object.getPrototypeOf(scope);
  }

  if (!propDesc) {
    return false;
  }

  if (!propDesc.get) {
    return false;
  }

  // The property is safe if 'get' isn't a function or if the function has a
  // prototype (in which case it's native)
  return typeof propDesc.get !== 'function' || 'prototype' in propDesc.get;
};

JavascriptType.prototype.name = 'javascript';

exports.JavascriptType = JavascriptType;


});
/*
 * Copyright 2012, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

define('gcli/types/node', ['require', 'exports', 'module' , 'util/promise', 'util/host', 'util/l10n', 'gcli/types', 'gcli/argument'], function(require, exports, module) {

'use strict';

var Promise = require('util/promise');
var host = require('util/host');
var l10n = require('util/l10n');
var types = require('gcli/types');
var Type = require('gcli/types').Type;
var Status = require('gcli/types').Status;
var Conversion = require('gcli/types').Conversion;
var BlankArgument = require('gcli/argument').BlankArgument;


/**
 * Registration and de-registration.
 */
exports.startup = function() {
  types.addType(NodeType);
  types.addType(NodeListType);
};

exports.shutdown = function() {
  types.removeType(NodeType);
  types.removeType(NodeListType);
};

/**
 * The object against which we complete, which is usually 'window' if it exists
 * but could be something else in non-web-content environments.
 */
var doc = undefined;
if (typeof document !== 'undefined') {
  doc = document;
}

/**
 * For testing only.
 * The fake empty NodeList used when there are no matches, we replace this with
 * something that looks better as soon as we have a document, so not only
 * should you not use this, but you shouldn't cache it either.
 */
exports._empty = [];

/**
 * Setter for the document that contains the nodes we're matching
 */
exports.setDocument = function(document) {
  doc = document;
  if (doc != null) {
    exports._empty = doc.querySelectorAll('x>:root');
  }
};

/**
 * Undo the effects of setDocument()
 */
exports.unsetDocument = function() {
  doc = undefined;
  exports._empty = undefined;
};

/**
 * Getter for the document that contains the nodes we're matching
 * Most for changing things back to how they were for unit testing
 */
exports.getDocument = function() {
  return doc;
};


/**
 * A CSS expression that refers to a single node
 */
function NodeType(typeSpec) {
}

NodeType.prototype = Object.create(Type.prototype);

NodeType.prototype.stringify = function(value, context) {
  if (value == null) {
    return '';
  }
  return value.__gcliQuery || 'Error';
};

NodeType.prototype.parse = function(arg, context) {
  if (arg.text === '') {
    return Promise.resolve(new Conversion(undefined, arg, Status.INCOMPLETE));
  }

  var nodes;
  try {
    nodes = doc.querySelectorAll(arg.text);
  }
  catch (ex) {
    return Promise.resolve(new Conversion(undefined, arg, Status.ERROR,
                                          l10n.lookup('nodeParseSyntax')));
  }

  if (nodes.length === 0) {
    return Promise.resolve(new Conversion(undefined, arg, Status.INCOMPLETE,
                                          l10n.lookup('nodeParseNone')));
  }

  if (nodes.length === 1) {
    var node = nodes.item(0);
    node.__gcliQuery = arg.text;

    host.flashNodes(node, true);

    return Promise.resolve(new Conversion(node, arg, Status.VALID, ''));
  }

  host.flashNodes(nodes, false);

  var message = l10n.lookupFormat('nodeParseMultiple', [ nodes.length ]);
  return Promise.resolve(new Conversion(undefined, arg, Status.ERROR, message));
};

NodeType.prototype.name = 'node';



/**
 * A CSS expression that refers to a node list.
 *
 * The 'allowEmpty' option ensures that we do not complain if the entered CSS
 * selector is valid, but does not match any nodes. There is some overlap
 * between this option and 'defaultValue'. What the user wants, in most cases,
 * would be to use 'defaultText' (i.e. what is typed rather than the value that
 * it represents). However this isn't a concept that exists yet and should
 * probably be a part of GCLI if/when it does.
 * All NodeListTypes have an automatic defaultValue of an empty NodeList so
 * they can easily be used in named parameters.
 */
function NodeListType(typeSpec) {
  if ('allowEmpty' in typeSpec && typeof typeSpec.allowEmpty !== 'boolean') {
    throw new Error('Legal values for allowEmpty are [true|false]');
  }

  this.allowEmpty = typeSpec.allowEmpty;
}

NodeListType.prototype = Object.create(Type.prototype);

NodeListType.prototype.getBlank = function() {
  return new Conversion(exports._empty, new BlankArgument(), Status.VALID);
};

NodeListType.prototype.stringify = function(value, context) {
  if (value == null) {
    return '';
  }
  return value.__gcliQuery || 'Error';
};

NodeListType.prototype.parse = function(arg, context) {
  if (arg.text === '') {
    return Promise.resolve(new Conversion(undefined, arg, Status.INCOMPLETE));
  }

  var nodes;
  try {
    nodes = doc.querySelectorAll(arg.text);
  }
  catch (ex) {
    return Promise.resolve(new Conversion(undefined, arg, Status.ERROR,
                                    l10n.lookup('nodeParseSyntax')));
  }

  if (nodes.length === 0 && !this.allowEmpty) {
    return Promise.resolve(new Conversion(undefined, arg, Status.INCOMPLETE,
                                    l10n.lookup('nodeParseNone')));
  }

  host.flashNodes(nodes, false);
  return Promise.resolve(new Conversion(nodes, arg, Status.VALID, ''));
};

NodeListType.prototype.name = 'nodelist';


});
/*
 * Copyright 2012, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

define('util/host', ['require', 'exports', 'module' ], function(require, exports, module) {

  'use strict';

  /**
   * The chromeWindow as as required by Highlighter, so it knows where to
   * create temporary highlight nodes.
   */
  exports.chromeWindow = undefined;

  /**
   * Helper to turn a set of nodes background another color for 0.5 seconds.
   * There is likely a better way to do this, but this will do for now.
   */
  exports.flashNodes = function(nodes, match) {
    // Commented out until Bug 653545 is completed
    /*
    if (exports.chromeWindow == null) {
      console.log('flashNodes has no chromeWindow. Skipping flash');
      return;
    }

    var imports = {};
    Components.utils.import("resource:///modules/highlighter.jsm", imports);

    imports.Highlighter.flashNodes(nodes, exports.chromeWindow, match);
    */
  };


});
/*
 * Copyright 2012, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

define('gcli/types/resource', ['require', 'exports', 'module' , 'util/promise', 'gcli/types', 'gcli/types/selection'], function(require, exports, module) {

'use strict';

var Promise = require('util/promise');
var types = require('gcli/types');
var SelectionType = require('gcli/types/selection').SelectionType;


/**
 * Registration and de-registration.
 */
exports.startup = function() {
  types.addType(ResourceType);
};

exports.shutdown = function() {
  types.removeType(ResourceType);
  exports.clearResourceCache();
};

exports.clearResourceCache = function() {
  ResourceCache.clear();
};

/**
 * The object against which we complete, which is usually 'window' if it exists
 * but could be something else in non-web-content environments.
 */
var doc = undefined;
if (typeof document !== 'undefined') {
  doc = document;
}

/**
 * Setter for the document that contains the nodes we're matching
 */
exports.setDocument = function(document) {
  doc = document;
};

/**
 * Undo the effects of setDocument()
 */
exports.unsetDocument = function() {
  ResourceCache.clear();
  doc = undefined;
};

/**
 * Getter for the document that contains the nodes we're matching
 * Most for changing things back to how they were for unit testing
 */
exports.getDocument = function() {
  return doc;
};

/**
 * Resources are bits of CSS and JavaScript that the page either includes
 * directly or as a result of reading some remote resource.
 * Resource should not be used directly, but instead through a sub-class like
 * CssResource or ScriptResource.
 */
function Resource(name, type, inline, element) {
  this.name = name;
  this.type = type;
  this.inline = inline;
  this.element = element;
}

/**
 * Get the contents of the given resource as a string.
 * The base Resource leaves this unimplemented.
 */
Resource.prototype.getContents = function() {
  throw new Error('not implemented');
};

Resource.TYPE_SCRIPT = 'text/javascript';
Resource.TYPE_CSS = 'text/css';

/**
 * A CssResource provides an implementation of Resource that works for both
 * [style] elements and [link type='text/css'] elements in the [head].
 */
function CssResource(domSheet) {
  this.name = domSheet.href;
  if (!this.name) {
    this.name = domSheet.ownerNode.id ?
            'css#' + domSheet.ownerNode.id :
            'inline-css';
  }

  this.inline = (domSheet.href == null);
  this.type = Resource.TYPE_CSS;
  this.element = domSheet;
}

CssResource.prototype = Object.create(Resource.prototype);

CssResource.prototype.loadContents = function(callback) {
  callback(this.element.ownerNode.innerHTML);
};

CssResource._getAllStyles = function() {
  var resources = [];
  if (doc == null) {
    return resources;
  }

  Array.prototype.forEach.call(doc.styleSheets, function(domSheet) {
    CssResource._getStyle(domSheet, resources);
  });

  dedupe(resources, function(clones) {
    for (var i = 0; i < clones.length; i++) {
      clones[i].name = clones[i].name + '-' + i;
    }
  });

  return resources;
};

CssResource._getStyle = function(domSheet, resources) {
  var resource = ResourceCache.get(domSheet);
  if (!resource) {
    resource = new CssResource(domSheet);
    ResourceCache.add(domSheet, resource);
  }
  resources.push(resource);

  // Look for imported stylesheets
  try {
    Array.prototype.forEach.call(domSheet.cssRules, function(domRule) {
      if (domRule.type == CSSRule.IMPORT_RULE && domRule.styleSheet) {
        CssResource._getStyle(domRule.styleSheet, resources);
      }
    }, this);
  }
  catch (ex) {
    // For system stylesheets
  }
};

/**
 * A ScriptResource provides an implementation of Resource that works for
 * [script] elements (both with a src attribute, and used directly).
 */
function ScriptResource(scriptNode) {
  this.name = scriptNode.src;
  if (!this.name) {
    this.name = scriptNode.id ?
            'script#' + scriptNode.id :
            'inline-script';
  }

  this.inline = (scriptNode.src === '' || scriptNode.src == null);
  this.type = Resource.TYPE_SCRIPT;
  this.element = scriptNode;
}

ScriptResource.prototype = Object.create(Resource.prototype);

ScriptResource.prototype.loadContents = function(callback) {
  if (this.inline) {
    callback(this.element.innerHTML);
  }
  else {
    // It would be good if there was a better way to get the script source
    var xhr = new XMLHttpRequest();
    xhr.onreadystatechange = function() {
      if (xhr.readyState !== xhr.DONE) {
        return;
      }
      callback(xhr.responseText);
    };
    xhr.open('GET', this.element.src, true);
    xhr.send();
  }
};

ScriptResource._getAllScripts = function() {
  if (doc == null) {
    return [];
  }

  var scriptNodes = doc.querySelectorAll('script');
  var resources = Array.prototype.map.call(scriptNodes, function(scriptNode) {
    var resource = ResourceCache.get(scriptNode);
    if (!resource) {
      resource = new ScriptResource(scriptNode);
      ResourceCache.add(scriptNode, resource);
    }
    return resource;
  });

  dedupe(resources, function(clones) {
    for (var i = 0; i < clones.length; i++) {
      clones[i].name = clones[i].name + '-' + i;
    }
  });

  return resources;
};

/**
 * Find resources with the same name, and call onDupe to change the names
 */
function dedupe(resources, onDupe) {
  // first create a map of name->[array of resources with same name]
  var names = {};
  resources.forEach(function(scriptResource) {
    if (names[scriptResource.name] == null) {
      names[scriptResource.name] = [];
    }
    names[scriptResource.name].push(scriptResource);
  });

  // Call the de-dupe function for each set of dupes
  Object.keys(names).forEach(function(name) {
    var clones = names[name];
    if (clones.length > 1) {
      onDupe(clones);
    }
  });
}

/**
 * Use the Resource implementations to create a type based on SelectionType
 */
function ResourceType(typeSpec) {
  this.include = typeSpec.include;
  if (this.include !== Resource.TYPE_SCRIPT &&
      this.include !== Resource.TYPE_CSS &&
      this.include != null) {
    throw new Error('invalid include property: ' + this.include);
  }
}

ResourceType.prototype = Object.create(SelectionType.prototype);

/**
 * There are several ways to get selection data. This unifies them into one
 * single function.
 * @return A map of names to values.
 */
ResourceType.prototype.getLookup = function() {
  var resources = [];
  if (this.include !== Resource.TYPE_SCRIPT) {
    Array.prototype.push.apply(resources, CssResource._getAllStyles());
  }
  if (this.include !== Resource.TYPE_CSS) {
    Array.prototype.push.apply(resources, ScriptResource._getAllScripts());
  }

  return Promise.resolve(resources.map(function(resource) {
    return { name: resource.name, value: resource };
  }));
};

ResourceType.prototype.name = 'resource';


/**
 * A quick cache of resources against nodes
 * TODO: Potential memory leak when the target document has css or script
 * resources repeatedly added and removed. Solution might be to use a weak
 * hash map or some such.
 */
var ResourceCache = {
  _cached: [],

  /**
   * Do we already have a resource that was created for the given node
   */
  get: function(node) {
    for (var i = 0; i < ResourceCache._cached.length; i++) {
      if (ResourceCache._cached[i].node === node) {
        return ResourceCache._cached[i].resource;
      }
    }
    return null;
  },

  /**
   * Add a resource for a given node
   */
  add: function(node, resource) {
    ResourceCache._cached.push({ node: node, resource: resource });
  },

  /**
   * Drop all cache entries. Helpful to prevent memory leaks
   */
  clear: function() {
    ResourceCache._cached = [];
  }
};


});
/*
 * Copyright 2012, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

define('gcli/types/setting', ['require', 'exports', 'module' , 'gcli/settings', 'gcli/types'], function(require, exports, module) {

'use strict';

var settings = require('gcli/settings');
var types = require('gcli/types');

/**
 * A type for selecting a known setting
 */
var settingType = {
  constructor: function() {
    settings.onChange.add(function(ev) {
      this.clearCache();
    }, this);
  },
  name: 'setting',
  parent: 'selection',
  cacheable: true,
  lookup: function() {
    return settings.getAll().map(function(setting) {
      return { name: setting.name, value: setting };
    });
  }
};

/**
 * A type for entering the value of a known setting
 * Customizations:
 * - settingParamName The name of the setting parameter so we can customize the
 *   type that we are expecting to read
 */
var settingValueType = {
  name: 'settingValue',
  parent: 'delegate',
  settingParamName: 'setting',
  delegateType: function(context) {
    if (context != null) {
      var setting = context.getArgsObject()[this.settingParamName];
      if (setting != null) {
        return setting.type;
      }
    }

    return types.createType('blank');
  }
};

/**
 * Registration and de-registration.
 */
exports.startup = function() {
  types.addType(settingType);
  types.addType(settingValueType);
};

exports.shutdown = function() {
  types.removeType(settingType);
  types.removeType(settingValueType);
};


});
/*
 * Copyright 2012, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

define('gcli/settings', ['require', 'exports', 'module' , 'util/util', 'gcli/types'], function(require, exports, module) {

'use strict';

var imports = {};

Components.utils.import('resource://gre/modules/XPCOMUtils.jsm', imports);

imports.XPCOMUtils.defineLazyGetter(imports, 'prefBranch', function() {
  var prefService = Components.classes['@mozilla.org/preferences-service;1']
          .getService(Components.interfaces.nsIPrefService);
  return prefService.getBranch(null)
          .QueryInterface(Components.interfaces.nsIPrefBranch2);
});

imports.XPCOMUtils.defineLazyGetter(imports, 'supportsString', function() {
  return Components.classes["@mozilla.org/supports-string;1"]
          .createInstance(Components.interfaces.nsISupportsString);
});


var util = require('util/util');
var types = require('gcli/types');

/**
 * All local settings have this prefix when used in Firefox
 */
var DEVTOOLS_PREFIX = 'devtools.gcli.';

/**
 * A class to wrap up the properties of a preference.
 * @see toolkit/components/viewconfig/content/config.js
 */
function Setting(prefSpec) {
  if (typeof prefSpec === 'string') {
    // We're coming from getAll() i.e. a full listing of prefs
    this.name = prefSpec;
    this.description = '';
  }
  else {
    // A specific addition by GCLI
    this.name = DEVTOOLS_PREFIX + prefSpec.name;

    if (prefSpec.ignoreTypeDifference !== true && prefSpec.type) {
      if (this.type.name !== prefSpec.type) {
        throw new Error('Locally declared type (' + prefSpec.type + ') != ' +
            'Mozilla declared type (' + this.type.name + ') for ' + this.name);
      }
    }

    this.description = prefSpec.description;
  }

  this.onChange = util.createEvent('Setting.onChange');
}

/**
 * What type is this property: boolean/integer/string?
 */
Object.defineProperty(Setting.prototype, 'type', {
  get: function() {
    switch (imports.prefBranch.getPrefType(this.name)) {
      case imports.prefBranch.PREF_BOOL:
        return types.createType('boolean');

      case imports.prefBranch.PREF_INT:
        return types.createType('number');

      case imports.prefBranch.PREF_STRING:
        return types.createType('string');

      default:
        throw new Error('Unknown type for ' + this.name);
    }
  },
  enumerable: true
});

/**
 * What type is this property: boolean/integer/string?
 */
Object.defineProperty(Setting.prototype, 'value', {
  get: function() {
    switch (imports.prefBranch.getPrefType(this.name)) {
      case imports.prefBranch.PREF_BOOL:
        return imports.prefBranch.getBoolPref(this.name);

      case imports.prefBranch.PREF_INT:
        return imports.prefBranch.getIntPref(this.name);

      case imports.prefBranch.PREF_STRING:
        var value = imports.prefBranch.getComplexValue(this.name,
                Components.interfaces.nsISupportsString).data;
        // In case of a localized string
        if (/^chrome:\/\/.+\/locale\/.+\.properties/.test(value)) {
          value = imports.prefBranch.getComplexValue(this.name,
                  Components.interfaces.nsIPrefLocalizedString).data;
        }
        return value;

      default:
        throw new Error('Invalid value for ' + this.name);
    }
  },

  set: function(value) {
    if (imports.prefBranch.prefIsLocked(this.name)) {
      throw new Error('Locked preference ' + this.name);
    }

    switch (imports.prefBranch.getPrefType(this.name)) {
      case imports.prefBranch.PREF_BOOL:
        imports.prefBranch.setBoolPref(this.name, value);
        break;

      case imports.prefBranch.PREF_INT:
        imports.prefBranch.setIntPref(this.name, value);
        break;

      case imports.prefBranch.PREF_STRING:
        imports.supportsString.data = value;
        imports.prefBranch.setComplexValue(this.name,
                Components.interfaces.nsISupportsString,
                imports.supportsString);
        break;

      default:
        throw new Error('Invalid value for ' + this.name);
    }

    Services.prefs.savePrefFile(null);
  },

  enumerable: true
});

/**
 * Reset this setting to it's initial default value
 */
Setting.prototype.setDefault = function() {
  imports.prefBranch.clearUserPref(this.name);
  Services.prefs.savePrefFile(null);
};


/**
 * Collection of preferences for sorted access
 */
var settingsAll = [];

/**
 * Collection of preferences for fast indexed access
 */
var settingsMap = new Map();

/**
 * Flag so we know if we've read the system preferences
 */
var hasReadSystem = false;

/**
 * Clear out all preferences and return to initial state
 */
function reset() {
  settingsMap = new Map();
  settingsAll = [];
  hasReadSystem = false;
}

/**
 * Reset everything on startup and shutdown because we're doing lazy loading
 */
exports.startup = function() {
  reset();
};

exports.shutdown = function() {
  reset();
};

/**
 * Load system prefs if they've not been loaded already
 * @return true
 */
function readSystem() {
  if (hasReadSystem) {
    return;
  }

  imports.prefBranch.getChildList('').forEach(function(name) {
    var setting = new Setting(name);
    settingsAll.push(setting);
    settingsMap.set(name, setting);
  });

  settingsAll.sort(function(s1, s2) {
    return s1.name.localeCompare(s2.name);
  });

  hasReadSystem = true;
}

/**
 * Get an array containing all known Settings filtered to match the given
 * filter (string) at any point in the name of the setting
 */
exports.getAll = function(filter) {
  readSystem();

  if (filter == null) {
    return settingsAll;
  }

  return settingsAll.filter(function(setting) {
    return setting.name.indexOf(filter) !== -1;
  });
};

/**
 * Add a new setting.
 */
exports.addSetting = function(prefSpec) {
  var setting = new Setting(prefSpec);

  if (settingsMap.has(setting.name)) {
    // Once exists already, we're going to need to replace it in the array
    for (var i = 0; i < settingsAll.length; i++) {
      if (settingsAll[i].name === setting.name) {
        settingsAll[i] = setting;
      }
    }
  }

  settingsMap.set(setting.name, setting);
  exports.onChange({ added: setting.name });

  return setting;
};

/**
 * Getter for an existing setting. Generally use of this function should be
 * avoided. Systems that define a setting should export it if they wish it to
 * be available to the outside, or not otherwise. Use of this function breaks
 * that boundary and also hides dependencies. Acceptable uses include testing
 * and embedded uses of GCLI that pre-define all settings (e.g. Firefox)
 * @param name The name of the setting to fetch
 * @return The found Setting object, or undefined if the setting was not found
 */
exports.getSetting = function(name) {
  // We might be able to give the answer without needing to read all system
  // settings if this is an internal setting
  var found = settingsMap.get(name);
  if (found) {
    return found;
  }

  if (hasReadSystem) {
    return undefined;
  }
  else {
    readSystem();
    return settingsMap.get(name);
  }
};

/**
 * Event for use to detect when the list of settings changes
 */
exports.onChange = util.createEvent('Settings.onChange');

/**
 * Remove a setting. A no-op in this case
 */
exports.removeSetting = function() { };


});
/*
 * Copyright 2012, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

define('gcli/ui/intro', ['require', 'exports', 'module' , 'util/l10n', 'gcli/settings', 'gcli/ui/view', 'gcli/cli', 'text!gcli/ui/intro.html'], function(require, exports, module) {

'use strict';

var l10n = require('util/l10n');
var settings = require('gcli/settings');
var view = require('gcli/ui/view');
var Output = require('gcli/cli').Output;

/**
 * Record if the user has clicked on 'Got It!'
 */
var hideIntroSettingSpec = {
  name: 'hideIntro',
  type: 'boolean',
  description: l10n.lookup('hideIntroDesc'),
  defaultValue: false
};
var hideIntro;

/**
 * Register (and unregister) the hide-intro setting
 */
exports.startup = function() {
  hideIntro = settings.addSetting(hideIntroSettingSpec);
};

exports.shutdown = function() {
  settings.removeSetting(hideIntroSettingSpec);
  hideIntro = undefined;
};

/**
 * Called when the UI is ready to add a welcome message to the output
 */
exports.maybeShowIntro = function(commandOutputManager, conversionContext) {
  if (hideIntro.value) {
    return;
  }

  var output = new Output();
  output.type = 'view';
  commandOutputManager.onOutput({ output: output });

  var viewData = this.createView(conversionContext, output);

  output.complete({ isTypedData: true, type: 'view', data: viewData });
};

/**
 * Called when the UI is ready to add a welcome message to the output
 */
exports.createView = function(conversionContext, output) {
  return view.createView({
    html: require('text!gcli/ui/intro.html'),
    options: { stack: 'intro.html' },
    data: {
      l10n: l10n.propertyLookup,
      onclick: function(ev) {
        conversionContext.update(ev.currentTarget);
      },
      ondblclick: function(ev) {
        conversionContext.updateExec(ev.currentTarget);
      },
      showHideButton: (output != null),
      onGotIt: function(ev) {
        hideIntro.value = true;
        output.onClose();
      }
    }
  });
};

});
/*
 * Copyright 2012, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

define('gcli/ui/view', ['require', 'exports', 'module' , 'util/util', 'util/domtemplate'], function(require, exports, module) {

'use strict';

var util = require('util/util');
var domtemplate = require('util/domtemplate');


/**
 * We want to avoid commands having to create DOM structures because that's
 * messy and because we're going to need to have command output displayed in
 * different documents. A View is a way to wrap an HTML template (for
 * domtemplate) in with the data and options to render the template, so anyone
 * can later run the template in the context of any document.
 * View also cuts out a chunk of boiler place code.
 * @param options The information needed to create the DOM from HTML. Includes:
 * - html (required): The HTML source, probably from a call to require
 * - options (default={}): The domtemplate options. See domtemplate for details
 * - data (default={}): The data to domtemplate. See domtemplate for details.
 * - css (default=none): Some CSS to be added to the final document. If 'css'
 *   is used, use of cssId is strongly recommended.
 * - cssId (default=none): An ID to prevent multiple CSS additions. See
 *   util.importCss for more details.
 * @return An object containing a single function 'appendTo()' which runs the
 * template adding the result to the specified element. Takes 2 parameters:
 * - element (required): the element to add to
 * - clear (default=false): if clear===true then remove all pre-existing
 *   children of 'element' before appending the results of this template.
 */
exports.createView = function(options) {
  if (options.html == null) {
    throw new Error('options.html is missing');
  }

  return {
    /**
     * RTTI. Yeah.
     */
    isView: true,

    /**
     * Run the template against the document to which element belongs.
     * @param element The element to append the result to
     * @param clear Set clear===true to remove all children of element
     */
    appendTo: function(element, clear) {
      // Strict check on the off-chance that we later think of other options
      // and want to replace 'clear' with an 'options' parameter, but want to
      // support backwards compat.
      if (clear === true) {
        util.clearElement(element);
      }

      element.appendChild(this.toDom(element.ownerDocument));
    },

    /**
     * Actually convert the view data into a DOM suitable to be appended to
     * an element
     * @param document to use in realizing the template
     */
    toDom: function(document) {
      if (options.css) {
        util.importCss(options.css, document, options.cssId);
      }

      var child = util.toDom(document, options.html);
      domtemplate.template(child, options.data || {}, options.options || {});
      return child;
    }
  };
};


});
/*
 * Copyright 2012, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

define('util/domtemplate', ['require', 'exports', 'module' ], function(require, exports, module) {

  'use strict';

  var obj = {};
  Components.utils.import('resource://gre/modules/devtools/Templater.jsm', obj);
  exports.template = obj.template;

});
/*
 * Copyright 2012, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

define('gcli/cli', ['require', 'exports', 'module' , 'util/promise', 'util/util', 'util/l10n', 'gcli/ui/view', 'gcli/canon', 'gcli/types', 'gcli/argument'], function(require, exports, module) {

'use strict';

var Promise = require('util/promise');
var util = require('util/util');
var l10n = require('util/l10n');

var view = require('gcli/ui/view');
var canon = require('gcli/canon');
var CommandOutputManager = require('gcli/canon').CommandOutputManager;

var Status = require('gcli/types').Status;
var Conversion = require('gcli/types').Conversion;

var Argument = require('gcli/argument').Argument;
var ArrayArgument = require('gcli/argument').ArrayArgument;
var NamedArgument = require('gcli/argument').NamedArgument;
var TrueNamedArgument = require('gcli/argument').TrueNamedArgument;
var MergedArgument = require('gcli/argument').MergedArgument;
var ScriptArgument = require('gcli/argument').ScriptArgument;

var evalCommand;

/**
 * Registration and de-registration.
 */
exports.startup = function() {
  evalCommand = canon.addCommand(evalCommandSpec);
};

exports.shutdown = function() {
  canon.removeCommand(evalCommandSpec.name);
  evalCommand = undefined;
};


/**
 * Assignment is a link between a parameter and the data for that parameter.
 * The data for the parameter is available as in the preferred type and as
 * an Argument for the CLI.
 * <p>We also record validity information where applicable.
 * <p>For values, null and undefined have distinct definitions. null means
 * that a value has been provided, undefined means that it has not.
 * Thus, null is a valid default value, and common because it identifies an
 * parameter that is optional. undefined means there is no value from
 * the command line.
 *
 * <h2>Events<h2>
 * Assignment publishes the following event:<ul>
 * <li>onAssignmentChange: Either the value or the text has changed. It is
 * likely that any UI component displaying this argument will need to be
 * updated.
 * The event object looks like:
 * <tt>{ assignment: ..., conversion: ..., oldConversion: ... }</tt>
 * @constructor
 */
function Assignment(param, paramIndex) {
  // The parameter that we are assigning to
  this.param = param;

  this.conversion = undefined;

  // The index of this parameter in the parent Requisition. paramIndex === -1
  // is the command assignment although this should not be relied upon, it is
  // better to test param instanceof CommandAssignment
  this.paramIndex = paramIndex;

  this.onAssignmentChange = util.createEvent('Assignment.onAssignmentChange');
}

/**
 * Easy accessor for conversion.arg.
 * This is a read-only property because writes to arg should be done through
 * the 'conversion' property.
 */
Object.defineProperty(Assignment.prototype, 'arg', {
  get: function() {
    return this.conversion.arg;
  },
  enumerable: true
});

/**
 * Easy accessor for conversion.value.
 * This is a read-only property because writes to value should be done through
 * the 'conversion' property.
 */
Object.defineProperty(Assignment.prototype, 'value', {
  get: function() {
    return this.conversion.value;
  },
  enumerable: true
});

/**
 * Easy (and safe) accessor for conversion.message
 */
Assignment.prototype.getMessage = function() {
  return this.conversion.message ? this.conversion.message : '';
};

/**
 * Easy (and safe) accessor for conversion.getPredictions()
 * @return An array of objects with name and value elements. For example:
 * [ { name:'bestmatch', value:foo1 }, { name:'next', value:foo2 }, ... ]
 */
Assignment.prototype.getPredictions = function() {
  return this.conversion.getPredictions();
};

/**
 * Accessor for a prediction by index.
 * This is useful above <tt>getPredictions()[index]</tt> because it normalizes
 * index to be within the bounds of the predictions, which means that the UI
 * can maintain an index of which prediction to choose without caring how many
 * predictions there are.
 * @param index The index of the prediction to choose
 */
Assignment.prototype.getPredictionAt = function(index) {
  if (index == null) {
    index = 0;
  }

  if (this.isInName()) {
    return Promise.resolve(undefined);
  }

  return this.getPredictions().then(function(predictions) {
    if (predictions.length === 0) {
      return undefined;
    }

    index = index % predictions.length;
    if (index < 0) {
      index = predictions.length + index;
    }
    return predictions[index];
  }.bind(this));
};

/**
 * Some places want to take special action if we are in the name part of a
 * named argument (i.e. the '--foo' bit).
 * Currently this does not take actual cursor position into account, it just
 * assumes that the cursor is at the end. In the future we will probably want
 * to take this into account.
 */
Assignment.prototype.isInName = function() {
  return this.conversion.arg.type === 'NamedArgument' &&
         this.conversion.arg.prefix.slice(-1) !== ' ';
};

/**
 * Work out what the status of the current conversion is which involves looking
 * not only at the conversion, but also checking if data has been provided
 * where it should.
 * @param arg For assignments with multiple args (e.g. array assignments) we
 * can narrow the search for status to a single argument.
 */
Assignment.prototype.getStatus = function(arg) {
  if (this.param.isDataRequired && !this.conversion.isDataProvided()) {
    return Status.INCOMPLETE;
  }

  // Selection/Boolean types with a defined range of values will say that
  // '' is INCOMPLETE, but the parameter may be optional, so we don't ask
  // if the user doesn't need to enter something and hasn't done so.
  if (!this.param.isDataRequired && this.arg.type === 'BlankArgument') {
    return Status.VALID;
  }

  return this.conversion.getStatus(arg);
};

/**
 * Helper when we're rebuilding command lines.
 */
Assignment.prototype.toString = function() {
  return this.conversion.toString();
};

/**
 * For test/debug use only. The output from this function is subject to wanton
 * random change without notice, and should not be relied upon to even exist
 * at some later date.
 */
Object.defineProperty(Assignment.prototype, '_summaryJson', {
  get: function() {
    var predictionCount = '<async>';
    this.getPredictions().then(function(predictions) {
      predictionCount = predictions.length;
    }, console.log);
    return {
      param: this.param.name + '/' + this.param.type.name,
      defaultValue: this.param.defaultValue,
      arg: this.conversion.arg._summaryJson,
      value: this.value,
      message: this.getMessage(),
      status: this.getStatus().toString(),
      predictionCount: predictionCount
    };
  },
  enumerable: true
});

exports.Assignment = Assignment;


/**
 * How to dynamically execute JavaScript code
 */
var customEval = eval;

/**
 * Setup a function to be called in place of 'eval', generally for security
 * reasons
 */
exports.setEvalFunction = function(newCustomEval) {
  customEval = newCustomEval;
};

/**
 * Remove the binding done by setEvalFunction().
 * We purposely set customEval to undefined rather than to 'eval' because there
 * is an implication of setEvalFunction that we're in a security sensitive
 * situation. What if we can trick GCLI into calling unsetEvalFunction() at the
 * wrong time?
 * So to properly undo the effects of setEvalFunction(), you need to call
 * setEvalFunction(eval) rather than unsetEvalFunction(), however the latter is
 * preferred in most cases.
 */
exports.unsetEvalFunction = function() {
  customEval = undefined;
};

/**
 * 'eval' command
 */
var evalCommandSpec = {
  name: '{',
  params: [
    {
      name: 'javascript',
      type: 'javascript',
      description: ''
    }
  ],
  hidden: true,
  returnType: 'object',
  description: { key: 'cliEvalJavascript' },
  exec: function(args, context) {
    return customEval(args.javascript);
  },
  evalRegexp: /^\s*{\s*/
};


/**
 * This is a special assignment to reflect the command itself.
 */
function CommandAssignment() {
  var commandParamMetadata = { name: '__command', type: 'command' };
  // This is a hack so that rather than reply with a generic description of the
  // command assignment, we reply with the description of the assigned command,
  // (using a generic term if there is no assigned command)
  var self = this;
  Object.defineProperty(commandParamMetadata, 'description', {
    get: function() {
      var value = self.value;
      return value && value.description ?
          value.description :
          'The command to execute';
    },
    enumerable: true
  });
  this.param = new canon.Parameter(commandParamMetadata);
  this.paramIndex = -1;
  this.onAssignmentChange = util.createEvent('CommandAssignment.onAssignmentChange');
}

CommandAssignment.prototype = Object.create(Assignment.prototype);

CommandAssignment.prototype.getStatus = function(arg) {
  return Status.combine(
    Assignment.prototype.getStatus.call(this, arg),
    this.conversion.value && this.conversion.value.exec ?
            Status.VALID : Status.INCOMPLETE
  );
};

exports.CommandAssignment = CommandAssignment;


/**
 * Special assignment used when ignoring parameters that don't have a home
 */
function UnassignedAssignment(requisition, arg) {
  this.param = new canon.Parameter({
    name: '__unassigned',
    description: l10n.lookup('cliOptions'),
    type: {
      name: 'param',
      requisition: requisition,
      isIncompleteName: (arg.text.charAt(0) === '-')
    }
  });
  this.paramIndex = -1;
  this.onAssignmentChange = util.createEvent('UnassignedAssignment.onAssignmentChange');

  // synchronize is ok because we can be sure that param type is synchronous
  var parsed = this.param.type.parse(arg, requisition.executionContext);
  this.conversion = util.synchronize(parsed);
  this.conversion.assign(this);
}

UnassignedAssignment.prototype = Object.create(Assignment.prototype);

UnassignedAssignment.prototype.getStatus = function(arg) {
  return this.conversion.getStatus();
};

exports.logErrors = true;

/**
 * A Requisition collects the information needed to execute a command.
 *
 * (For a definition of the term, see http://en.wikipedia.org/wiki/Requisition)
 * This term is used because carries the notion of a work-flow, or process to
 * getting the information to execute a command correct.
 * There is little point in a requisition for parameter-less commands because
 * there is no information to collect. A Requisition is a collection of
 * assignments of values to parameters, each handled by an instance of
 * Assignment.
 *
 * <h2>Events<h2>
 * <p>Requisition publishes the following events:
 * <ul>
 * <li>onAssignmentChange: This is a forward of the onAssignmentChange event on
 * Assignment. It is fired when any assignment (except the commandAssignment)
 * changes.
 * <li>onTextChange: The text to be mirrored in a command line has changed.
 * </ul>
 *
 * @param environment An optional opaque object passed to commands in the
 * Execution Context.
 * @param doc A DOM Document passed to commands using the Execution Context in
 * order to allow creation of DOM nodes. If missing Requisition will use the
 * global 'document'.
 * @param commandOutputManager A custom commandOutputManager to which output
 * should be sent (optional)
 * @constructor
 */
function Requisition(environment, doc, commandOutputManager) {
  this.environment = environment;
  this.document = doc;
  if (this.document == null) {
    try {
      this.document = document;
    }
    catch (ex) {
      // Ignore
    }
  }

  this.commandOutputManager = commandOutputManager || new CommandOutputManager();

  // The command that we are about to execute.
  // @see setCommandConversion()
  this.commandAssignment = new CommandAssignment();
  var promise = this.setAssignment(this.commandAssignment, null,
                                   { skipArgUpdate: true });
  util.synchronize(promise);

  // The object that stores of Assignment objects that we are filling out.
  // The Assignment objects are stored under their param.name for named
  // lookup. Note: We make use of the property of Javascript objects that
  // they are not just hashmaps, but linked-list hashmaps which iterate in
  // insertion order.
  // _assignments excludes the commandAssignment.
  this._assignments = {};

  // The count of assignments. Excludes the commandAssignment
  this.assignmentCount = 0;

  // Used to store cli arguments in the order entered on the cli
  this._args = [];

  // Used to store cli arguments that were not assigned to parameters
  this._unassigned = [];

  // Temporarily set this to true to prevent _assignmentChanged resetting
  // argument positions
  this._structuralChangeInProgress = false;

  // We can set a prefix to typed commands to make it easier to focus on
  // Allowing us to type "add -a; commit" in place of "git add -a; git commit"
  this.prefix = '';

  this.commandAssignment.onAssignmentChange.add(this._commandAssignmentChanged, this);
  this.commandAssignment.onAssignmentChange.add(this._assignmentChanged, this);

  this.onAssignmentChange = util.createEvent('Requisition.onAssignmentChange');
  this.onTextChange = util.createEvent('Requisition.onTextChange');
}

/**
 * Avoid memory leaks
 */
Requisition.prototype.destroy = function() {
  this.commandAssignment.onAssignmentChange.remove(this._commandAssignmentChanged, this);
  this.commandAssignment.onAssignmentChange.remove(this._assignmentChanged, this);

  delete this.document;
  delete this.environment;
};

var legacy = false;

/**
 * Functions and data related to the execution of a command
 */
Object.defineProperty(Requisition.prototype, 'executionContext', {
  get: function() {
    if (this._executionContext == null) {
      this._executionContext = {
        defer: function() {
          return Promise.defer();
        },
        typedData: function(type, data) {
          return {
            isTypedData: true,
            data: data,
            type: type
          };
        },
        getArgsObject: this.getArgsObject.bind(this)
      };

      // Alias requisition so we're clear about what's what
      var requisition = this;
      Object.defineProperty(this._executionContext, 'typed', {
        get: function() { return requisition.toString(); },
        enumerable: true
      });
      Object.defineProperty(this._executionContext, 'environment', {
        get: function() { return requisition.environment; },
        enumerable: true
      });

      /**
       * This is a temporary property that will change and/or be removed.
       * Do not use it
       */
      Object.defineProperty(this._executionContext, '__dlhjshfw', {
        get: function() { return requisition; },
        enumerable: false
      });

      if (legacy) {
        this._executionContext.createView = view.createView;
        this._executionContext.exec = this.exec.bind(this);
        this._executionContext.update = this.update.bind(this);
        this._executionContext.updateExec = this.updateExec.bind(this);

        Object.defineProperty(this._executionContext, 'document', {
          get: function() { return requisition.document; },
          enumerable: true
        });
      }
    }

    return this._executionContext;
  },
  enumerable: true
});

/**
 * Functions and data related to the conversion of the output of a command
 */
Object.defineProperty(Requisition.prototype, 'conversionContext', {
  get: function() {
    if (this._conversionContext == null) {
      this._conversionContext = {
        defer: function() {
          return Promise.defer();
        },

        createView: view.createView,
        exec: this.exec.bind(this),
        update: this.update.bind(this),
        updateExec: this.updateExec.bind(this)
      };

      // Alias requisition so we're clear about what's what
      var requisition = this;

      Object.defineProperty(this._conversionContext, 'document', {
        get: function() { return requisition.document; },
        enumerable: true
      });
      Object.defineProperty(this._conversionContext, 'environment', {
        get: function() { return requisition.environment; },
        enumerable: true
      });

      /**
       * This is a temporary property that will change and/or be removed.
       * Do not use it
       */
      Object.defineProperty(this._conversionContext, '__dlhjshfw', {
        get: function() { return requisition; },
        enumerable: false
      });
    }

    return this._conversionContext;
  },
  enumerable: true
});

/**
 * When any assignment changes, we might need to update the _args array to
 * match and inform people of changes to the typed input text.
 */
Requisition.prototype._assignmentChanged = function(ev) {
  // Don't report an event if the value is unchanged
  if (ev.oldConversion != null &&
      ev.conversion.valueEquals(ev.oldConversion)) {
    return;
  }

  if (this._structuralChangeInProgress) {
    return;
  }

  this.onAssignmentChange(ev);

  // Both for argument position and the onTextChange event, we only care
  // about changes to the argument.
  if (ev.conversion.argEquals(ev.oldConversion)) {
    return;
  }

  this.onTextChange();
};

/**
 * When the command changes, we need to keep a bunch of stuff in sync
 */
Requisition.prototype._commandAssignmentChanged = function(ev) {
  // Assignments fire AssignmentChange events on any change, including minor
  // things like whitespace change in arg prefix, so we ignore anything but
  // an actual value change
  if (ev.conversion.valueEquals(ev.oldConversion)) {
    return;
  }

  this._assignments = {};

  var command = this.commandAssignment.value;
  if (command) {
    for (var i = 0; i < command.params.length; i++) {
      var param = command.params[i];
      var assignment = new Assignment(param, i);
      var promise = this.setAssignment(assignment, null,
                                       { skipArgUpdate: true });
      util.synchronize(promise);
      assignment.onAssignmentChange.add(this._assignmentChanged, this);
      this._assignments[param.name] = assignment;
    }
  }
  this.assignmentCount = Object.keys(this._assignments).length;
};

/**
 * Assignments have an order, so we need to store them in an array.
 * But we also need named access ...
 * @return The found assignment, or undefined, if no match was found
 */
Requisition.prototype.getAssignment = function(nameOrNumber) {
  var name = (typeof nameOrNumber === 'string') ?
    nameOrNumber :
    Object.keys(this._assignments)[nameOrNumber];
  return this._assignments[name] || undefined;
};

/**
 * There are a few places where we need to know what the 'next thing' is. What
 * is the user going to be filling out next (assuming they don't enter a named
 * argument). The next argument is the first in line that is both blank, and
 * that can be filled in positionally.
 * @return The next assignment to be used, or null if all the positional
 * parameters have values.
 */
Requisition.prototype._getFirstBlankPositionalAssignment = function() {
  var reply = null;
  Object.keys(this._assignments).some(function(name) {
    var assignment = this.getAssignment(name);
    if (assignment.arg.type === 'BlankArgument' &&
            assignment.param.isPositionalAllowed) {
      reply = assignment;
      return true; // i.e. break
    }
    return false;
  }, this);
  return reply;
};

/**
 * Where parameter name == assignment names - they are the same
 */
Requisition.prototype.getParameterNames = function() {
  return Object.keys(this._assignments);
};

/**
 * A *shallow* clone of the assignments.
 * This is useful for systems that wish to go over all the assignments
 * finding values one way or another and wish to trim an array as they go.
 */
Requisition.prototype.cloneAssignments = function() {
  return Object.keys(this._assignments).map(function(name) {
    return this._assignments[name];
  }, this);
};

/**
 * The overall status is the most severe status.
 * There is no such thing as an INCOMPLETE overall status because the
 * definition of INCOMPLETE takes into account the cursor position to say 'this
 * isn't quite ERROR because the user can fix it by typing', however overall,
 * this is still an error status.
 */
Requisition.prototype.getStatus = function() {
  var status = Status.VALID;
  if (this._unassigned.length !== 0) {
    var isAllIncomplete = true;
    this._unassigned.forEach(function(assignment) {
      if (!assignment.param.type.isIncompleteName) {
        isAllIncomplete = false;
      }
    });
    status = isAllIncomplete ? Status.INCOMPLETE : Status.ERROR;
  }

  this.getAssignments(true).forEach(function(assignment) {
    var assignStatus = assignment.getStatus();
    if (assignStatus > status) {
      status = assignStatus;
    }
  }, this);
  if (status === Status.INCOMPLETE) {
    status = Status.ERROR;
  }
  return status;
};

/**
 * Extract the names and values of all the assignments, and return as
 * an object.
 */
Requisition.prototype.getArgsObject = function() {
  var args = {};
  this.getAssignments().forEach(function(assignment) {
    args[assignment.param.name] = assignment.conversion.isDataProvided() ?
            assignment.value :
            assignment.param.defaultValue;
  }, this);
  return args;
};

/**
 * Access the arguments as an array.
 * @param includeCommand By default only the parameter arguments are
 * returned unless (includeCommand === true), in which case the list is
 * prepended with commandAssignment.arg
 */
Requisition.prototype.getAssignments = function(includeCommand) {
  var assignments = [];
  if (includeCommand === true) {
    assignments.push(this.commandAssignment);
  }
  Object.keys(this._assignments).forEach(function(name) {
    assignments.push(this.getAssignment(name));
  }, this);
  return assignments;
};

/**
 * Internal function to alter the given assignment using the given arg.
 * @param assignment The assignment to alter
 * @param arg The new value for the assignment. An instance of Argument, or an
 * instance of Conversion, or null to set the blank value.
 * @param options There are a number of ways to customize how the assignment
 * is made, including:
 * - skipArgUpdate: (default:false) Adjusts the args in this requisition to keep
 *   things up to date. Args should only be skipped when setAssignment is being
 *   called as part of the update process.
 * - matchPadding: (default:false) Altering the whitespace on the prefix and
 *   suffix of the new argument to match that of the old argument. This only
 *   makes sense with skipArgUpdate=false
 *   then further take the step of
 */
Requisition.prototype.setAssignment = function(assignment, arg, options) {
  options = options || {};
  if (options.skipArgUpdate !== true) {
    var originalArgs = assignment.arg.getArgs();

    // Update the args array
    var replacementArgs = arg.getArgs();
    var maxLen = Math.max(originalArgs.length, replacementArgs.length);
    for (var i = 0; i < maxLen; i++) {
      // If there are no more original args, or if the original arg was blank
      // (i.e. not typed by the user), we'll just need to add at the end
      if (i >= originalArgs.length || originalArgs[i].type === 'BlankArgument') {
        this._args.push(replacementArgs[i]);
        continue;
      }

      var index = this._args.indexOf(originalArgs[i]);
      if (index === -1) {
        console.error('Couldn\'t find ', originalArgs[i], ' in ', this._args);
        throw new Error('Couldn\'t find ' + originalArgs[i]);
      }

      // If there are no more replacement args, we just remove the original args
      // Otherwise swap original args and replacements
      if (i >= replacementArgs.length) {
        this._args.splice(index, 1);
      }
      else {
        if (options.matchPadding) {
          if (replacementArgs[i].prefix.length === 0 &&
              this._args[index].prefix.length !== 0) {
            replacementArgs[i].prefix = this._args[index].prefix;
          }
          if (replacementArgs[i].suffix.length === 0 &&
              this._args[index].suffix.length !== 0) {
            replacementArgs[i].suffix = this._args[index].suffix;
          }
        }
        this._args[index] = replacementArgs[i];
      }
    }
  }

  function setAssignmentInternal(conversion) {
    var oldConversion = assignment.conversion;

    assignment.conversion = conversion;
    assignment.conversion.assign(assignment);

    if (assignment.conversion.equals(oldConversion)) {
      return;
    }

    assignment.onAssignmentChange({
      assignment: assignment,
      conversion: assignment.conversion,
      oldConversion: oldConversion
    });
  }

  if (arg == null) {
    setAssignmentInternal(assignment.param.type.getBlank());
  }
  else if (typeof arg.getStatus === 'function') {
    setAssignmentInternal(arg);
  }
  else {
    var parsed = assignment.param.type.parse(arg, this.executionContext);
    return parsed.then(function(conversion) {
      setAssignmentInternal(conversion);
    }.bind(this));
  }

  return Promise.resolve(undefined);
};

/**
 * Reset all the assignments to their default values
 */
Requisition.prototype.setBlankArguments = function() {
  this.getAssignments().forEach(function(assignment) {
    var promise = this.setAssignment(assignment, null, { skipArgUpdate: true });
    util.synchronize(promise);
  }, this);
};

/**
 * Complete the argument at <tt>cursor</tt>.
 * Basically the same as:
 *   assignment = getAssignmentAt(cursor);
 *   assignment.value = assignment.conversion.predictions[0];
 * Except it's done safely, and with particular care to where we place the
 * space, which is complex, and annoying if we get it wrong.
 *
 * WARNING: complete() can happen asynchronously.
 *
 * @param cursor The cursor configuration. Should have start and end properties
 * which should be set to start and end of the selection.
 * @param predictionChoice The index of the prediction that we should choose.
 * This number is not bounded by the size of the prediction array, we take the
 * modulus to get it within bounds
 * @return A promise which completes (with undefined) when any outstanding
 * completion tasks are done.
 */
Requisition.prototype.complete = function(cursor, predictionChoice) {
  var assignment = this.getAssignmentAt(cursor.start);

  var predictionPromise = assignment.getPredictionAt(predictionChoice);
  return predictionPromise.then(function(prediction) {
    var outstanding = [];
    this.onTextChange.holdFire();

    // Note: Since complete is asynchronous we should perhaps have a system to
    // bail out of making changes if the command line has changed since TAB
    // was pressed. It's not yet clear if this will be a problem.

    if (prediction == null) {
      // No predictions generally means we shouldn't change anything on TAB,
      // but TAB has the connotation of 'next thing' and when we're at the end
      // of a thing that implies that we should add a space. i.e.
      // 'help<TAB>' -> 'help '
      // But we should only do this if the thing that we're 'completing' is
      // valid and doesn't already end in a space.
      if (assignment.arg.suffix.slice(-1) !== ' ' &&
              assignment.getStatus() === Status.VALID) {
        outstanding.push(this._addSpace(assignment));
      }

      // Also add a space if we are in the name part of an assignment, however
      // this time we don't want the 'push the space to the next assignment'
      // logic, so we don't use addSpace
      if (assignment.isInName()) {
        var newArg = assignment.conversion.arg.beget({ prefixPostSpace: true });
        var p = this.setAssignment(assignment, newArg);
        outstanding.push(p);
      }
    }
    else {
      // Mutate this argument to hold the completion
      var arg = assignment.arg.beget({
        text: prediction.name,
        dontQuote: (assignment === this.commandAssignment)
      });
      var promise = this.setAssignment(assignment, arg);

      if (!prediction.incomplete) {
        promise = promise.then(function() {
          // The prediction is complete, add a space to let the user move-on
          return this._addSpace(assignment).then(function() {
            // Bug 779443 - Remove or explain the re-parse
            if (assignment instanceof UnassignedAssignment) {
              return this.update(this.toString());
            }
          }.bind(this));
        }.bind(this));
      }

      outstanding.push(promise);
    }

    return util.all(outstanding).then(function() {
      this.onTextChange();
      this.onTextChange.resumeFire();
    }.bind(this));
  }.bind(this));
};

/**
 * A test method to check that all args are assigned in some way
 */
Requisition.prototype._assertArgsAssigned = function() {
  this._args.forEach(function(arg) {
    if (arg.assignment == null) {
      console.log('No assignment for ' + arg);
    }
  }, this);
};

/**
 * Pressing TAB sometimes requires that we add a space to denote that we're on
 * to the 'next thing'.
 * @param assignment The assignment to which to append the space
 */
Requisition.prototype._addSpace = function(assignment) {
  var arg = assignment.conversion.arg.beget({ suffixSpace: true });
  if (arg !== assignment.conversion.arg) {
    return this.setAssignment(assignment, arg);
  }
  else {
    return Promise.resolve(undefined);
  }
};

/**
 * Replace the current value with the lower value if such a concept exists.
 */
Requisition.prototype.decrement = function(assignment) {
  var replacement = assignment.param.type.decrement(assignment.conversion.value,
                                                    this.executionContext);
  if (replacement != null) {
    var str = assignment.param.type.stringify(replacement,
                                              this.executionContext);
    var arg = assignment.conversion.arg.beget({ text: str });
    var promise = this.setAssignment(assignment, arg);
    util.synchronize(promise);
  }
};

/**
 * Replace the current value with the higher value if such a concept exists.
 */
Requisition.prototype.increment = function(assignment) {
  var replacement = assignment.param.type.increment(assignment.conversion.value,
                                                    this.executionContext);
  if (replacement != null) {
    var str = assignment.param.type.stringify(replacement,
                                              this.executionContext);
    var arg = assignment.conversion.arg.beget({ text: str });
    var promise = this.setAssignment(assignment, arg);
    util.synchronize(promise);
  }
};

/**
 * Extract a canonical version of the input
 */
Requisition.prototype.toCanonicalString = function() {
  var line = [];

  var cmd = this.commandAssignment.value ?
      this.commandAssignment.value.name :
      this.commandAssignment.arg.text;
  line.push(cmd);

  Object.keys(this._assignments).forEach(function(name) {
    var assignment = this._assignments[name];
    var type = assignment.param.type;
    // Bug 664377: This will cause problems if there is a non-default value
    // after a default value. Also we need to decide when to use
    // named parameters in place of positional params. Both can wait.
    if (assignment.value !== assignment.param.defaultValue) {
      line.push(' ');
      line.push(type.stringify(assignment.value, this.executionContext));
    }
  }, this);

  // Canonically, if we've opened with a { then we should have a } to close
  if (cmd === '{') {
    if (this.getAssignment(0).arg.suffix.indexOf('}') === -1) {
      line.push(' }');
    }
  }

  return line.join('');
};

/**
 * Input trace gives us an array of Argument tracing objects, one for each
 * character in the typed input, from which we can derive information about how
 * to display this typed input. It's a bit like toString on steroids.
 * <p>
 * The returned object has the following members:<ul>
 * <li>character: The character to which this arg trace refers.
 * <li>arg: The Argument to which this character is assigned.
 * <li>part: One of ['prefix'|'text'|suffix'] - how was this char understood
 * </ul>
 * <p>
 * The Argument objects are as output from #tokenize() rather than as applied
 * to Assignments by #_assign() (i.e. they are not instances of NamedArgument,
 * ArrayArgument, etc).
 * <p>
 * To get at the arguments applied to the assignments simply call
 * <tt>arg.assignment.arg</tt>. If <tt>arg.assignment.arg !== arg</tt> then
 * the arg applied to the assignment will contain the original arg.
 * See #_assign() for details.
 */
Requisition.prototype.createInputArgTrace = function() {
  if (!this._args) {
    throw new Error('createInputMap requires a command line. See source.');
    // If this is a problem then we can fake command line input using
    // something like the code in #toCanonicalString().
  }

  var args = [];
  var i;
  this._args.forEach(function(arg) {
    for (i = 0; i < arg.prefix.length; i++) {
      args.push({ arg: arg, character: arg.prefix[i], part: 'prefix' });
    }
    for (i = 0; i < arg.text.length; i++) {
      args.push({ arg: arg, character: arg.text[i], part: 'text' });
    }
    for (i = 0; i < arg.suffix.length; i++) {
      args.push({ arg: arg, character: arg.suffix[i], part: 'suffix' });
    }
  });

  return args;
};

/**
 * Reconstitute the input from the args
 */
Requisition.prototype.toString = function() {
  if (this._args) {
    return this._args.map(function(arg) {
      return arg.toString();
    }).join('');
  }

  return this.toCanonicalString();
};

/**
 * If the last character is whitespace then things that we suggest to add to
 * the end don't need a space prefix.
 * While this is quite a niche function, it has 2 benefits:
 * - it's more correct because we can distinguish between final whitespace that
 *   is part of an unclosed string, and parameter separating whitespace.
 * - also it's faster than toString() the whole thing and checking the end char
 * @return true iff the last character is interpreted as parameter separating
 * whitespace
 */
Requisition.prototype.typedEndsWithSeparator = function() {
  // This is not as easy as doing (this.toString().slice(-1) === ' ')
  // See the doc comments above; We're checking for separators, not spaces
  if (this._args) {
    var lastArg = this._args.slice(-1)[0];
    if (lastArg.suffix.slice(-1) === ' ') {
      return true;
    }
    return lastArg.text === '' && lastArg.suffix === ''
        && lastArg.prefix.slice(-1) === ' ';
  }

  return this.toCanonicalString().slice(-1) === ' ';
};

/**
 * Return an array of Status scores so we can create a marked up
 * version of the command line input.
 * @param cursor We only take a status of INCOMPLETE to be INCOMPLETE when the
 * cursor is actually in the argument. Otherwise it's an error.
 * @return Array of objects each containing <tt>status</tt> property and a
 * <tt>string</tt> property containing the characters to which the status
 * applies. Concatenating the strings in order gives the original input.
 */
Requisition.prototype.getInputStatusMarkup = function(cursor) {
  var argTraces = this.createInputArgTrace();
  // Generally the 'argument at the cursor' is the argument before the cursor
  // unless it is before the first char, in which case we take the first.
  cursor = cursor === 0 ? 0 : cursor - 1;
  var cTrace = argTraces[cursor];

  var markup = [];
  for (var i = 0; i < argTraces.length; i++) {
    var argTrace = argTraces[i];
    var arg = argTrace.arg;
    var status = Status.VALID;
    if (argTrace.part === 'text') {
      status = arg.assignment.getStatus(arg);
      // Promote INCOMPLETE to ERROR  ...
      if (status === Status.INCOMPLETE) {
        // If the cursor is in the prefix or suffix of an argument then we
        // don't consider it in the argument for the purposes of preventing
        // the escalation to ERROR. However if this is a NamedArgument, then we
        // allow the suffix (as space between 2 parts of the argument) to be in.
        // We use arg.assignment.arg not arg because we're looking at the arg
        // that got put into the assignment not as returned by tokenize()
        var isNamed = (cTrace.arg.assignment.arg.type === 'NamedArgument');
        var isInside = cTrace.part === 'text' ||
                        (isNamed && cTrace.part === 'suffix');
        if (arg.assignment !== cTrace.arg.assignment || !isInside) {
          // And if we're not in the command
          if (!(arg.assignment instanceof CommandAssignment)) {
            status = Status.ERROR;
          }
        }
      }
    }

    markup.push({ status: status, string: argTrace.character });
  }

  // De-dupe: merge entries where 2 adjacent have same status
  var i = 0;
  while (i < markup.length - 1) {
    if (markup[i].status === markup[i + 1].status) {
      markup[i].string += markup[i + 1].string;
      markup.splice(i + 1, 1);
    }
    else {
      i++;
    }
  }

  return markup;
};

/**
 * Look through the arguments attached to our assignments for the assignment
 * at the given position.
 * @param {number} cursor The cursor position to query
 */
Requisition.prototype.getAssignmentAt = function(cursor) {
  if (!this._args) {
    console.trace();
    throw new Error('Missing args');
  }

  // We short circuit this one because we may have no args, or no args with
  // any size and the alg below only finds arguments with size.
  if (cursor === 0) {
    return this.commandAssignment;
  }

  var assignForPos = [];
  var i, j;
  for (i = 0; i < this._args.length; i++) {
    var arg = this._args[i];
    var assignment = arg.assignment;

    // prefix and text are clearly part of the argument
    for (j = 0; j < arg.prefix.length; j++) {
      assignForPos.push(assignment);
    }
    for (j = 0; j < arg.text.length; j++) {
      assignForPos.push(assignment);
    }

    // suffix is part of the argument only if this is a named parameter,
    // otherwise it looks forwards
    if (arg.assignment.arg.type === 'NamedArgument') {
      // leave the argument as it is
    }
    else if (this._args.length > i + 1) {
      // first to the next argument
      assignment = this._args[i + 1].assignment;
    }
    else {
      // then to the first blank positional parameter, leaving 'as is' if none
      var nextAssignment = this._getFirstBlankPositionalAssignment();
      if (nextAssignment != null) {
        assignment = nextAssignment;
      }
    }

    for (j = 0; j < arg.suffix.length; j++) {
      assignForPos.push(assignment);
    }
  }

  // Possible shortcut, we don't really need to go through all the args
  // to work out the solution to this

  var reply = assignForPos[cursor - 1];

  if (!reply) {
    throw new Error('Missing assignment.' +
        ' cursor=' + cursor + ' text=' + this.toString());
  }

  return reply;
};

/**
 * Entry point for keyboard accelerators or anything else that wants to execute
 * a command.
 * @param options Object describing how the execution should be handled.
 * (optional). Contains some of the following properties:
 * - hidden (boolean, default=false) Should the output be hidden from the
 *   commandOutputManager for this requisition
 * - command/args A fast shortcut to executing a known command with a known
 *   set of parsed arguments.
 * - typed (string, deprecated) Don't use this. Also don't set the options
 *   object itself to be a string.
 */
Requisition.prototype.exec = function(options) {
  var command = null;
  var args = null;
  var hidden = false;
  if (options && options.hidden) {
    hidden = true;
  }

  if (options) {
    if (typeof input === 'string') {
      // Deprecated - does not handle async properly
      this.update(options);
    }
    else if (typeof options.typed === 'string') {
      // Deprecated - does not handle async properly
      this.update(options.typed);
    }
    else if (options.command != null) {
      // Fast track by looking up the command directly since passed args
      // means there is no command line to parse.
      command = canon.getCommand(options.command);
      if (!command) {
        console.error('Command not found: ' + options.command);
      }
      args = options.args;
    }
  }

  if (!command) {
    command = this.commandAssignment.value;
    args = this.getArgsObject();
  }

  if (!command) {
    throw new Error('Unknown command');
  }

  // Display JavaScript input without the initial { or closing }
  var typed = this.toString();
  if (evalCommandSpec.evalRegexp.test(typed)) {
    typed = typed.replace(evalCommandSpec.evalRegexp, '');
    // Bug 717763: What if the JavaScript naturally ends with a }?
    typed = typed.replace(/\s*}\s*$/, '');
  }

  var output = new Output({
    command: command,
    args: args,
    typed: typed,
    canonical: this.toCanonicalString(),
    hidden: hidden
  });

  this.commandOutputManager.onOutput({ output: output });

  var onDone = function(data) {
    output.complete(data, false);
  };

  var onError = function(ex) {
    if (exports.logErrors) {
      util.errorHandler(ex);
    }

    var data = ex.isTypedData ? ex : {
      isTypedData: true,
      data: ex,
      type: 'error'
    };
    output.complete(data, true);
  };

  try {
    var reply = command.exec(args, this.executionContext);
    Promise.resolve(reply).then(onDone, onError);
  }
  catch (ex) {
    onError(ex);
  }

  this.clear();
  return output;
};

/**
 * A shortcut for calling update, resolving the promise and then exec.
 * @param input The string to execute
 * @param options Passed to exec
 * @return A promise of an output object
 */
Requisition.prototype.updateExec = function(input, options) {
  return this.update(input).then(function() {
    return this.exec(options);
  }.bind(this));
};

/**
 * Similar to update('') except that it's guaranteed to execute synchronously
 */
Requisition.prototype.clear = function() {
  this._structuralChangeInProgress = true;

  var arg = new Argument('', '', '');
  this._args = [ arg ];

  var commandType = this.commandAssignment.param.type;
  var promise = commandType.parse(arg, this.executionContext);
  this.setAssignment(this.commandAssignment,
                     util.synchronize(promise),
                     { skipArgUpdate: true });

  this._structuralChangeInProgress = false;
  this.onTextChange();
};

/**
 * Helper to find the 'data-command' attribute, used by |update()|
 */
function getDataCommandAttribute(element) {
  var command = element.getAttribute('data-command');
  if (!command) {
    command = element.querySelector('*[data-command]')
                     .getAttribute('data-command');
  }
  return command;
}

/**
 * Called by the UI when ever the user interacts with a command line input
 * @param typed The contents of the input field
 */
Requisition.prototype.update = function(typed) {
  if (typeof HTMLElement !== 'undefined' && typed instanceof HTMLElement) {
    typed = getDataCommandAttribute(typed);
  }
  if (typeof Event !== 'undefined' && typed instanceof Event) {
    typed = getDataCommandAttribute(typed.currentTarget);
  }

  this._structuralChangeInProgress = true;

  this._args = exports.tokenize(typed);
  var args = this._args.slice(0); // i.e. clone

  return this._split(args).then(function() {
    return this._assign(args).then(function() {
      this._structuralChangeInProgress = false;
      this.onTextChange();
    }.bind(this));
  }.bind(this));
};

/**
 * For test/debug use only. The output from this function is subject to wanton
 * random change without notice, and should not be relied upon to even exist
 * at some later date.
 */
Object.defineProperty(Requisition.prototype, '_summaryJson', {
  get: function() {
    var summary = {
      $args: this._args.map(function(arg) {
        return arg._summaryJson;
      }),
      _command: this.commandAssignment._summaryJson,
      _unassigned: this._unassigned.forEach(function(assignment) {
        return assignment._summaryJson;
      })
    };

    Object.keys(this._assignments).forEach(function(name) {
      summary[name] = this.getAssignment(name)._summaryJson;
    }.bind(this));

    return summary;
  },
  enumerable: true
});

/**
 * tokenize() is a state machine. These are the states.
 */
var In = {
  /**
   * The last character was ' '.
   * Typing a ' ' character will not change the mode
   * Typing one of '"{ will change mode to SINGLE_Q, DOUBLE_Q or SCRIPT.
   * Anything else goes into SIMPLE mode.
   */
  WHITESPACE: 1,

  /**
   * The last character was part of a parameter.
   * Typing ' ' returns to WHITESPACE mode. Any other character
   * (including '"{} which are otherwise special) does not change the mode.
   */
  SIMPLE: 2,

  /**
   * We're inside single quotes: '
   * Typing ' returns to WHITESPACE mode. Other characters do not change mode.
   */
  SINGLE_Q: 3,

  /**
   * We're inside double quotes: "
   * Typing " returns to WHITESPACE mode. Other characters do not change mode.
   */
  DOUBLE_Q: 4,

  /**
   * We're inside { and }
   * Typing } returns to WHITESPACE mode. Other characters do not change mode.
   * SCRIPT mode is slightly different from other modes in that spaces between
   * the {/} delimiters and the actual input are not considered significant.
   * e.g: " x " is a 3 character string, delimited by double quotes, however
   * { x } is a 1 character JavaScript surrounded by whitespace and {}
   * delimiters.
   * In the short term we assume that the JS routines can make sense of the
   * extra whitespace, however at some stage we may need to move the space into
   * the Argument prefix/suffix.
   * Also we don't attempt to handle nested {}. See bug 678961
   */
  SCRIPT: 5
};

/**
 * Split up the input taking into account ', " and {.
 * We don't consider \t or other classical whitespace characters to split
 * arguments apart. For one thing these characters are hard to type, but also
 * if the user has gone to the trouble of pasting a TAB character into the
 * input field (or whatever it takes), they probably mean it.
 */
exports.tokenize = function(typed) {
  // For blank input, place a dummy empty argument into the list
  if (typed == null || typed.length === 0) {
    return [ new Argument('', '', '') ];
  }

  if (isSimple(typed)) {
    return [ new Argument(typed, '', '') ];
  }

  var mode = In.WHITESPACE;

  // First we un-escape. This list was taken from:
  // https://developer.mozilla.org/en/Core_JavaScript_1.5_Guide/Core_Language_Features#Unicode
  // We are generally converting to their real values except for the strings
  // '\'', '\"', '\ ', '{' and '}' which we are converting to unicode private
  // characters so we can distinguish them from '"', ' ', '{', '}' and ''',
  // which are special. They need swapping back post-split - see unescape2()
  typed = typed
      .replace(/\\\\/g, '\\')
      .replace(/\\b/g, '\b')
      .replace(/\\f/g, '\f')
      .replace(/\\n/g, '\n')
      .replace(/\\r/g, '\r')
      .replace(/\\t/g, '\t')
      .replace(/\\v/g, '\v')
      .replace(/\\n/g, '\n')
      .replace(/\\r/g, '\r')
      .replace(/\\ /g, '\uF000')
      .replace(/\\'/g, '\uF001')
      .replace(/\\"/g, '\uF002')
      .replace(/\\{/g, '\uF003')
      .replace(/\\}/g, '\uF004');

  function unescape2(escaped) {
    return escaped
        .replace(/\uF000/g, ' ')
        .replace(/\uF001/g, '\'')
        .replace(/\uF002/g, '"')
        .replace(/\uF003/g, '{')
        .replace(/\uF004/g, '}');
  }

  var i = 0;          // The index of the current character
  var start = 0;      // Where did this section start?
  var prefix = '';    // Stuff that comes before the current argument
  var args = [];      // The array that we're creating
  var blockDepth = 0; // For JS with nested {}

  // This is just a state machine. We're going through the string char by char
  // The 'mode' is one of the 'In' states. As we go, we're adding Arguments
  // to the 'args' array.

  while (true) {
    var c = typed[i];
    var str;
    switch (mode) {
      case In.WHITESPACE:
        if (c === '\'') {
          prefix = typed.substring(start, i + 1);
          mode = In.SINGLE_Q;
          start = i + 1;
        }
        else if (c === '"') {
          prefix = typed.substring(start, i + 1);
          mode = In.DOUBLE_Q;
          start = i + 1;
        }
        else if (c === '{') {
          prefix = typed.substring(start, i + 1);
          mode = In.SCRIPT;
          blockDepth++;
          start = i + 1;
        }
        else if (/ /.test(c)) {
          // Still whitespace, do nothing
        }
        else {
          prefix = typed.substring(start, i);
          mode = In.SIMPLE;
          start = i;
        }
        break;

      case In.SIMPLE:
        // There is an edge case of xx'xx which we are assuming to
        // be a single parameter (and same with ")
        if (c === ' ') {
          str = unescape2(typed.substring(start, i));
          args.push(new Argument(str, prefix, ''));
          mode = In.WHITESPACE;
          start = i;
          prefix = '';
        }
        break;

      case In.SINGLE_Q:
        if (c === '\'') {
          str = unescape2(typed.substring(start, i));
          args.push(new Argument(str, prefix, c));
          mode = In.WHITESPACE;
          start = i + 1;
          prefix = '';
        }
        break;

      case In.DOUBLE_Q:
        if (c === '"') {
          str = unescape2(typed.substring(start, i));
          args.push(new Argument(str, prefix, c));
          mode = In.WHITESPACE;
          start = i + 1;
          prefix = '';
        }
        break;

      case In.SCRIPT:
        if (c === '{') {
          blockDepth++;
        }
        else if (c === '}') {
          blockDepth--;
          if (blockDepth === 0) {
            str = unescape2(typed.substring(start, i));
            args.push(new ScriptArgument(str, prefix, c));
            mode = In.WHITESPACE;
            start = i + 1;
            prefix = '';
          }
        }
        break;
    }

    i++;

    if (i >= typed.length) {
      // There is nothing else to read - tidy up
      if (mode === In.WHITESPACE) {
        if (i !== start) {
          // There's whitespace at the end of the typed string. Add it to the
          // last argument's suffix, creating an empty argument if needed.
          var extra = typed.substring(start, i);
          var lastArg = args[args.length - 1];
          if (!lastArg) {
            args.push(new Argument('', extra, ''));
          }
          else {
            lastArg.suffix += extra;
          }
        }
      }
      else if (mode === In.SCRIPT) {
        str = unescape2(typed.substring(start, i + 1));
        args.push(new ScriptArgument(str, prefix, ''));
      }
      else {
        str = unescape2(typed.substring(start, i + 1));
        args.push(new Argument(str, prefix, ''));
      }
      break;
    }
  }

  return args;
};

/**
 * If the input has no spaces, quotes, braces or escapes,
 * we can take the fast track.
 */
function isSimple(typed) {
   for (var i = 0; i < typed.length; i++) {
     var c = typed.charAt(i);
     if (c === ' ' || c === '"' || c === '\'' ||
         c === '{' || c === '}' || c === '\\') {
       return false;
     }
   }
   return true;
}

/**
 * Looks in the canon for a command extension that matches what has been
 * typed at the command line.
 */
Requisition.prototype._split = function(args) {
  // We're processing args, so we don't want the assignments that we make to
  // try to adjust other args assuming this is an external update
  var noArgUp = { skipArgUpdate: true };

  // Handle the special case of the user typing { javascript(); }
  // We use the hidden 'eval' command directly rather than shift()ing one of
  // the parameters, and parse()ing it.
  var conversion = undefined;
  if (args[0].type === 'ScriptArgument') {
    // Special case: if the user enters { console.log('foo'); } then we need to
    // use the hidden 'eval' command
    conversion = new Conversion(evalCommand, new ScriptArgument());
    return this.setAssignment(this.commandAssignment, conversion, noArgUp);
  }

  var argsUsed = 1;

  var promise;
  var commandType = this.commandAssignment.param.type;
  while (argsUsed <= args.length) {
    var arg = (argsUsed === 1) ?
              args[0] :
              new MergedArgument(args, 0, argsUsed);

    // Making the commandType.parse() promise as synchronous is OK because we
    // know that commandType is a synchronous type.

    if (this.prefix != null && this.prefix != '') {
      var prefixArg = new Argument(this.prefix, '', ' ');
      var prefixedArg = new MergedArgument([ prefixArg, arg ]);

      promise = commandType.parse(prefixedArg, this.executionContext);
      conversion = util.synchronize(promise);

      if (conversion.value == null) {
        promise = commandType.parse(arg, this.executionContext);
        conversion = util.synchronize(promise);
      }
    }
    else {
      promise = commandType.parse(arg, this.executionContext);
      conversion = util.synchronize(promise);
    }

    // We only want to carry on if this command is a parent command,
    // which means that there is a commandAssignment, but not one with
    // an exec function.
    if (!conversion.value || conversion.value.exec) {
      break;
    }

    // Previously we needed a way to hide commands depending context.
    // We have not resurrected that feature yet, but if we do we should
    // insert code here to ignore certain commands depending on the
    // context/environment

    argsUsed++;
  }

  for (var i = 0; i < argsUsed; i++) {
    args.shift();
  }

  return this.setAssignment(this.commandAssignment, conversion, noArgUp);

  // This could probably be re-written to consume args as we go
};

/**
 * Add all the passed args to the list of unassigned assignments.
 */
Requisition.prototype._addUnassignedArgs = function(args) {
  args.forEach(function(arg) {
    this._unassigned.push(new UnassignedAssignment(this, arg));
  }.bind(this));
};

/**
 * Work out which arguments are applicable to which parameters.
 */
Requisition.prototype._assign = function(args) {
  // See comment in _split. Avoid multiple updates
  var noArgUp = { skipArgUpdate: true };

  this._unassigned = [];
  var outstanding = [];

  if (!this.commandAssignment.value) {
    this._addUnassignedArgs(args);
    return util.all(outstanding);
  }

  if (args.length === 0) {
    this.setBlankArguments();
    return util.all(outstanding);
  }

  // Create an error if the command does not take parameters, but we have
  // been given them ...
  if (this.assignmentCount === 0) {
    this._addUnassignedArgs(args);
    return util.all(outstanding);
  }

  // Special case: if there is only 1 parameter, and that's of type
  // text, then we put all the params into the first param
  if (this.assignmentCount === 1) {
    var assignment = this.getAssignment(0);
    if (assignment.param.type.name === 'string') {
      var arg = (args.length === 1) ? args[0] : new MergedArgument(args);
      outstanding.push(this.setAssignment(assignment, arg, noArgUp));
      return util.all(outstanding);
    }
  }

  // Positional arguments can still be specified by name, but if they are
  // then we need to ignore them when working them out positionally
  var unassignedParams = this.getParameterNames();

  // We collect the arguments used in arrays here before assigning
  var arrayArgs = {};

  // Extract all the named parameters
  this.getAssignments(false).forEach(function(assignment) {
    // Loop over the arguments
    // Using while rather than loop because we remove args as we go
    var i = 0;
    while (i < args.length) {
      if (assignment.param.isKnownAs(args[i].text)) {
        var arg = args.splice(i, 1)[0];
        unassignedParams = unassignedParams.filter(function(test) {
          return test !== assignment.param.name;
        });

        // boolean parameters don't have values, default to false
        if (assignment.param.type.name === 'boolean') {
          arg = new TrueNamedArgument(arg);
        }
        else {
          var valueArg = null;
          if (i + 1 <= args.length) {
            valueArg = args.splice(i, 1)[0];
          }
          arg = new NamedArgument(arg, valueArg);
        }

        if (assignment.param.type.name === 'array') {
          var arrayArg = arrayArgs[assignment.param.name];
          if (!arrayArg) {
            arrayArg = new ArrayArgument();
            arrayArgs[assignment.param.name] = arrayArg;
          }
          arrayArg.addArgument(arg);
        }
        else {
          outstanding.push(this.setAssignment(assignment, arg, noArgUp));
        }
      }
      else {
        // Skip this parameter and handle as a positional parameter
        i++;
      }
    }
  }, this);

  // What's left are positional parameters assign in order
  unassignedParams.forEach(function(name) {
    var assignment = this.getAssignment(name);

    // If not set positionally, and we can't set it non-positionally,
    // we have to default it to prevent previous values surviving
    if (!assignment.param.isPositionalAllowed) {
      outstanding.push(this.setAssignment(assignment, null, noArgUp));
      return;
    }

    // If this is a positional array argument, then it swallows the
    // rest of the arguments.
    if (assignment.param.type.name === 'array') {
      var arrayArg = arrayArgs[assignment.param.name];
      if (!arrayArg) {
        arrayArg = new ArrayArgument();
        arrayArgs[assignment.param.name] = arrayArg;
      }
      arrayArg.addArguments(args);
      args = [];
    }
    else {
      if (args.length === 0) {
        outstanding.push(this.setAssignment(assignment, null, noArgUp));
      }
      else {
        var arg = args.splice(0, 1)[0];
        // --foo and -f are named parameters, -4 is a number. So '-' is either
        // the start of a named parameter or a number depending on the context
        var isIncompleteName = assignment.param.type.name === 'number' ?
            /-[-a-zA-Z_]/.test(arg.text) :
            arg.text.charAt(0) === '-';

        if (isIncompleteName) {
          this._unassigned.push(new UnassignedAssignment(this, arg));
        }
        else {
          outstanding.push(this.setAssignment(assignment, arg, noArgUp));
        }
      }
    }
  }, this);

  // Now we need to assign the array argument (if any)
  Object.keys(arrayArgs).forEach(function(name) {
    var assignment = this.getAssignment(name);
    outstanding.push(this.setAssignment(assignment, arrayArgs[name], noArgUp));
  }, this);

  // What's left is can't be assigned, but we need to extract
  this._addUnassignedArgs(args);

  return util.all(outstanding);
};

exports.Requisition = Requisition;

/**
 * A simple object to hold information about the output of a command
 */
function Output(options) {
  options = options || {};
  this.command = options.command || '';
  this.args = options.args || {};
  this.typed = options.typed || '';
  this.canonical = options.canonical || '';
  this.hidden = options.hidden === true ? true : false;

  this.type = undefined;
  this.data = undefined;
  this.completed = false;
  this.error = false;
  this.start = new Date();

  this._deferred = Promise.defer();
  this.promise = this._deferred.promise;

  this.onClose = util.createEvent('Output.onClose');
}

/**
 * Called when there is data to display, and the command has finished executing
 * See changed() for details on parameters.
 */
Output.prototype.complete = function(data, error) {
  this.end = new Date();
  this.duration = this.end.getTime() - this.start.getTime();
  this.completed = true;
  this.error = error;

  if (data != null && data.isTypedData) {
    this.data = data.data;
    this.type = data.type;
  }
  else {
    this.data = data;
    this.type = this.command.returnType;
    if (this.type == null) {
      this.type = (this.data == null) ? 'undefined' : typeof this.data;
    }
  }

  if (this.type === 'object') {
    throw new Error('No type from output of ' + this.typed);
  }

  if (error) {
    this._deferred.reject();
  }
  else {
    this._deferred.resolve();
  }
};

exports.Output = Output;


});
define("text!gcli/ui/intro.html", [], "\n" +
  "<div>\n" +
  "  <p>${l10n.introTextOpening2}</p>\n" +
  "\n" +
  "  <p>\n" +
  "    ${l10n.introTextCommands}\n" +
  "    <span class=\"gcli-out-shortcut\" onclick=\"${onclick}\"\n" +
  "        ondblclick=\"${ondblclick}\" data-command=\"help\">help</span>${l10n.introTextKeys2}\n" +
  "    <code>${l10n.introTextF1Escape}</code>.\n" +
  "  </p>\n" +
  "\n" +
  "  <button onclick=\"${onGotIt}\" if=\"${showHideButton}\">${l10n.introTextGo}</button>\n" +
  "</div>\n" +
  "");

/*
 * Copyright 2012, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

define('gcli/ui/focus', ['require', 'exports', 'module' , 'util/util', 'util/l10n', 'gcli/settings', 'gcli/canon'], function(require, exports, module) {

'use strict';

var util = require('util/util');
var l10n = require('util/l10n');
var settings = require('gcli/settings');
var canon = require('gcli/canon');

/**
 * Record how much help the user wants from the tooltip
 */
var Eagerness = {
  NEVER: 1,
  SOMETIMES: 2,
  ALWAYS: 3
};
var eagerHelperSettingSpec = {
  name: 'eagerHelper',
  type: {
    name: 'selection',
    lookup: [
      { name: 'never', value: Eagerness.NEVER },
      { name: 'sometimes', value: Eagerness.SOMETIMES },
      { name: 'always', value: Eagerness.ALWAYS }
    ]
  },
  defaultValue: Eagerness.SOMETIMES,
  description: l10n.lookup('eagerHelperDesc'),
  ignoreTypeDifference: true
};
var eagerHelper;

/**
 * Register (and unregister) the hide-intro setting
 */
exports.startup = function() {
  eagerHelper = settings.addSetting(eagerHelperSettingSpec);
};

exports.shutdown = function() {
  settings.removeSetting(eagerHelperSettingSpec);
  eagerHelper = undefined;
};

/**
 * FocusManager solves the problem of tracking focus among a set of nodes.
 * The specific problem we are solving is when the hint element must be visible
 * if either the command line or any of the inputs in the hint element has the
 * focus, and invisible at other times, without hiding and showing the hint
 * element even briefly as the focus changes between them.
 * It does this simply by postponing the hide events by 250ms to see if
 * something else takes focus.
 * @param options Object containing user customization properties, including:
 * - blurDelay (default=150ms)
 * - debug (default=false)
 * @param components Object that links to other UI components. GCLI provided:
 * - document
 * - requisition
 */
function FocusManager(options, components) {
  options = options || {};

  this._document = components.document || document;
  this._requisition = components.requisition;

  this._debug = options.debug || false;
  this._blurDelay = options.blurDelay || 150;
  this._window = this._document.defaultView;

  this._requisition.commandOutputManager.onOutput.add(this._outputted, this);

  this._blurDelayTimeout = null; // Result of setTimeout in delaying a blur
  this._monitoredElements = [];  // See addMonitoredElement()

  this._isError = false;
  this._hasFocus = false;
  this._helpRequested = false;
  this._recentOutput = false;

  this.onVisibilityChange = util.createEvent('FocusManager.onVisibilityChange');

  this._focused = this._focused.bind(this);
  this._document.addEventListener('focus', this._focused, true);

  eagerHelper.onChange.add(this._eagerHelperChanged, this);

  this.isTooltipVisible = undefined;
  this.isOutputVisible = undefined;
  this._checkShow();
}

/**
 * Avoid memory leaks
 */
FocusManager.prototype.destroy = function() {
  eagerHelper.onChange.remove(this._eagerHelperChanged, this);

  this._document.removeEventListener('focus', this._focused, true);
  this._requisition.commandOutputManager.onOutput.remove(this._outputted, this);

  for (var i = 0; i < this._monitoredElements.length; i++) {
    var monitor = this._monitoredElements[i];
    console.error('Hanging monitored element: ', monitor.element);

    monitor.element.removeEventListener('focus', monitor.onFocus, true);
    monitor.element.removeEventListener('blur', monitor.onBlur, true);
  }

  if (this._blurDelayTimeout) {
    this._window.clearTimeout(this._blurDelayTimeout);
    this._blurDelayTimeout = null;
  }

  delete this._focused;
  delete this._document;
  delete this._window;
  delete this._requisition;
};

/**
 * The easy way to include an element in the set of things that are part of the
 * aggregate focus. Using [add|remove]MonitoredElement() is a simpler way of
 * option than calling report[Focus|Blur]()
 * @param element The element on which to track focus|blur events
 * @param where Optional source string for debugging only
 */
FocusManager.prototype.addMonitoredElement = function(element, where) {
  if (this._debug) {
    console.log('FocusManager.addMonitoredElement(' + (where || 'unknown') + ')');
  }

  var monitor = {
    element: element,
    where: where,
    onFocus: function() { this._reportFocus(where); }.bind(this),
    onBlur: function() { this._reportBlur(where); }.bind(this)
  };

  element.addEventListener('focus', monitor.onFocus, true);
  element.addEventListener('blur', monitor.onBlur, true);

  if (this._document.activeElement === element) {
    this._reportFocus(where);
  }

  this._monitoredElements.push(monitor);
};

/**
 * Undo the effects of addMonitoredElement()
 * @param element The element to stop tracking
 * @param where Optional source string for debugging only
 */
FocusManager.prototype.removeMonitoredElement = function(element, where) {
  if (this._debug) {
    console.log('FocusManager.removeMonitoredElement(' + (where || 'unknown') + ')');
  }

  var newMonitoredElements = this._monitoredElements.filter(function(monitor) {
    if (monitor.element === element) {
      element.removeEventListener('focus', monitor.onFocus, true);
      element.removeEventListener('blur', monitor.onBlur, true);
      return false;
    }
    return true;
  });

  this._monitoredElements = newMonitoredElements;
};

/**
 * Monitor for new command executions
 */
FocusManager.prototype.updatePosition = function(dimensions) {
  var ev = {
    tooltipVisible: this.isTooltipVisible,
    outputVisible: this.isOutputVisible,
    dimensions: dimensions
  };
  this.onVisibilityChange(ev);
};

/**
 * Monitor for new command executions
 */
FocusManager.prototype._outputted = function(ev) {
  this._recentOutput = true;
  this._helpRequested = false;
  this._checkShow();
};

/**
 * We take a focus event anywhere to be an indication that we might be about
 * to lose focus
 */
FocusManager.prototype._focused = function() {
  this._reportBlur('document');
};

/**
 * Some component has received a 'focus' event. This sets the internal status
 * straight away and informs the listeners
 * @param where Optional source string for debugging only
 */
FocusManager.prototype._reportFocus = function(where) {
  if (this._debug) {
    console.log('FocusManager._reportFocus(' + (where || 'unknown') + ')');
  }

  if (this._blurDelayTimeout) {
    if (this._debug) {
      console.log('FocusManager.cancelBlur');
    }
    this._window.clearTimeout(this._blurDelayTimeout);
    this._blurDelayTimeout = null;
  }

  if (!this._hasFocus) {
    this._hasFocus = true;
  }
  this._checkShow();
};

/**
 * Some component has received a 'blur' event. This waits for a while to see if
 * we are going to get any subsequent 'focus' events and then sets the internal
 * status and informs the listeners
 * @param where Optional source string for debugging only
 */
FocusManager.prototype._reportBlur = function(where) {
  if (this._debug) {
    console.log('FocusManager._reportBlur(' + where + ')');
  }

  if (this._hasFocus) {
    if (this._blurDelayTimeout) {
      if (this._debug) {
        console.log('FocusManager.blurPending');
      }
      return;
    }

    this._blurDelayTimeout = this._window.setTimeout(function() {
      if (this._debug) {
        console.log('FocusManager.blur');
      }
      this._hasFocus = false;
      this._checkShow();
      this._blurDelayTimeout = null;
    }.bind(this), this._blurDelay);
  }
};

/**
 * The setting has changed
 */
FocusManager.prototype._eagerHelperChanged = function() {
  this._checkShow();
};

/**
 * The inputter tells us about keyboard events so we can decide to delay
 * showing the tooltip element
 */
FocusManager.prototype.onInputChange = function() {
  this._recentOutput = false;
  this._checkShow();
};

/**
 * Generally called for something like a F1 key press, when the user explicitly
 * wants help
 */
FocusManager.prototype.helpRequest = function() {
  if (this._debug) {
    console.log('FocusManager.helpRequest');
  }

  this._helpRequested = true;
  this._recentOutput = false;
  this._checkShow();
};

/**
 * Generally called for something like a ESC key press, when the user explicitly
 * wants to get rid of the help
 */
FocusManager.prototype.removeHelp = function() {
  if (this._debug) {
    console.log('FocusManager.removeHelp');
  }

  this._importantFieldFlag = false;
  this._isError = false;
  this._helpRequested = false;
  this._recentOutput = false;
  this._checkShow();
};

/**
 * Set to true whenever a field thinks it's output is important
 */
FocusManager.prototype.setImportantFieldFlag = function(flag) {
  if (this._debug) {
    console.log('FocusManager.setImportantFieldFlag', flag);
  }
  this._importantFieldFlag = flag;
  this._checkShow();
};

/**
 * Set to true whenever a field thinks it's output is important
 */
FocusManager.prototype.setError = function(isError) {
  if (this._debug) {
    console.log('FocusManager._isError', isError);
  }
  this._isError = isError;
  this._checkShow();
};

/**
 * Helper to compare the current showing state with the value calculated by
 * _shouldShow() and take appropriate action
 */
FocusManager.prototype._checkShow = function() {
  var fire = false;
  var ev = {
    tooltipVisible: this.isTooltipVisible,
    outputVisible: this.isOutputVisible
  };

  var showTooltip = this._shouldShowTooltip();
  if (this.isTooltipVisible !== showTooltip.visible) {
    ev.tooltipVisible = this.isTooltipVisible = showTooltip.visible;
    fire = true;
  }

  var showOutput = this._shouldShowOutput();
  if (this.isOutputVisible !== showOutput.visible) {
    ev.outputVisible = this.isOutputVisible = showOutput.visible;
    fire = true;
  }

  if (fire) {
    if (this._debug) {
      console.log('FocusManager.onVisibilityChange', ev);
    }
    this.onVisibilityChange(ev);
  }
};

/**
 * Calculate if we should be showing or hidden taking into account all the
 * available inputs
 */
FocusManager.prototype._shouldShowTooltip = function() {
  if (!this._hasFocus) {
    return { visible: false, reason: 'notHasFocus' };
  }

  if (eagerHelper.value === Eagerness.NEVER) {
    return { visible: false, reason: 'eagerHelperNever' };
  }

  if (eagerHelper.value === Eagerness.ALWAYS) {
    return { visible: true, reason: 'eagerHelperAlways' };
  }

  if (this._isError) {
    return { visible: true, reason: 'isError' };
  }

  if (this._helpRequested) {
    return { visible: true, reason: 'helpRequested' };
  }

  if (this._importantFieldFlag) {
    return { visible: true, reason: 'importantFieldFlag' };
  }

  return { visible: false, reason: 'default' };
};

/**
 * Calculate if we should be showing or hidden taking into account all the
 * available inputs
 */
FocusManager.prototype._shouldShowOutput = function() {
  if (!this._hasFocus) {
    return { visible: false, reason: 'notHasFocus' };
  }

  if (this._recentOutput) {
    return { visible: true, reason: 'recentOutput' };
  }

  return { visible: false, reason: 'default' };
};

exports.FocusManager = FocusManager;


});
/*
 * Copyright 2012, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

define('gcli/ui/fields/basic', ['require', 'exports', 'module' , 'util/util', 'util/l10n', 'gcli/argument', 'gcli/types', 'gcli/ui/fields'], function(require, exports, module) {

'use strict';

var util = require('util/util');
var l10n = require('util/l10n');

var Argument = require('gcli/argument').Argument;
var TrueNamedArgument = require('gcli/argument').TrueNamedArgument;
var FalseNamedArgument = require('gcli/argument').FalseNamedArgument;
var ArrayArgument = require('gcli/argument').ArrayArgument;
var ArrayConversion = require('gcli/types').ArrayConversion;

var Field = require('gcli/ui/fields').Field;
var fields = require('gcli/ui/fields');


/**
 * Registration and de-registration.
 */
exports.startup = function() {
  fields.addField(StringField);
  fields.addField(NumberField);
  fields.addField(BooleanField);
  fields.addField(DelegateField);
  fields.addField(ArrayField);
};

exports.shutdown = function() {
  fields.removeField(StringField);
  fields.removeField(NumberField);
  fields.removeField(BooleanField);
  fields.removeField(DelegateField);
  fields.removeField(ArrayField);
};


/**
 * A field that allows editing of strings
 */
function StringField(type, options) {
  Field.call(this, type, options);
  this.arg = new Argument();

  this.element = util.createElement(this.document, 'input');
  this.element.type = 'text';
  this.element.classList.add('gcli-field');

  this.onInputChange = this.onInputChange.bind(this);
  this.element.addEventListener('keyup', this.onInputChange, false);

  this.onFieldChange = util.createEvent('StringField.onFieldChange');
}

StringField.prototype = Object.create(Field.prototype);

StringField.prototype.destroy = function() {
  Field.prototype.destroy.call(this);
  this.element.removeEventListener('keyup', this.onInputChange, false);
  delete this.element;
  delete this.document;
  delete this.onInputChange;
};

StringField.prototype.setConversion = function(conversion) {
  this.arg = conversion.arg;
  this.element.value = conversion.arg.text;
  this.setMessage(conversion.message);
};

StringField.prototype.getConversion = function() {
  // This tweaks the prefix/suffix of the argument to fit
  this.arg = this.arg.beget({ text: this.element.value, prefixSpace: true });
  return this.type.parse(this.arg, this.requisition.context);
};

StringField.claim = function(type, context) {
  return type.name === 'string' ? Field.MATCH : Field.BASIC;
};


/**
 * A field that allows editing of numbers using an [input type=number] field
 */
function NumberField(type, options) {
  Field.call(this, type, options);
  this.arg = new Argument();

  this.element = util.createElement(this.document, 'input');
  this.element.type = 'number';
  if (this.type.max) {
    this.element.max = this.type.max;
  }
  if (this.type.min) {
    this.element.min = this.type.min;
  }
  if (this.type.step) {
    this.element.step = this.type.step;
  }

  this.onInputChange = this.onInputChange.bind(this);
  this.element.addEventListener('keyup', this.onInputChange, false);

  this.onFieldChange = util.createEvent('NumberField.onFieldChange');
}

NumberField.prototype = Object.create(Field.prototype);

NumberField.claim = function(type, context) {
  return type.name === 'number' ? Field.MATCH : Field.NO_MATCH;
};

NumberField.prototype.destroy = function() {
  Field.prototype.destroy.call(this);
  this.element.removeEventListener('keyup', this.onInputChange, false);
  delete this.element;
  delete this.document;
  delete this.onInputChange;
};

NumberField.prototype.setConversion = function(conversion) {
  this.arg = conversion.arg;
  this.element.value = conversion.arg.text;
  this.setMessage(conversion.message);
};

NumberField.prototype.getConversion = function() {
  this.arg = this.arg.beget({ text: this.element.value, prefixSpace: true });
  return this.type.parse(this.arg, this.requisition.context);
};


/**
 * A field that uses a checkbox to toggle a boolean field
 */
function BooleanField(type, options) {
  Field.call(this, type, options);

  this.name = options.name;
  this.named = options.named;

  this.element = util.createElement(this.document, 'input');
  this.element.type = 'checkbox';
  this.element.id = 'gcliForm' + this.name;

  this.onInputChange = this.onInputChange.bind(this);
  this.element.addEventListener('change', this.onInputChange, false);

  this.onFieldChange = util.createEvent('BooleanField.onFieldChange');
}

BooleanField.prototype = Object.create(Field.prototype);

BooleanField.claim = function(type, context) {
  return type.name === 'boolean' ? Field.MATCH : Field.NO_MATCH;
};

BooleanField.prototype.destroy = function() {
  Field.prototype.destroy.call(this);
  this.element.removeEventListener('change', this.onInputChange, false);
  delete this.element;
  delete this.document;
  delete this.onInputChange;
};

BooleanField.prototype.setConversion = function(conversion) {
  this.element.checked = conversion.value;
  this.setMessage(conversion.message);
};

BooleanField.prototype.getConversion = function() {
  var arg;
  if (this.named) {
    arg = this.element.checked ?
            new TrueNamedArgument(new Argument(' --' + this.name)) :
            new FalseNamedArgument();
  }
  else {
    arg = new Argument(' ' + this.element.checked);
  }
  return this.type.parse(arg, this.requisition.context);
};


/**
 * A field that works with delegate types by delaying resolution until that
 * last possible time
 */
function DelegateField(type, options) {
  Field.call(this, type, options);
  this.options = options;
  this.requisition.onAssignmentChange.add(this.update, this);

  this.element = util.createElement(this.document, 'div');
  this.update();

  this.onFieldChange = util.createEvent('DelegateField.onFieldChange');
}

DelegateField.prototype = Object.create(Field.prototype);

DelegateField.prototype.update = function() {
  var subtype = this.type.delegateType();
  if (subtype === this.subtype) {
    return;
  }

  if (this.field) {
    this.field.onFieldChange.remove(this.fieldChanged, this);
    this.field.destroy();
  }

  this.subtype = subtype;
  this.field = fields.getField(subtype, this.options);
  this.field.onFieldChange.add(this.fieldChanged, this);

  util.clearElement(this.element);
  this.element.appendChild(this.field.element);
};

DelegateField.claim = function(type, context) {
  return type.isDelegate ? Field.MATCH : Field.NO_MATCH;
};

DelegateField.prototype.destroy = function() {
  Field.prototype.destroy.call(this);
  this.requisition.onAssignmentChange.remove(this.update, this);
  delete this.element;
  delete this.document;
  delete this.onInputChange;
};

DelegateField.prototype.setConversion = function(conversion) {
  this.field.setConversion(conversion);
};

DelegateField.prototype.getConversion = function() {
  return this.field.getConversion();
};

Object.defineProperty(DelegateField.prototype, 'isImportant', {
  get: function() {
    return this.field.isImportant;
  },
  enumerable: true
});


/**
 * Adds add/delete buttons to a normal field allowing there to be many values
 * given for a parameter.
 */
function ArrayField(type, options) {
  Field.call(this, type, options);
  this.options = options;

  this._onAdd = this._onAdd.bind(this);
  this.members = [];

  // <div class=gcliArrayParent save="${element}">
  this.element = util.createElement(this.document, 'div');
  this.element.classList.add('gcli-array-parent');

  // <button class=gcliArrayMbrAdd onclick="${_onAdd}" save="${addButton}">Add
  this.addButton = util.createElement(this.document, 'button');
  this.addButton.classList.add('gcli-array-member-add');
  this.addButton.addEventListener('click', this._onAdd, false);
  this.addButton.textContent = l10n.lookup('fieldArrayAdd');
  this.element.appendChild(this.addButton);

  // <div class=gcliArrayMbrs save="${mbrElement}">
  this.container = util.createElement(this.document, 'div');
  this.container.classList.add('gcli-array-members');
  this.element.appendChild(this.container);

  this.onInputChange = this.onInputChange.bind(this);

  this.onFieldChange = util.createEvent('ArrayField.onFieldChange');
}

ArrayField.prototype = Object.create(Field.prototype);

ArrayField.claim = function(type, context) {
  return type.name === 'array' ? Field.MATCH : Field.NO_MATCH;
};

ArrayField.prototype.destroy = function() {
  Field.prototype.destroy.call(this);
  this.addButton.removeEventListener('click', this._onAdd, false);
};

ArrayField.prototype.setConversion = function(conversion) {
  // BUG 653568: this is too brutal - it removes focus from any the current field
  util.clearElement(this.container);
  this.members = [];

  conversion.conversions.forEach(function(subConversion) {
    this._onAdd(null, subConversion);
  }, this);
};

ArrayField.prototype.getConversion = function() {
  var conversions = [];
  var arrayArg = new ArrayArgument();
  for (var i = 0; i < this.members.length; i++) {
    Promise.resolve(this.members[i].field.getConversion()).then(function(conversion) {
      conversions.push(conversion);
      arrayArg.addArgument(conversion.arg);
    }.bind(this), util.errorHandler);
  }
  return new ArrayConversion(conversions, arrayArg);
};

ArrayField.prototype._onAdd = function(ev, subConversion) {
  // <div class=gcliArrayMbr save="${element}">
  var element = util.createElement(this.document, 'div');
  element.classList.add('gcli-array-member');
  this.container.appendChild(element);

  // ${field.element}
  var field = fields.getField(this.type.subtype, this.options);
  field.onFieldChange.add(function() {
    Promise.resolve(this.getConversion()).then(function(conversion) {
      this.onFieldChange({ conversion: conversion });
      this.setMessage(conversion.message);
    }.bind(this), util.errorHandler);
  }, this);

  if (subConversion) {
    field.setConversion(subConversion);
  }
  element.appendChild(field.element);

  // <div class=gcliArrayMbrDel onclick="${_onDel}">
  var delButton = util.createElement(this.document, 'button');
  delButton.classList.add('gcli-array-member-del');
  delButton.addEventListener('click', this._onDel, false);
  delButton.textContent = l10n.lookup('fieldArrayDel');
  element.appendChild(delButton);

  var member = {
    element: element,
    field: field,
    parent: this
  };
  member.onDelete = function() {
    this.parent.container.removeChild(this.element);
    this.parent.members = this.parent.members.filter(function(test) {
      return test !== this;
    });
    this.parent.onInputChange();
  }.bind(member);
  delButton.addEventListener('click', member.onDelete, false);

  this.members.push(member);
};


});
/*
 * Copyright 2012, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

define('gcli/ui/fields', ['require', 'exports', 'module' , 'util/promise', 'util/util'], function(require, exports, module) {

'use strict';

var Promise = require('util/promise');
var util = require('util/util');
var KeyEvent = require('util/util').KeyEvent;

/**
 * A Field is a way to get input for a single parameter.
 * This class is designed to be inherited from. It's important that all
 * subclasses have a similar constructor signature because they are created
 * via getField(...)
 * @param type The type to use in conversions
 * @param options A set of properties to help fields configure themselves:
 * - document: The document we use in calling createElement
 * - named: Is this parameter named? That is to say, are positional
 *         arguments disallowed, if true, then we need to provide updates to
 *         the command line that explicitly name the parameter in use
 *         (e.g. --verbose, or --name Fred rather than just true or Fred)
 * - name: If this parameter is named, what name should we use
 * - requisition: The requisition that we're attached to
 * - required: Boolean to indicate if this is a mandatory field
 */
function Field(type, options) {
  this.type = type;
  this.document = options.document;
  this.requisition = options.requisition;
}

/**
 * Subclasses should assign their element with the DOM node that gets added
 * to the 'form'. It doesn't have to be an input node, just something that
 * contains it.
 */
Field.prototype.element = undefined;

/**
 * Indicates that this field should drop any resources that it has created
 */
Field.prototype.destroy = function() {
  delete this.messageElement;
};

// Note: We could/should probably change Fields from working with Conversions
// to working with Arguments (Tokens), which makes for less calls to parse()

/**
 * Update this field display with the value from this conversion.
 * Subclasses should provide an implementation of this function.
 */
Field.prototype.setConversion = function(conversion) {
  throw new Error('Field should not be used directly');
};

/**
 * Extract a conversion from the values in this field.
 * Subclasses should provide an implementation of this function.
 */
Field.prototype.getConversion = function() {
  throw new Error('Field should not be used directly');
};

/**
 * Set the element where messages and validation errors will be displayed
 * @see setMessage()
 */
Field.prototype.setMessageElement = function(element) {
  this.messageElement = element;
};

/**
 * Display a validation message in the UI
 */
Field.prototype.setMessage = function(message) {
  if (this.messageElement) {
    util.setTextContent(this.messageElement, message || '');
  }
};

/**
 * Method to be called by subclasses when their input changes, which allows us
 * to properly pass on the onFieldChange event.
 */
Field.prototype.onInputChange = function(ev) {
  Promise.resolve(this.getConversion()).then(function(conversion) {
    this.onFieldChange({ conversion: conversion });
    this.setMessage(conversion.message);

    if (ev.keyCode === KeyEvent.DOM_VK_RETURN) {
      this.requisition.exec();
    }
  }.bind(this), util.errorHandler);
};

/**
 * Some fields contain information that is more important to the user, for
 * example error messages and completion menus.
 */
Field.prototype.isImportant = false;

/**
 * 'static/abstract' method to allow implementations of Field to lay a claim
 * to a type. This allows claims of various strength to be weighted up.
 * See the Field.*MATCH values.
 */
Field.claim = function(type, context) {
  throw new Error('Field should not be used directly');
};

/**
 * About minimalism - If we're producing a dialog, we want a field for every
 * parameter. If we're providing a quick tooltip, we only want a field when
 * it's really going to help.
 * The getField() function takes an option of 'tooltip: true'. Fields are
 * expected to reply with a TOOLTIP_* constant if they should be shown in the
 * tooltip case.
 */
Field.TOOLTIP_MATCH = 5;   // A best match, that works for a tooltip
Field.TOOLTIP_DEFAULT = 4; // A default match that should show in a tooltip
Field.MATCH = 3;           // Match, but ignorable if we're being minimalist
Field.DEFAULT = 2;         // This is a default (non-minimalist) match
Field.BASIC = 1;           // OK in an emergency. i.e. assume Strings
Field.NO_MATCH = 0;        // This field can't help with the given type

exports.Field = Field;


/**
 * Internal array of known fields
 */
var fieldCtors = [];

/**
 * Add a field definition by field constructor
 * @param fieldCtor Constructor function of new Field
 */
exports.addField = function(fieldCtor) {
  if (typeof fieldCtor !== 'function') {
    console.error('addField erroring on ', fieldCtor);
    throw new Error('addField requires a Field constructor');
  }
  fieldCtors.push(fieldCtor);
};

/**
 * Remove a Field definition
 * @param field A previously registered field, specified either with a field
 * name or from the field name
 */
exports.removeField = function(field) {
  if (typeof field !== 'string') {
    fields = fields.filter(function(test) {
      return test !== field;
    });
    delete fields[field];
  }
  else if (field instanceof Field) {
    removeField(field.name);
  }
  else {
    console.error('removeField erroring on ', field);
    throw new Error('removeField requires an instance of Field');
  }
};

/**
 * Find the best possible matching field from the specification of the type
 * of field required.
 * @param type An instance of Type that we will represent
 * @param options A set of properties that we should attempt to match, and use
 * in the construction of the new field object:
 * - document: The document to use in creating new elements
 * - name: The parameter name, (i.e. assignment.param.name)
 * - requisition: The requisition we're monitoring,
 * - required: Is this a required parameter (i.e. param.isDataRequired)
 * - named: Is this a named parameters (i.e. !param.isPositionalAllowed)
 * @return A newly constructed field that best matches the input options
 */
exports.getField = function(type, options) {
  var ctor;
  var highestClaim = -1;
  fieldCtors.forEach(function(fieldCtor) {
    var claim = fieldCtor.claim(type, options.requisition.context);
    if (claim > highestClaim) {
      highestClaim = claim;
      ctor = fieldCtor;
    }
  });

  if (!ctor) {
    console.error('Unknown field type ', type, ' in ', fieldCtors);
    throw new Error('Can\'t find field for ' + type);
  }

  if (options.tooltip && highestClaim < Field.TOOLTIP_DEFAULT) {
    return new BlankField(type, options);
  }

  return new ctor(type, options);
};


/**
 * For use with delegate types that do not yet have anything to resolve to.
 * BlankFields are not for general use.
 */
function BlankField(type, options) {
  Field.call(this, type, options);

  this.element = util.createElement(this.document, 'div');

  this.onFieldChange = util.createEvent('BlankField.onFieldChange');
}

BlankField.prototype = Object.create(Field.prototype);

BlankField.claim = function(type, context) {
  return type.name === 'blank' ? Field.MATCH : Field.NO_MATCH;
};

BlankField.prototype.setConversion = function(conversion) {
  this.setMessage(conversion.message);
};

BlankField.prototype.getConversion = function() {
  return this.type.parse(new Argument(), this.requisition.context);
};

exports.addField(BlankField);


});
/*
 * Copyright 2012, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

define('gcli/ui/fields/javascript', ['require', 'exports', 'module' , 'util/util', 'util/promise', 'gcli/types', 'gcli/argument', 'gcli/ui/fields/menu', 'gcli/ui/fields'], function(require, exports, module) {

'use strict';

var util = require('util/util');
var Promise = require('util/promise');

var Status = require('gcli/types').Status;
var Conversion = require('gcli/types').Conversion;
var ScriptArgument = require('gcli/argument').ScriptArgument;

var Menu = require('gcli/ui/fields/menu').Menu;
var Field = require('gcli/ui/fields').Field;
var fields = require('gcli/ui/fields');


/**
 * Registration and de-registration.
 */
exports.startup = function() {
  fields.addField(JavascriptField);
};

exports.shutdown = function() {
  fields.removeField(JavascriptField);
};


/**
 * A field that allows editing of javascript
 */
function JavascriptField(type, options) {
  Field.call(this, type, options);

  this.onInputChange = this.onInputChange.bind(this);
  this.arg = new ScriptArgument('', '{ ', ' }');

  this.element = util.createElement(this.document, 'div');

  this.input = util.createElement(this.document, 'input');
  this.input.type = 'text';
  this.input.addEventListener('keyup', this.onInputChange, false);
  this.input.classList.add('gcli-field');
  this.input.classList.add('gcli-field-javascript');
  this.element.appendChild(this.input);

  this.menu = new Menu({
    document: this.document,
    field: true,
    type: type
  });
  this.element.appendChild(this.menu.element);

  var initial = new Conversion(undefined, new ScriptArgument(''),
                               Status.INCOMPLETE, '');
  this.setConversion(initial);

  this.onFieldChange = util.createEvent('JavascriptField.onFieldChange');

  // i.e. Register this.onItemClick as the default action for a menu click
  this.menu.onItemClick.add(this.itemClicked, this);
}

JavascriptField.prototype = Object.create(Field.prototype);

JavascriptField.claim = function(type, context) {
  return type.name === 'javascript' ? Field.TOOLTIP_MATCH : Field.NO_MATCH;
};

JavascriptField.prototype.destroy = function() {
  Field.prototype.destroy.call(this);
  this.input.removeEventListener('keyup', this.onInputChange, false);
  this.menu.onItemClick.remove(this.itemClicked, this);
  this.menu.destroy();
  delete this.element;
  delete this.input;
  delete this.menu;
  delete this.document;
  delete this.onInputChange;
};

JavascriptField.prototype.setConversion = function(conversion) {
  this.arg = conversion.arg;
  this.input.value = conversion.arg.text;

  var prefixLen = 0;
  if (this.type.name === 'javascript') {
    var typed = conversion.arg.text;
    var lastDot = typed.lastIndexOf('.');
    if (lastDot !== -1) {
      prefixLen = lastDot;
    }
  }

  this.setMessage(conversion.message);

  conversion.getPredictions().then(function(predictions) {
    var items = [];
    predictions.forEach(function(item) {
      // Commands can be hidden
      if (!item.hidden) {
        items.push({
          name: item.name.substring(prefixLen),
          complete: item.name,
          description: item.description || ''
        });
      }
    }, this);
    this.menu.show(items);
  }.bind(this), util.errorHandler);
};

JavascriptField.prototype.itemClicked = function(ev) {
  var parsed = this.type.parse(ev.arg, this.requisition.context);
  Promise.resolve(parsed).then(function(conversion) {
    this.onFieldChange({ conversion: conversion });
    this.setMessage(conversion.message);
  }.bind(this), util.errorHandler);
};

JavascriptField.prototype.onInputChange = function(ev) {
  this.item = ev.currentTarget.item;
  Promise.resolve(this.getConversion()).then(function(conversion) {
    this.onFieldChange({ conversion: conversion });
    this.setMessage(conversion.message);
  }.bind(this), util.errorHandler);
};

JavascriptField.prototype.getConversion = function() {
  // This tweaks the prefix/suffix of the argument to fit
  this.arg = new ScriptArgument(this.input.value, '{ ', ' }');
  return this.type.parse(this.arg, this.requisition.context);
};

JavascriptField.DEFAULT_VALUE = '__JavascriptField.DEFAULT_VALUE';


});
/*
 * Copyright 2012, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

define('gcli/ui/fields/menu', ['require', 'exports', 'module' , 'util/util', 'util/l10n', 'util/domtemplate', 'gcli/argument', 'gcli/types', 'gcli/canon', 'text!gcli/ui/fields/menu.css', 'text!gcli/ui/fields/menu.html'], function(require, exports, module) {

'use strict';

var util = require('util/util');
var l10n = require('util/l10n');
var domtemplate = require('util/domtemplate');

var Argument = require('gcli/argument').Argument;
var Conversion = require('gcli/types').Conversion;
var canon = require('gcli/canon');

var menuCss = require('text!gcli/ui/fields/menu.css');
var menuHtml = require('text!gcli/ui/fields/menu.html');


/**
 * Menu is a display of the commands that are possible given the state of a
 * requisition.
 * @param options A way to customize the menu display. Valid options are:
 * - field: [boolean] Turns the menu display into a drop-down for use inside a
 *   JavascriptField.
 * - document: The document to use in creating widgets
 * - menuClass: Custom class name when generating the top level element
 *   which allows different layout systems
 * - type: The version of SelectionType that we're picking an option from
 */
function Menu(options) {
  options = options || {};
  this.document = options.document || document;
  this.type = options.type;

  // FF can be really hard to debug if doc is null, so we check early on
  if (!this.document) {
    throw new Error('No document');
  }

  this.element =  util.createElement(this.document, 'div');
  this.element.classList.add(options.menuClass || 'gcli-menu');
  if (options && options.field) {
    this.element.classList.add(options.menuFieldClass || 'gcli-menu-field');
  }

  // Pull the HTML into the DOM, but don't add it to the document
  if (menuCss != null) {
    util.importCss(menuCss, this.document, 'gcli-menu');
  }

  this.template = util.toDom(this.document, menuHtml);
  this.templateOptions = { blankNullUndefined: true, stack: 'menu.html' };

  // Contains the items that should be displayed
  this.items = null;

  this.onItemClick = util.createEvent('Menu.onItemClick');
}

/**
 * Allow the template engine to get at localization strings
 */
Menu.prototype.l10n = l10n.propertyLookup;

/**
 * Avoid memory leaks
 */
Menu.prototype.destroy = function() {
  delete this.element;
  delete this.template;
  delete this.document;
};

/**
 * The default is to do nothing when someone clicks on the menu.
 * This is called from template.html
 * @param ev The click event from the browser
 */
Menu.prototype.onItemClickInternal = function(ev) {
  var name = ev.currentTarget.querySelector('.gcli-menu-name').textContent;
  var arg = new Argument(name);
  arg.suffix = ' ';

  this.onItemClick({ arg: arg });
};

/**
 * Display a number of items in the menu (or hide the menu if there is nothing
 * to display)
 * @param items The items to show in the menu
 * @param match Matching text to highlight in the output
 */
Menu.prototype.show = function(items, match) {
  this.items = items.filter(function(item) {
    return item.hidden === undefined || item.hidden !== true;
  }.bind(this));

  if (match) {
    this.items = this.items.map(function(item) {
      return getHighlightingProxy(item, match, this.template.ownerDocument);
    }.bind(this));
  }

  if (this.items.length === 0) {
    this.element.style.display = 'none';
    return;
  }

  if (this.items.length >= Conversion.maxPredictions) {
    this.items.splice(-1);
    this.items.hasMore = true;
  }

  var options = this.template.cloneNode(true);
  domtemplate.template(options, this, this.templateOptions);

  util.clearElement(this.element);
  this.element.appendChild(options);

  this.element.style.display = 'block';
};

/**
 * Create a proxy around an item that highlights matching text
 */
function getHighlightingProxy(item, match, document) {
  if (typeof Proxy === 'undefined') {
    return item;
  }
  return Proxy.create({
    get: function(rcvr, name) {
      var value = item[name];
      if (name !== 'name') {
        return value;
      }

      var startMatch = value.indexOf(match);
      if (startMatch === -1) {
        return value;
      }

      var before = value.substr(0, startMatch);
      var after = value.substr(startMatch + match.length);
      var parent = document.createElement('span');
      parent.appendChild(document.createTextNode(before));
      var highlight = document.createElement('span');
      highlight.classList.add('gcli-menu-typed');
      highlight.appendChild(document.createTextNode(match));
      parent.appendChild(highlight);
      parent.appendChild(document.createTextNode(after));
      return parent;
    }
  });
}

/**
 * Highlight a given option
 */
Menu.prototype.setChoiceIndex = function(choice) {
  var nodes = this.element.querySelectorAll('.gcli-menu-option');
  for (var i = 0; i < nodes.length; i++) {
    nodes[i].classList.remove('gcli-menu-highlight');
  }

  if (choice == null) {
    return;
  }

  if (nodes.length <= choice) {
    console.error('Cant highlight ' + choice + '. Only ' + nodes.length + ' options');
    return;
  }

  nodes.item(choice).classList.add('gcli-menu-highlight');
};

/**
 * Allow the inputter to use RETURN to chose the current menu item when
 * it can't execute the command line
 * @return true if an item was 'clicked', false otherwise
 */
Menu.prototype.selectChoice = function() {
  var selected = this.element.querySelector('.gcli-menu-highlight .gcli-menu-name');
  if (!selected) {
    return false;
  }

  var name = selected.textContent;
  var arg = new Argument(name);
  arg.suffix = ' ';
  arg.prefix = ' ';

  this.onItemClick({ arg: arg });
  return true;
};

/**
 * Hide the menu
 */
Menu.prototype.hide = function() {
  this.element.style.display = 'none';
};

/**
 * Change how much vertical space this menu can take up
 */
Menu.prototype.setMaxHeight = function(height) {
  this.element.style.maxHeight = height + 'px';
};

exports.Menu = Menu;


});
define("text!gcli/ui/fields/menu.css", [], "");

define("text!gcli/ui/fields/menu.html", [], "\n" +
  "<div>\n" +
  "  <table class=\"gcli-menu-template\" aria-live=\"polite\">\n" +
  "    <tr class=\"gcli-menu-option\" foreach=\"item in ${items}\"\n" +
  "        onclick=\"${onItemClickInternal}\" title=\"${item.manual}\">\n" +
  "      <td class=\"gcli-menu-name\">${item.name}</td>\n" +
  "      <td class=\"gcli-menu-desc\">${item.description}</td>\n" +
  "    </tr>\n" +
  "  </table>\n" +
  "  <div class=\"gcli-menu-more\" if=\"${items.hasMore}\">${l10n.fieldMenuMore}</div>\n" +
  "</div>\n" +
  "");

/*
 * Copyright 2012, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

define('gcli/ui/fields/selection', ['require', 'exports', 'module' , 'util/promise', 'util/util', 'util/l10n', 'gcli/argument', 'gcli/types', 'gcli/ui/fields/menu', 'gcli/ui/fields'], function(require, exports, module) {

'use strict';

var Promise = require('util/promise');
var util = require('util/util');
var l10n = require('util/l10n');

var Argument = require('gcli/argument').Argument;
var Status = require('gcli/types').Status;
var Conversion = require('gcli/types').Conversion;

var Menu = require('gcli/ui/fields/menu').Menu;
var Field = require('gcli/ui/fields').Field;
var fields = require('gcli/ui/fields');


/**
 * Registration and de-registration.
 */
exports.startup = function() {
  fields.addField(SelectionField);
  fields.addField(SelectionTooltipField);
};

exports.shutdown = function() {
  fields.removeField(SelectionField);
  fields.removeField(SelectionTooltipField);
};


/**
 * Model an instanceof SelectionType as a select input box.
 * <p>There are 3 slightly overlapping concepts to be aware of:
 * <ul>
 * <li>value: This is the (probably non-string) value, known as a value by the
 *   assignment
 * <li>optValue: This is the text value as known by the DOM option element, as
 *   in &lt;option value=???%gt...
 * <li>optText: This is the contents of the DOM option element.
 * </ul>
 */
function SelectionField(type, options) {
  Field.call(this, type, options);

  this.items = [];

  this.element = util.createElement(this.document, 'select');
  this.element.classList.add('gcli-field');
  this._addOption({
    name: l10n.lookupFormat('fieldSelectionSelect', [ options.name ])
  });

  Promise.resolve(this.type.getLookup()).then(function(lookup) {
    lookup.forEach(this._addOption, this);
  }.bind(this), util.errorHandler);

  this.onInputChange = this.onInputChange.bind(this);
  this.element.addEventListener('change', this.onInputChange, false);

  this.onFieldChange = util.createEvent('SelectionField.onFieldChange');
}

SelectionField.prototype = Object.create(Field.prototype);

SelectionField.claim = function(type, context) {
  if (type.name === 'boolean') {
    return Field.BASIC;
  }
  return type.isSelection ? Field.DEFAULT : Field.NO_MATCH;
};

SelectionField.prototype.destroy = function() {
  Field.prototype.destroy.call(this);
  this.element.removeEventListener('change', this.onInputChange, false);
  delete this.element;
  delete this.document;
  delete this.onInputChange;
};

SelectionField.prototype.setConversion = function(conversion) {
  var index;
  this.items.forEach(function(item) {
    if (item.value && item.value === conversion.value) {
      index = item.index;
    }
  }, this);
  this.element.value = index;
  this.setMessage(conversion.message);
};

SelectionField.prototype.getConversion = function() {
  var item = this.items[this.element.value];
  return this.type.parse(new Argument(item.name, ' '));
};

SelectionField.prototype._addOption = function(item) {
  item.index = this.items.length;
  this.items.push(item);

  var option = util.createElement(this.document, 'option');
  option.textContent = item.name;
  option.value = item.index;
  this.element.appendChild(option);
};


/**
 * A field that allows selection of one of a number of options
 */
function SelectionTooltipField(type, options) {
  Field.call(this, type, options);

  this.onInputChange = this.onInputChange.bind(this);
  this.arg = new Argument();

  this.menu = new Menu({ document: this.document, type: type });
  this.element = this.menu.element;

  this.onFieldChange = util.createEvent('SelectionTooltipField.onFieldChange');

  // i.e. Register this.onItemClick as the default action for a menu click
  this.menu.onItemClick.add(this.itemClicked, this);
}

SelectionTooltipField.prototype = Object.create(Field.prototype);

SelectionTooltipField.claim = function(type, context) {
  return type.getType(context).isSelection ?
      Field.TOOLTIP_MATCH :
      Field.NO_MATCH;
};

SelectionTooltipField.prototype.destroy = function() {
  Field.prototype.destroy.call(this);
  this.menu.onItemClick.remove(this.itemClicked, this);
  this.menu.destroy();
  delete this.element;
  delete this.document;
  delete this.onInputChange;
};

SelectionTooltipField.prototype.setConversion = function(conversion) {
  this.arg = conversion.arg;
  this.setMessage(conversion.message);

  conversion.getPredictions().then(function(predictions) {
    var items = predictions.map(function(prediction) {
      // If the prediction value is an 'item' (that is an object with a name and
      // description) then use that, otherwise use the prediction itself, because
      // at least that has a name.
      return prediction.value.description ? prediction.value : prediction;
    }, this);
    this.menu.show(items, conversion.arg.text);
  }.bind(this), util.errorHandler);
};

SelectionTooltipField.prototype.itemClicked = function(ev) {
  var parsed = this.type.parse(ev.arg, this.requisition.context);
  Promise.resolve(parsed).then(function(conversion) {
    this.onFieldChange({ conversion: conversion });
    this.setMessage(conversion.message);
  }.bind(this), util.errorHandler);
};

SelectionTooltipField.prototype.onInputChange = function(ev) {
  this.item = ev.currentTarget.item;
  Promise.resolve(this.getConversion()).then(function(conversion) {
    this.onFieldChange({ conversion: conversion });
    this.setMessage(conversion.message);
  }.bind(this), util.errorHandler);
};

SelectionTooltipField.prototype.getConversion = function() {
  // This tweaks the prefix/suffix of the argument to fit
  this.arg = this.arg.beget({ text: this.input.value });
  return this.type.parse(this.arg, this.requisition.context);
};

/**
 * Allow the menu to highlight the correct prediction choice
 */
SelectionTooltipField.prototype.setChoiceIndex = function(choice) {
  this.menu.setChoiceIndex(choice);
};

/**
 * Allow the inputter to use RETURN to chose the current menu item when
 * it can't execute the command line
 * @return true if an item was 'clicked', false otherwise
 */
SelectionTooltipField.prototype.selectChoice = function() {
  return this.menu.selectChoice();
};

Object.defineProperty(SelectionTooltipField.prototype, 'isImportant', {
  get: function() {
    return this.type.name !== 'command';
  },
  enumerable: true
});

SelectionTooltipField.DEFAULT_VALUE = '__SelectionTooltipField.DEFAULT_VALUE';


});
/*
 * Copyright 2012, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

define('gcli/commands/connect', ['require', 'exports', 'module' , 'util/l10n', 'gcli/types', 'gcli/canon', 'util/connect/connector'], function(require, exports, module) {

'use strict';

var l10n = require('util/l10n');
var types = require('gcli/types');
var canon = require('gcli/canon');
var connector = require('util/connect/connector');

/**
 * A lookup of the current connection
 */
var connections = {};

/**
 * 'connect' command
 */
var connect = {
  name: 'connect',
  description: l10n.lookup('connectDesc'),
  manual: l10n.lookup('connectManual'),
  params: [
    {
      name: 'prefix',
      type: 'string',
      description: l10n.lookup('connectPrefixDesc')
    },
    {
      name: 'host',
      type: 'string',
      description: l10n.lookup('connectHostDesc'),
      defaultValue: 'localhost',
      option: true
    },
    {
      name: 'port',
      type: { name: 'number', max: 65536, min: 0 },
      description: l10n.lookup('connectPortDesc'),
      defaultValue: connector.defaultPort,
      option: true
    }
  ],
  returnType: 'string',

  exec: function(args, context) {
    if (connections[args.prefix] != null) {
      throw new Error(l10n.lookupFormat('connectDupReply', [ args.prefix ]));
    }

    var cxp = connector.connect(args.prefix, args.host, args.port);
    return cxp.then(function(connection) {
      connections[args.prefix] = connection;

      return connection.getCommandSpecs().then(function(commandSpecs) {
        var remoter = this.createRemoter(args.prefix, connection);
        canon.addProxyCommands(args.prefix, commandSpecs, remoter);

        // commandSpecs doesn't include the parent command that we added
        return l10n.lookupFormat('connectReply',
                                 [ Object.keys(commandSpecs).length + 1 ]);
      }.bind(this));
    }.bind(this));
  },

  /**
   * When we register a set of remote commands, we need to provide the canon
   * with a proxy executor. This is that executor.
   */
  createRemoter: function(prefix, connection) {
    return function(cmdArgs, context) {
      var typed = context.typed;
      if (typed.indexOf(prefix) !== 0) {
        throw new Error("Missing prefix");
      }
      typed = typed.substring(prefix.length).replace(/^ */, "");

      return connection.execute(typed, cmdArgs).then(function(reply) {
        var typedData = context.typedData(reply.type, reply.data);
        if (!reply.error) {
          return typedData;
        }
        else {
          throw typedData;
        }
      });
    }.bind(this);
  }
};

/**
 * 'connection' type
 */
var connection = {
  name: 'connection',
  parent: 'selection',
  lookup: function() {
    return Object.keys(connections).map(function(prefix) {
      return { name: prefix, value: connections[prefix] };
    });
  }
};

/**
 * 'disconnect' command
 */
var disconnect = {
  name: 'disconnect',
  description: l10n.lookup('disconnectDesc'),
  manual: l10n.lookup('disconnectManual'),
  params: [
    {
      name: 'prefix',
      type: 'connection',
      description: l10n.lookup('disconnectPrefixDesc'),
    }
  ],
  returnType: 'string',

  exec: function(args, context) {
    return args.prefix.disconnect().then(function() {
      var removed = canon.removeProxyCommands(args.prefix.prefix);
      delete connections[args.prefix.prefix];
      return l10n.lookupFormat('disconnectReply', [ removed.length ]);
    });
  }
};


/**
 * Registration and de-registration.
 */
exports.startup = function() {
  types.addType(connection);

  canon.addCommand(connect);
  canon.addCommand(disconnect);
};

exports.shutdown = function() {
  canon.removeCommand(connect);
  canon.removeCommand(disconnect);

  types.removeType(connection);
};


});
/*
 * Copyright 2012, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

define('util/connect/connector', ['require', 'exports', 'module' ], function(require, exports, module) {

'use strict';

var debuggerSocketConnect = Components.utils.import('resource://gre/modules/devtools/dbg-client.jsm', {}).debuggerSocketConnect;
var DebuggerClient = Components.utils.import('resource://gre/modules/devtools/dbg-client.jsm', {}).DebuggerClient;

/**
 * What port should we use by default?
 */
Object.defineProperty(exports, 'defaultPort', {
  get: function() {
    var Services = Components.utils.import('resource://gre/modules/Services.jsm', {}).Services;
    try {
      return Services.prefs.getIntPref('devtools.debugger.chrome-debugging-port');
    }
    catch (ex) {
      console.error('Can\'t use default port from prefs. Using 9999');
      return 9999;
    }
  },
  enumerable: true
});


/**
 * Create a Connection object and initiate a connection.
 */
exports.connect = function(prefix, host, port) {
  var connection = new Connection(prefix, host, port);
  return connection.connect().then(function() {
    return connection;
  });
};

/**
 * Manage a named connection to an HTTP server over web-sockets using socket.io
 */
function Connection(prefix, host, port) {
  this.prefix = prefix;
  this.host = host;
  this.port = port;

  // Properties setup by connect()
  this.actor = undefined;
  this.transport = undefined;
  this.client = undefined;

  this.requests = {};
  this.nextRequestId = 0;
}

/**
 * Setup socket.io, retrieve the list of remote commands and register them with
 * the local canon.
 * @return a promise which resolves (to undefined) when the connection is made
 * or is rejected (with an error message) if the connection fails
 */
Connection.prototype.connect = function() {
  var deferred = Promise.defer();

  this.transport = debuggerSocketConnect(this.host, this.port);
  this.client = new DebuggerClient(this.transport);

  this.client.connect(function() {
    this.client.listTabs(function(response) {
      this.actor = response.gcliActor;
      deferred.resolve();
    }.bind(this));
  }.bind(this));

  return deferred.promise;
};

/**
 * Retrieve the list of remote commands.
 * @return a promise of an array of commandSpecs
 */
Connection.prototype.getCommandSpecs = function() {
  var deferred = Promise.defer();

  var request = { to: this.actor, type: 'getCommandSpecs' };

  this.client.request(request, function(response) {
    deferred.resolve(response.commandSpecs);
  });

  return deferred.promise;
};

/**
 * Send an execute request. Replies are handled by the setup in connect()
 */
Connection.prototype.execute = function(typed, cmdArgs) {
  var deferred = Promise.defer();

  var request = {
    to: this.actor,
    type: 'execute',
    typed: typed,
    args: cmdArgs
  };

  this.client.request(request, function(response) {
    deferred.resolve(response.reply);
  });

  return deferred.promise;
};

/**
 * Send an execute request.
 */
Connection.prototype.execute = function(typed, cmdArgs) {
  var request = new Request(this.actor, typed, cmdArgs);
  this.requests[request.json.id] = request;

  this.client.request(request.json, function(response) {
    var request = this.requests[response.id];
    delete this.requests[response.id];

    request.complete(response.error, response.type, response.data);
  }.bind(this));

  return request.promise;
};

/**
 * Kill this connection
 */
Connection.prototype.disconnect = function() {
  var deferred = Promise.defer();

  this.client.close(function() {
    deferred.resolve();
  });

  return request.promise;
};

/**
 * A Request is a command typed at the client which lives until the command
 * has finished executing on the server
 */
function Request(actor, typed, args) {
  this.json = {
    to: actor,
    type: 'execute',
    typed: typed,
    args: args,
    id: Request._nextRequestId++,
  };

  this._deferred = Promise.defer();
  this.promise = this._deferred.promise;
}

Request._nextRequestId = 0;

/**
 * Called by the connection when a remote command has finished executing
 * @param error boolean indicating output state
 * @param type the type of the returned data
 * @param data the data itself
 */
Request.prototype.complete = function(error, type, data) {
  this._deferred.resolve({
    error: error,
    type: type,
    data: data
  });
};


});
/*
 * Copyright 2012, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

define('gcli/commands/context', ['require', 'exports', 'module' , 'util/l10n', 'gcli/canon'], function(require, exports, module) {

'use strict';

var l10n = require('util/l10n');
var canon = require('gcli/canon');

/**
 * 'context' command
 */
var contextCmdSpec = {
  name: 'context',
  description: l10n.lookup('contextDesc'),
  manual: l10n.lookup('contextManual'),
  params: [
   {
     name: 'prefix',
     type: 'command',
     description: l10n.lookup('contextPrefixDesc'),
     defaultValue: null
   }
  ],
  returnType: 'string',
  exec: function echo(args, context) {
    // Do not copy this code
    var requisition = context.__dlhjshfw;

    if (args.prefix == null) {
      requisition.prefix = null;
      return l10n.lookup('contextEmptyReply');
    }

    if (args.prefix.exec != null) {
      throw new Error(l10n.lookupFormat('contextNotParentError',
                                        [ args.prefix.name ]));
    }

    requisition.prefix = args.prefix.name;
    return l10n.lookupFormat('contextReply', [ args.prefix.name ]);
  }
};

/**
 * Registration and de-registration.
 */
exports.startup = function() {
  canon.addCommand(contextCmdSpec);
};

exports.shutdown = function() {
  canon.removeCommand(contextCmdSpec);
};

});
/*
 * Copyright 2012, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

define('gcli/commands/help', ['require', 'exports', 'module' , 'util/l10n', 'gcli/canon', 'gcli/converters', 'text!gcli/commands/help_man.html', 'text!gcli/commands/help_list.html', 'text!gcli/commands/help.css'], function(require, exports, module) {

'use strict';

var l10n = require('util/l10n');
var canon = require('gcli/canon');
var converters = require('gcli/converters');

var helpManHtml = require('text!gcli/commands/help_man.html');
var helpListHtml = require('text!gcli/commands/help_list.html');
var helpCss = require('text!gcli/commands/help.css');

/**
 * Convert a command into a man page
 */
var commandConverterSpec = {
  from: 'commandData',
  to: 'view',
  exec: function(commandData, context) {
    return context.createView({
      html: helpManHtml,
      options: { allowEval: true, stack: 'help_man.html' },
      data: {
        l10n: l10n.propertyLookup,
        onclick: context.update,
        ondblclick: context.updateExec,
        describe: function(item) {
          return item.manual || item.description;
        },
        getTypeDescription: function(param) {
          var input = '';
          if (param.defaultValue === undefined) {
            input = l10n.lookup('helpManRequired');
          }
          else if (param.defaultValue === null) {
            input = l10n.lookup('helpManOptional');
          }
          else {
            input = param.defaultValue;
          }
          return '(' + param.type.name + ', ' + input + ')';
        },
        command: commandData.command,
        subcommands: commandData.subcommands
      },
      css: helpCss,
      cssId: 'gcli-help'
    });
  }
};

/**
 * Convert a list of commands into a formatted list
 */
var commandsConverterSpec = {
  from: 'commandsData',
  to: 'view',
  exec: function(commandsData, context) {
    var heading;
    if (commandsData.commands.length === 0) {
      heading = l10n.lookupFormat('helpListNone', [ commandsData.prefix ]);
    }
    else if (commandsData.prefix == null) {
      heading = l10n.lookup('helpListAll');
    }
    else {
      heading = l10n.lookupFormat('helpListPrefix', [ commandsData.prefix ]);
    }

    return context.createView({
      html: helpListHtml,
      options: { allowEval: true, stack: 'help_list.html' },
      data: {
        l10n: l10n.propertyLookup,
        includeIntro: commandsData.prefix == null,
        heading: heading,
        onclick: context.update,
        ondblclick: context.updateExec,
        matchingCommands: commandsData.commands
      },
      css: helpCss,
      cssId: 'gcli-help'
    });
  }
};

/**
 * 'help' command
 */
var helpCommandSpec = {
  name: 'help',
  description: l10n.lookup('helpDesc'),
  manual: l10n.lookup('helpManual'),
  params: [
    {
      name: 'search',
      type: 'string',
      description: l10n.lookup('helpSearchDesc'),
      manual: l10n.lookup('helpSearchManual3'),
      defaultValue: null
    }
  ],

  exec: function(args, context) {
    var command = canon.getCommand(args.search || undefined);
    if (command) {
      return context.typedData('commandData', {
        command: command,
        subcommands: getSubCommands(command)
      });
    }

    return context.typedData('commandsData', {
      prefix: args.search,
      commands: getMatchingCommands(args.search)
    });
  }
};

/**
 * Registration and de-registration.
 */
exports.startup = function() {
  canon.addCommand(helpCommandSpec);
  converters.addConverter(commandConverterSpec);
  converters.addConverter(commandsConverterSpec);
};

exports.shutdown = function() {
  canon.removeCommand(helpCommandSpec);
  converters.removeConverter(commandConverterSpec);
  converters.removeConverter(commandsConverterSpec);
};

/**
 * Create a block of data suitable to be passed to the help_list.html template
 */
function getMatchingCommands(prefix) {
  var commands = canon.getCommands().filter(function(command) {
    if (command.hidden) {
      return false;
    }

    if (prefix && command.name.indexOf(prefix) !== 0) {
      // Filtered out because they don't match the search
      return false;
    }
    if (!prefix && command.name.indexOf(' ') != -1) {
      // We don't show sub commands with plain 'help'
      return false;
    }
    return true;
  });
  commands.sort(function(c1, c2) {
    return c1.name.localeCompare(c2.name);
  });

  return commands;
}

/**
 * Find all the sub commands of the given command
 */
function getSubCommands(command) {
  if (command.exec != null) {
    return [];
  }

  var subcommands = canon.getCommands().filter(function(subcommand) {
    return subcommand.name.indexOf(command.name) === 0 &&
            subcommand.name !== command.name;
  });

  subcommands.sort(function(c1, c2) {
    return c1.name.localeCompare(c2.name);
  });

  return subcommands;
}

});
/*
 * Copyright 2012, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

define('gcli/converters', ['require', 'exports', 'module' , 'util/util', 'util/promise'], function(require, exports, module) {

'use strict';

var util = require('util/util');
var Promise = require('util/promise');

// It's probably easiest to read this bottom to top

/**
 * Best guess at creating a DOM element from random data
 */
var fallbackDomConverter = {
  from: '*',
  to: 'dom',
  exec: function(data, conversionContext) {
    if (data == null) {
      return conversionContext.document.createTextNode('');
    }

    if (typeof HTMLElement !== 'undefined' && data instanceof HTMLElement) {
      return data;
    }

    var node = util.createElement(conversionContext.document, 'p');
    util.setContents(node, data.toString());
    return node;
  }
};

/**
 * Best guess at creating a string from random data
 */
var fallbackStringConverter = {
  from: '*',
  to: 'string',
  exec: function(data, conversionContext) {
    if (data.isView) {
      return data.toDom(conversionContext.document).textContent;
    }

    if (typeof HTMLElement !== 'undefined' && data instanceof HTMLElement) {
      return data.textContent;
    }

    return data == null ? '' : data.toString();
  }
};

/**
 * Convert a view object to a DOM element
 */
var viewDomConverter = {
  from: 'view',
  to: 'dom',
  exec: function(view, conversionContext) {
    return view.toDom(conversionContext.document);
  }
};

/**
 * Convert a view object to a string
 */
var viewStringConverter = {
  from: 'view',
  to: 'string',
  exec: function(view, conversionContext) {
    return view.toDom(conversionContext.document).textContent;
  }
};

/**
 * Convert a terminal object (to help traditional CLI integration) to an element
 */
var terminalDomConverter = {
  from: 'terminal',
  to: 'dom',
  createTextArea: function(text, conversionContext) {
    var node = util.createElement(conversionContext.document, 'textarea');
    node.classList.add('gcli-row-subterminal');
    node.readOnly = true;
    node.textContent = text;
    return node;
  },
  exec: function(data, context) {
    if (Array.isArray(data)) {
      var node = util.createElement(conversionContext.document, 'div');
      data.forEach(function(member) {
        node.appendChild(this.createTextArea(member, conversionContext));
      });
      return node;
    }
    return this.createTextArea(data);
  }
};

/**
 * Several converters are just data.toString inside a 'p' element
 */
function nodeFromDataToString(data, conversionContext) {
  var node = util.createElement(conversionContext.document, 'p');
  node.textContent = data.toString();
  return node;
}

/**
 * Convert a string to a DOM element
 */
var stringDomConverter = {
  from: 'string',
  to: 'dom',
  exec: nodeFromDataToString
};

/**
 * Convert a number to a DOM element
 */
var numberDomConverter = {
  from: 'number',
  to: 'dom',
  exec: nodeFromDataToString
};

/**
 * Convert a number to a DOM element
 */
var booleanDomConverter = {
  from: 'boolean',
  to: 'dom',
  exec: nodeFromDataToString
};

/**
 * Convert a number to a DOM element
 */
var undefinedDomConverter = {
  from: 'undefined',
  to: 'dom',
  exec: function(data, conversionContext) {
    return util.createElement(conversionContext.document, 'span');
  }
};

/**
 * Convert a string to a DOM element
 */
var errorDomConverter = {
  from: 'error',
  to: 'dom',
  exec: function(ex, conversionContext) {
    var node = util.createElement(conversionContext.document, 'p');
    node.className = "gcli-error";
    node.textContent = ex;
    return node;
  }
};

/**
 * Create a new converter by using 2 converters, one after the other
 */
function getChainConverter(first, second) {
  if (first.to !== second.from) {
    throw new Error('Chain convert impossible: ' + first.to + '!=' + second.from);
  }
  return {
    from: first.from,
    to: second.to,
    exec: function(data, conversionContext) {
      var intermediate = first.exec(data, conversionContext);
      return second.exec(intermediate, conversionContext);
    }
  };
}

/**
 * This is where we cache the converters that we know about
 */
var converters = {
  from: {}
};

/**
 * Add a new converter to the cache
 */
exports.addConverter = function(converter) {
  var fromMatch = converters.from[converter.from];
  if (fromMatch == null) {
    fromMatch = {};
    converters.from[converter.from] = fromMatch;
  }

  fromMatch[converter.to] = converter;
};

/**
 * Remove an existing converter from the cache
 */
exports.removeConverter = function(converter) {
  var fromMatch = converters.from[converter.from];
  if (fromMatch == null) {
    return;
  }

  if (fromMatch[converter.to] === converter) {
    fromMatch[converter.to] = null;
  }
};

/**
 * Work out the best converter that we've got, for a given conversion.
 */
function getConverter(from, to) {
  var fromMatch = converters.from[from];
  if (fromMatch == null) {
    return getFallbackConverter(from, to);
  }

  var converter = fromMatch[to];
  if (converter == null) {
    // Someone is going to love writing a graph search algorithm to work out
    // the smallest number of conversions, or perhaps the least 'lossy'
    // conversion but for now the only 2 step conversion is foo->view->dom,
    // which we are going to special case.
    if (to === 'dom') {
      converter = fromMatch['view'];
      if (converter != null) {
        return getChainConverter(converter, viewDomConverter);
      }
    }
    if (to === 'string') {
      converter = fromMatch['view'];
      if (converter != null) {
        return getChainConverter(converter, viewStringConverter);
      }
    }
    return getFallbackConverter(from, to);
  }
  return converter;
}

/**
 * Helper for getConverter to pick the best fallback converter
 */
function getFallbackConverter(from, to) {
  console.error('No converter from ' + from + ' to ' + to + '. Using fallback');

  if (to === 'dom') {
    return fallbackDomConverter;
  }

  if (to === 'string') {
    return fallbackStringConverter;
  }

  throw new Error('No conversion possible from ' + from + ' to ' + to + '.');
}

/**
 * Convert some data from one type to another
 * @param data The object to convert
 * @param from The type of the data right now
 * @param to The type that we would like the data in
 * @param conversionContext An execution context (i.e. simplified requisition) which is
 * often required for access to a document, or createView function
 */
exports.convert = function(data, from, to, conversionContext) {
  if (from === to) {
    return Promise.resolve(data);
  }
  return Promise.resolve(getConverter(from, to).exec(data, conversionContext));
};

exports.addConverter(viewDomConverter);
exports.addConverter(viewStringConverter);
exports.addConverter(terminalDomConverter);
exports.addConverter(stringDomConverter);
exports.addConverter(numberDomConverter);
exports.addConverter(booleanDomConverter);
exports.addConverter(undefinedDomConverter);
exports.addConverter(errorDomConverter);


});
define("text!gcli/commands/help_man.html", [], "\n" +
  "<div>\n" +
  "  <h3>${command.name}</h3>\n" +
  "\n" +
  "  <h4 class=\"gcli-help-header\">\n" +
  "    ${l10n.helpManSynopsis}:\n" +
  "    <span class=\"gcli-out-shortcut\" onclick=\"${onclick}\" data-command=\"${command.name}\">\n" +
  "      ${command.name}\n" +
  "      <span foreach=\"param in ${command.params}\">\n" +
  "        ${param.defaultValue !== undefined ? '[' + param.name + ']' : param.name}\n" +
  "      </span>\n" +
  "    </span>\n" +
  "  </h4>\n" +
  "\n" +
  "  <h4 class=\"gcli-help-header\">${l10n.helpManDescription}:</h4>\n" +
  "\n" +
  "  <p class=\"gcli-help-description\">${describe(command)}</p>\n" +
  "\n" +
  "  <div if=\"${command.exec}\">\n" +
  "    <h4 class=\"gcli-help-header\">${l10n.helpManParameters}:</h4>\n" +
  "\n" +
  "    <ul class=\"gcli-help-parameter\">\n" +
  "      <li if=\"${command.params.length === 0}\">${l10n.helpManNone}</li>\n" +
  "      <li foreach=\"param in ${command.params}\">\n" +
  "        ${param.name} <em>${getTypeDescription(param)}</em>\n" +
  "        <br/>\n" +
  "        ${describe(param)}\n" +
  "      </li>\n" +
  "    </ul>\n" +
  "  </div>\n" +
  "\n" +
  "  <div if=\"${!command.exec}\">\n" +
  "    <h4 class=\"gcli-help-header\">${l10n.subCommands}:</h4>\n" +
  "\n" +
  "    <ul class=\"gcli-help-${subcommands}\">\n" +
  "      <li if=\"${subcommands.length === 0}\">${l10n.subcommandsNone}</li>\n" +
  "      <li foreach=\"subcommand in ${subcommands}\">\n" +
  "        <strong>${subcommand.name}</strong>:\n" +
  "        ${subcommand.description}\n" +
  "        <span class=\"gcli-out-shortcut\" data-command=\"help ${subcommand.name}\"\n" +
  "            onclick=\"${onclick}\" ondblclick=\"${ondblclick}\">\n" +
  "          help ${subcommand.name}\n" +
  "        </span>\n" +
  "      </li>\n" +
  "    </ul>\n" +
  "  </div>\n" +
  "\n" +
  "</div>\n" +
  "");

define("text!gcli/commands/help_list.html", [], "\n" +
  "<div>\n" +
  "  <h3>${heading}</h3>\n" +
  "\n" +
  "  <table>\n" +
  "    <tr foreach=\"command in ${matchingCommands}\"\n" +
  "        onclick=\"${onclick}\" ondblclick=\"${ondblclick}\">\n" +
  "      <th class=\"gcli-help-name\">${command.name}</th>\n" +
  "      <td class=\"gcli-help-arrow\">-</td>\n" +
  "      <td>\n" +
  "        ${command.description}\n" +
  "        <span class=\"gcli-out-shortcut\" data-command=\"help ${command.name}\">help ${command.name}</span>\n" +
  "      </td>\n" +
  "    </tr>\n" +
  "  </table>\n" +
  "</div>\n" +
  "");

define("text!gcli/commands/help.css", [], "");

/*
 * Copyright 2012, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

define('gcli/commands/pref', ['require', 'exports', 'module' , 'util/l10n', 'gcli/canon', 'gcli/converters', 'gcli/settings', 'text!gcli/commands/pref_set_check.html'], function(require, exports, module) {

'use strict';

var l10n = require('util/l10n');
var canon = require('gcli/canon');
var converters = require('gcli/converters');
var settings = require('gcli/settings');

/**
 * Record if the user has clicked on 'Got It!'
 */
var allowSetSettingSpec = {
  name: 'allowSet',
  type: 'boolean',
  description: l10n.lookup('allowSetDesc'),
  defaultValue: false
};
exports.allowSet = undefined;

/**
 * 'pref' command
 */
var prefCmdSpec = {
  name: 'pref',
  description: l10n.lookup('prefDesc'),
  manual: l10n.lookup('prefManual')
};

/**
 * 'pref show' command
 */
var prefShowCmdSpec = {
  name: 'pref show',
  description: l10n.lookup('prefShowDesc'),
  manual: l10n.lookup('prefShowManual'),
  params: [
    {
      name: 'setting',
      type: 'setting',
      description: l10n.lookup('prefShowSettingDesc'),
      manual: l10n.lookup('prefShowSettingManual')
    }
  ],
  exec: function Command_prefShow(args, context) {
    return l10n.lookupFormat('prefShowSettingValue',
                             [ args.setting.name, args.setting.value ]);
  }
};

/**
 * 'pref set' command
 */
var prefSetCmdSpec = {
  name: 'pref set',
  description: l10n.lookup('prefSetDesc'),
  manual: l10n.lookup('prefSetManual'),
  params: [
    {
      name: 'setting',
      type: 'setting',
      description: l10n.lookup('prefSetSettingDesc'),
      manual: l10n.lookup('prefSetSettingManual')
    },
    {
      name: 'value',
      type: 'settingValue',
      description: l10n.lookup('prefSetValueDesc'),
      manual: l10n.lookup('prefSetValueManual')
    }
  ],
  exec: function(args, context) {
    if (!exports.allowSet.value &&
        args.setting.name !== exports.allowSet.name) {
      return context.typedData('prefSetWarning', null);
    }

    args.setting.value = args.value;
  }
};

var prefSetWarningConverterSpec = {
  from: 'prefSetWarning',
  to: 'view',
  exec: function(data, context) {
    return context.createView({
      html: require('text!gcli/commands/pref_set_check.html'),
      options: { allowEval: true, stack: 'pref_set_check.html' },
      data: {
        l10n: l10n.propertyLookup,
        activate: function() {
          context.updateExec('pref set ' + exports.allowSet.name + ' true');
        }
      }
    });
  }
};

/**
 * 'pref reset' command
 */
var prefResetCmdSpec = {
  name: 'pref reset',
  description: l10n.lookup('prefResetDesc'),
  manual: l10n.lookup('prefResetManual'),
  params: [
    {
      name: 'setting',
      type: 'setting',
      description: l10n.lookup('prefResetSettingDesc'),
      manual: l10n.lookup('prefResetSettingManual')
    }
  ],
  exec: function(args, context) {
    args.setting.setDefault();
  }
};

/**
 * Registration and de-registration.
 */
exports.startup = function() {
  exports.allowSet = settings.addSetting(allowSetSettingSpec);

  canon.addCommand(prefCmdSpec);
  canon.addCommand(prefShowCmdSpec);
  canon.addCommand(prefSetCmdSpec);
  canon.addCommand(prefResetCmdSpec);
  converters.addConverter(prefSetWarningConverterSpec);
};

exports.shutdown = function() {
  canon.removeCommand(prefCmdSpec);
  canon.removeCommand(prefShowCmdSpec);
  canon.removeCommand(prefSetCmdSpec);
  canon.removeCommand(prefResetCmdSpec);
  converters.removeConverter(prefSetWarningConverterSpec);

  settings.removeSetting(allowSetSettingSpec);
  exports.allowSet = undefined;
};


});
define("text!gcli/commands/pref_set_check.html", [], "<div>\n" +
  "  <p><strong>${l10n.prefSetCheckHeading}</strong></p>\n" +
  "  <p>${l10n.prefSetCheckBody}</p>\n" +
  "  <button onclick=\"${activate}\">${l10n.prefSetCheckGo}</button>\n" +
  "</div>\n" +
  "");

/*
 * Copyright 2012, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

define('gcli/ui/ffdisplay', ['require', 'exports', 'module' , 'gcli/ui/inputter', 'gcli/ui/completer', 'gcli/ui/tooltip', 'gcli/ui/focus', 'gcli/cli', 'gcli/types/javascript', 'gcli/types/node', 'gcli/types/resource', 'util/host', 'gcli/ui/intro', 'gcli/canon'], function(require, exports, module) {

'use strict';

var Inputter = require('gcli/ui/inputter').Inputter;
var Completer = require('gcli/ui/completer').Completer;
var Tooltip = require('gcli/ui/tooltip').Tooltip;
var FocusManager = require('gcli/ui/focus').FocusManager;

var Requisition = require('gcli/cli').Requisition;

var cli = require('gcli/cli');
var jstype = require('gcli/types/javascript');
var nodetype = require('gcli/types/node');
var resource = require('gcli/types/resource');
var host = require('util/host');
var intro = require('gcli/ui/intro');

var CommandOutputManager = require('gcli/canon').CommandOutputManager;

/**
 * Handy utility to inject the content document (i.e. for the viewed page,
 * not for chrome) into the various components.
 */
function setContentDocument(document) {
  if (document) {
    nodetype.setDocument(document);
    resource.setDocument(document);
  }
  else {
    resource.unsetDocument();
    nodetype.unsetDocument();
    jstype.unsetGlobalObject();
  }
}

/**
 * FFDisplay is responsible for generating the UI for GCLI, this implementation
 * is a special case for use inside Firefox
 * @param options A configuration object containing the following:
 * - contentDocument (optional)
 * - chromeDocument
 * - hintElement
 * - inputElement
 * - completeElement
 * - backgroundElement
 * - outputDocument
 * - consoleWrap (optional)
 * - eval (optional)
 * - environment
 * - scratchpad (optional)
 * - chromeWindow
 * - commandOutputManager (optional)
 */
function FFDisplay(options) {
  if (options.eval) {
    cli.setEvalFunction(options.eval);
  }
  setContentDocument(options.contentDocument);
  host.chromeWindow = options.chromeWindow;

  this.commandOutputManager = options.commandOutputManager;
  if (this.commandOutputManager == null) {
    this.commandOutputManager = new CommandOutputManager();
  }

  this.onOutput = this.commandOutputManager.onOutput;
  this.requisition = new Requisition(options.environment,
                                     options.outputDocument,
                                     this.commandOutputManager);

  this.focusManager = new FocusManager(options, {
    document: options.chromeDocument,
    requisition: this.requisition,
  });
  this.onVisibilityChange = this.focusManager.onVisibilityChange;

  this.inputter = new Inputter(options, {
    requisition: this.requisition,
    focusManager: this.focusManager,
    element: options.inputElement
  });

  this.completer = new Completer(options, {
    requisition: this.requisition,
    inputter: this.inputter,
    backgroundElement: options.backgroundElement,
    element: options.completeElement
  });

  this.tooltip = new Tooltip(options, {
    requisition: this.requisition,
    focusManager: this.focusManager,
    inputter: this.inputter,
    element: options.hintElement
  });

  this.inputter.tooltip = this.tooltip;

  if (options.consoleWrap) {
    this.consoleWrap = options.consoleWrap;
    var win = options.consoleWrap.ownerDocument.defaultView;
    this.resizer = this.resizer.bind(this);

    win.addEventListener('resize', this.resizer, false);
    this.requisition.onTextChange.add(this.resizer, this);
  }

  this.options = options;
}

/**
 * The main Display calls this as part of startup since it registers listeners
 * for output first. The firefox display can't do this, so it has to be a
 * separate method
 */
FFDisplay.prototype.maybeShowIntro = function() {
  intro.maybeShowIntro(this.commandOutputManager);
};

/**
 * Called when the page to which we're attached changes
 * @params options Object with the following properties:
 * - contentDocument: Points to the page that we should now work against
 * - environment: A replacement environment for Requisition use
 * - chromeWindow: Allow node type to create overlay
 */
FFDisplay.prototype.reattach = function(options) {
  setContentDocument(options.contentDocument);
  host.chromeWindow = options.chromeWindow;
  this.requisition.environment = options.environment;
};

/**
 * Avoid memory leaks
 */
FFDisplay.prototype.destroy = function() {
  if (this.consoleWrap) {
    var win = this.options.consoleWrap.ownerDocument.defaultView;

    this.requisition.onTextChange.remove(this.resizer, this);
    win.removeEventListener('resize', this.resizer, false);
  }

  this.tooltip.destroy();
  this.completer.destroy();
  this.inputter.destroy();
  this.focusManager.destroy();

  this.requisition.destroy();

  host.chromeWindow = undefined;
  setContentDocument(null);
  cli.unsetEvalFunction();

  delete this.options;

  // We could also delete the following objects if we have hard-to-track-down
  // memory leaks, as a belt-and-braces approach, however this prevents our
  // DOM node hunter script from looking in all the nooks and crannies, so it's
  // better if we can be leak-free without deleting them:
  // - consoleWrap, resizer, tooltip, completer, inputter,
  // - focusManager, onVisibilityChange, requisition, commandOutputManager
};

/**
 * Called on chrome window resize, or on divider slide
 */
FFDisplay.prototype.resizer = function() {
  // Bug 705109: There are several numbers hard-coded in this function.
  // This is simpler than calculating them, but error-prone when the UI setup,
  // the styling or display settings change.

  var parentRect = this.options.consoleWrap.getBoundingClientRect();
  // Magic number: 64 is the size of the toolbar above the output area
  var parentHeight = parentRect.bottom - parentRect.top - 64;

  // Magic number: 100 is the size at which we decide the hints are too small
  // to be useful, so we hide them
  if (parentHeight < 100) {
    this.options.hintElement.classList.add('gcliterm-hint-nospace');
  }
  else {
    this.options.hintElement.classList.remove('gcliterm-hint-nospace');
    this.options.hintElement.style.overflowY = null;
    this.options.hintElement.style.borderBottomColor = 'white';
  }

  // We also try to make the max-width of any GCLI elements so they don't
  // extend outside the scroll area.
  var doc = this.options.hintElement.ownerDocument;

  var outputNode = this.options.hintElement.parentNode.parentNode.children[1];
  var outputs = outputNode.getElementsByClassName('gcliterm-msg-body');
  var listItems = outputNode.getElementsByClassName('hud-msg-node');

  // This is an top-side estimate. We could try to calculate it, maybe using
  // something along these lines http://www.alexandre-gomes.com/?p=115 However
  // experience has shown this to be hard to get to work reliably
  // Also we don't need to be precise. If we use a number that is too big then
  // the only down-side is too great a right margin
  var scrollbarWidth = 20;

  if (listItems.length > 0) {
    var parentWidth = outputNode.getBoundingClientRect().width - scrollbarWidth;
    var otherWidth;
    var body;

    for (var i = 0; i < listItems.length; ++i) {
      var listItem = listItems[i];
      // a.k.a 'var otherWidth = 132'
      otherWidth = 0;
      body = null;

      for (var j = 0; j < listItem.children.length; j++) {
        var child = listItem.children[j];

        if (child.classList.contains('gcliterm-msg-body')) {
          body = child.children[0];
        }
        else {
          otherWidth += child.getBoundingClientRect().width;
        }

        var styles = doc.defaultView.getComputedStyle(child, null);
        otherWidth += parseInt(styles.borderLeftWidth, 10) +
                      parseInt(styles.borderRightWidth, 10) +
                      parseInt(styles.paddingLeft, 10) +
                      parseInt(styles.paddingRight, 10) +
                      parseInt(styles.marginLeft, 10) +
                      parseInt(styles.marginRight, 10);
      }

      if (body) {
        body.style.width = (parentWidth - otherWidth) + 'px';
      }
    }
  }
};

exports.FFDisplay = FFDisplay;

});
/*
 * Copyright 2012, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

define('gcli/ui/inputter', ['require', 'exports', 'module' , 'util/promise', 'util/util', 'gcli/types', 'gcli/history', 'text!gcli/ui/inputter.css'], function(require, exports, module) {

'use strict';

var Promise = require('util/promise');
var util = require('util/util');
var KeyEvent = require('util/util').KeyEvent;

var Status = require('gcli/types').Status;
var History = require('gcli/history').History;

var inputterCss = require('text!gcli/ui/inputter.css');


var RESOLVED = Promise.resolve(undefined);

/**
 * A wrapper to take care of the functions concerning an input element
 * @param options Object containing user customization properties, including:
 * - scratchpad (default=none)
 * - promptWidth (default=22px)
 * @param components Object that links to other UI components. GCLI provided:
 * - requisition
 * - focusManager
 * - element
 */
function Inputter(options, components) {
  this.requisition = components.requisition;
  this.focusManager = components.focusManager;

  this.element = components.element;
  this.element.classList.add('gcli-in-input');
  this.element.spellcheck = false;

  this.document = this.element.ownerDocument;

  this.scratchpad = options.scratchpad;

  if (inputterCss != null) {
    this.style = util.importCss(inputterCss, this.document, 'gcli-inputter');
  }

  // Used to distinguish focus from TAB in CLI. See onKeyUp()
  this.lastTabDownAt = 0;

  // Used to effect caret changes. See _processCaretChange()
  this._caretChange = null;

  // Ensure that TAB/UP/DOWN isn't handled by the browser
  this.onKeyDown = this.onKeyDown.bind(this);
  this.onKeyUp = this.onKeyUp.bind(this);
  this.element.addEventListener('keydown', this.onKeyDown, false);
  this.element.addEventListener('keyup', this.onKeyUp, false);

  // Setup History
  this.history = new History();
  this._scrollingThroughHistory = false;

  // Used when we're selecting which prediction to complete with
  this._choice = null;
  this.onChoiceChange = util.createEvent('Inputter.onChoiceChange');

  // Cursor position affects hint severity
  this.onMouseUp = this.onMouseUp.bind(this);
  this.element.addEventListener('mouseup', this.onMouseUp, false);

  if (this.focusManager) {
    this.focusManager.addMonitoredElement(this.element, 'input');
  }

  // Start looking like an asynchronous completion isn't happening
  this._completed = RESOLVED;

  this.requisition.onTextChange.add(this.textChanged, this);

  this.assignment = this.requisition.getAssignmentAt(0);
  this.onAssignmentChange = util.createEvent('Inputter.onAssignmentChange');
  this.onInputChange = util.createEvent('Inputter.onInputChange');

  this.onResize = util.createEvent('Inputter.onResize');
  this.onWindowResize = this.onWindowResize.bind(this);
  this.document.defaultView.addEventListener('resize', this.onWindowResize, false);

  this.requisition.update(this.element.value || '');
}

/**
 * Avoid memory leaks
 */
Inputter.prototype.destroy = function() {
  this.document.defaultView.removeEventListener('resize', this.onWindowResize, false);

  this.requisition.onTextChange.remove(this.textChanged, this);
  if (this.focusManager) {
    this.focusManager.removeMonitoredElement(this.element, 'input');
  }

  this.element.removeEventListener('mouseup', this.onMouseUp, false);
  this.element.removeEventListener('keydown', this.onKeyDown, false);
  this.element.removeEventListener('keyup', this.onKeyUp, false);

  this.history.destroy();

  if (this.style) {
    this.style.parentNode.removeChild(this.style);
    delete this.style;
  }

  delete this.onMouseUp;
  delete this.onKeyDown;
  delete this.onKeyUp;
  delete this.onWindowResize;
  delete this.tooltip;
  delete this.document;
  delete this.element;
};

/**
 * Make ourselves visually similar to the input element, and make the input
 * element transparent so our background shines through
 */
Inputter.prototype.onWindowResize = function() {
  // Mochitest sometimes causes resize after shutdown. See Bug 743190
  if (!this.element) {
    return;
  }

  // Simplify when jsdom does getBoundingClientRect(). See Bug 717269
  var dimensions = this.getDimensions();
  if (dimensions) {
    this.onResize(dimensions);
  }
};

/**
 * Make ourselves visually similar to the input element, and make the input
 * element transparent so our background shines through
 */
Inputter.prototype.getDimensions = function() {
  // Remove this when jsdom does getBoundingClientRect(). See Bug 717269
  if (!this.element.getBoundingClientRect) {
    return undefined;
  }

  var fixedLoc = {};
  var currentElement = this.element.parentNode;
  while (currentElement && currentElement.nodeName !== '#document') {
    var style = this.document.defaultView.getComputedStyle(currentElement, '');
    if (style) {
      var position = style.getPropertyValue('position');
      if (position === 'absolute' || position === 'fixed') {
        var bounds = currentElement.getBoundingClientRect();
        fixedLoc.top = bounds.top;
        fixedLoc.left = bounds.left;
        break;
      }
    }
    currentElement = currentElement.parentNode;
  }

  var rect = this.element.getBoundingClientRect();
  return {
    top: rect.top - (fixedLoc.top || 0) + 1,
    height: rect.bottom - rect.top - 1,
    left: rect.left - (fixedLoc.left || 0) + 2,
    width: rect.right - rect.left
  };
};

/**
 * Handler for the input-element.onMouseUp event
 */
Inputter.prototype.onMouseUp = function(ev) {
  this._checkAssignment();
};

/**
 * Handler for the Requisition.textChanged event
 */
Inputter.prototype.textChanged = function() {
  if (!this.document) {
    return; // This can happen post-destroy()
  }

  if (this._caretChange == null) {
    // We weren't expecting a change so this was requested by the hint system
    // we should move the cursor to the end of the 'changed section', and the
    // best we can do for that right now is the end of the current argument.
    this._caretChange = Caret.TO_ARG_END;
  }

  var newStr = this.requisition.toString();
  var input = this.getInputState();

  input.typed = newStr;
  this._processCaretChange(input);

  if (this.element.value !== newStr) {
    this.element.value = newStr;
  }
  this.onInputChange({ inputState: input });
};

/**
 * Various ways in which we need to manipulate the caret/selection position.
 * A value of null means we're not expecting a change
 */
var Caret = {
  /**
   * We are expecting changes, but we don't need to move the cursor
   */
  NO_CHANGE: 0,

  /**
   * We want the entire input area to be selected
   */
  SELECT_ALL: 1,

  /**
   * The whole input has changed - push the cursor to the end
   */
  TO_END: 2,

  /**
   * A part of the input has changed - push the cursor to the end of the
   * changed section
   */
  TO_ARG_END: 3
};

/**
 * If this._caretChange === Caret.TO_ARG_END, we alter the input object to move
 * the selection start to the end of the current argument.
 * @param input An object shaped like { typed:'', cursor: { start:0, end:0 }}
 */
Inputter.prototype._processCaretChange = function(input) {
  var start, end;
  switch (this._caretChange) {
    case Caret.SELECT_ALL:
      start = 0;
      end = input.typed.length;
      break;

    case Caret.TO_END:
      start = input.typed.length;
      end = input.typed.length;
      break;

    case Caret.TO_ARG_END:
      // There could be a fancy way to do this involving assignment/arg math
      // but it doesn't seem easy, so we cheat a move the cursor to just before
      // the next space, or the end of the input
      start = input.cursor.start;
      do {
        start++;
      }
      while (start < input.typed.length && input.typed[start - 1] !== ' ');

      end = start;
      break;

    default:
      start = input.cursor.start;
      end = input.cursor.end;
      break;
  }

  start = (start > input.typed.length) ? input.typed.length : start;
  end = (end > input.typed.length) ? input.typed.length : end;

  var newInput = {
    typed: input.typed,
    cursor: { start: start, end: end }
  };

  if (this.element.selectionStart !== start) {
    this.element.selectionStart = start;
  }
  if (this.element.selectionEnd !== end) {
    this.element.selectionEnd = end;
  }

  this._checkAssignment(start);

  this._caretChange = null;
  return newInput;
};

/**
 * To be called internally whenever we think that the current assignment might
 * have changed, typically on mouse-clicks or key presses.
 * @param start Optional - if specified, the cursor position to use in working
 * out the current assignment. This is needed because setting the element
 * selection start is only recognised when the event loop has finished
 */
Inputter.prototype._checkAssignment = function(start) {
  if (start == null) {
    start = this.element.selectionStart;
  }
  var newAssignment = this.requisition.getAssignmentAt(start);
  if (this.assignment !== newAssignment) {
    this.assignment = newAssignment;
    this.onAssignmentChange({ assignment: this.assignment });
  }

  // This is slightly nasty - the focusManager generally relies on people
  // telling it what it needs to know (which makes sense because the event
  // system to do it with events would be unnecessarily complex). However
  // requisition doesn't know about the focusManager either. So either one
  // needs to know about the other, or a third-party needs to break the
  // deadlock. These 2 lines are all we're quibbling about, so for now we hack
  if (this.focusManager) {
    var message = this.assignment.conversion.message;
    this.focusManager.setError(message != null && message !== '');
  }
};

/**
 * Set the input field to a value, for external use.
 * This function updates the data model. It sets the caret to the end of the
 * input. It does not make any similarity checks so calling this function with
 * it's current value resets the cursor position.
 * It does not execute the input or affect the history.
 * This function should not be called internally, by Inputter and never as a
 * result of a keyboard event on this.element or bug 676520 could be triggered.
 */
Inputter.prototype.setInput = function(str) {
  return this.requisition.update(str);
};

/**
 * Counterpart to |setInput| for moving the cursor.
 * @param cursor An object shaped like { start: x, end: y }
 */
Inputter.prototype.setCursor = function(cursor) {
  this._caretChange = Caret.NO_CHANGE;
  this._processCaretChange({ typed: this.element.value, cursor: cursor });
};

/**
 * Focus the input element
 */
Inputter.prototype.focus = function() {
  this.element.focus();
  this._checkAssignment();
};

/**
 * Ensure certain keys (arrows, tab, etc) that we would like to handle
 * are not handled by the browser
 */
Inputter.prototype.onKeyDown = function(ev) {
  if (ev.keyCode === KeyEvent.DOM_VK_UP || ev.keyCode === KeyEvent.DOM_VK_DOWN) {
    ev.preventDefault();
    return;
  }

  // The following keys do not affect the state of the command line so we avoid
  // informing the focusManager about keyboard events that involve these keys
  if (ev.keyCode === KeyEvent.DOM_VK_F1 ||
      ev.keyCode === KeyEvent.DOM_VK_ESCAPE ||
      ev.keyCode === KeyEvent.DOM_VK_UP ||
      ev.keyCode === KeyEvent.DOM_VK_DOWN) {
    return;
  }

  if (this.focusManager) {
    this.focusManager.onInputChange();
  }

  if (ev.keyCode === KeyEvent.DOM_VK_TAB) {
    this.lastTabDownAt = 0;
    if (!ev.shiftKey) {
      ev.preventDefault();
      // Record the timestamp of this TAB down so onKeyUp can distinguish
      // focus from TAB in the CLI.
      this.lastTabDownAt = ev.timeStamp;
    }
    if (ev.metaKey || ev.altKey || ev.crtlKey) {
      if (this.document.commandDispatcher) {
        this.document.commandDispatcher.advanceFocus();
      }
      else {
        this.element.blur();
      }
    }
  }
};

/**
 * Handler for use with DOM events, which just calls the promise enabled
 * handleKeyUp function but checks the exit state of the promise so we know
 * if something went wrong.
 */
Inputter.prototype.onKeyUp = function(ev) {
  this.handleKeyUp(ev).then(null, util.errorHandler);
};

/**
 * The main keyboard processing loop
 * @return A promise that resolves (to undefined) when the actions kicked off
 * by this handler are completed.
 */
Inputter.prototype.handleKeyUp = function(ev) {
  if (this.focusManager && ev.keyCode === KeyEvent.DOM_VK_F1) {
    this.focusManager.helpRequest();
    return RESOLVED;
  }

  if (this.focusManager && ev.keyCode === KeyEvent.DOM_VK_ESCAPE) {
    this.focusManager.removeHelp();
    return RESOLVED;
  }

  if (ev.keyCode === KeyEvent.DOM_VK_UP) {
    if (this.tooltip && this.tooltip.isMenuShowing) {
      this.changeChoice(-1);
    }
    else if (this.element.value === '' || this._scrollingThroughHistory) {
      this._scrollingThroughHistory = true;
      return this.requisition.update(this.history.backward());
    }
    else {
      // If the user is on a valid value, then we increment the value, but if
      // they've typed something that's not right we page through predictions
      if (this.assignment.getStatus() === Status.VALID) {
        this.requisition.increment(this.assignment);
        // See notes on focusManager.onInputChange in onKeyDown
        if (this.focusManager) {
          this.focusManager.onInputChange();
        }
      }
      else {
        this.changeChoice(-1);
      }
    }
    return RESOLVED;
  }

  if (ev.keyCode === KeyEvent.DOM_VK_DOWN) {
    if (this.tooltip && this.tooltip.isMenuShowing) {
      this.changeChoice(+1);
    }
    else if (this.element.value === '' || this._scrollingThroughHistory) {
      this._scrollingThroughHistory = true;
      return this.requisition.update(this.history.forward());
    }
    else {
      // See notes above for the UP key
      if (this.assignment.getStatus() === Status.VALID) {
        this.requisition.decrement(this.assignment, this.requisition.context);
        // See notes on focusManager.onInputChange in onKeyDown
        if (this.focusManager) {
          this.focusManager.onInputChange();
        }
      }
      else {
        this.changeChoice(+1);
      }
    }
    return RESOLVED;
  }

  // RETURN checks status and might exec
  if (ev.keyCode === KeyEvent.DOM_VK_RETURN) {
    var worst = this.requisition.getStatus();
    // Deny RETURN unless the command might work
    if (worst === Status.VALID) {
      this._scrollingThroughHistory = false;
      this.history.add(this.element.value);
      this.requisition.exec();
    }
    else {
      // If we can't execute the command, but there is a menu choice to use
      // then use it.
      if (!this.tooltip.selectChoice()) {
        this.focusManager.setError(true);
      }
    }

    this._choice = null;
    return RESOLVED;
  }

  if (ev.keyCode === KeyEvent.DOM_VK_TAB && !ev.shiftKey) {
    // Being able to complete 'nothing' is OK if there is some context, but
    // when there is nothing on the command line it just looks bizarre.
    var hasContents = (this.element.value.length > 0);

    // If the TAB keypress took the cursor from another field to this one,
    // then they get the keydown/keypress, and we get the keyup. In this
    // case we don't want to do any completion.
    // If the time of the keydown/keypress of TAB was close (i.e. within
    // 1 second) to the time of the keyup then we assume that we got them
    // both, and do the completion.
    if (hasContents && this.lastTabDownAt + 1000 > ev.timeStamp) {
      // It's possible for TAB to not change the input, in which case the
      // textChanged event will not fire, and the caret move will not be
      // processed. So we check that this is done first
      this._caretChange = Caret.TO_ARG_END;
      var inputState = this.getInputState();
      this._processCaretChange(inputState);

      if (this._choice == null) {
        this._choice = 0;
      }

      // The changes made by complete may happen asynchronously, so after the
      // the call to complete() we should avoid making changes before the end
      // of the event loop
      this._completed = this.requisition.complete(inputState.cursor,
                                                  this._choice);
    }
    this.lastTabDownAt = 0;
    this._scrollingThroughHistory = false;

    return this._completed.then(function() {
      this._choice = null;
      this.onChoiceChange({ choice: this._choice });
    }.bind(this));
  }

  // Give the scratchpad (if enabled) a chance to activate
  if (this.scratchpad && this.scratchpad.shouldActivate(ev)) {
    if (this.scratchpad.activate(this.element.value)) {
      return this.requisition.update('');
    }
    return RESOLVED;
  }

  this._scrollingThroughHistory = false;
  this._caretChange = Caret.NO_CHANGE;

  this._completed = this.requisition.update(this.element.value);

  return this._completed.then(function() {
    this._choice = null;
    this.onChoiceChange({ choice: this._choice });
  }.bind(this));
};

/**
 * Used by onKeyUp for UP/DOWN to change the current choice from an options
 * menu.
 */
Inputter.prototype.changeChoice = function(amount) {
  if (this._choice == null) {
    this._choice = 0;
  }
  // There's an annoying up is down thing here, the menu is presented
  // with the zeroth index at the top working down, so the UP arrow needs
  // pick the choice below because we're working down
  this._choice += amount;
  this.onChoiceChange({ choice: this._choice });
};

/**
 * Accessor for the assignment at the cursor.
 * i.e Requisition.getAssignmentAt(cursorPos);
 */
Inputter.prototype.getCurrentAssignment = function() {
  var start = this.element.selectionStart;
  return this.requisition.getAssignmentAt(start);
};

/**
 * Pull together an input object, which may include XUL hacks
 */
Inputter.prototype.getInputState = function() {
  var input = {
    typed: this.element.value,
    cursor: {
      start: this.element.selectionStart,
      end: this.element.selectionEnd
    }
  };

  // Workaround for potential XUL bug 676520 where textbox gives incorrect
  // values for its content
  if (input.typed == null) {
    input = { typed: '', cursor: { start: 0, end: 0 } };
  }

  // Workaround for a Bug 717268 (which is really a jsdom bug)
  if (input.cursor.start == null) {
    input.cursor.start = 0;
  }

  return input;
};

exports.Inputter = Inputter;


});
/*
 * Copyright 2012, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

define('gcli/history', ['require', 'exports', 'module' ], function(require, exports, module) {

'use strict';

/**
 * A History object remembers commands that have been entered in the past and
 * provides an API for accessing them again.
 * See Bug 681340: Search through history (like C-r in bash)?
 */
function History() {
  // This is the actual buffer where previous commands are kept.
  // 'this._buffer[0]' should always be equal the empty string. This is so
  // that when you try to go in to the "future", you will just get an empty
  // command.
  this._buffer = [''];

  // This is an index in to the history buffer which points to where we
  // currently are in the history.
  this._current = 0;
}

/**
 * Avoid memory leaks
 */
History.prototype.destroy = function() {
  delete this._buffer;
};

/**
 * Record and save a new command in the history.
 */
History.prototype.add = function(command) {
  this._buffer.splice(1, 0, command);
  this._current = 0;
};

/**
 * Get the next (newer) command from history.
 */
History.prototype.forward = function() {
  if (this._current > 0 ) {
    this._current--;
  }
  return this._buffer[this._current];
};

/**
 * Get the previous (older) item from history.
 */
History.prototype.backward = function() {
  if (this._current < this._buffer.length - 1) {
    this._current++;
  }
  return this._buffer[this._current];
};

exports.History = History;

});
define("text!gcli/ui/inputter.css", [], "");

/*
 * Copyright 2012, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

define('gcli/ui/completer', ['require', 'exports', 'module' , 'util/promise', 'util/util', 'util/domtemplate', 'text!gcli/ui/completer.html'], function(require, exports, module) {

'use strict';

var Promise = require('util/promise');
var util = require('util/util');
var domtemplate = require('util/domtemplate');

var completerHtml = require('text!gcli/ui/completer.html');

/**
 * Completer is an 'input-like' element that sits  an input element annotating
 * it with visual goodness.
 * @param options Object containing user customization properties, including:
 * - scratchpad (default=none) A way to move JS content to custom JS editor
 * @param components Object that links to other UI components. GCLI provided:
 * - requisition: A GCLI Requisition object whose state is monitored
 * - element: Element to use as root
 * - autoResize: (default=false): Should we attempt to sync the dimensions of
 *   the complete element with the input element.
 */
function Completer(options, components) {
  this.requisition = components.requisition;
  this.scratchpad = options.scratchpad;
  this.input = { typed: '', cursor: { start: 0, end: 0 } };
  this.choice = 0;

  this.element = components.element;
  this.element.classList.add('gcli-in-complete');
  this.element.setAttribute('tabindex', '-1');
  this.element.setAttribute('aria-live', 'polite');

  this.document = this.element.ownerDocument;

  this.inputter = components.inputter;

  this.inputter.onInputChange.add(this.update, this);
  this.inputter.onAssignmentChange.add(this.update, this);
  this.inputter.onChoiceChange.add(this.update, this);

  this.autoResize = components.autoResize;
  if (this.autoResize) {
    this.inputter.onResize.add(this.resized, this);

    var dimensions = this.inputter.getDimensions();
    if (dimensions) {
      this.resized(dimensions);
    }
  }

  this.template = util.toDom(this.document, completerHtml);
  // We want the spans to line up without the spaces in the template
  util.removeWhitespace(this.template, true);

  this.update();
}

/**
 * Avoid memory leaks
 */
Completer.prototype.destroy = function() {
  this.inputter.onInputChange.remove(this.update, this);
  this.inputter.onAssignmentChange.remove(this.update, this);
  this.inputter.onChoiceChange.remove(this.update, this);

  if (this.autoResize) {
    this.inputter.onResize.remove(this.resized, this);
  }

  delete this.document;
  delete this.element;
  delete this.template;
  delete this.inputter;
};

/**
 * Ensure that the completion element is the same size and the inputter element
 */
Completer.prototype.resized = function(ev) {
  this.element.style.top = ev.top + 'px';
  this.element.style.height = ev.height + 'px';
  this.element.style.lineHeight = ev.height + 'px';
  this.element.style.left = ev.left + 'px';
  this.element.style.width = ev.width + 'px';
};

/**
 * Bring the completion element up to date with what the requisition says
 */
Completer.prototype.update = function(ev) {
  this.choice = (ev && ev.choice != null) ? ev.choice : 0;

  var data = this._getCompleterTemplateData();
  var template = this.template.cloneNode(true);
  domtemplate.template(template, data, { stack: 'completer.html' });

  util.clearElement(this.element);
  while (template.hasChildNodes()) {
    this.element.appendChild(template.firstChild);
  }
};

/**
 * Calculate the properties required by the template process for completer.html
 */
Completer.prototype._getCompleterTemplateData = function() {
  // Some of the data created by this function can be calculated synchronously
  // but other parts depend on predictions which are asynchronous.
  var promisedDirectTabText = Promise.defer();
  var promisedArrowTabText = Promise.defer();
  var promisedEmptyParameters = Promise.defer();

  var input = this.inputter.getInputState();
  var current = this.requisition.getAssignmentAt(input.cursor.start);
  var predictionPromise = undefined;

  if (input.typed.trim().length !== 0) {
    predictionPromise = current.getPredictionAt(this.choice);
  }

  // If anything goes wrong, we pass the error on to all the child promises
  var onError = function(ex) {
    promisedDirectTabText.reject(ex);
    promisedArrowTabText.reject(ex);
    promisedEmptyParameters.reject(ex);
  };

  Promise.resolve(predictionPromise).then(function(prediction) {
    // directTabText is for when the current input is a prefix of the completion
    // arrowTabText is for when we need to use an -> to show what will be used
    var directTabText = '';
    var arrowTabText = '';
    var emptyParameters = [];

    if (input.typed.trim().length !== 0) {
      var cArg = current.arg;

      if (prediction) {
        var tabText = prediction.name;
        var existing = cArg.text;

        // Normally the cursor being just before whitespace means that you are
        // 'in' the previous argument, which means that the prediction is based
        // on that argument, however NamedArguments break this by having 2 parts
        // so we need to prepend the tabText with a space for NamedArguments,
        // but only when there isn't already a space at the end of the prefix
        // (i.e. ' --name' not ' --name ')
        if (current.isInName()) {
          tabText = ' ' + tabText;
        }

        if (existing !== tabText) {
          // Decide to use directTabText or arrowTabText
          // Strip any leading whitespace from the user inputted value because
          // the tabText will never have leading whitespace.
          var inputValue = existing.replace(/^\s*/, '');
          var isStrictCompletion = tabText.indexOf(inputValue) === 0;
          if (isStrictCompletion && input.cursor.start === input.typed.length) {
            // Display the suffix of the prediction as the completion
            var numLeadingSpaces = existing.match(/^(\s*)/)[0].length;

            directTabText = tabText.slice(existing.length - numLeadingSpaces);
          }
          else {
            // Display the '-> prediction' at the end of the completer element
            // \u21E5 is the JS escape right arrow
            arrowTabText = '\u21E5 ' + tabText;
          }
        }
      }
      else {
        // There's no prediction, but if this is a named argument that needs a
        // value (that is without any) then we need to show that one is needed
        // For example 'git commit --message ', clearly needs some more text
        if (cArg.type === 'NamedArgument' && cArg.text === '') {
          emptyParameters.push('<' + current.param.type.name + '>\u00a0');
        }
      }
    }

    // Add a space between the typed text (+ directTabText) and the hints,
    // making sure we don't add 2 sets of padding
    if (directTabText !== '') {
      directTabText += '\u00a0';
    }
    else if (!this.requisition.typedEndsWithSeparator()) {
      emptyParameters.unshift('\u00a0');
    }

    // Calculate the list of parameters to be filled in
    // We generate an array of emptyParameter markers for each positional
    // parameter to the current command.
    // Generally each emptyParameter marker begins with a space to separate it
    // from whatever came before, unless what comes before ends in a space.

    this.requisition.getAssignments().forEach(function(assignment) {
      // Named arguments are handled with a group [options] marker
      if (!assignment.param.isPositionalAllowed) {
        return;
      }

      // No hints if we've got content for this parameter
      if (assignment.arg.toString().trim() !== '') {
        return;
      }

      if (directTabText !== '' && current === assignment) {
        return;
      }

      var text = (assignment.param.isDataRequired) ?
          '<' + assignment.param.name + '>\u00a0' :
          '[' + assignment.param.name + ']\u00a0';

      emptyParameters.push(text);
    }.bind(this));

    var command = this.requisition.commandAssignment.value;
    var addOptionsMarker = false;

    // We add an '[options]' marker when there are named parameters that are
    // not filled in and not hidden, and we don't have any directTabText
    if (command && command.hasNamedParameters) {
      command.params.forEach(function(param) {
        var arg = this.requisition.getAssignment(param.name).arg;
        if (!param.isPositionalAllowed && !param.hidden
                && arg.type === "BlankArgument") {
          addOptionsMarker = true;
        }
      }, this);
    }

    if (addOptionsMarker) {
      // Add an nbsp if we don't have one at the end of the input or if
      // this isn't the first param we've mentioned
      emptyParameters.push('[options]\u00a0');
    }

    promisedDirectTabText.resolve(directTabText);
    promisedArrowTabText.resolve(arrowTabText);
    promisedEmptyParameters.resolve(emptyParameters);
  }.bind(this), onError);

  return {
    statusMarkup: this._getStatusMarkup(input),
    unclosedJs: this._getUnclosedJs(),
    scratchLink: this._getScratchLink(),
    directTabText: promisedDirectTabText.promise,
    arrowTabText: promisedArrowTabText.promise,
    emptyParameters: promisedEmptyParameters.promise
  };
};

/**
 * Calculate the statusMarkup required to show wavy lines underneath the input
 * text (like that of an inline spell-checker) which used by the template
 * process for completer.html
 */
Completer.prototype._getStatusMarkup = function(input) {
  // statusMarkup is wrapper around requisition.getInputStatusMarkup converting
  // space to &nbsp; in the string member (for HTML display) and status to an
  // appropriate class name (i.e. lower cased, prefixed with gcli-in-)
  var statusMarkup = this.requisition.getInputStatusMarkup(input.cursor.start);

  statusMarkup.forEach(function(member) {
    member.string = member.string.replace(/ /g, '\u00a0'); // i.e. &nbsp;
    member.className = 'gcli-in-' + member.status.toString().toLowerCase();
  }, this);

  return statusMarkup;
};

/**
 * Is the entered command a JS command with no closing '}'?
 */
Completer.prototype._getUnclosedJs = function() {
  // TWEAK: This code should be considered for promotion to Requisition
  var command = this.requisition.commandAssignment.value;
  return command && command.name === '{' &&
      this.requisition.getAssignment(0).arg.suffix.indexOf('}') === -1;
};

/**
 * The text for the 'jump to scratchpad' feature, or '' if it is disabled
 */
Completer.prototype._getScratchLink = function() {
  var command = this.requisition.commandAssignment.value;
  return this.scratchpad && command && command.name === '{' ?
      this.scratchpad.linkText :
      '';
};

exports.Completer = Completer;


});
define("text!gcli/ui/completer.html", [], "\n" +
  "<description\n" +
  "    xmlns=\"http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul\">\n" +
  "  <loop foreach=\"member in ${statusMarkup}\">\n" +
  "    <label class=\"${member.className}\" value=\"${member.string}\"></label>\n" +
  "  </loop>\n" +
  "  <label class=\"gcli-in-ontab\" value=\"${directTabText}\"/>\n" +
  "  <label class=\"gcli-in-todo\" foreach=\"param in ${emptyParameters}\" value=\"${param}\"/>\n" +
  "  <label class=\"gcli-in-ontab\" value=\"${arrowTabText}\"/>\n" +
  "  <label class=\"gcli-in-closebrace\" if=\"${unclosedJs}\" value=\"}\"/>\n" +
  "</description>\n" +
  "");

/*
 * Copyright 2012, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

define('gcli/ui/tooltip', ['require', 'exports', 'module' , 'util/util', 'util/domtemplate', 'gcli/cli', 'gcli/ui/fields', 'text!gcli/ui/tooltip.css', 'text!gcli/ui/tooltip.html'], function(require, exports, module) {

'use strict';

var util = require('util/util');
var domtemplate = require('util/domtemplate');

var CommandAssignment = require('gcli/cli').CommandAssignment;
var fields = require('gcli/ui/fields');

var tooltipCss = require('text!gcli/ui/tooltip.css');
var tooltipHtml = require('text!gcli/ui/tooltip.html');


/**
 * A widget to display an inline dialog which allows the user to fill out
 * the arguments to a command.
 * @param options Object containing user customization properties, including:
 * - tooltipClass (default='gcli-tooltip'): Custom class name when generating
 *   the top level element which allows different layout systems
 * @param components Object that links to other UI components. GCLI provided:
 * - requisition: The Requisition to fill out
 * - inputter: An instance of Inputter
 * - focusManager: Component to manage hiding/showing this element
 * - panelElement (optional): The element to show/hide on visibility events
 * - element: The root element to populate
 */
function Tooltip(options, components) {
  this.inputter = components.inputter;
  this.requisition = components.requisition;
  this.focusManager = components.focusManager;

  this.element = components.element;
  this.element.classList.add(options.tooltipClass || 'gcli-tooltip');
  this.document = this.element.ownerDocument;

  this.panelElement = components.panelElement;
  if (this.panelElement) {
    this.panelElement.classList.add('gcli-panel-hide');
    this.focusManager.onVisibilityChange.add(this.visibilityChanged, this);
  }
  this.focusManager.addMonitoredElement(this.element, 'tooltip');

  // We cache the fields we create so we can destroy them later
  this.fields = [];

  // Pull the HTML into the DOM, but don't add it to the document
  if (tooltipCss != null) {
    this.style = util.importCss(tooltipCss, this.document, 'gcli-tooltip');
  }

  this.template = util.toDom(this.document, tooltipHtml);
  this.templateOptions = { blankNullUndefined: true, stack: 'tooltip.html' };

  this.inputter.onChoiceChange.add(this.choiceChanged, this);
  this.inputter.onAssignmentChange.add(this.assignmentChanged, this);

  // We keep a track of which assignment the cursor is in
  this.assignment = undefined;
  this.assignmentChanged({ assignment: this.inputter.assignment });
}

/**
 * Avoid memory leaks
 */
Tooltip.prototype.destroy = function() {
  this.inputter.onAssignmentChange.remove(this.assignmentChanged, this);
  this.inputter.onChoiceChange.remove(this.choiceChanged, this);

  if (this.panelElement) {
    this.focusManager.onVisibilityChange.remove(this.visibilityChanged, this);
  }
  this.focusManager.removeMonitoredElement(this.element, 'tooltip');

  if (this.style) {
    this.style.parentNode.removeChild(this.style);
    delete this.style;
  }

  this.field.onFieldChange.remove(this.fieldChanged, this);
  this.field.destroy();

  delete this.errorEle;
  delete this.descriptionEle;
  delete this.highlightEle;

  delete this.document;
  delete this.element;
  delete this.panelElement;
  delete this.template;
};

/**
 * The inputter acts on UP/DOWN if there is a menu showing
 */
Object.defineProperty(Tooltip.prototype, 'isMenuShowing', {
  get: function() {
    return this.focusManager.isTooltipVisible &&
           this.field != null &&
           this.field.menu != null;
  },
  enumerable: true
});

/**
 * Called whenever the assignment that we're providing help with changes
 */
Tooltip.prototype.assignmentChanged = function(ev) {
  // This can be kicked off either by requisition doing an assign or by
  // inputter noticing a cursor movement out of a command, so we should check
  // that this really is a new assignment
  if (this.assignment === ev.assignment) {
    return;
  }

  if (this.assignment) {
    this.assignment.onAssignmentChange.remove(this.assignmentContentsChanged, this);
  }
  this.assignment = ev.assignment;

  if (this.field) {
    this.field.onFieldChange.remove(this.fieldChanged, this);
    this.field.destroy();
  }

  this.field = fields.getField(this.assignment.param.type, {
    document: this.document,
    name: this.assignment.param.name,
    requisition: this.requisition,
    required: this.assignment.param.isDataRequired,
    named: !this.assignment.param.isPositionalAllowed,
    tooltip: true
  });

  this.focusManager.setImportantFieldFlag(this.field.isImportant);

  this.field.onFieldChange.add(this.fieldChanged, this);
  this.assignment.onAssignmentChange.add(this.assignmentContentsChanged, this);

  this.field.setConversion(this.assignment.conversion);

  // Filled in by the template process
  this.errorEle = undefined;
  this.descriptionEle = undefined;
  this.highlightEle = undefined;

  var contents = this.template.cloneNode(true);
  domtemplate.template(contents, this, this.templateOptions);
  util.clearElement(this.element);
  this.element.appendChild(contents);
  this.element.style.display = 'block';

  this.field.setMessageElement(this.errorEle);

  this._updatePosition();
};

/**
 * Forward the event to the current field
 */
Tooltip.prototype.choiceChanged = function(ev) {
  if (this.field && this.field.setChoiceIndex) {
    var conversion = this.assignment.conversion;
    conversion.constrainPredictionIndex(ev.choice).then(function(choice) {
      this.field.setChoiceIndex(choice);
    }.bind(this)).then(null, util.errorHandler);
  }
};

/**
 * Allow the inputter to use RETURN to chose the current menu item when
 * it can't execute the command line
 * @return true if there was a selection to use, false otherwise
 */
Tooltip.prototype.selectChoice = function(ev) {
  if (this.field && this.field.selectChoice) {
    return this.field.selectChoice();
  }
  return false;
};

/**
 * Called by the onFieldChange event on the current Field
 */
Tooltip.prototype.fieldChanged = function(ev) {
  this.requisition.setAssignment(this.assignment, ev.conversion.arg,
                                 { matchPadding: true });

  var isError = ev.conversion.message != null && ev.conversion.message !== '';
  this.focusManager.setError(isError);

  // Nasty hack, the inputter won't know about the text change yet, so it will
  // get it's calculations wrong. We need to wait until the current set of
  // changes has had a chance to propagate
  this.document.defaultView.setTimeout(function() {
    this.inputter.focus();
  }.bind(this), 10);
};

/**
 * Called by the onAssignmentChange event on the current Assignment
 */
Tooltip.prototype.assignmentContentsChanged = function(ev) {
  // Assignments fire AssignmentChange events on any change, including minor
  // things like whitespace change in arg prefix, so we ignore anything but
  // an actual value change.
  if (ev.conversion.arg.text === ev.oldConversion.arg.text) {
    return;
  }

  this.field.setConversion(ev.conversion);
  util.setTextContent(this.descriptionEle, this.description);

  this._updatePosition();
};

/**
 * Called to move the tooltip to the correct horizontal position
 */
Tooltip.prototype._updatePosition = function() {
  var dimensions = this.getDimensionsOfAssignment();

  // 10 is roughly the width of a char
  if (this.panelElement) {
    this.panelElement.style.left = (dimensions.start * 10) + 'px';
  }

  this.focusManager.updatePosition(dimensions);
};

/**
 * Returns a object containing 'start' and 'end' properties which identify the
 * number of pixels from the left hand edge of the input element that represent
 * the text portion of the current assignment.
 */
Tooltip.prototype.getDimensionsOfAssignment = function() {
  var before = '';
  var assignments = this.requisition.getAssignments(true);
  for (var i = 0; i < assignments.length; i++) {
    if (assignments[i] === this.assignment) {
      break;
    }
    before += assignments[i].toString();
  }
  before += this.assignment.arg.prefix;

  var startChar = before.length;
  before += this.assignment.arg.text;
  var endChar = before.length;

  return { start: startChar, end: endChar };
};

/**
 * The description (displayed at the top of the hint area) should be blank if
 * we're entering the CommandAssignment (because it's obvious) otherwise it's
 * the parameter description.
 */
Object.defineProperty(Tooltip.prototype, 'description', {
  get: function() {
    if (this.assignment instanceof CommandAssignment &&
            this.assignment.value == null) {
      return '';
    }

    return this.assignment.param.manual || this.assignment.param.description;
  },
  enumerable: true
});

/**
 * Tweak CSS to show/hide the output
 */
Tooltip.prototype.visibilityChanged = function(ev) {
  if (!this.panelElement) {
    return;
  }

  if (ev.tooltipVisible) {
    this.panelElement.classList.remove('gcli-panel-hide');
  }
  else {
    this.panelElement.classList.add('gcli-panel-hide');
  }
};

exports.Tooltip = Tooltip;


});
define("text!gcli/ui/tooltip.css", [], "");

define("text!gcli/ui/tooltip.html", [], "\n" +
  "<div class=\"gcli-tt\" aria-live=\"polite\">\n" +
  "  <div class=\"gcli-tt-description\" save=\"${descriptionEle}\">${description}</div>\n" +
  "  ${field.element}\n" +
  "  <div class=\"gcli-tt-error\" save=\"${errorEle}\">${assignment.conversion.message}</div>\n" +
  "  <div class=\"gcli-tt-highlight\" save=\"${highlightEle}\"></div>\n" +
  "</div>\n" +
  "");


// Satisfy EXPORTED_SYMBOLS
this.gcli = require('gcli/index');
