/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let Cc = Components.classes;
let Ci = Components.interfaces;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

let listener = {
  receiveMessage: function (msg) {
    let text = msg.json.text;
    document.getElementById('data').innerHTML += text;
  },
  QueryInterface: XPCOMUtils.generateQI(["nsISupportsWeakReference"])
};

function getData() {
  let globalMM = Cc["@mozilla.org/globalmessagemanager;1"].getService(Ci.nsIMessageListenerManager);
  globalMM.addWeakMessageListener("AboutMedia:DataCollected", listener);
  globalMM.broadcastAsyncMessage("AboutMedia:CollectData");
}
