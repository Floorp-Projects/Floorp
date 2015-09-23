// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// While the Netscape certificate type extension is not a standard and has been
// discouraged from use for quite some time, it is still encountered. Thus, we
// handle it slightly differently from other unknown extensions.
// If it is not marked critical, we ignore it.
// If it is marked critical:
//   If the basic constraints and extended key usage extensions are also
//   present, we ignore it, because they are standardized and should convey the
//   same information.
//   Otherwise, we reject it with an error indicating an unknown critical
//   extension.

"use strict";

function run_test() {
  do_get_profile();
  add_tls_server_setup("BadCertServer", "bad_certs");
  add_connection_test("nsCertTypeNotCritical.example.com", PRErrorCodeSuccess);
  add_connection_test("nsCertTypeCriticalWithExtKeyUsage.example.com",
                      PRErrorCodeSuccess);
  add_connection_test("nsCertTypeCritical.example.com",
                      SEC_ERROR_UNKNOWN_CRITICAL_EXTENSION);
  run_next_test();
}
