/* -*- tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// This test ensures that nsICertOverrideService can be implemented in JS.
// It does so by creating and registering a mock implementation that indicates
// a specific host ("expired.example.com") has a matching override (ERROR_TIME).
// Connections to that host should succeed.

// Mock implementation of nsICertOverrideService
const gCertOverrideService = {
  rememberValidityOverride() {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  rememberTemporaryValidityOverrideUsingFingerprint() {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  hasMatchingOverride(hostname, port, cert, overrideBits, isTemporary) {
    Assert.equal(hostname, "expired.example.com",
                 "hasMatchingOverride: hostname should be expired.example.com");
    overrideBits.value = Ci.nsICertOverrideService.ERROR_TIME;
    isTemporary.value = false;
    return true;
  },

  getValidityOverride() {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  clearValidityOverride() {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  isCertUsedForOverrides() {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsICertOverrideService])
};

function run_test() {
  do_get_profile();
  let certOverrideServiceCID =
    MockRegistrar.register("@mozilla.org/security/certoverride;1",
                           gCertOverrideService);
  do_register_cleanup(() => {
    MockRegistrar.unregister(certOverrideServiceCID);
  });
  add_tls_server_setup("BadCertServer", "bad_certs");
  add_connection_test("expired.example.com", PRErrorCodeSuccess);
  run_next_test();
}
