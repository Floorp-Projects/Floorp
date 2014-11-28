/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://gre/modules/MobileIdentityVerificationFlow.jsm");

function verifyStrategy() {
  return Promise.resolve();
}

function cleanupStrategy() {
}

function run_test() {
  do_print("= Bug 1101444: Invalid verification code shouldn't restart " +
           "verification flow =");

  let client = new MockClient({
    // This will emulate two invalid attempts. The third time it will work.
    verifyCodeError: ["INVALID", "INVALID"]
  });
  let ui = new MockUi();

  let verificationFlow = new MobileIdentityVerificationFlow({
    external: true,
    sessionToken: SESSION_TOKEN,
    msisdn: PHONE_NUMBER
  }, ui, client, verifyStrategy, cleanupStrategy);

  verificationFlow.doVerification().then(() => {
    // We should only do the registration process once. We only try registering
    // again when the timeout fires, but not when we enter an invalid
    // verification code.
    client._("register").callsLength(1);
    client._("verifyCode").callsLength(3);
    // Because we do two invalid attempts, we should show the invalid code error twice.
    ui._("error").callsLength(2);
  });

  do_test_finished();
};
