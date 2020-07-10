/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = [
  "ContentPref",
  "cbHandleResult",
  "cbHandleError",
  "cbHandleCompletion",
  "safeCallback",
  "_methodsCallableFromChild",
];

function ContentPref(domain, name, value) {
  this.domain = domain;
  this.name = name;
  this.value = value;
}

ContentPref.prototype = {
  QueryInterface: ChromeUtils.generateQI(["nsIContentPref"]),
};

function cbHandleResult(callback, pref) {
  safeCallback(callback, "handleResult", [pref]);
}

function cbHandleCompletion(callback, reason) {
  safeCallback(callback, "handleCompletion", [reason]);
}

function cbHandleError(callback, nsresult) {
  safeCallback(callback, "handleError", [nsresult]);
}

function safeCallback(callbackObj, methodName, args) {
  if (!callbackObj || typeof callbackObj[methodName] != "function") {
    return;
  }
  try {
    callbackObj[methodName].apply(callbackObj, args);
  } catch (err) {
    Cu.reportError(err);
  }
}

const _methodsCallableFromChild = Object.freeze([
  ["getByName", ["name", "context", "callback"]],
  ["getByDomainAndName", ["domain", "name", "context", "callback"]],
  ["getBySubdomainAndName", ["domain", "name", "context", "callback"]],
  ["getGlobal", ["name", "context", "callback"]],
  ["set", ["domain", "name", "value", "context", "callback"]],
  ["setGlobal", ["name", "value", "context", "callback"]],
  ["removeByDomainAndName", ["domain", "name", "context", "callback"]],
  ["removeBySubdomainAndName", ["domain", "name", "context", "callback"]],
  ["removeGlobal", ["name", "context", "callback"]],
  ["removeByDomain", ["domain", "context", "callback"]],
  ["removeBySubdomain", ["domain", "context", "callback"]],
  ["removeByName", ["name", "context", "callback"]],
  ["removeAllDomains", ["context", "callback"]],
  ["removeAllGlobals", ["context", "callback"]],
]);
