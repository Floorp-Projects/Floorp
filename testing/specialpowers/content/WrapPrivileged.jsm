/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * This module handles wrapping privileged objects so that they can be exposed
 * to unprivileged contexts without involving UniversalXPConnect. It is only
 * to be used in automated tests.
 *
 * Its exact semantics are also liable to change at any time, so any callers
 * relying on undocumented behavior or subtle platform features should expect
 * breakage. Those callers should, wherever possible, migrate to fully
 * chrome-privileged scripts when they need to interact with privileged APIs.
 */

/* globals XPCNativeWrapper */

if (!Cu.isInAutomation) {
  throw new Error("WrapPrivileged.jsm is only for use in automation");
}

// This code currently works by creating Proxy objects in a privileged
// compartment and exposing them directly to content scopes. Enabling
// permissive COWs for this compartment allows the unprivileged compartment to
// access them.
//
// In the future, we want to remove support for permissive COWs, which will
// mean creating the proxy objects in the unprivileged caller's scope instead.
Cu.forcePermissiveCOWs();

var EXPORTED_SYMBOLS = ["WrapPrivileged"];

function isWrappable(x) {
  if (typeof x === "object") {
    return x !== null;
  }
  return typeof x === "function";
}

function isWrapper(x) {
  try {
    return (
      isWrappable(x) && typeof x.SpecialPowers_wrappedObject !== "undefined"
    );
  } catch (e) {
    // If `x` is a remote object proxy, trying to access an unexpected property
    // on it will throw a security error, even though we're chrome privileged.
    // However, remote proxies are not SpecialPowers wrappers, so:
    return false;
  }
}

function unwrapIfWrapped(x) {
  return isWrapper(x) ? unwrapPrivileged(x) : x;
}

function wrapIfUnwrapped(x) {
  return isWrapper(x) ? x : wrapPrivileged(x);
}

function isObjectOrArray(obj) {
  if (Object(obj) !== obj) {
    return false;
  }
  let arrayClasses = [
    "Object",
    "Array",
    "Int8Array",
    "Uint8Array",
    "Int16Array",
    "Uint16Array",
    "Int32Array",
    "Uint32Array",
    "Float32Array",
    "Float64Array",
    "Uint8ClampedArray",
  ];
  let className = Cu.getClassName(obj, true);
  return arrayClasses.includes(className);
}

// In general, we want Xray wrappers for content DOM objects, because waiving
// Xray gives us Xray waiver wrappers that clamp the principal when we cross
// compartment boundaries. However, there are some exceptions where we want
// to use a waiver:
//
// * Xray adds some gunk to toString(), which has the potential to confuse
//   consumers that aren't expecting Xray wrappers. Since toString() is a
//   non-privileged method that returns only strings, we can just waive Xray
//   for that case.
//
// * We implement Xrays to pure JS [[Object]] and [[Array]] instances that
//   filter out tricky things like callables. This is the right thing for
//   security in general, but tends to break tests that try to pass object
//   literals into SpecialPowers. So we waive [[Object]] and [[Array]]
//   instances before inspecting properties.
//
// * When we don't have meaningful Xray semantics, we create an Opaque
//   XrayWrapper for security reasons. For test code, we generally want to see
//   through that sort of thing.
function waiveXraysIfAppropriate(obj, propName) {
  if (
    propName == "toString" ||
    isObjectOrArray(obj) ||
    /Opaque/.test(Object.prototype.toString.call(obj))
  ) {
    return XPCNativeWrapper.unwrap(obj);
  }
  return obj;
}

// We can't call apply() directy on Xray-wrapped functions, so we have to be
// clever.
function doApply(fun, invocant, args) {
  // We implement Xrays to pure JS [[Object]] instances that filter out tricky
  // things like callables. This is the right thing for security in general,
  // but tends to break tests that try to pass object literals into
  // SpecialPowers. So we waive [[Object]] instances when they're passed to a
  // SpecialPowers-wrapped callable.
  //
  // Note that the transitive nature of Xray waivers means that any property
  // pulled off such an object will also be waived, and so we'll get principal
  // clamping for Xrayed DOM objects reached from literals, so passing things
  // like {l : xoWin.location} won't work. Hopefully the rabbit hole doesn't
  // go that deep.
  args = args.map(x => (isObjectOrArray(x) ? Cu.waiveXrays(x) : x));
  return Reflect.apply(fun, invocant, args);
}

function wrapPrivileged(obj) {
  // Primitives pass straight through.
  if (!isWrappable(obj)) {
    return obj;
  }

  // No double wrapping.
  if (isWrapper(obj)) {
    throw new Error("Trying to double-wrap object!");
  }

  let dummy;
  if (typeof obj === "function") {
    dummy = function() {};
  } else {
    dummy = Object.create(null);
  }

  return new Proxy(dummy, new SpecialPowersHandler(obj));
}

function unwrapPrivileged(x) {
  // We don't wrap primitives, so sometimes we have a primitive where we'd
  // expect to have a wrapper. The proxy pretends to be the type that it's
  // emulating, so we can just as easily check isWrappable() on a proxy as
  // we can on an unwrapped object.
  if (!isWrappable(x)) {
    return x;
  }

  // If we have a wrappable type, make sure it's wrapped.
  if (!isWrapper(x)) {
    throw new Error("Trying to unwrap a non-wrapped object!");
  }

  var obj = x.SpecialPowers_wrappedObject;
  // unwrapped.
  return obj;
}

function specialPowersHasInstance(value) {
  // Because we return wrapped versions of this function, when it's called its
  // wrapper will unwrap the "this" as well as the function itself.  So our
  // "this" is the unwrapped thing we started out with.
  return value instanceof this;
}

function SpecialPowersHandler(wrappedObject) {
  this.wrappedObject = wrappedObject;
}

SpecialPowersHandler.prototype = {
  construct(target, args) {
    // The arguments may or may not be wrappers. Unwrap them if necessary.
    var unwrappedArgs = Array.prototype.slice.call(args).map(unwrapIfWrapped);

    // We want to invoke "obj" as a constructor, but using unwrappedArgs as
    // the arguments.  Make sure to wrap and re-throw exceptions!
    try {
      return wrapIfUnwrapped(
        Reflect.construct(this.wrappedObject, unwrappedArgs)
      );
    } catch (e) {
      throw wrapIfUnwrapped(e);
    }
  },

  apply(target, thisValue, args) {
    // The invocant and arguments may or may not be wrappers. Unwrap
    // them if necessary.
    var invocant = unwrapIfWrapped(thisValue);
    var unwrappedArgs = Array.prototype.slice.call(args).map(unwrapIfWrapped);

    try {
      return wrapIfUnwrapped(
        doApply(this.wrappedObject, invocant, unwrappedArgs)
      );
    } catch (e) {
      // Wrap exceptions and re-throw them.
      throw wrapIfUnwrapped(e);
    }
  },

  has(target, prop) {
    if (prop === "SpecialPowers_wrappedObject") {
      return true;
    }

    return Reflect.has(this.wrappedObject, prop);
  },

  get(target, prop, receiver) {
    if (prop === "SpecialPowers_wrappedObject") {
      return this.wrappedObject;
    }

    let obj = waiveXraysIfAppropriate(this.wrappedObject, prop);
    let val = Reflect.get(obj, prop);
    if (val === undefined && prop == Symbol.hasInstance) {
      // Special-case Symbol.hasInstance to pass the hasInstance check on to our
      // target.  We only do this when the target doesn't have its own
      // Symbol.hasInstance already.  Once we get rid of JS engine class
      // instance hooks (bug 1448218) and always use Symbol.hasInstance, we can
      // remove this bit (bug 1448400).
      return wrapPrivileged(specialPowersHasInstance);
    }
    return wrapIfUnwrapped(val);
  },

  set(target, prop, val, receiver) {
    if (prop === "SpecialPowers_wrappedObject") {
      return false;
    }

    let obj = waiveXraysIfAppropriate(this.wrappedObject, prop);
    return Reflect.set(obj, prop, unwrapIfWrapped(val));
  },

  delete(target, prop) {
    if (prop === "SpecialPowers_wrappedObject") {
      return false;
    }

    return Reflect.deleteProperty(this.wrappedObject, prop);
  },

  defineProperty(target, prop, descriptor) {
    throw new Error(
      "Can't call defineProperty on SpecialPowers wrapped object"
    );
  },

  getOwnPropertyDescriptor(target, prop) {
    // Handle our special API.
    if (prop === "SpecialPowers_wrappedObject") {
      return {
        value: this.wrappedObject,
        writeable: true,
        configurable: true,
        enumerable: false,
      };
    }

    let obj = waiveXraysIfAppropriate(this.wrappedObject, prop);
    let desc = Reflect.getOwnPropertyDescriptor(obj, prop);

    if (desc === undefined) {
      if (prop == Symbol.hasInstance) {
        // Special-case Symbol.hasInstance to pass the hasInstance check on to
        // our target.  We only do this when the target doesn't have its own
        // Symbol.hasInstance already.  Once we get rid of JS engine class
        // instance hooks (bug 1448218) and always use Symbol.hasInstance, we
        // can remove this bit (bug 1448400).
        return {
          value: wrapPrivileged(specialPowersHasInstance),
          writeable: true,
          configurable: true,
          enumerable: false,
        };
      }

      return undefined;
    }

    // Transitively maintain the wrapper membrane.
    function wrapIfExists(key) {
      if (key in desc) {
        desc[key] = wrapIfUnwrapped(desc[key]);
      }
    }

    wrapIfExists("value");
    wrapIfExists("get");
    wrapIfExists("set");

    // A trapping proxy's properties must always be configurable, but sometimes
    // we come across non-configurable properties. Tell a white lie.
    desc.configurable = true;

    return desc;
  },

  ownKeys(target) {
    // Insert our special API. It's not enumerable, but ownKeys()
    // includes non-enumerable properties.
    let props = ["SpecialPowers_wrappedObject"];

    // Do the normal thing.
    let flt = a => !props.includes(a);
    props = props.concat(Reflect.ownKeys(this.wrappedObject).filter(flt));

    // If we've got an Xray wrapper, include the expandos as well.
    if ("wrappedJSObject" in this.wrappedObject) {
      props = props.concat(
        Reflect.ownKeys(this.wrappedObject.wrappedJSObject).filter(flt)
      );
    }

    return props;
  },

  preventExtensions(target) {
    throw new Error(
      "Can't call preventExtensions on SpecialPowers wrapped object"
    );
  },
};

function wrapCallback(cb) {
  return function SpecialPowersCallbackWrapper() {
    var args = Array.prototype.map.call(arguments, wrapIfUnwrapped);
    return cb.apply(this, args);
  };
}

function wrapCallbackObject(obj) {
  obj = Cu.waiveXrays(obj);
  var wrapper = {};
  for (var i in obj) {
    if (typeof obj[i] == "function") {
      wrapper[i] = wrapCallback(obj[i]);
    } else {
      wrapper[i] = obj[i];
    }
  }
  return wrapper;
}

var WrapPrivileged = {
  wrap: wrapIfUnwrapped,
  unwrap: unwrapIfWrapped,

  isWrapper,

  wrapCallback,
  wrapCallbackObject,
};
