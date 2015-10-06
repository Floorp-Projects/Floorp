/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

(function() {

var {classes: Cc, interfaces: Ci, utils: Cu} = Components;

var {Services} = Cu.import("resource://gre/modules/Services.jsm", {});

var nsIFile = Components.Constructor("@mozilla.org/file/local;1", "nsIFile",
                                     "initWithPath");

const MSG_JAR_FLUSH = "AddonJarFlush";


try {
  if (Services.appinfo.processType !== Services.appinfo.PROCESS_TYPE_DEFAULT) {
  // Propagate JAR cache flush notifications across process boundaries.
    addMessageListener(MSG_JAR_FLUSH, function jar_flushMessageListener(message) {
      let file = new nsIFile(message.data);
      Services.obs.notifyObservers(file, "flush-cache-entry", null);
    });
  }
} catch(e) {
  Cu.reportError(e);
}

})();
