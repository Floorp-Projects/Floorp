/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Bookmarks Sync.
 *
 * The Initial Developer of the Original Code is Mozilla.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Dan Mills <thunder@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

const EXPORTED_SYMBOLS = ['Async'];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://weave/log4moz.js");
Cu.import("resource://weave/util.js");

/*
 * Asynchronous generator helpers
 */

function AsyncException(generator, message) {
  this.generator = generator;
  this.message = message;
  this._trace = this.generator.trace;
}
AsyncException.prototype = {
  get generator() { return this._generator; },
  set generator(value) {
    if (!(value instanceof Generator))
      throw "expected type 'Generator'";
    this._generator = value;
  },

  get message() { return this._message; },
  set message(value) { this._message = value; },

  get trace() { return this._trace; },
  set trace(value) { this._trace = value; },
  
  toString: function AsyncException_toString() {
    return this.message;
  }
};

function Generator(object, method, onComplete, args) {
  this._log = Log4Moz.Service.getLogger("Async.Generator");
  this._object = object;
  this._method = method;
  this.onComplete = onComplete;
  this._args = args;

  let frame = Components.stack.caller;
  if (frame.name == "Async_run")
    frame = frame.caller;
  if (frame.name == "Async_sugar")
    frame = frame.caller;

  this._initFrame = frame;
}
Generator.prototype = {
  get name() { return this._method.name; },
  get generator() { return this._generator; },

  // set to true to error when generators to bottom out/return.
  // you will then have to ensure that all generators yield as the
  // last thing they do, and never call 'return'
  get errorOnStop() { return this._errorOnStop; },
  set errorOnStop(value) { this._errorOnStop = value; },

  get cb() {
    let cb = Utils.bind2(this, function(data) { this.cont(data); });
    cb.parentGenerator = this;
    return cb;
  },
  get listener() { return new Utils.EventListener(this.cb); },

  get _object() { return this.__object; },
  set _object(value) {
    if (typeof value != "object")
      throw "expected type 'object', got type '" + typeof(value) + "'";
    this.__object = value;
  },

  get _method() { return this.__method; },
  set _method(value) {
    if (typeof value != "function")
      throw "expected type 'function', got type '" + typeof(value) + "'";
    this.__method = value;
  },

  get onComplete() {
    if (this._onComplete)
      return this._onComplete;
    return function() { this._log.trace("Generator " + this.name + " has no onComplete"); };
  },
  set onComplete(value) {
    if (value && typeof value != "function")
      throw "expected type 'function', got type '" + typeof(value) + "'";
    this._onComplete = value;
  },

  get trace() {
    return Utils.stackTrace(this._initFrame) +
      "JS frame :: unknown (async) :: " + this.name;
  },

  _handleException: function AsyncGen__handleException(e) {
    if (e instanceof StopIteration) {
      if (this.errorOnStop) {
        this._log.error("Generator stopped unexpectedly");
        this._log.trace("Stack trace:\n" + this.trace);
        this._exception = "Generator stopped unexpectedly"; // don't propagate StopIteration
      }

    } else if (this.onComplete.parentGenerator instanceof Generator) {
      //this._log.trace("Saving exception and stack trace");

      switch (typeof e) {
      case "string":
        e = new AsyncException(this, e);
        break;
      case "object":
        if (e.trace) // means we're re-throwing up the stack
          e.trace = this.trace + "\n" + e.trace;
        else
          e.trace = this.trace;
        break;
      default:
        this._log.debug("Unknown exception type: " + typeof(e));
        break;
      }

      this._exception = e;

    } else {
      this._log.error(Async.exceptionStr(this, e));
      this._log.trace("Stack trace:\n" + this.trace +
                      (e.trace? "\n" + e.trace : ""));
    }

    // continue execution of caller.
    // in the case of StopIteration we could return an error right
    // away, but instead it's easiest/best to let the caller handle
    // the error after a yield / in a callback.
    this.done();
  },

  run: function AsyncGen_run() {
    try {
      this._generator = this._method.apply(this._object, this._args);
      this.generator.next(); // must initialize before sending
      this.generator.send(this);
    } catch (e) {
      this._handleException(e);
    }
  },

  cont: function AsyncGen_cont(data) {
    try { this.generator.send(data); }
    catch (e) { this._handleException(e); }
  },

  throw: function AsyncGen_throw(exception) {
    try { this.generator.throw(exception); }
    catch (e) { this._handleException(e); }
  },

  // async generators can't simply call a callback with the return
  // value, since that would cause the calling function to end up
  // running (after the yield) from inside the generator.  Instead,
  // generators can call this method which sets up a timer to call the
  // callback from a timer (and cleans up the timer to avoid leaks).
  // It also closes generators after the timeout, to keep things
  // clean.
  done: function AsyncGen_done(retval) {
    if (this._timer) // the generator/_handleException called self.done() twice
      return;
    let self = this;
    let cb = function() { self._done(retval); };
    this._timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    this._timer.initWithCallback(new Utils.EventListener(cb),
                                 0, this._timer.TYPE_ONE_SHOT);
  },

  _done: function AsyncGen__done(retval) {
    this._generator.close();
    this._generator = null;
    this._timer = null;

    if (this._exception) {
      this._log.trace("Propagating exception to parent generator");
      this.onComplete.parentGenerator.throw(this._exception);
    } else {
      try {
        this.onComplete(retval);
      } catch (e) {
        this._log.error("Exception caught from onComplete handler of " +
                        this.name + " generator");
        this._log.error("Exception: " + Utils.exceptionStr(e));
        this._log.trace("Current stack trace:\n" + Utils.stackTrace(Components.stack));
        this._log.trace("Initial stack trace:\n" + this.trace);
      }
    }
  }
};

Async = {

  // Use:
  // let gen = Async.run(this, this.fooGen, ...);
  // let ret = yield;
  //
  // where fooGen is a generator function, and gen is a Generator instance
  // ret is whatever the generator 'returns' via Generator.done().

  run: function Async_run(object, method, onComplete, args) {
    let args = Array.prototype.slice.call(arguments, 3);
    let gen = new Generator(object, method, onComplete, args);
    gen.run();
    return gen;
  },

  // Syntactic sugar for run().  Meant to be used like this in code
  // that imports this file:
  //
  // Function.prototype.async = Async.sugar;
  //
  // So that you can do:
  //
  // let gen = fooGen.async(...);
  // let ret = yield;
  //
  // Note that 'this' refers to the method being called, not the
  // Async object.

  sugar: function Async_sugar(object, onComplete, extra_args) {
    let args = Array.prototype.slice.call(arguments, 1);
    args.unshift(object, this);
    Async.run.apply(Async, args);
  },

  exceptionStr: function Async_exceptionStr(gen, ex) {
    return "Exception caught in " + gen.name + ": " + Utils.exceptionStr(ex);
  }
};
