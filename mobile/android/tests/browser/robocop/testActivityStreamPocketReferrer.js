/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-disable mozilla/use-chromeutils-import */

Cu.import("resource://gre/modules/Messaging.jsm");
Cu.import("resource://gre/modules/Services.jsm");

let java = new JavaBridge(this);
do_register_cleanup(() => {
    EventDispatcher.instance.unregisterListener(listener);

    java.disconnect();
});
do_test_pending();

var wasTabLoadReceived = false;
var tabLoadContainsPocketReferrer = false;

let listener = {
    onEvent: function(event, data, callback) {
        java.asyncCall("log", "Tab:Load url: " + data.url);
        java.asyncCall("log", "Tab:Load referrerURI: " + data.referrerURI);
        if (event !== "Tab:Load" ||
                data.url === "about:home") {
            return;
        }

        wasTabLoadReceived = true;
        if (data.referrerURI && data.referrerURI.search("pocket") > 0) {
            tabLoadContainsPocketReferrer = true;
        } else {
            tabLoadContainsPocketReferrer = false;
        }
    },
};

let win = Services.wm.getMostRecentWindow("navigator:browser");
EventDispatcher.for(win).registerListener(listener, ["Tab:Load"]);

// Java functions.
function copyTabLoadEventMetadataToJava() {
    java.syncCall("copyTabLoadEventMetadataToJavaReceiver", wasTabLoadReceived, tabLoadContainsPocketReferrer);
    wasTabLoadReceived = false;
}
