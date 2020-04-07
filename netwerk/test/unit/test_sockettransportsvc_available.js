"use strict";

function run_test() {
  try {
    var sts = Cc["@mozilla.org/network/socket-transport-service;1"].getService(
      Ci.nsISocketTransportService
    );
  } catch (e) {}

  Assert.ok(!!sts);
}
