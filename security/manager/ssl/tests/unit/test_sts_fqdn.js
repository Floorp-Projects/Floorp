/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
"use strict";

function run_test() {
  let SSService = Cc["@mozilla.org/ssservice;1"].getService(
    Ci.nsISiteSecurityService
  );
  let uri = Services.io.newURI("https://example.com");
  let uri1 = Services.io.newURI("https://example.com.");
  let uri2 = Services.io.newURI("https://example.com..");
  ok(!SSService.isSecureURI(uri));
  ok(!SSService.isSecureURI(uri1));
  // These cases are only relevant as long as bug 1118522 hasn't been fixed.
  ok(!SSService.isSecureURI(uri2));

  let secInfo = Cc[
    "@mozilla.org/security/transportsecurityinfo;1"
  ].createInstance(Ci.nsITransportSecurityInfo);
  SSService.processHeader(uri, "max-age=1000;includeSubdomains", secInfo);
  ok(SSService.isSecureURI(uri));
  ok(SSService.isSecureURI(uri1));
  ok(SSService.isSecureURI(uri2));

  SSService.resetState(uri);
  ok(!SSService.isSecureURI(uri));
  ok(!SSService.isSecureURI(uri1));
  ok(!SSService.isSecureURI(uri2));

  // Somehow creating this malformed URI succeeds - we need to handle it
  // gracefully.
  uri = Services.io.newURI("https://../foo");
  equal(uri.host, "..");
  throws(
    () => {
      SSService.isSecureURI(uri);
    },
    /NS_ERROR_UNEXPECTED/,
    "Malformed URI should be rejected"
  );
}
