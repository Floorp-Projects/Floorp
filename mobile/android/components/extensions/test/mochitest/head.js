"use strict";

/* exported isPageActionShown clickPageAction */

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/PageActions.jsm");

function isPageActionShown(uuid) {
  return PageActions.isShown(uuid);
}

function clickPageAction(uuid) {
  PageActions.synthesizeClick(uuid);
}
