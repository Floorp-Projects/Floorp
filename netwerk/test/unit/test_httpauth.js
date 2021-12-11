/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure the HTTP authenticated sessions are correctly cleared
// when entering and leaving the private browsing mode.

"use strict";

function run_test() {
  var am = Cc["@mozilla.org/network/http-auth-manager;1"].getService(
    Ci.nsIHttpAuthManager
  );

  const kHost1 = "pbtest3.example.com";
  const kHost2 = "pbtest4.example.com";
  const kPort = 80;
  const kHTTP = "http";
  const kBasic = "basic";
  const kRealm = "realm";
  const kDomain = "example.com";
  const kUser = "user";
  const kUser2 = "user2";
  const kPassword = "pass";
  const kPassword2 = "pass2";
  const kEmpty = "";

  const PRIVATE = true;
  const NOT_PRIVATE = false;

  try {
    var domain = { value: kEmpty },
      user = { value: kEmpty },
      pass = { value: kEmpty };
    // simulate a login via HTTP auth outside of the private mode
    am.setAuthIdentity(
      kHTTP,
      kHost1,
      kPort,
      kBasic,
      kRealm,
      kEmpty,
      kDomain,
      kUser,
      kPassword
    );
    // make sure the recently added auth entry is available outside the private browsing mode
    am.getAuthIdentity(
      kHTTP,
      kHost1,
      kPort,
      kBasic,
      kRealm,
      kEmpty,
      domain,
      user,
      pass,
      NOT_PRIVATE
    );
    Assert.equal(domain.value, kDomain);
    Assert.equal(user.value, kUser);
    Assert.equal(pass.value, kPassword);

    // make sure the added auth entry is no longer accessible in private
    domain = { value: kEmpty };
    user = { value: kEmpty };
    pass = { value: kEmpty };
    try {
      // should throw
      am.getAuthIdentity(
        kHTTP,
        kHost1,
        kPort,
        kBasic,
        kRealm,
        kEmpty,
        domain,
        user,
        pass,
        PRIVATE
      );
      do_throw(
        "Auth entry should not be retrievable after entering the private browsing mode"
      );
    } catch (e) {
      Assert.equal(domain.value, kEmpty);
      Assert.equal(user.value, kEmpty);
      Assert.equal(pass.value, kEmpty);
    }

    // simulate a login via HTTP auth inside of the private mode
    am.setAuthIdentity(
      kHTTP,
      kHost2,
      kPort,
      kBasic,
      kRealm,
      kEmpty,
      kDomain,
      kUser2,
      kPassword2,
      PRIVATE
    );
    // make sure the recently added auth entry is available inside the private browsing mode
    domain = { value: kEmpty };
    user = { value: kEmpty };
    pass = { value: kEmpty };
    am.getAuthIdentity(
      kHTTP,
      kHost2,
      kPort,
      kBasic,
      kRealm,
      kEmpty,
      domain,
      user,
      pass,
      PRIVATE
    );
    Assert.equal(domain.value, kDomain);
    Assert.equal(user.value, kUser2);
    Assert.equal(pass.value, kPassword2);

    try {
      // make sure the recently added auth entry is not available outside the private browsing mode
      domain = { value: kEmpty };
      user = { value: kEmpty };
      pass = { value: kEmpty };
      am.getAuthIdentity(
        kHTTP,
        kHost2,
        kPort,
        kBasic,
        kRealm,
        kEmpty,
        domain,
        user,
        pass,
        NOT_PRIVATE
      );
      do_throw(
        "Auth entry should not be retrievable outside of private browsing mode"
      );
    } catch (x) {
      Assert.equal(domain.value, kEmpty);
      Assert.equal(user.value, kEmpty);
      Assert.equal(pass.value, kEmpty);
    }

    // simulate leaving private browsing mode
    Services.obs.notifyObservers(null, "last-pb-context-exited");

    // make sure the added auth entry is no longer accessible in any privacy state
    domain = { value: kEmpty };
    user = { value: kEmpty };
    pass = { value: kEmpty };
    try {
      // should throw (not available in public mode)
      am.getAuthIdentity(
        kHTTP,
        kHost2,
        kPort,
        kBasic,
        kRealm,
        kEmpty,
        domain,
        user,
        pass,
        NOT_PRIVATE
      );
      do_throw(
        "Auth entry should not be retrievable after exiting the private browsing mode"
      );
    } catch (e) {
      Assert.equal(domain.value, kEmpty);
      Assert.equal(user.value, kEmpty);
      Assert.equal(pass.value, kEmpty);
    }
    try {
      // should throw (no longer available in private mode)
      am.getAuthIdentity(
        kHTTP,
        kHost2,
        kPort,
        kBasic,
        kRealm,
        kEmpty,
        domain,
        user,
        pass,
        PRIVATE
      );
      do_throw(
        "Auth entry should not be retrievable in private mode after exiting the private browsing mode"
      );
    } catch (x) {
      Assert.equal(domain.value, kEmpty);
      Assert.equal(user.value, kEmpty);
      Assert.equal(pass.value, kEmpty);
    }
  } catch (e) {
    do_throw("Unexpected exception while testing HTTP auth manager: " + e);
  }
}
