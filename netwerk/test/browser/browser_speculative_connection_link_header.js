/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Services.prefs.setBoolPref("network.http.debug-observations", true);

registerCleanupFunction(function () {
  Services.prefs.clearUserPref("network.http.debug-observations");
});

// Test steps:
// 1. Load file_link_header.sjs
// 2.`<link rel="preconnect" href="https://localhost">` is in
//    file_link_header.sjs, so we will create a speculative connection.
// 3. We use "speculative-connect-request" topic to observe whether the
//    speculative connection is attempted.
// 4. Finally, we check if the observed host and partition key are the same and
//    as the expected.
add_task(async function test_link_preconnect() {
  let requestUrl = `https://example.com/browser/netwerk/test/browser/file_link_header.sjs`;

  let observed = "";
  let observer = {
    QueryInterface: ChromeUtils.generateQI(["nsIObserver"]),
    observe(aSubject, aTopic, aData) {
      if (aTopic == "speculative-connect-request") {
        Services.obs.removeObserver(observer, "speculative-connect-request");
        observed = aData;
      }
    },
  };
  Services.obs.addObserver(observer, "speculative-connect-request");

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: requestUrl,
      waitForLoad: true,
    },
    async function () {}
  );

  // The hash key should be like:
  // ".S........[tlsflags0x00000000]localhost:443^partitionKey=%28https%2Cexample.com%29"

  // Extracting "localhost:443"
  let hostPortRegex = /\[.*\](.*?)\^/;
  let hostPortMatch = hostPortRegex.exec(observed);
  let hostPort = hostPortMatch ? hostPortMatch[1] : "";
  // Extracting "%28https%2Cexample.com%29"
  let partitionKeyRegex = /\^partitionKey=(.*)$/;
  let partitionKeyMatch = partitionKeyRegex.exec(observed);
  let partitionKey = partitionKeyMatch ? partitionKeyMatch[1] : "";

  Assert.equal(hostPort, "localhost:443");
  Assert.equal(partitionKey, "%28https%2Cexample.com%29");
});
