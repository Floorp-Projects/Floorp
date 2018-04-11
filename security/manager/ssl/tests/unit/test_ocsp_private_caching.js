// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

"use strict";

// In which we connect to a host and encounter OCSP responses with the
// Cache-Control header set, which Necko will normally cache. We need to ensure
// that these responses aren't cached to disk when the original https request
// was in a private context.

do_get_profile(); // must be called before getting nsIX509CertDB
const certdb = Cc["@mozilla.org/security/x509certdb;1"]
                 .getService(Ci.nsIX509CertDB);

const SERVER_PORT = 8888;

function start_ocsp_responder(expectedCertNames, expectedPaths,
                              expectedMethods) {
  return startOCSPResponder(SERVER_PORT, "www.example.com",
                            "test_ocsp_fetch_method", expectedCertNames,
                            expectedPaths, expectedMethods);
}

function add_flush_cache() {
  add_test(() => {
    // This appears to either fire multiple times or fire once for every
    // observer that has ever been passed to flush. To prevent multiple calls to
    // run_next_test, keep track of if this observer has already called it.
    let observed = false;
    let observer = { observe: () => {
        if (!observed) {
          observed = true;
          run_next_test();
        }
      }
    };
    Services.cache2.QueryInterface(Ci.nsICacheTesting).flush(observer);
  });
}

function add_ocsp_necko_cache_test(loadContext, shouldFindEntry) {
  // Pre-testcase cleanup/setup.
  add_test(() => {
    Services.cache2.clear();
    run_next_test();
  });
  add_flush_cache();

  let responder;
  add_test(() => {
    clearOCSPCache();
    clearSessionCache();
    responder = startOCSPResponder(SERVER_PORT, "localhost", "ocsp_certs",
                                   ["default-ee"], [], [], [],
                                   [["Cache-Control", "max-age: 1000"]]);
    run_next_test();
  });

  // Prepare a connection that will cause an OCSP request.
  add_connection_test("ocsp-stapling-none.example.com", PRErrorCodeSuccess,
                      null, null, null, loadContext.originAttributes);

  add_flush_cache();

  // Traverse the cache and ensure the response made it into the cache with the
  // appropriate properties (private or not private).
  add_test(() => {
    let foundEntry = false;
    let visitor = {
      onCacheStorageInfo() {},
      onCacheEntryInfo(aURI, aIdEnhance, aDataSize, aFetchCount,
                       aLastModifiedTime, aExpirationTime, aPinned, aInfo) {
        Assert.equal(aURI.spec, "http://localhost:8888/",
                     "expected OCSP request URI should match");
        foundEntry = true;
      },
      onCacheEntryVisitCompleted() {
        Assert.equal(foundEntry, shouldFindEntry,
                     "should only find a cached entry if we're expecting one");
        run_next_test();
      },
      QueryInterface(iid) {
        if (iid.equals(Ci.nsICacheStorageVisitor)) {
          return this;
        }
        throw Cr.NS_ERROR_NO_INTERFACE;
      },
    };
    Services.cache2.asyncVisitAllStorages(visitor, true);
  });

  // Clean up (stop the responder).
  add_test(() => {
    responder.stop(run_next_test);
  });
}

function run_test() {
  Services.prefs.setIntPref("security.OCSP.enabled", 1);
  add_tls_server_setup("OCSPStaplingServer", "ocsp_certs");
  add_ocsp_necko_cache_test(Services.loadContextInfo.private, false);
  add_ocsp_necko_cache_test(Services.loadContextInfo.default, true);
  run_next_test();
}
