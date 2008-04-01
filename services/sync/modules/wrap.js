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
Cu.import("resource://weave/log4moz.js");
Cu.import("resource://weave/dav.js");
Cu.import("resource://weave/async.js");

Function.prototype.async = Async.sugar;

/*
 * Wrapper utility functions
 *
 * Not in util.js because that would cause a circular dependency
 * between util.js and async.js (we include async.js so that our
 * returned generator functions have the .async sugar defined)
 */

let Wrap = {

  // NOTE: requires _osPrefix string property in your object
  // NOTE2: copy this function over to your objects, use like this:
  //
  // MyObj.prototype = {
  //   _notify: Wrap.notify,
  //   ...
  //   method: function MyMethod() {
  //     let self = yield;
  //     ...
  //     // doFoo is assumed to be an asynchronous method
  //     this._notify("foo", this._doFoo, arg1, arg2).async(this, self.cb);
  //     let ret = yield;
  //     ...
  //   }
  // };
  notify: function Weave_notify(name, method /* , arg1, arg2, ..., argN */) {
    let savedName = name;
    let savedMethod = method;
    let savedArgs = Array.prototype.slice.call(arguments, 2);

    return function WeaveNotifyWrapper(/* argN+1, argN+2, ... */) {
      let self = yield;
      let ret;
      let args = Array.prototype.slice.call(arguments);

      try {
        this._os.notifyObservers(null, this._osPrefix + savedName + ":start", "");

        args = savedArgs.concat(args);
        args.unshift(this, savedMethod, self.cb);
        Async.run.apply(Async, args);
        ret = yield;

        this._os.notifyObservers(null, this._osPrefix + savedName + ":success", "");

      } catch (e) {
        this._os.notifyObservers(null, this._osPrefix + savedName + ":error", "");
        throw e;
      }

      self.done(ret);
    };
  },

  // NOTE: see notify, this works the same way.  they can be
  // chained together as well.
  lock: function WeaveSync_lock(method /* , arg1, arg2, ..., argN */) {
    let savedMethod = method;
    let savedArgs = Array.prototype.slice.call(arguments, 1);

    return function WeaveLockWrapper( /* argN+1, argN+2, ... */) {
      let self = yield;
      let ret;
      let args = Array.prototype.slice.call(arguments);

      DAV.lock.async(DAV, self.cb);
      let locked = yield;
      if (!locked)
        throw "Could not acquire lock";

      try {
        args = savedArgs.concat(args);
        args.unshift(this, savedMethod, self.cb);
        Async.run.apply(Async, args);
        ret = yield;

      } catch (e) {
        throw e;

      } finally {
        DAV.unlock.async(DAV, self.cb);
        yield;
      }

      self.done(ret);
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

      if (DAV.locked)
        throw "Could not acquire lock";
      DAV.allowLock = false;

      try {
        args = savedArgs.concat(args);
        args.unshift(this, savedMethod, self.cb);
        Async.run.apply(Async, args);
        ret = yield;
      }
      catch (e) { throw e; }
      finally { DAV.allowLock = true; }

      self.done(ret);
    };
  }
}
