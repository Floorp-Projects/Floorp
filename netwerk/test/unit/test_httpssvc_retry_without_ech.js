/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/NetUtil.jsm");

let trrServer;

const certOverrideService = Cc[
  "@mozilla.org/security/certoverride;1"
].getService(Ci.nsICertOverrideService);

function setup() {
  trr_test_setup();

  Services.prefs.setIntPref("network.trr.mode", Ci.nsIDNSService.MODE_TRRFIRST);
  Services.prefs.setBoolPref("network.dns.upgrade_with_https_rr", true);
  Services.prefs.setBoolPref("network.dns.use_https_rr_as_altsvc", true);
  Services.prefs.setBoolPref("network.dns.echconfig.enabled", true);

  // An arbitrary, non-ECH server.
  add_tls_server_setup(
    "DelegatedCredentialsServer",
    "../../../security/manager/ssl/tests/unit/test_delegated_credentials"
  );

  let nssComponent = Cc["@mozilla.org/psm;1"].getService(Ci.nsINSSComponent);
  nssComponent.clearSSLExternalAndInternalSessionCache();
}

setup();
registerCleanupFunction(async () => {
  trr_clear_prefs();
  Services.prefs.clearUserPref("network.dns.upgrade_with_https_rr");
  Services.prefs.clearUserPref("network.dns.use_https_rr_as_altsvc");
  Services.prefs.clearUserPref("network.dns.echconfig.enabled");
  Services.prefs.clearUserPref("network.dns.echconfig.fallback_to_origin");
  if (trrServer) {
    await trrServer.stop();
  }
});

function makeChan(url) {
  let chan = NetUtil.newChannel({
    uri: url,
    loadUsingSystemPrincipal: true,
    contentPolicyType: Ci.nsIContentPolicy.TYPE_DOCUMENT,
  }).QueryInterface(Ci.nsIHttpChannel);
  return chan;
}

function channelOpenPromise(chan, flags) {
  return new Promise(resolve => {
    function finish(req, buffer) {
      certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
        false
      );
      resolve([req, buffer]);
    }
    certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
      false
    );
    let internal = chan.QueryInterface(Ci.nsIHttpChannelInternal);
    internal.setWaitForHTTPSSVCRecord();
    chan.asyncOpen(new ChannelListener(finish, null, flags));
  });
}

add_task(async function testRetryWithoutECH() {
  const ECH_CONFIG_FIXED =
    "AEn+DQBFTQAgACCKB1Y5SfrGIyk27W82xPpzWTDs3q72c04xSurDWlb9CgAEAAEAA2QWZWNoLXB1YmxpYy5leGFtcGxlLmNvbQAA";
  trrServer = new TRRServer();
  await trrServer.start();

  Services.prefs.setIntPref("network.trr.mode", 3);
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${trrServer.port}/dns-query`
  );

  // Only the last record is valid to use.
  await trrServer.registerDoHAnswers(
    "delegated-disabled.example.com",
    "HTTPS",
    {
      answers: [
        {
          name: "delegated-disabled.example.com",
          ttl: 55,
          type: "HTTPS",
          flush: false,
          data: {
            priority: 1,
            name: "delegated-disabled.example.com",
            values: [
              {
                key: "echconfig",
                value: ECH_CONFIG_FIXED,
                needBase64Decode: true,
              },
            ],
          },
        },
      ],
    }
  );

  await trrServer.registerDoHAnswers("delegated-disabled.example.com", "A", {
    answers: [
      {
        name: "delegated-disabled.example.com",
        ttl: 55,
        type: "A",
        flush: false,
        data: "127.0.0.1",
      },
    ],
  });

  await new TRRDNSListener("delegated-disabled.example.com", {
    type: Ci.nsIDNSService.RESOLVE_TYPE_HTTPSSVC,
  });

  let chan = makeChan(`https://delegated-disabled.example.com:8443`);
  await channelOpenPromise(chan, CL_ALLOW_UNKNOWN_CL);
  let securityInfo = chan.securityInfo.QueryInterface(
    Ci.nsITransportSecurityInfo
  );

  Assert.ok(
    !securityInfo.isAcceptedEch,
    "This host should not have accepted ECH"
  );
  await trrServer.stop();
});
