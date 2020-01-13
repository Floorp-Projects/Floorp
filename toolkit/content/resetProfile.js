/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

document.addEventListener("dialogaccept", onResetProfileAccepted);

function onResetProfileAccepted() {
  let retVals = window.arguments[0];
  retVals.reset = true;
}

document.addEventListener("DOMContentLoaded", async () => {
  await document.l10n.ready;
  let desc = document.querySelector("description");
  desc.style.height = desc.getBoundingClientRect().height + "px";
});
