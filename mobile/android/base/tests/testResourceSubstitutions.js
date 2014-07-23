// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Promise.jsm"); /*global Promise */
Cu.import("resource://gre/modules/Services.jsm"); /*global Services */
Cu.import("resource://gre/modules/NetUtil.jsm"); /*global NetUtil */

function readChannel(url) {
  let deferred = Promise.defer();

  let channel = NetUtil.newChannel(url);
  channel.contentType = "text/plain";

  NetUtil.asyncFetch(channel, function(inputStream, status) {
    if (!Components.isSuccessCode(status)) {
      deferred.reject();
      return;
    }

    let content = NetUtil.readInputStreamToString(inputStream, inputStream.available());
    deferred.resolve(content);
  });

  return deferred.promise;
}

add_task(function test_Android() {
    let protocolHandler = Services.io
       .getProtocolHandler("resource")
       .QueryInterface(Ci.nsIResProtocolHandler);

    do_check_true(protocolHandler.hasSubstitution("android"));

    // This can be any file that we know exists in the root of every APK.
    let packageName = yield readChannel("resource://android/package-name.txt");

    // It's difficult to fish ANDROID_PACKAGE_NAME from JavaScript, so we test the
    // following weaker condition.
    let expectedPrefix = "org.mozilla.";
    do_check_eq(packageName.substring(0, expectedPrefix.length), expectedPrefix);
});

run_next_test();
