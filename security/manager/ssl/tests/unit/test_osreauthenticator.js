/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/
"use strict";

// Tests nsIOSReauthenticator.asyncReauthenticateUser().
// Currently this always resolves to false on all platforms. As this gets implemented on various
// platforms, running this rest will result in a prompt from the OS. Consequently, we won't be able
// to run this in automation, but it will help in testing locally.
add_task(async function test_asyncReauthenticateUser() {
  const reauthenticator = Cc[
    "@mozilla.org/security/osreauthenticator;1"
  ].getService(Ci.nsIOSReauthenticator);
  ok(reauthenticator, "nsIOSReauthenticator should be available");
  ok(
    !(await reauthenticator.asyncReauthenticateUser(
      "this is the prompt string"
    )),
    "nsIOSReauthenticator.asyncReauthenticateUser always resolves to false for now"
  );
});
