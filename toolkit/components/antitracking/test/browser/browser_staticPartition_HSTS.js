/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var unsecureEmptyURL =
  "http://example.org/browser/toolkit/components/antitracking/test/browser/empty.html";
var secureURL =
  "https://example.com/browser/toolkit/components/antitracking/test/browser/browser_staticPartition_HSTS.sjs";
var unsecureURL =
  "http://example.com/browser/toolkit/components/antitracking/test/browser/browser_staticPartition_HSTS.sjs";

function cleanupHSTS(aPartitionEnabled, aUseSite) {
  // Ensure to remove example.com from the HSTS list.
  let sss = Cc["@mozilla.org/ssservice;1"].getService(
    Ci.nsISiteSecurityService
  );

  let originAttributes = {};

  if (aPartitionEnabled) {
    if (aUseSite) {
      originAttributes = { partitionKey: "(http,example.com)" };
    } else {
      originAttributes = { partitionKey: "example.com" };
    }
  }

  sss.resetState(
    Ci.nsISiteSecurityService.HEADER_HSTS,
    NetUtil.newURI("http://example.com/"),
    0,
    originAttributes
  );
}

function promiseTabLoadEvent(aTab, aURL, aFinalURL) {
  info("Wait for load tab event");
  BrowserTestUtils.loadURI(aTab.linkedBrowser, aURL);
  return BrowserTestUtils.browserLoaded(aTab.linkedBrowser, false, aFinalURL);
}

function waitFor(host) {
  return new Promise(resolve => {
    const observer = channel => {
      if (
        channel instanceof Ci.nsIHttpChannel &&
        channel.URI.host === host &&
        channel.loadInfo.internalContentPolicyType ===
          Ci.nsIContentPolicy.TYPE_INTERNAL_IFRAME
      ) {
        Services.obs.removeObserver(observer, "http-on-stop-request");
        resolve(channel.URI.spec);
      }
    };
    Services.obs.addObserver(observer, "http-on-stop-request");
  });
}

add_task(async function() {
  for (let networkIsolation of [true, false]) {
    for (let partitionPerSite of [true, false]) {
      await SpecialPowers.pushPrefEnv({
        set: [
          ["privacy.partition.network_state", networkIsolation],
          ["privacy.dynamic_firstparty.use_site", partitionPerSite],
        ],
      });

      let tab = (gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser));

      // Let's load the secureURL as first-party in order to activate HSTS.
      await promiseTabLoadEvent(tab, secureURL, secureURL);

      // Let's test HSTS: unsecure -> secure.
      await promiseTabLoadEvent(tab, unsecureURL, secureURL);
      ok(true, "unsecure -> secure, first-party works!");

      // Let's load a first-party.
      await promiseTabLoadEvent(tab, unsecureEmptyURL, unsecureEmptyURL);

      let finalURL = waitFor("example.com");

      await SpecialPowers.spawn(tab.linkedBrowser, [unsecureURL], async url => {
        let ifr = content.document.createElement("iframe");
        content.document.body.appendChild(ifr);
        ifr.src = url;
      });

      if (networkIsolation) {
        is(await finalURL, unsecureURL, "HSTS doesn't work for 3rd parties");
      } else {
        is(await finalURL, secureURL, "HSTS works for 3rd parties");
      }

      gBrowser.removeCurrentTab();
      cleanupHSTS(networkIsolation, partitionPerSite);
    }
  }
});
