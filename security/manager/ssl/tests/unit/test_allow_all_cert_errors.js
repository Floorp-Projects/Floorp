/* -*- tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

function run_test() {
  do_get_profile();
  let certOverrideService = Cc[
    "@mozilla.org/security/certoverride;1"
  ].getService(Ci.nsICertOverrideService);
  certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
    true
  );

  add_tls_server_setup("BadCertAndPinningServer", "bad_certs");
  add_connection_test("expired.example.com", PRErrorCodeSuccess);
  add_test(function () {
    certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
      false
    );
    run_next_test();
  });
  run_next_test();
}
