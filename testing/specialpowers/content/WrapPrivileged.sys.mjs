/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This module handles wrapping privileged objects so that they can be exposed
 * to unprivileged contexts. It is only to be used in automated tests.
 *
 * Its exact semantics are also liable to change at any time, so any callers
 * relying on undocumented behavior or subtle platform features should expect
 * breakage. Those callers should, wherever possible, migrate to fully
 * chrome-privileged scripts when they need to interact with privileged APIs.
 */

// XPCNativeWrapper is not defined globally in ESLint as it may be going away.
// See bug 1481337.
/* globals XPCNativeWrapper */

Cu.crashIfNotInAutomation();

let wrappedObjects = new WeakMap();
let perWindowInfo = new WeakMap();
let noAutoWrap = new WeakSet();

function isWrappable(x) {
  if (typeof x === "object") {
    return x !== null;
  }
  return typeof x === "function";
}

function isWrapper(x) {
  try {
    return isWrappable(x) && wrappedObjects.has(x);
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

function wrapIfUnwrapped(x, w) {
  return isWrapper(x) ? x : wrapPrivileged(x, w);
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

function wrapPrivileged(obj, win) {
  // Primitives pass straight through.
  if (!isWrappable(obj)) {
    return obj;
  }

  // No double wrapping.
  if (isWrapper(obj)) {
    throw new Error("Trying to double-wrap object!");
  }

  let { windowID, proxies, handler } = perWindowInfo.get(win) || {};
  // |windowUtils| is undefined if |win| is a non-window object
  // such as a sandbox.
  let currentID = win.windowGlobalChild
    ? win.windowGlobalChild.innerWindowId
    : 0;
  // Values are dead objects if the inner window is changed.
  if (windowID !== currentID) {
    windowID = currentID;
    proxies = new WeakMap();
    handler = Cu.cloneInto(SpecialPowersHandler, win, {
      cloneFunctions: true,
    });
    handler.wrapped = new win.WeakMap();
    perWindowInfo.set(win, { windowID, proxies, handler });
  }

  if (proxies.has(obj)) {
    return proxies.get(obj).proxy;
  }

  let className = Cu.getClassName(obj, true);
  if (className === "ArrayBuffer") {
    // Since |new Uint8Array(<proxy>)| doesn't work as expected, we have to
    // return a real ArrayBuffer.
    return obj instanceof win.ArrayBuffer ? obj : Cu.cloneInto(obj, win);
  }

  let dummy;
  if (typeof obj === "function") {
    dummy = Cu.exportFunction(function () {}, win);
  } else {
    dummy = new win.Object();
  }
  handler.wrapped.set(dummy, { obj });

  let proxy = new win.Proxy(dummy, handler);
  wrappedObjects.set(proxy, obj);
  switch (className) {
    case "AnonymousContent":
      // Caching anonymous content will cause crashes (bug 1636015).
      break;
    case "CSS2Properties":
    case "CSSStyleRule":
    case "CSSStyleSheet":
      // Caching these classes will cause memory leaks.
      break;
    default:
      proxies.set(obj, { proxy });
      break;
  }
  return proxy;
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

  // unwrapped.
  return wrappedObjects.get(x);
}

function wrapExceptions(global, fn) {
  try {
    return fn();
  } catch (e) {
    throw wrapIfUnwrapped(e, global);
  }
}

let SpecialPowersHandler = {
  construct(target, args) {
    // The arguments may or may not be wrappers. Unwrap them if necessary.
    var unwrappedArgs = Array.from(Cu.waiveXrays(args), x =>
      unwrapIfWrapped(Cu.unwaiveXrays(x))
    );

    // We want to invoke "obj" as a constructor, but using unwrappedArgs as
    // the arguments.
    let global = Cu.getGlobalForObject(this);
    return wrapExceptions(global, () =>
      wrapIfUnwrapped(
        Reflect.construct(this.wrapped.get(target).obj, unwrappedArgs),
        global
      )
    );
  },

  apply(target, thisValue, args) {
    let wrappedObject = this.wrapped.get(target).obj;
    let global = Cu.getGlobalForObject(this);
    // The invocant and arguments may or may not be wrappers. Unwrap
    // them if necessary.
    var invocant = unwrapIfWrapped(thisValue);

    return wrapExceptions(global, () => {
      if (noAutoWrap.has(wrappedObject)) {
        args = Array.from(Cu.waiveXrays(args), x => Cu.unwaiveXrays(x));
        return doApply(wrappedObject, invocant, args);
      }

      if (wrappedObject.name == "then") {
        args = Array.from(Cu.waiveXrays(args), x =>
          wrapCallback(Cu.unwaiveXrays(x), global)
        );
      } else {
        args = Array.from(Cu.waiveXrays(args), x =>
          unwrapIfWrapped(Cu.unwaiveXrays(x))
        );
      }

      return wrapIfUnwrapped(doApply(wrappedObject, invocant, args), global);
    });
  },

  has(target, prop) {
    return Reflect.has(this.wrapped.get(target).obj, prop);
  },

  get(target, prop, receiver) {
    let global = Cu.getGlobalForObject(this);
    return wrapExceptions(global, () => {
      let obj = waiveXraysIfAppropriate(this.wrapped.get(target).obj, prop);
      let val = Reflect.get(obj, prop);
      return wrapIfUnwrapped(val, global);
    });
  },

  set(target, prop, val, receiver) {
    return wrapExceptions(Cu.getGlobalForObject(this), () => {
      let obj = waiveXraysIfAppropriate(this.wrapped.get(target).obj, prop);
      return Reflect.set(obj, prop, unwrapIfWrapped(val));
    });
  },

  delete(target, prop) {
    return wrapExceptions(Cu.getGlobalForObject(this), () => {
      return Reflect.deleteProperty(this.wrapped.get(target).obj, prop);
    });
  },

  defineProperty(target, prop, descriptor) {
    throw new Error(
      "Can't call defineProperty on SpecialPowers wrapped object"
    );
  },

  getOwnPropertyDescriptor(target, prop) {
    let global = Cu.getGlobalForObject(this);
    return wrapExceptions(global, () => {
      let obj = waiveXraysIfAppropriate(this.wrapped.get(target).obj, prop);
      let desc = Reflect.getOwnPropertyDescriptor(obj, prop);

      if (desc === undefined) {
        return undefined;
      }

      // Transitively maintain the wrapper membrane.
      let wrapIfExists = key => {
        if (key in desc) {
          desc[key] = wrapIfUnwrapped(desc[key], global);
        }
      };

      wrapIfExists("value");
      wrapIfExists("get");
      wrapIfExists("set");

      // A trapping proxy's properties must always be configurable, but sometimes
      // we come across non-configurable properties. Tell a white lie.
      desc.configurable = true;

      return wrapIfUnwrapped(desc, global);
    });
  },

  ownKeys(target) {
    let props = [];

    // Do the normal thing.
    let wrappedObject = this.wrapped.get(target).obj;
    let flt = a => !props.includes(a);
    props = props.concat(Reflect.ownKeys(wrappedObject).filter(flt));

    // If we've got an Xray wrapper, include the expandos as well.
    if ("wrappedJSObject" in wrappedObject) {
      props = props.concat(
        Reflect.ownKeys(wrappedObject.wrappedJSObject).filter(flt)
      );
    }

    return Cu.cloneInto(props, Cu.getGlobalForObject(this));
  },

  preventExtensions(target) {
    throw new Error(
      "Can't call preventExtensions on SpecialPowers wrapped object"
    );
  },
};

function wrapCallback(cb, win) {
  // Do not wrap if it is already privileged.
  if (!isWrappable(cb) || Cu.getObjectPrincipal(cb).isSystemPrincipal) {
    return cb;
  }
  return function SpecialPowersCallbackWrapper() {
    var args = Array.from(arguments, obj => wrapIfUnwrapped(obj, win));
    let invocant = wrapIfUnwrapped(this, win);
    return unwrapIfWrapped(cb.apply(invocant, args));
  };
}

function wrapCallbackObject(obj, win) {
  // Do not wrap if it is already privileged.
  if (!isWrappable(obj) || Cu.getObjectPrincipal(obj).isSystemPrincipal) {
    return obj;
  }
  obj = Cu.waiveXrays(obj);
  var wrapper = {};
  for (var i in obj) {
    if (typeof obj[i] == "function") {
      wrapper[i] = wrapCallback(Cu.unwaiveXrays(obj[i]), win);
    } else {
      wrapper[i] = obj[i];
    }
  }
  return wrapper;
}

function disableAutoWrap(...objs) {
  objs.forEach(x => noAutoWrap.add(x));
}

export var WrapPrivileged = {
  wrap: wrapIfUnwrapped,
  unwrap: unwrapIfWrapped,

  isWrapper,

  wrapCallback,
  wrapCallbackObject,

  disableAutoWrap,
};
