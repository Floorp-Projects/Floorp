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

let gCurrentId = 0;
let gCurrentCbId = 0;
let gOutstandingGenerators = 0;

function AsyncException(initFrame, message) {
  this.message = message;
  this._trace = initFrame;
}
AsyncException.prototype = {
  get message() { return this._message; },
  set message(value) { this._message = value; },

  get trace() { return this._trace; },
  set trace(value) { this._trace = value; },

  addFrame: function AsyncException_addFrame(frame) {
    this.trace += (this.trace? "\n" : "") + formatFrame(frame);
  },

  toString: function AsyncException_toString() {
    return this.message;
  }
};

function Generator(thisArg, method, onComplete, args) {
  gOutstandingGenerators++;
  this._outstandingCbs = 0;
  this._log = Log4Moz.Service.getLogger("Async.Generator");
  this._log.level =
    Log4Moz.Level[Utils.prefs.getCharPref("log.logger.async")];
  this._thisArg = thisArg;
  this._method = method;
  this._id = gCurrentId++;
  this.onComplete = onComplete;
  this._args = args;
  this._initFrame = Components.stack.caller;
  // skip our frames
  // FIXME: we should have a pref for this, for debugging async.js itself
  while (this._initFrame.name.match(/^Async(Gen|)_/))
    this._initFrame = this._initFrame.caller;
}
Generator.prototype = {
  get name() { return this._method.name + "-" + this._id; },
  get generator() { return this._generator; },

  get cb() {
    let caller = Components.stack.caller;
    let cbId = gCurrentCbId++;
    this._outstandingCbs++;
    this._log.trace(this.name + ": cb-" + cbId + " generated at:\n" +
                    formatFrame(caller));
    let self = this;
    let cb = function(data) {
      self._log.trace(self.name + ": cb-" + cbId + " called.");
      self._cont(data);
    };
    cb.parentGenerator = this;
    return cb;
  },
  get listener() { return new Utils.EventListener(this.cb); },

  get _thisArg() { return this.__thisArg; },
  set _thisArg(value) {
    if (typeof value != "object")
      throw "Generator: expected type 'object', got type '" + typeof(value) + "'";
    this.__thisArg = value;
  },

  get _method() { return this.__method; },
  set _method(value) {
    if (typeof value != "function")
      throw "Generator: expected type 'function', got type '" + typeof(value) + "'";
    this.__method = value;
  },

  get onComplete() {
    if (this._onComplete)
      return this._onComplete;
    return function() {
      //this._log.trace("Generator " + this.name + " has no onComplete");
    };
  },
  set onComplete(value) {
    if (value && typeof value != "function")
      throw "Generator: expected type 'function', got type '" + typeof(value) + "'";
    this._onComplete = value;
  },

  get trace() {
    return "unknown (async) :: " + this.name + "\n" + trace(this._initFrame);
  },

  _handleException: function AsyncGen__handleException(e) {
    if (e instanceof StopIteration) {
      this._log.trace(this.name + ": End of coroutine reached.");
      // skip to calling done()

    } else if (this.onComplete.parentGenerator instanceof Generator) {
      this._log.trace("[" + this.name + "] Saving exception and stack trace");
      this._log.trace("Exception: " + Utils.exceptionStr(e));

        if (e instanceof AsyncException) {
          // FIXME: attempt to skip repeated frames, which can happen if the
          // child generator never yielded.  Would break for valid repeats (recursion)
          if (e.trace.indexOf(formatFrame(this._initFrame)) == -1)
            e.addFrame(this._initFrame);
        } else {
          e = new AsyncException(this.trace, e);
        }

      this._exception = e;

    } else {
      this._log.error("Exception: " + Utils.exceptionStr(e));
      this._log.debug("Stack trace:\n" + (e.trace? e.trace : this.trace));
    }

    // continue execution of caller.
    // in the case of StopIteration we could return an error right
    // away, but instead it's easiest/best to let the caller handle
    // the error after a yield / in a callback.
    if (!this._timer) {
      this._log.trace("[" + this.name + "] running done() from _handleException()");
      this.done();
    }
  },

  _detectDeadlock: function AsyncGen__detectDeadlock() {
    if (this._outstandingCbs == 0)
      this._log.warn("Async method '" + this.name +
                     "' may have yielded without an outstanding callback.");
  },

  run: function AsyncGen_run() {
    this._continued = false;
    try {
      this._generator = this._method.apply(this._thisArg, this._args);
      this.generator.next(); // must initialize before sending
      this.generator.send(this);
      this._detectDeadlock();
    } catch (e) {
      if (!(e instanceof StopIteration) || !this._timer)
        this._handleException(e);
    }
  },

  _cont: function AsyncGen__cont(data) {
    this._outstandingCbs--;
    this._log.trace(this.name + ": resuming coroutine.");
    this._continued = true;
    try {
      this.generator.send(data);
      this._detectDeadlock();
    } catch (e) {
      if (!(e instanceof StopIteration) || !this._timer)
        this._handleException(e);
    }
  },

  _throw: function AsyncGen__throw(exception) {
    this._outstandingCbs--;
    try { this.generator.throw(exception); }
    catch (e) {
      if (!(e instanceof StopIteration) || !this._timer)
        this._handleException(e);
    }
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
    this._log.trace(this.name + ": done() called.");
    if (!this._exception && this._outstandingCbs > 0)
      this._log.warn("Async method '" + this.name +
                     "' may have outstanding callbacks.");
    this._timer = Utils.makeTimerForCall(cb);
  },

  _done: function AsyncGen__done(retval) {
    if (!this._generator) {
      this._log.error("Async method '" + this.name + "' is missing a 'yield' call " +
                      "(or called done() after being finalized)");
      this._log.trace("Initial stack trace:\n" + this.trace);
    } else {
      this._generator.close();
    }
    this._generator = null;
    this._timer = null;

    if (this._exception) {
      this._log.trace("[" + this.name + "] Propagating exception to parent generator");
      this.onComplete.parentGenerator._throw(this._exception);
    } else {
      try {
        this._log.trace("[" + this.name + "] Running onComplete()");
        this.onComplete(retval);
      } catch (e) {
        this._log.error("Exception caught from onComplete handler of " +
                        this.name + " generator");
        this._log.error("Exception: " + Utils.exceptionStr(e));
        this._log.trace("Current stack trace:\n" + trace(Components.stack));
        this._log.trace("Initial stack trace:\n" + this.trace);
      }
    }
    gOutstandingGenerators--;
  }
};

function formatFrame(frame) {
  // FIXME: sort of hackish, might be confusing if there are multiple
  // extensions with similar filenames
  let tmp = frame.filename.replace(/^file:\/\/.*\/([^\/]+.js)$/, "module:$1");
  tmp += ":" + frame.lineNumber + " :: " + frame.name;
  return tmp;
}

function trace(frame, str) {
  if (!str)
    str = "";

  // skip our frames
  // FIXME: we should have a pref for this, for debugging async.js itself
  while (frame.name && frame.name.match(/^Async(Gen|)_/))
    frame = frame.caller;

  if (frame.caller)
    str = trace(frame.caller, str);
  str = formatFrame(frame) + (str? "\n" : "") + str;

  return str;
}


Async = {
  get outstandingGenerators() { return gOutstandingGenerators; },

  // Use:
  // let gen = Async.run(this, this.fooGen, ...);
  // let ret = yield;
  //
  // where fooGen is a generator function, and gen is a Generator instance
  // ret is whatever the generator 'returns' via Generator.done().

  run: function Async_run(thisArg, method, onComplete /* , arg1, arg2, ... */) {
    let args = Array.prototype.slice.call(arguments, 3);
    let gen = new Generator(thisArg, method, onComplete, args);
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

  sugar: function Async_sugar(thisArg, onComplete  /* , arg1, arg2, ... */) {
    let args = Array.prototype.slice.call(arguments, 1);
    args.unshift(thisArg, this);
    Async.run.apply(Async, args);
  },

  exceptionStr: function Async_exceptionStr(gen, ex) {
    return "Exception caught in " + gen.name + ": " + Utils.exceptionStr(ex);
  }
};
