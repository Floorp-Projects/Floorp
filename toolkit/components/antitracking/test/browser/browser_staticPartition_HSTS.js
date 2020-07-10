/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var unsecureEmptyURL =
  "http://example.org/browser/toolkit/components/antitracking/test/browser/empty.html";
var secureURL =
  "https://example.com/browser/toolkit/components/antitracking/test/browser/browser_staticPartition_HSTS.sjs";
var unsecureURL =
  "http://example.com/browser/toolkit/components/antitracking/test/browser/browser_staticPartition_HSTS.sjs";

function cleanupHSTS() {
  // Ensure to remove example.com from the HSTS list.
  let sss = Cc["@mozilla.org/ssservice;1"].getService(
    Ci.nsISiteSecurityService
  );
  sss.resetState(
    Ci.nsISiteSecurityService.HEADER_HSTS,
    NetUtil.newURI("http://example.com/"),
    0
  );
}

function promiseTabLoadEvent(aTab, aURL, aFinalURL) {
  info("Wait for load tab event");
  BrowserTestUtils.loadURI(aTab.linkedBrowser, aURL);
  return BrowserTestUtils.browserLoaded(aTab.linkedBrowser, false, aFinalURL);
}

add_task(async function() {
  for (let prefValue of [true, false]) {
    await SpecialPowers.pushPrefEnv({
      set: [["privacy.partition.network_state", prefValue]],
    });

    let tab = (gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser));

    // Let's load the secureURL as first-party in order to activate HSTS.
    await promiseTabLoadEvent(tab, secureURL, secureURL);

    // Let's test HSTS: unsecure -> secure.
    await promiseTabLoadEvent(tab, unsecureURL, secureURL);
    ok(true, "unsecure -> secure, first-party works!");

    // Let's load a first-party.
    await promiseTabLoadEvent(tab, unsecureEmptyURL, unsecureEmptyURL);

    let finalURL = await SpecialPowers.spawn(
      tab.linkedBrowser,
      [unsecureURL],
      async url => {
        return new content.Promise(resolve => {
          let ifr = content.document.createElement("iframe");
          ifr.onload = _ => {
            resolve(ifr.contentWindow.location.href);
          };

          content.document.body.appendChild(ifr);
          ifr.src = url;
        });
      }
    );

    if (prefValue) {
      is(finalURL, unsecureURL, "HSTS doesn't work for 3rd parties");
    } else {
      is(finalURL, secureURL, "HSTS works for 3rd parties");
    }

    gBrowser.removeCurrentTab();
    cleanupHSTS();
  }
});
