/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Promise",
  "resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
  "resource://gre/modules/PlacesUtils.jsm");

const MANIFEST_PREFS = Services.prefs.getBranch("social.manifest.");
const gProfD = do_get_profile();

const XULAPPINFO_CONTRACTID = "@mozilla.org/xre/app-info;1";
const XULAPPINFO_CID = Components.ID("{c763b610-9d49-455a-bbd2-ede71682a1ac}");

var gAppInfo = null;

function createAppInfo(ID, name, version, platformVersion="1.0") {
  let tmp = {};
  Cu.import("resource://testing-common/AppInfo.jsm", tmp);
  tmp.updateAppInfo({
    ID, name, version, platformVersion,
    crashReporter: true,
  });
  gAppInfo = tmp.getAppInfo();
}

function initApp() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9");
  // prepare a blocklist file for the blocklist service
  var blocklistFile = gProfD.clone();
  blocklistFile.append("blocklist.xml");
  if (blocklistFile.exists())
    blocklistFile.remove(false);
  var source = do_get_file("blocklist.xml");
  source.copyTo(gProfD, "blocklist.xml");
  blocklistFile.lastModifiedTime = Date.now();
}

function AsyncRunner() {
  do_test_pending();
  do_register_cleanup(() => this.destroy());

  this._callbacks = {
    done: do_test_finished,
    error: function (err) {
      // xpcshell test functions like do_check_eq throw NS_ERROR_ABORT on
      // failure.  Ignore those so they aren't rethrown here.
      if (err !== Cr.NS_ERROR_ABORT) {
        if (err.stack) {
          err = err + " - See following stack:\n" + err.stack +
                      "\nUseless do_throw stack";
        }
        do_throw(err);
      }
    },
    consoleError: function (scriptErr) {
      // Try to ensure the error is related to the test.
      let filename = scriptErr.sourceName || scriptErr.toString() || "";
      if (filename.indexOf("/toolkit/components/social/") >= 0)
        do_throw(scriptErr);
    },
  };
  this._iteratorQueue = [];

  // This catches errors reported to the console, e.g., via Cu.reportError, but
  // not on the runner's stack.
  Cc["@mozilla.org/consoleservice;1"].
    getService(Ci.nsIConsoleService).
    registerListener(this);
}

AsyncRunner.prototype = {

  appendIterator: function appendIterator(iter) {
    this._iteratorQueue.push(iter);
  },

  next: function next(arg) {
    if (!this._iteratorQueue.length) {
      this.destroy();
      this._callbacks.done();
      return;
    }

    try {
      var { done, value: val } = this._iteratorQueue[0].next(arg);
      if (done) {
        this._iteratorQueue.shift();
        this.next();
        return;
      }
    }
    catch (err) {
      this._callbacks.error(err);
    }

    // val is an iterator => prepend it to the queue and start on it
    // val is otherwise truthy => call next
    if (val) {
      if (typeof(val) != "boolean")
        this._iteratorQueue.unshift(val);
      this.next();
    }
  },

  destroy: function destroy() {
    Cc["@mozilla.org/consoleservice;1"].
      getService(Ci.nsIConsoleService).
      unregisterListener(this);
    this.destroy = function alreadyDestroyed() {};
  },

  observe: function observe(msg) {
    if (msg instanceof Ci.nsIScriptError &&
        !(msg.flags & Ci.nsIScriptError.warningFlag))
    {
      this._callbacks.consoleError(msg);
    }
  },
};
