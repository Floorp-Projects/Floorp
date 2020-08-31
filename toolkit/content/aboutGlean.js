/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function onLoad() {
  document.getElementById("controls-submit").addEventListener("click", () => {
    let tag = document.getElementById("tag-pings").value;
    let log = document.getElementById("log-pings").checked;
    let send = document.getElementById("send-pings").value;
    let AboutGlean = Cc["@mozilla.org/toolkit/glean;1"].createInstance(
      Ci.nsIAboutGlean
    );
    AboutGlean.setLogPings(log);
    AboutGlean.setTagPings(tag);
    AboutGlean.sendPing(send);
  });
}

window.addEventListener("load", onLoad);
