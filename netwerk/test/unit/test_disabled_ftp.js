"use strict";

registerCleanupFunction(function() {
  Services.prefs.clearUserPref("network.ftp.enabled");
});

add_task(async function ftp_disabled() {
  Services.prefs.setBoolPref("network.ftp.enabled", false);
  Services.prefs.setBoolPref("network.protocol-handler.external.ftp", false);

  let chan = null;
  Assert.throws(
    () => {
      chan = NetUtil.newChannel({
        uri: "ftp://ftp.de.debian.org/",
        loadUsingSystemPrincipal: true,
      }).QueryInterface(Ci.nsIHttpChannel);
    },
    /NS_ERROR_UNKNOWN_PROTOCOL/,
    "creating the FTP channel must throw"
  );
});
