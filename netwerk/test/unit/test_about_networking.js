/* -*- Mode: Javasript; indent-tab-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;

let gDashboard = Cc['@mozilla.org/network/dashboard;1']
  .getService(Ci.nsIDashboard);

const RESOLVE_DISABLE_IPV6 = (1 << 5);

add_test(function test_http() {
  gDashboard.requestHttpConnections(function(data) {
    do_check_neq(data.host.indexOf("example.com"), -1);

    run_next_test();
  });
});

add_test(function test_dns() {
  gDashboard.requestDNSInfo(function(data) {
    do_check_neq(data.hostname.indexOf("example.com"), -1);

    run_next_test();
  });
});

add_test(function test_sockets() {
  let dns = Cc["@mozilla.org/network/dns-service;1"].getService(Ci.nsIDNSService);
  let record = dns.resolve("example.com", RESOLVE_DISABLE_IPV6);
  let answer = record.getNextAddrAsString();

  gDashboard.requestSockets(function(data) {
    do_check_neq(data.host.indexOf(answer), -1);

    run_next_test();
  });
});

function run_test() {
  let ioService = Cc["@mozilla.org/network/io-service;1"]
    .getService(Ci.nsIIOService);
  let uri = ioService.newURI("http://example.com", null, null);
  let channel = ioService.newChannelFromURI(uri);

  channel.open();

  run_next_test();
}

