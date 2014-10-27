// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");

function ok(passed, text) {
  do_report_result(passed, text, Components.stack.caller, false);
}

add_test(function check_linktype() {
  let network = Cc["@mozilla.org/network/network-link-service;1"].getService(Ci.nsINetworkLinkService);
  do_print("LinkUp = " + network.isLinkUp);
  do_print("LinkStatus = " + network.linkStatusKnown);
  do_print("Linktype = " + network.linkType);
  ok(network.linkType != Ci.nsINetworkLinkService.LINK_TYPE_UNKNOWN, "LinkType is not UNKNOWN");

  run_next_test();
});

run_next_test();
