/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/NetUtil.jsm");
var { setTimeout } = ChromeUtils.import("resource://gre/modules/Timer.jsm");

let h2Port;
let h3Port;

const certOverrideService = Cc[
  "@mozilla.org/security/certoverride;1"
].getService(Ci.nsICertOverrideService);

function setup() {
  let env = Cc["@mozilla.org/process/environment;1"].getService(
    Ci.nsIEnvironment
  );
  h2Port = env.get("MOZHTTP2_PORT");
  Assert.notEqual(h2Port, null);
  Assert.notEqual(h2Port, "");

  h3Port = env.get("MOZHTTP3_PORT");
  Assert.notEqual(h3Port, null);
  Assert.notEqual(h3Port, "");
  Services.prefs.setBoolPref("network.http.http3.enable", true);
}

setup();
registerCleanupFunction(async () => {
  Services.prefs.clearUserPref("network.dns.upgrade_with_https_rr");
  Services.prefs.clearUserPref("network.dns.use_https_rr_as_altsvc");
  Services.prefs.clearUserPref("network.dns.echconfig.enabled");
  Services.prefs.clearUserPref("network.dns.echconfig.fallback_to_origin");
  Services.prefs.clearUserPref("network.dns.httpssvc.reset_exclustion_list");
  Services.prefs.clearUserPref("network.http.http3.enable");
  Services.prefs.clearUserPref(
    "network.dns.httpssvc.http3_fast_fallback_timeout"
  );
  Services.prefs.clearUserPref(
    "network.http.http3.alt-svc-mapping-for-testing"
  );
  Services.prefs.clearUserPref("network.dns.localDomains");
  Services.prefs.clearUserPref("network.http.speculative-parallel-limit");
});

function makeChan(url) {
  let chan = NetUtil.newChannel({
    uri: url,
    loadUsingSystemPrincipal: true,
    contentPolicyType: Ci.nsIContentPolicy.TYPE_DOCUMENT,
  }).QueryInterface(Ci.nsIHttpChannel);
  chan.loadFlags = Ci.nsIChannel.LOAD_INITIAL_DOCUMENT_URI;
  return chan;
}

function channelOpenPromise(chan, flags) {
  return new Promise(resolve => {
    function finish(req, buffer) {
      resolve([req, buffer]);
      certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
        false
      );
    }
    certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
      true
    );
    chan.asyncOpen(new ChannelListener(finish, null, flags));
  });
}

async function H3CoalescingTest(host1, host2) {
  Services.prefs.setCharPref(
    "network.http.http3.alt-svc-mapping-for-testing",
    `${host1};h3-29=:${h3Port}`
  );
  Services.prefs.setCharPref("network.dns.localDomains", host1);

  let chan = makeChan(`https://${host1}`);
  let [req] = await channelOpenPromise(chan, CL_ALLOW_UNKNOWN_CL);
  req.QueryInterface(Ci.nsIHttpChannel);
  Assert.equal(req.protocolVersion, "h3-29");
  let hash = req.getResponseHeader("x-http3-conn-hash");

  Services.prefs.setCharPref(
    "network.http.http3.alt-svc-mapping-for-testing",
    `${host2};h3-29=:${h3Port}`
  );
  Services.prefs.setCharPref("network.dns.localDomains", host2);

  chan = makeChan(`https://${host2}`);
  [req] = await channelOpenPromise(chan, CL_ALLOW_UNKNOWN_CL);
  req.QueryInterface(Ci.nsIHttpChannel);
  Assert.equal(req.protocolVersion, "h3-29");
  // The port used by the second connection should be the same as the first one.
  Assert.equal(req.getResponseHeader("x-http3-conn-hash"), hash);
}

add_task(async function testH3CoalescingWithSpeculativeConnection() {
  await http3_setup_tests("h3-29");
  Services.prefs.setIntPref("network.http.speculative-parallel-limit", 6);
  await H3CoalescingTest("foo.h3_coalescing.org", "bar.h3_coalescing.org");
});

add_task(async function testH3CoalescingWithoutSpeculativeConnection() {
  Services.prefs.setIntPref("network.http.speculative-parallel-limit", 0);
  Services.obs.notifyObservers(null, "net:cancel-all-connections");
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(resolve => setTimeout(resolve, 1000));
  await H3CoalescingTest("baz.h3_coalescing.org", "qux.h3_coalescing.org");
});
