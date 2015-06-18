// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

let Log = Cu.import("resource://gre/modules/AndroidLog.jsm", {}).AndroidLog.bind("TestJavaAddons");
Cu.import("resource://gre/modules/JavaAddonManager.jsm"); /*global JavaAddonManager */
Cu.import("resource://gre/modules/Promise.jsm"); /*global Promise */
Cu.import("resource://gre/modules/Services.jsm"); /*global Services */
Cu.import("resource://gre/modules/Messaging.jsm"); /*global Messaging */

const DEX_FILE = "chrome://roboextender/content/javaaddons-test.apk";
const CLASS = "org.mozilla.javaaddons.test.JavaAddonV1";

const MESSAGE = "JavaAddon:V1";

add_task(function testFailureCases() {
  do_print("Loading Java Addon from non-existent class.");
  let gotError1 = yield JavaAddonManager.classInstanceFromFile(CLASS + "GARBAGE", DEX_FILE)
    .then((result) => false)
    .catch((error) => true);
  do_check_eq(gotError1, true);

  do_print("Loading Java Addon from non-existent DEX file.");
  let gotError2 = yield JavaAddonManager.classInstanceFromFile(CLASS, DEX_FILE + "GARBAGE")
    .then((result) => false)
    .catch((error) => true);
  do_check_eq(gotError2, true);
});

// Make a request to a dynamically loaded Java Addon; wait for a response.
// Then expect the add-on to make a request; respond.
// Then expect the add-on to make a second request; use it to verify the response to the first request.
add_task(function testJavaAddonV1() {
  do_print("Loading Java Addon from: " + DEX_FILE);

  let javaAddon = yield JavaAddonManager.classInstanceFromFile(CLASS, DEX_FILE);
  do_check_neq(javaAddon, null);
  do_check_neq(javaAddon._guid, null);
  do_check_eq(javaAddon._classname, CLASS);
  do_check_eq(javaAddon._loaded, true);

  let messagePromise = Promise.defer();
  var count = 0;
  function listener(data) {
    do_print("Got request initiated from Java Addon: " + data + ", " + typeof(data) + ", " + JSON.stringify(data));
    count += 1;
    messagePromise.resolve(); // It's okay to resolve before returning: we'll wait on the verification promise no matter what.
    return {
      outputStringKey: "inputStringKey=" + data.inputStringKey,
      outputIntKey: data.inputIntKey - 1
    };
  }
  javaAddon.addListener(listener, "JavaAddon:V1:Request");

  let verifierPromise = Promise.defer();
  function verifier(data) {
    do_print("Got verification request initiated from Java Addon: " + data + ", " + typeof(data) + ", " + JSON.stringify(data));
    // These values are from the test Java Addon, after being processed by the :Request listener above.
    do_check_eq(data.outputStringKey, "inputStringKey=raw");
    do_check_eq(data.outputIntKey, 2);
    verifierPromise.resolve();
    return {};
  }
  javaAddon.addListener(verifier, "JavaAddon:V1:VerificationRequest");

  let message = {type: MESSAGE, inputStringKey: "test", inputIntKey: 5};
  do_print("Sending request to Java Addon: " + JSON.stringify(message));
  let output = yield javaAddon.sendRequestForResult(message);

  do_print("Got response from Java Addon: " + output + ", " + typeof(output) + ", " + JSON.stringify(output));
  do_check_eq(output.outputStringKey, "inputStringKey=test");
  do_check_eq(output.outputIntKey, 6);

  // We don't worry about timing out: the harness will (very much later)
  // kill us if we don't see the expected messages.

  do_print("Waiting for request initiated from Java Addon.");
  yield messagePromise.promise;
  do_check_eq(count, 1);

  do_print("Send request for result 2 for request initiated from Java Addon.");

  // The JavaAddon should have removed its listener, so we shouldn't get a response and count should stay the same.
  let gotError = yield javaAddon.sendRequestForResult(message)
    .then((result) => false)
    .catch((error) => true);
  do_check_eq(gotError, true);
  do_check_eq(count, 1);

  do_print("Waiting for verification request initiated from Java Addon.");
  yield verifierPromise.promise;
});

run_next_test();
