/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals addMessageListener*/

"use strict";

(function() {

var {classes: Cc, interfaces: Ci, utils: Cu} = Components;

var {Services} = Cu.import("resource://gre/modules/Services.jsm", {});

const MSG_JAR_FLUSH = "AddonJarFlush";
const MSG_MESSAGE_MANAGER_CACHES_FLUSH = "AddonMessageManagerCachesFlush";


try {
  if (Services.appinfo.processType !== Services.appinfo.PROCESS_TYPE_DEFAULT) {
    // Propagate JAR cache flush notifications across process boundaries.
    addMessageListener(MSG_JAR_FLUSH, function(message) {
      Services.obs.notifyObservers(null, "flush-cache-entry", message.data);
    });
    // Propagate message manager caches flush notifications across processes.
    addMessageListener(MSG_MESSAGE_MANAGER_CACHES_FLUSH, function() {
      Services.obs.notifyObservers(null, "message-manager-flush-caches");
    });
  }
} catch (e) {
  Cu.reportError(e);
}

})();
