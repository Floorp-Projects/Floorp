/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/ExtensionUtils.jsm");
var {
  instanceOf,
} = ExtensionUtils;

this.EXPORTED_SYMBOLS = ["Schemas"];

/* globals Schemas */

Cu.import("resource://gre/modules/NetUtil.jsm");

function readJSON(uri) {
  return new Promise((resolve, reject) => {
    NetUtil.asyncFetch({uri, loadUsingSystemPrincipal: true}, (inputStream, status) => {
      if (!Components.isSuccessCode(status)) {
        reject(new Error(status));
        return;
      }
      try {
        let text = NetUtil.readInputStreamToString(inputStream, inputStream.available());

        // Chrome JSON files include a license comment that we need to
        // strip off for this to be valid JSON. As a hack, we just
        // look for the first '[' character, which signals the start
        // of the JSON content.
        let index = text.indexOf("[");
        text = text.slice(index);

        resolve(JSON.parse(text));
      } catch (e) {
        reject(e);
      }
    });
  });
}

function getValueBaseType(value) {
  let t = typeof(value);
  if (t == "object") {
    if (value === null) {
      return "null";
    } else if (Array.isArray(value)) {
      return "array";
    } else if (Object.prototype.toString.call(value) == "[object ArrayBuffer]") {
      return "binary";
    }
  } else if (t == "number") {
    if (value % 1 == 0) {
      return "integer";
    }
  }
  return t;
}

// Schema files contain namespaces, and each namespace contains types,
// properties, functions, and events. An Entry is a base class for
// types, properties, functions, and events.
class Entry {
  // Injects JS values for the entry into the extension API
  // namespace. The default implementation is to do
  // nothing. |wrapperFuncs| is used to call the actual implementation
  // of a given function or event. It's an object with properties
  // callFunction, addListener, removeListener, and hasListener.
  inject(name, dest, wrapperFuncs) {
  }
}

// Corresponds either to a type declared in the "types" section of the
// schema or else to any type object used throughout the schema.
class Type extends Entry {
  // Takes a value, checks that it has the correct type, and returns a
  // "normalized" version of the value. The normalized version will
  // include "nulls" in place of omitted optional properties. The
  // result of this function is either {error: "Some type error"} or
  // {value: <normalized-value>}.
  normalize(value) {
    return {error: "invalid type"};
  }

  // Unlike normalize, this function does a shallow check to see if
  // |baseType| (one of the possible getValueBaseType results) is
  // valid for this type. It returns true or false. It's used to fill
  // in optional arguments to functions before actually type checking
  // the arguments.
  checkBaseType(baseType) {
    return false;
  }

  // Helper method that simply relies on checkBaseType to implement
  // normalize. Subclasses can choose to use it or not.
  normalizeBase(type, value) {
    if (this.checkBaseType(getValueBaseType(value))) {
      return {value};
    }
    return {error: `Expected ${type} instead of ${JSON.stringify(value)}`};
  }
}

// Type that allows any value.
class AnyType extends Type {
  normalize(value) {
    return {value};
  }

  checkBaseType(baseType) {
    return true;
  }
}

// An untagged union type.
class ChoiceType extends Type {
  constructor(choices) {
    super();
    this.choices = choices;
  }

  normalize(value) {
    for (let choice of this.choices) {
      let r = choice.normalize(value);
      if (!r.error) {
        return r;
      }
    }

    return {error: "No valid choice"};
  }

  checkBaseType(baseType) {
    return this.choices.some(t => t.checkBaseType(baseType));
  }
}

// This is a reference to another type--essentially a typedef.
class RefType extends Type {
  // For a reference to a type named T declared in namespace NS,
  // namespaceName will be NS and reference will be T.
  constructor(namespaceName, reference) {
    super();
    this.namespaceName = namespaceName;
    this.reference = reference;
  }

  normalize(value) {
    let ns = Schemas.namespaces.get(this.namespaceName);
    let type = ns.get(this.reference);
    if (!type) {
      throw new Error(`Internal error: Type ${this.reference} not found`);
    }
    return type.normalize(value);
  }

  checkBaseType(baseType) {
    let ns = Schemas.namespaces.get(this.namespaceName);
    let type = ns.get(this.reference);
    if (!type) {
      throw new Error(`Internal error: Type ${this.reference} not found`);
    }
    return type.checkBaseType(baseType);
  }
}

class StringType extends Type {
  constructor(enumeration, minLength, maxLength) {
    super();
    this.enumeration = enumeration;
    this.minLength = minLength;
    this.maxLength = maxLength;
  }

  normalize(value) {
    let r = this.normalizeBase("string", value);
    if (r.error) {
      return r;
    }

    if (this.enumeration) {
      if (this.enumeration.includes(value)) {
        return {value};
      }
      return {error: `Invalid enumeration value ${JSON.stringify(value)}`};
    }

    if (value.length < this.minLength) {
      return {error: `String ${JSON.stringify(value)} is too short (must be ${this.minLength})`};
    }
    if (value.length > this.maxLength) {
      return {error: `String ${JSON.stringify(value)} is too long (must be ${this.maxLength})`};
    }

    return r;
  }

  checkBaseType(baseType) {
    return baseType == "string";
  }

  inject(name, dest, wrapperFuncs) {
    if (this.enumeration) {
      let obj = Cu.createObjectIn(dest, {defineAs: name});
      for (let e of this.enumeration) {
        let key = e.toUpperCase();
        obj[key] = e;
      }
    }
  }
}

class ObjectType extends Type {
  constructor(properties, additionalProperties, isInstanceOf) {
    super();
    this.properties = properties;
    this.additionalProperties = additionalProperties;
    this.isInstanceOf = isInstanceOf;
  }

  checkBaseType(baseType) {
    return baseType == "object";
  }

  normalize(value) {
    let v = this.normalizeBase("object", value);
    if (v.error) {
      return v;
    }

    if (this.isInstanceOf) {
      if (Object.keys(this.properties).length ||
          !(this.additionalProperties instanceof AnyType)) {
        throw new Error("InternalError: isInstanceOf can only be used with objects that are otherwise unrestricted");
      }

      if (!instanceOf(value, this.isInstanceOf)) {
        return {error: `Object must be an instance of ${this.isInstanceOf}`};
      }

      // This is kind of a hack, but we can't normalize things that
      // aren't JSON, so we just return them.
      return {value};
    }

    // |value| should be a JS Xray wrapping an object in the
    // extension compartment. This works well except when we need to
    // access callable properties on |value| since JS Xrays don't
    // support those. To work around the problem, we verify that
    // |value| is a plain JS object (i.e., not anything scary like a
    // Proxy). Then we copy the properties out of it into a normal
    // object using a waiver wrapper.

    let klass = Cu.getClassName(value, true);
    if (klass != "Object") {
      return {error: `Expected a plain JavaScript object, got a ${klass}`};
    }

    let properties = Object.create(null);
    {
      // |waived| is scoped locally to avoid accessing it when we
      // don't mean to.
      let waived = Cu.waiveXrays(value);
      for (let prop of Object.getOwnPropertyNames(waived)) {
        let desc = Object.getOwnPropertyDescriptor(waived, prop);
        if (desc.get || desc.set) {
          return {error: "Objects cannot have getters or setters on properties"};
        }
        if (!desc.enumerable) {
          // Chrome ignores non-enumerable properties.
          continue;
        }
        properties[prop] = Cu.unwaiveXrays(desc.value);
      }
    }

    let result = {};
    for (let prop of Object.keys(this.properties)) {
      let {type, optional, unsupported} = this.properties[prop];
      if (unsupported) {
        if (prop in properties) {
          return {error: `Property "${prop}" is unsupported by Firefox`};
        }
      } else if (prop in properties) {
        if (optional && (properties[prop] === null || properties[prop] === undefined)) {
          result[prop] = null;
        } else {
          let r = type.normalize(properties[prop]);
          if (r.error) {
            return r;
          }
          result[prop] = r.value;
        }
      } else if (!optional) {
        return {error: `Property "${prop}" is required`};
      } else {
        result[prop] = null;
      }
    }

    for (let prop of Object.keys(properties)) {
      if (!(prop in this.properties)) {
        if (this.additionalProperties) {
          let r = this.additionalProperties.normalize(properties[prop]);
          if (r.error) {
            return r;
          }
          result[prop] = r.value;
        } else {
          return {error: `Unexpected property "${prop}"`};
        }
      }
    }

    return {value: result};
  }
}

class NumberType extends Type {
  normalize(value) {
    let r = this.normalizeBase("number", value);
    if (r.error) {
      return r;
    }

    if (isNaN(value) || !Number.isFinite(value)) {
      return {error: "NaN or infinity are not valid"};
    }

    return r;
  }

  checkBaseType(baseType) {
    return baseType == "number" || baseType == "integer";
  }
}

class IntegerType extends Type {
  constructor(minimum, maximum) {
    super();
    this.minimum = minimum;
    this.maximum = maximum;
  }

  normalize(value) {
    let r = this.normalizeBase("integer", value);
    if (r.error) {
      return r;
    }

    // Ensure it's between -2**31 and 2**31-1
    if ((value | 0) !== value) {
      return {error: "Integer is out of range"};
    }

    if (value < this.minimum) {
      return {error: `Integer ${value} is too small (must be at least ${this.minimum})`};
    }
    if (value > this.maximum) {
      return {error: `Integer ${value} is too big (must be at most ${this.maximum})`};
    }

    return r;
  }

  checkBaseType(baseType) {
    return baseType == "integer";
  }
}

class BooleanType extends Type {
  normalize(value) {
    return this.normalizeBase("boolean", value);
  }

  checkBaseType(baseType) {
    return baseType == "boolean";
  }
}

class ArrayType extends Type {
  constructor(itemType, minItems, maxItems) {
    super();
    this.itemType = itemType;
    this.minItems = minItems;
    this.maxItems = maxItems;
  }

  normalize(value) {
    let v = this.normalizeBase("array", value);
    if (v.error) {
      return v;
    }

    let result = [];
    for (let element of value) {
      element = this.itemType.normalize(element);
      if (element.error) {
        return element;
      }
      result.push(element.value);
    }

    if (result.length < this.minItems) {
      return {error: `Array requires at least ${this.minItems} items; you have ${result.length}`};
    }

    if (result.length > this.maxItems) {
      return {error: `Array requires at most ${this.maxItems} items; you have ${result.length}`};
    }

    return {value: result};
  }

  checkBaseType(baseType) {
    return baseType == "array";
  }
}

class FunctionType extends Type {
  constructor(parameters) {
    super();
    this.parameters = parameters;
  }

  normalize(value) {
    return this.normalizeBase("function", value);
  }

  checkBaseType(baseType) {
    return baseType == "function";
  }
}

// Represents a "property" defined in a schema namespace with a
// particular value. Essentially this is a constant.
class ValueProperty extends Entry {
  constructor(name, value) {
    super();
    this.name = name;
    this.value = value;
  }

  inject(name, dest, wrapperFuncs) {
    dest[name] = this.value;
  }
}

// Represents a "property" defined in a schema namespace that is not a
// constant.
class TypeProperty extends Entry {
  constructor(name, type) {
    super();
    this.name = name;
    this.type = type;
  }
}

// This class is a base class for FunctionEntrys and Events. It takes
// care of validating parameter lists (i.e., handling of optional
// parameters and parameter type checking).
class CallEntry extends Entry {
  constructor(namespaceName, name, parameters, allowAmbiguousOptionalArguments) {
    super();
    this.namespaceName = namespaceName;
    this.name = name;
    this.parameters = parameters;
    this.allowAmbiguousOptionalArguments = allowAmbiguousOptionalArguments;
  }

  throwError(global, msg) {
    global = Cu.getGlobalForObject(global);
    throw new global.Error(`${msg} for ${this.namespaceName}.${this.name}.`);
  }

  checkParameters(args, global) {
    let fixedArgs = [];

    // First we create a new array, fixedArgs, that is the same as
    // |args| but with null values in place of omitted optional
    // parameters.
    let check = (parameterIndex, argIndex) => {
      if (parameterIndex == this.parameters.length) {
        if (argIndex == args.length) {
          return true;
        }
        return false;
      }

      let parameter = this.parameters[parameterIndex];
      if (parameter.optional) {
        // Try skipping it.
        fixedArgs[parameterIndex] = null;
        if (check(parameterIndex + 1, argIndex)) {
          return true;
        }
      }

      if (argIndex == args.length) {
        return false;
      }

      let arg = args[argIndex];
      if (!parameter.type.checkBaseType(getValueBaseType(arg))) {
        if (parameter.optional && (arg === null || arg === undefined)) {
          fixedArgs[parameterIndex] = null;
        } else {
          return false;
        }
      } else {
        fixedArgs[parameterIndex] = arg;
      }

      return check(parameterIndex + 1, argIndex + 1);
    };

    if (this.allowAmbiguousOptionalArguments) {
      // When this option is set, it's up to the implementation to
      // parse arguments.
      return args;
    } else {
      let success = check(0, 0);
      if (!success) {
        this.throwError(global, "Incorrect argument types");
      }
    }

    // Now we normalize (and fully type check) all non-omitted arguments.
    fixedArgs = fixedArgs.map((arg, parameterIndex) => {
      if (arg === null) {
        return null;
      } else {
        let parameter = this.parameters[parameterIndex];
        let r = parameter.type.normalize(arg);
        if (r.error) {
          this.throwError(global, `Type error for parameter ${parameter.name} (${r.error})`);
        }
        return r.value;
      }
    });

    return fixedArgs;
  }
}

// Represents a "function" defined in a schema namespace.
class FunctionEntry extends CallEntry {
  constructor(namespaceName, name, type, unsupported, allowAmbiguousOptionalArguments) {
    super(namespaceName, name, type.parameters, allowAmbiguousOptionalArguments);
    this.unsupported = unsupported;
  }

  inject(name, dest, wrapperFuncs) {
    if (this.unsupported) {
      return;
    }

    let stub = (...args) => {
      let actuals = this.checkParameters(args, dest);
      return wrapperFuncs.callFunction(this.namespaceName, name, actuals);
    };
    Cu.exportFunction(stub, dest, {defineAs: name});
  }
}

// Represents an "event" defined in a schema namespace.
class Event extends CallEntry {
  constructor(namespaceName, name, type, extraParameters, unsupported) {
    super(namespaceName, name, extraParameters);
    this.type = type;
    this.unsupported = unsupported;
  }

  checkListener(global, listener) {
    let r = this.type.normalize(listener);
    if (r.error) {
      this.throwError(global, "Invalid listener");
    }
    return r.value;
  }

  inject(name, dest, wrapperFuncs) {
    if (this.unsupported) {
      return;
    }

    let addStub = (listener, ...args) => {
      listener = this.checkListener(dest, listener);
      let actuals = this.checkParameters(args, dest);
      return wrapperFuncs.addListener(this.namespaceName, name, listener, actuals);
    };

    let removeStub = (listener) => {
      listener = this.checkListener(dest, listener);
      return wrapperFuncs.removeListener(this.namespaceName, name, listener);
    };

    let hasStub = (listener) => {
      listener = this.checkListener(dest, listener);
      return wrapperFuncs.hasListener(this.namespaceName, name, listener);
    };

    let obj = Cu.createObjectIn(dest, {defineAs: name});
    Cu.exportFunction(addStub, obj, {defineAs: "addListener"});
    Cu.exportFunction(removeStub, obj, {defineAs: "removeListener"});
    Cu.exportFunction(hasStub, obj, {defineAs: "hasListener"});
  }
}

this.Schemas = {
  // Map[<schema-name> -> Map[<symbol-name> -> Entry]]
  // This keeps track of all the schemas that have been loaded so far.
  namespaces: new Map(),

  register(namespaceName, symbol, value) {
    let ns = this.namespaces.get(namespaceName);
    if (!ns) {
      ns = new Map();
      this.namespaces.set(namespaceName, ns);
    }
    ns.set(symbol, value);
  },

  parseType(namespaceName, type, extraProperties = []) {
    let allowedProperties = new Set(extraProperties);

    // Do some simple validation of our own schemas.
    function checkTypeProperties(...extra) {
      let allowedSet = new Set([...allowedProperties, ...extra, "description"]);
      for (let prop of Object.keys(type)) {
        if (!allowedSet.has(prop)) {
          throw new Error(`Internal error: Namespace ${namespaceName} has invalid type property "${prop}" in type "${type.name}"`);
        }
      }
    }

    if ("choices" in type) {
      checkTypeProperties("choices");

      let choices = type.choices.map(t => this.parseType(namespaceName, t));
      return new ChoiceType(choices);
    } else if ("$ref" in type) {
      checkTypeProperties("$ref");
      let ref = type.$ref;
      let ns = namespaceName;
      if (ref.includes(".")) {
        [ns, ref] = ref.split(".");
      }
      return new RefType(ns, ref);
    }

    if (!("type" in type)) {
      throw new Error(`Unexpected value for type: ${JSON.stringify(type)}`);
    }

    allowedProperties.add("type");

    // Otherwise it's a normal type...
    if (type.type == "string") {
      checkTypeProperties("enum", "minLength", "maxLength");

      let enumeration = type.enum || null;
      if (enumeration) {
        // The "enum" property is either a list of strings that are
        // valid values or else a list of {name, description} objects,
        // where the .name values are the valid values.
        enumeration = enumeration.map(e => {
          if (typeof(e) == "object") {
            return e.name;
          } else {
            return e;
          }
        });
      }
      return new StringType(enumeration,
                            type.minLength || 0,
                            type.maxLength || Infinity);
    } else if (type.type == "object") {
      let properties = {};
      for (let propName of Object.keys(type.properties || {})) {
        let propType = this.parseType(namespaceName, type.properties[propName],
                                      ["optional", "unsupported", "deprecated"]);
        properties[propName] = {
          type: propType,
          optional: type.properties[propName].optional || false,
          unsupported: type.properties[propName].unsupported || false,
        };
      }

      let additionalProperties = null;
      if ("additionalProperties" in type) {
        additionalProperties = this.parseType(namespaceName, type.additionalProperties);
      }

      checkTypeProperties("properties", "additionalProperties", "isInstanceOf");
      return new ObjectType(properties, additionalProperties, type.isInstanceOf || null);
    } else if (type.type == "array") {
      checkTypeProperties("items", "minItems", "maxItems");
      return new ArrayType(this.parseType(namespaceName, type.items),
                           type.minItems || 0, type.maxItems || Infinity);
    } else if (type.type == "number") {
      checkTypeProperties();
      return new NumberType();
    } else if (type.type == "integer") {
      checkTypeProperties("minimum", "maximum");
      return new IntegerType(type.minimum || 0, type.maximum || Infinity);
    } else if (type.type == "boolean") {
      checkTypeProperties();
      return new BooleanType();
    } else if (type.type == "function") {
      let parameters = null;
      if ("parameters" in type) {
        parameters = [];
        for (let param of type.parameters) {
          parameters.push({
            type: this.parseType(namespaceName, param, ["name", "optional"]),
            name: param.name,
            optional: param.optional || false,
          });
        }
      }

      checkTypeProperties("parameters");
      return new FunctionType(parameters);
    } else if (type.type == "any") {
      // Need to see what minimum and maximum are supposed to do here.
      checkTypeProperties("minimum", "maximum");
      return new AnyType();
    } else {
      throw new Error(`Unexpected type ${type.type}`);
    }
  },

  loadType(namespaceName, type) {
    this.register(namespaceName, type.id, this.parseType(namespaceName, type, ["id"]));
  },

  loadProperty(namespaceName, name, prop) {
    if ("value" in prop) {
      this.register(namespaceName, name, new ValueProperty(name, prop.value));
    } else {
      // We ignore the "optional" attribute on properties since we
      // don't inject anything here anyway.
      let type = this.parseType(namespaceName, prop, ["optional"]);
      this.register(namespaceName, name, new TypeProperty(name, type));
    }
  },

  loadFunction(namespaceName, fun) {
    // We ignore this property for now.
    let returns = fun.returns;  // eslint-disable-line no-unused-vars

    let f = new FunctionEntry(namespaceName, fun.name,
                              this.parseType(namespaceName, fun,
                                             ["name", "unsupported", "deprecated", "returns",
                                              "allowAmbiguousOptionalArguments"]),
                              fun.unsupported || false,
                              fun.allowAmbiguousOptionalArguments || false);
    this.register(namespaceName, fun.name, f);
  },

  loadEvent(namespaceName, event) {
    let extras = event.extraParameters || [];
    extras = extras.map(param => {
      return {
        type: this.parseType(namespaceName, param, ["name", "optional"]),
        name: param.name,
        optional: param.optional || false,
      };
    });

    // We ignore these properties for now.
    /* eslint-disable no-unused-vars */
    let returns = event.returns;
    let filters = event.filters;
    /* eslint-enable no-unused-vars */

    let type = this.parseType(namespaceName, event,
                              ["name", "unsupported", "deprecated",
                               "extraParameters", "returns", "filters"]);

    let e = new Event(namespaceName, event.name, type, extras,
                      event.unsupported || false);
    this.register(namespaceName, event.name, e);
  },

  load(uri) {
    return readJSON(uri).then(json => {
      for (let namespace of json) {
        let name = namespace.namespace;

        let types = namespace.types || [];
        for (let type of types) {
          this.loadType(name, type);
        }

        let properties = namespace.properties || {};
        for (let propertyName of Object.keys(properties)) {
          this.loadProperty(name, propertyName, properties[propertyName]);
        }

        let functions = namespace.functions || [];
        for (let fun of functions) {
          this.loadFunction(name, fun);
        }

        let events = namespace.events || [];
        for (let event of events) {
          this.loadEvent(name, event);
        }
      }
    });
  },

  inject(dest, wrapperFuncs) {
    for (let [namespace, ns] of this.namespaces) {
      let obj = Cu.createObjectIn(dest, {defineAs: namespace});
      for (let [name, entry] of ns) {
        entry.inject(name, obj, wrapperFuncs);
      }
    }
  },
};
