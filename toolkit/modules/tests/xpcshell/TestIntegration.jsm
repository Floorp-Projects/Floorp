/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Internal module used to test the generation of Integration.jsm getters.
 */

"use strict";

this.EXPORTED_SYMBOLS = [
  "TestIntegration",
];

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/Task.jsm");

this.TestIntegration = {
  value: "value",

  get valueFromThis() {
    return this.value;
  },

  get property() {
    return this._property;
  },

  set property(value) {
    this._property = value;
  },

  method(argument) {
    this.methodArgument = argument;
    return "method" + argument;
  },

  asyncMethod: Task.async(function* (argument) {
    this.asyncMethodArgument = argument;
    return "asyncMethod" + argument;
  }),
};
