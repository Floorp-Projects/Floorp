"use strict";

/* exported isPageActionShown */

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/PageActions.jsm");

function isPageActionShown(extensionId) {
  return PageActions.isShown(extensionId);
}
