"use strict";

/* exported waitForLoad */

function waitForLoad(win) {
  return new Promise(resolve => {
    win.addEventListener("load", function listener() {
      win.removeEventListener("load", listener, true);
      resolve();
    }, true);
  });
}
