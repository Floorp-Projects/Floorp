/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/PrivateBrowsingUtils.jsm");

if (!PrivateBrowsingUtils.isContentWindowPrivate(window)) {
  document.addEventListener("DOMContentLoaded", function () {
    document.body.setAttribute("class", "normal");
  }, false);
}
