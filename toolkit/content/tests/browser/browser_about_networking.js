/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_first() {
  registerCleanupFunction(() => {
    // Must clear mode first, otherwise we'll have non-local connections to
    // the cloudflare URL.
    Services.prefs.clearUserPref("network.trr.mode");
    Services.prefs.clearUserPref("network.trr.uri");
  });

  await BrowserTestUtils.withNewTab(
    "about:networking#dns",
    async function (browser) {
      ok(!browser.isRemoteBrowser, "Browser should not be remote.");
      await ContentTask.spawn(browser, null, async function () {
        let url_tbody = content.document.getElementById("dns_trr_url");
        info(url_tbody);
        is(
          url_tbody.children[0].children[0].textContent,
          "https://mozilla.cloudflare-dns.com/dns-query"
        );
        is(url_tbody.children[0].children[1].textContent, "0");
      });
    }
  );

  Services.prefs.setCharPref("network.trr.uri", "https://localhost/testytest");
  Services.prefs.setIntPref("network.trr.mode", 2);
  await BrowserTestUtils.withNewTab(
    "about:networking#dns",
    async function (browser) {
      ok(!browser.isRemoteBrowser, "Browser should not be remote.");
      await ContentTask.spawn(browser, null, async function () {
        let url_tbody = content.document.getElementById("dns_trr_url");
        info(url_tbody);
        is(
          url_tbody.children[0].children[0].textContent,
          "https://localhost/testytest"
        );
        is(url_tbody.children[0].children[1].textContent, "2");
      });
    }
  );
});
