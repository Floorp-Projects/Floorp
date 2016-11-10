/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// functions common to Identity.jsm and MinimalIdentity.jsm

"use strict";

this.EXPORTED_SYMBOLS = [
  "checkDeprecated",
  "checkRenamed",
  "getRandomId",
  "objectCopy",
  "makeMessageObject",
];

const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "uuidgen",
                                   "@mozilla.org/uuid-generator;1",
                                   "nsIUUIDGenerator");

XPCOMUtils.defineLazyModuleGetter(this, "Logger",
                                  "resource://gre/modules/identity/LogUtils.jsm");

function log(...aMessageArgs) {
  Logger.log.apply(Logger, ["Identity"].concat(aMessageArgs));
}

function defined(item) {
  return typeof item !== 'undefined';
}

var checkDeprecated = this.checkDeprecated = function checkDeprecated(aOptions, aField) {
  if (defined(aOptions[aField])) {
    log("WARNING: field is deprecated:", aField);
    return true;
  }
  return false;
};

this.checkRenamed = function checkRenamed(aOptions, aOldName, aNewName) {
  if (defined(aOptions[aOldName]) &&
      defined(aOptions[aNewName])) {
    let err = "You cannot provide both " + aOldName + " and " + aNewName;
    Logger.reportError(err);
    throw new Error(err);
  }

  if (checkDeprecated(aOptions, aOldName)) {
    aOptions[aNewName] = aOptions[aOldName];
    delete aOptions[aOldName];
  }
};

this.getRandomId = function getRandomId() {
  return uuidgen.generateUUID().toString();
};

/*
 * copy source object into target, excluding private properties
 * (those whose names begin with an underscore)
 */
this.objectCopy = function objectCopy(source, target) {
  let desc;
  Object.getOwnPropertyNames(source).forEach(function(name) {
    if (name[0] !== '_') {
      desc = Object.getOwnPropertyDescriptor(source, name);
      Object.defineProperty(target, name, desc);
    }
  });
};

this.makeMessageObject = function makeMessageObject(aRpCaller) {
  let options = {};

  options.id = aRpCaller.id;
  options.origin = aRpCaller.origin;

  // Backwards compatibility with Persona beta:
  // loggedInUser can be undefined, null, or a string
  options.loggedInUser = aRpCaller.loggedInUser;

  // Special flag for internal calls for Persona in b2g
  options._internal = aRpCaller._internal;

  Object.keys(aRpCaller).forEach(function(option) {
    // Duplicate the callerobject, scrubbing out functions and other
    // internal variables (like _mm, the message manager object)
    if (!Object.hasOwnProperty(this, option)
        && option[0] !== '_'
        && typeof aRpCaller[option] !== 'function') {
      options[option] = aRpCaller[option];
    }
  });

  // check validity of message structure
  if ((typeof options.id === 'undefined') ||
      (typeof options.origin === 'undefined')) {
    let err = "id and origin required in relying-party message: " + JSON.stringify(options);
    reportError(err);
    throw new Error(err);
  }

  return options;
}

