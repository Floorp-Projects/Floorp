// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

"use strict";

// In which we connect to a host and encounter OCSP responses with the
// Cache-Control header set, which normally Necko would cache. This test
// ensures that these responses aren't cached. PSM has its own OCSP cache, so
// Necko shouldn't also be caching them.

do_get_profile(); // must be called before getting nsIX509CertDB

const SERVER_PORT = 8888;

function add_flush_cache() {
  add_test(() => {
    // This appears to either fire multiple times or fire once for every
    // observer that has ever been passed to flush. To prevent multiple calls to
    // run_next_test, keep track of if this observer has already called it.
    let observed = false;
    let observer = {
      observe: () => {
        if (!observed) {
          observed = true;
          run_next_test();
        }
      },
    };
    Services.cache2.QueryInterface(Ci.nsICacheTesting).flush(observer);
  });
}

function add_ocsp_necko_cache_test(loadContext) {
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
    responder = startOCSPResponder(
      SERVER_PORT,
      "localhost",
      "ocsp_certs",
      ["default-ee"],
      [],
      [],
      [],
      [["Cache-Control", "max-age=1000"]]
    );
    run_next_test();
  });

  // Prepare a connection that will cause an OCSP request.
  add_connection_test(
    "ocsp-stapling-none.example.com",
    PRErrorCodeSuccess,
    null,
    null,
    null,
    loadContext.originAttributes
  );

  add_flush_cache();

  // Traverse the cache and ensure the response was not cached.
  add_test(() => {
    let foundEntry = false;
    let visitor = {
      onCacheStorageInfo() {},
      onCacheEntryInfo(
        aURI,
        aIdEnhance,
        aDataSize,
        aFetchCount,
        aLastModifiedTime,
        aExpirationTime,
        aPinned,
        aInfo
      ) {
        Assert.equal(
          aURI.spec,
          "http://localhost:8888/",
          "expected OCSP request URI should match"
        );
        foundEntry = true;
      },
      onCacheEntryVisitCompleted() {
        Assert.ok(!foundEntry, "should not find a cached entry");
        run_next_test();
      },
      QueryInterface: ChromeUtils.generateQI(["nsICacheStorageVisitor"]),
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
  add_ocsp_necko_cache_test(Services.loadContextInfo.private);
  add_ocsp_necko_cache_test(Services.loadContextInfo.default);
  run_next_test();
}
