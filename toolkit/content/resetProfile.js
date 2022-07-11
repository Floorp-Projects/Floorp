/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

document.addEventListener("dialogaccept", onResetProfileAccepted);
document
  .getElementById("refreshProfileLearnMore")
  .addEventListener("click", e => {
    e.preventDefault();
    let retVals = window.arguments[0];
    retVals.learnMore = true;
    window.close();
  });

document.addEventListener("DOMContentLoaded", function() {
  document
    .getElementById("resetProfileDialog")
    .getButton("accept")
    .classList.add("danger-button");
});

function onResetProfileAccepted() {
  let retVals = window.arguments[0];
  retVals.reset = true;
}
