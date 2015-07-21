// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");

add_test(function check_linktype() {
  // Let's exercise the interface. Even if the network is not up, we can make sure nothing blows up.
  let network = Cc["@mozilla.org/network/network-link-service;1"].getService(Ci.nsINetworkLinkService);
  if (network.isLinkUp) {
    ok(network.linkType != Ci.nsINetworkLinkService.LINK_TYPE_UNKNOWN, "LinkType is not UNKNOWN");
  } else {
    ok(network.linkType == Ci.nsINetworkLinkService.LINK_TYPE_UNKNOWN, "LinkType is UNKNOWN");
  }

  run_next_test();
});

run_next_test();
