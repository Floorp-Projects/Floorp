"use strict";

/* exported isPageActionShown clickPageAction */

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/PageActions.jsm");

function isPageActionShown(extensionId) {
  return PageActions.isShown(extensionId);
}

function clickPageAction(extensionId) {
  PageActions.synthesizeClick(extensionId);
}
