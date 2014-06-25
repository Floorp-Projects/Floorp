// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function sendMessageToJava(message) {
  SpecialPowers.Services.androidBridge.handleGeckoMessage(message);
}

function _evalURI(uri, sandbox) {
  // We explicitly allow Cross-Origin requests, since it is useful for
  // testing, but we allow relative URLs by maintaining our baseURI.
  let req = SpecialPowers.Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
                         .createInstance();

  let baseURI = SpecialPowers.Services.io
                             .newURI(window.document.baseURI, window.document.characterSet, null);
  let theURI = SpecialPowers.Services.io
                            .newURI(uri, window.document.characterSet, baseURI);

  req.open('GET', theURI.spec, false);
  req.send();

  return SpecialPowers.Cu.evalInSandbox(req.responseText, sandbox, "1.8", uri, 1);
}

/**
 * Execute the Javascript file at `uri` in a testing sandbox populated
 * with the Javascript test harness.
 *
 * `uri` should be a String, relative (to window.document.baseURI) or
 * absolute.
 *
 * The Javascript test harness sends all output to Java via
 * Robocop:JS messages.
 */
function testOneFile(uri) {
  let HEAD_JS = "robocop_head.js";

  // System principal.  This is dangerous, but this is test code that
  // should only run on developer and build farm machines, and the
  // test harness needs access to a lot of the Components API,
  // including Components.stack.  Wrapping Components.stack in
  // SpecialPowers magic obfuscates stack traces wonderfully,
  // defeating much of the point of the test harness.
  let principal = SpecialPowers.Cc["@mozilla.org/systemprincipal;1"]
                               .createInstance(SpecialPowers.Ci.nsIPrincipal);

  let testScope = SpecialPowers.Cu.Sandbox(principal);

  // Populate test environment with test harness prerequisites.
  testScope.Components = SpecialPowers.Components;
  testScope._TEST_FILE = uri;

  // Output from head.js is fed, line by line, to this function.  We
  // send any such output back to the Java Robocop harness.
  testScope.dump = function (str) {
    let message = { type: "Robocop:JS",
                    innerType: "progress",
                    message: str,
                  };
    sendMessageToJava(message);
  };

  // Populate test environment with test harness.  The symbols defined
  // above must be present before executing the test harness.
  _evalURI(HEAD_JS, testScope);

  return _evalURI(uri, testScope);
}
