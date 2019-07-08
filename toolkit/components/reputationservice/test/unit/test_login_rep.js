/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { NetUtil } = ChromeUtils.import("resource://gre/modules/NetUtil.jsm");

// LoginReputationService
const gLoginRep = Cc[
  "@mozilla.org/reputationservice/login-reputation-service;1"
].getService(Ci.nsILoginReputationService);

let gListManager = Cc["@mozilla.org/url-classifier/listmanager;1"].getService(
  Ci.nsIUrlListManager
);

var gHttpServ = null;

const whitelistedURI = "http://foo:bar@whitelisted.com/index.htm#junk";
const exampleURI = "http://user:password@example.com/i.html?foo=bar";

const LOCAL_WHITELIST_DATA = {
  tableName: "test-passwordwhite-proto",
  providerName: "test",
  updateUrl: "http://localhost:5555/safebrowsing/update?",
  gethashUrl: "",
};

add_task(async function test_setup() {
  // Enable login reputation service
  Services.prefs.setBoolPref("browser.safebrowsing.passwords.enabled", true);
  gLoginRep.init();

  // Setup local whitelist table.
  Services.prefs.setCharPref(
    "urlclassifier.passwordAllowTable",
    "test-passwordwhite-proto"
  );
  gListManager.registerTable(
    LOCAL_WHITELIST_DATA.tableName,
    LOCAL_WHITELIST_DATA.providerName,
    LOCAL_WHITELIST_DATA.updateUrl,
    LOCAL_WHITELIST_DATA.gethashUrl
  );

  registerCleanupFunction(function() {
    gListManager.unregisterTable(LOCAL_WHITELIST_DATA.tableName);

    Services.prefs.clearUserPref("browser.safebrowsing.passwords.enabled");
    Services.prefs.clearUserPref("urlclassifier.passwordAllowTable");
  });
});

add_test(function test_setup_local_whitelist() {
  // Setup the http server for SafeBrowsing update, so we can save SAFE entries
  // to SafeBrowsing database while update.
  gHttpServ = new HttpServer();
  gHttpServ.registerDirectory("/", do_get_cwd());
  gHttpServ.registerPathHandler("/safebrowsing/update", function(
    request,
    response
  ) {
    response.setHeader(
      "Content-Type",
      "application/vnd.google.safebrowsing-update",
      false
    );

    response.setStatusLine(request.httpVersion, 200, "OK");

    // The protobuf binary represention of response:
    //
    // [
    //   {
    //     'threat_type': 8, // CSD_WHITELIST
    //     'response_type': 2, // FULL_UPDATE
    //     'new_client_state': 'sta\x00te', // NEW_CLIENT_STATE
    //     'checksum': { "sha256": CHECKSUM }, // CHECKSUM
    //     'additions': { 'compression_type': RAW,
    //                    'prefix_size': 32,
    //                    'raw_hashes': "whitelisted.com/index.htm"}
    //   }
    // ]
    //
    let content =
      "\x0A\x36\x08\x08\x20\x02\x2A\x28\x08\x01\x12\x24\x08" +
      "\x20\x12\x20\x0F\xE4\x66\xBB\xDD\x34\xAB\x1E\xF7\x8F" +
      "\xDD\x9D\x8C\xF8\x9F\x4E\x42\x97\x92\x86\x02\x03\xE0" +
      "\xE9\x60\xBD\xD6\x3A\x85\xCD\x08\xD0\x3A\x06\x73\x74" +
      "\x61\x00\x74\x65\x12\x04\x08\x0C\x10\x0A";

    response.bodyOutputStream.write(content, content.length);
  });

  gHttpServ.start(5555);

  // Trigger the update once http server is ready.
  Services.obs.addObserver(function(aSubject, aTopic, aData) {
    if (aData.startsWith("success")) {
      run_next_test();
    } else {
      do_throw("update fail");
    }
  }, "safebrowsing-update-finished");
  gListManager.forceUpdates(LOCAL_WHITELIST_DATA.tableName);

  registerCleanupFunction(function() {
    return (async function() {
      await new Promise(resolve => {
        gHttpServ.stop(resolve);
      });
    })();
  });
});

add_test(function test_disable() {
  Services.prefs.setBoolPref("browser.safebrowsing.passwords.enabled", false);

  gLoginRep.queryReputation(
    {
      formURI: NetUtil.newURI("http://example.com"),
    },
    {
      onComplete(aStatus, aVerdict) {
        Assert.equal(aStatus, Cr.NS_ERROR_ABORT);
        Assert.equal(aVerdict, Ci.nsILoginReputationVerdictType.UNSPECIFIED);

        Services.prefs.setBoolPref(
          "browser.safebrowsing.passwords.enabled",
          true
        );

        run_next_test();
      },
    }
  );
});

add_test(function test_nullQuery() {
  try {
    gLoginRep.queryReputation(null, {
      onComplete(aStatus, aVerdict) {},
    });
    do_throw("Query parameter cannot be null");
  } catch (ex) {
    Assert.equal(ex.result, Cr.NS_ERROR_INVALID_POINTER);

    run_next_test();
  }
});

add_test(function test_local_whitelist() {
  gLoginRep.queryReputation(
    {
      formURI: NetUtil.newURI(whitelistedURI),
    },
    {
      onComplete(aStatus, aVerdict) {
        Assert.equal(aStatus, Cr.NS_OK);
        Assert.equal(aVerdict, Ci.nsILoginReputationVerdictType.SAFE);

        run_next_test();
      },
    }
  );
});

add_test(function test_notin_local_whitelist() {
  gLoginRep.queryReputation(
    {
      formURI: NetUtil.newURI(exampleURI),
    },
    {
      onComplete(aStatus, aVerdict) {
        Assert.equal(aStatus, Cr.NS_OK);
        Assert.equal(aVerdict, Ci.nsILoginReputationVerdictType.UNSPECIFIED);

        run_next_test();
      },
    }
  );
});
