// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

"use strict";

// Tests the dynamic preloading of HPKP hosts:
// * checks that preloads can be set
// * checks that includeSubdomains is honored
// * checks that clearing preloads works correctly
// * checks that clearing a host's HSTS state via a header correctly
//   overrides dynamic preload entries

function run_test() {
  let SSService = Cc["@mozilla.org/ssservice;1"]
                    .getService(Ci.nsISiteSecurityService);
  let sslStatus = new FakeSSLStatus();

  // first check that a host probably not on the preload list is not identified
  // as an sts host
  let unlikelyHost = "highlyunlikely.example.com";
  ok(!SSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS,
                             unlikelyHost, 0));

  // now add a preload entry for this host
  SSService.setHSTSPreload(unlikelyHost, false, Date.now() + 60000);

  // check that it's now an STS host
  ok(SSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS,
                            unlikelyHost, 0));

  // check that it's honoring the fact we set includeSubdomains to false
  ok(!SSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS,
                             "subdomain." + unlikelyHost, 0));

  // clear the non-preloaded entries
  SSService.clearAll();

  // check that it's still an STS host
  ok(SSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS,
                            unlikelyHost, 0));

  // clear the preloads
  SSService.clearPreloads();

  // Check that it's no longer an STS host now that the preloads have been
  // cleared
  ok(!SSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS,
                             unlikelyHost, 0));

  // Now let's do the same, this time with includeSubdomains on
  SSService.setHSTSPreload(unlikelyHost, true, Date.now() + 60000);

  // check that it's now an STS host
  ok(SSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS,
                            unlikelyHost, 0));

  // check that it's now including subdomains
  ok(SSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS,
                            "subdomain." + unlikelyHost, 0));

  // Now let's simulate overriding the entry by setting an entry from a header
  // with max-age set to 0
  let uri = Services.io.newURI("https://" + unlikelyHost);
  SSService.processHeader(Ci.nsISiteSecurityService.HEADER_HSTS, uri,
                          "max-age=0", sslStatus, 0);

  // this should no longer be an HSTS host
  ok(!SSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS,
                             unlikelyHost, 0));
}
