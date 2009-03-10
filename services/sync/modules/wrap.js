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

const EXPORTED_SYMBOLS = ['Wrap'];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://weave/ext/Observers.js");
Cu.import("resource://weave/log4moz.js");
Cu.import("resource://weave/async.js");
Cu.import("resource://weave/util.js");
Cu.import("resource://weave/faultTolerance.js");

Function.prototype.async = Async.sugar;

/*
 * Wrapper utility functions
 *
 * Not in util.js because that would cause a circular dependency
 * between util.js and async.js (we include async.js so that our
 * returned generator functions have the .async sugar defined)
 */

let Wrap = {

  // NOTE: copy this function over to your objects, use like this:
  //
  // function MyObj() {
  //   this._notify = Wrap.notify("some:prefix:");
  // }
  // MyObj.prototype = {
  //   ...
  //   method: function MyMethod() {
  //     let self = yield;
  //     ...
  //     // doFoo is assumed to be an asynchronous method
  //     this._notify("foo", "", this._doFoo, arg1, arg2).async(this, self.cb);
  //     let ret = yield;
  //     ...
  //   }
  // };
  notify: function WeaveWrap_notify(prefix) {
    return function NotifyWrapMaker(name, subject, method) {
      let savedArgs = Array.prototype.slice.call(arguments, 4);
      return function NotifyWrap() {
        let self = yield;
        let ret;
        let args = Array.prototype.slice.call(arguments);

        try {
          this._log.debug("Event: " + prefix + name + ":start");
          Observers.notify(prefix + name + ":start", subject);

          args = savedArgs.concat(args);
          args.unshift(this, method, self.cb);
          Async.run.apply(Async, args);
          ret = yield;

          this._log.debug("Event: " + prefix + name + ":finish");
          let foo = Observers.notify(prefix + name + ":finish", subject);

        } catch (e) {
          this._log.debug("Event: " + prefix + name + ":error");
          Observers.notify(prefix + name + ":error", subject);
	  throw e;
        }

        self.done(ret);
      };
    };
  },

  // NOTE: see notify, this works the same way.  they can be
  // chained together as well.
  localLock: function WeaveSync_localLock(method /* , arg1, arg2, ..., argN */) {
    let savedMethod = method;
    let savedArgs = Array.prototype.slice.call(arguments, 1);

    return function WeaveLocalLockWrapper(/* argN+1, argN+2, ... */) {
      let self = yield;
      let ret;
      let args = Array.prototype.slice.call(arguments);

      ret = this.lock();
      if (!ret)
        throw "Could not acquire lock";

      try {
        args = savedArgs.concat(args);
        args.unshift(this, savedMethod, self.cb);
        ret = yield Async.run.apply(Async, args);

      } catch (e) {
        throw e;

      } finally {
        this.unlock();
      }

      self.done(ret);
    };
  },

  // NOTE: see notify, this works the same way.  they can be
  // chained together as well.
  // catchAll catches any exceptions and prints a stack trace for them
  catchAll: function WeaveSync_catchAll(method /* , arg1, arg2, ..., argN */) {
    let savedMethod = method;
    let savedArgs = Array.prototype.slice.call(arguments, 1);

    return function WeaveCatchAllWrapper(/* argN+1, argN+2, ... */) {
      let self = yield;
      let ret;
      let args = Array.prototype.slice.call(arguments);

      try {
        args = savedArgs.concat(args);
        args.unshift(this, savedMethod, self.cb);
        ret = yield Async.run.apply(Async, args);

      } catch (e) {
	this._log.debug("Caught exception: " + Utils.exceptionStr(e));
	this._log.debug("\n" + Utils.stackTrace(e));
      }
      self.done(ret);
    };
  }
}
