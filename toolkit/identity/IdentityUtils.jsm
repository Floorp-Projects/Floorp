/* -*- Mode: js2; js2-basic-offset: 2; indent-tabs-mode: nil; -*- */
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
  "objectCopy"
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
    delete(aOptions[aOldName]);
  }
};

this.getRandomId = function getRandomId() {
  return uuidgen.generateUUID().toString();
};

/*
 * copy source object into target, excluding private properties
 * (those whose names begin with an underscore)
 */
this.objectCopy = function objectCopy(source, target){
  let desc;
  Object.getOwnPropertyNames(source).forEach(function(name) {
    if (name[0] !== '_') {
      desc = Object.getOwnPropertyDescriptor(source, name);
      Object.defineProperty(target, name, desc);
    }
  });
};
