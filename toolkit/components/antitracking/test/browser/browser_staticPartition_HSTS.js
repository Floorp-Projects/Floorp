/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var unsecureEmptyURL =
  "http://example.org/browser/toolkit/components/antitracking/test/browser/empty.html";
var secureEmptyURL =
  "https://example.org/browser/toolkit/components/antitracking/test/browser/empty.html";
var secureAnotherEmptyURL =
  "https://example.com/browser/toolkit/components/antitracking/test/browser/empty.html";
var secureURL =
  "https://example.com/browser/toolkit/components/antitracking/test/browser/browser_staticPartition_HSTS.sjs";
var unsecureURL =
  "http://example.com/browser/toolkit/components/antitracking/test/browser/browser_staticPartition_HSTS.sjs";
var secureImgURL =
  "https://example.com/browser/toolkit/components/antitracking/test/browser/browser_staticPartition_HSTS.sjs?image";
var unsecureImgURL =
  "http://example.com/browser/toolkit/components/antitracking/test/browser/browser_staticPartition_HSTS.sjs?image";
var secureIncludeSubURL =
  "https://example.com/browser/toolkit/components/antitracking/test/browser/browser_staticPartition_HSTS.sjs?includeSub";
var unsecureSubEmptyURL =
  "http://test1.example.com/browser/toolkit/components/antitracking/test/browser/empty.html";
var secureSubEmptyURL =
  "https://test1.example.com/browser/toolkit/components/antitracking/test/browser/empty.html";
var unsecureNoCertSubEmptyURL =
  "http://nocert.example.com/browser/toolkit/components/antitracking/test/browser/empty.html";

function cleanupHSTS(aPartitionEnabled, aUseSite) {
  // Ensure to remove example.com from the HSTS list.
  let sss = Cc["@mozilla.org/ssservice;1"].getService(
    Ci.nsISiteSecurityService
  );

  for (let origin of ["example.com", "example.org"]) {
    let originAttributes = {};

    if (aPartitionEnabled) {
      if (aUseSite) {
        originAttributes = { partitionKey: `(http,${origin})` };
      } else {
        originAttributes = { partitionKey: origin };
      }
    }

    sss.resetState(NetUtil.newURI("http://example.com/"), originAttributes);
  }
}

function promiseTabLoadEvent(aTab, aURL, aFinalURL) {
  info("Wait for load tab event");
  BrowserTestUtils.startLoadingURIString(aTab.linkedBrowser, aURL);
  return BrowserTestUtils.browserLoaded(aTab.linkedBrowser, false, aFinalURL);
}

function waitFor(host, type) {
  return new Promise(resolve => {
    const observer = channel => {
      if (
        channel instanceof Ci.nsIHttpChannel &&
        channel.URI.host === host &&
        channel.loadInfo.internalContentPolicyType === type
      ) {
        Services.obs.removeObserver(observer, "http-on-stop-request");
        resolve(channel.URI.spec);
      }
    };
    Services.obs.addObserver(observer, "http-on-stop-request");
  });
}

add_task(async function () {
  for (let networkIsolation of [true, false]) {
    for (let partitionPerSite of [true, false]) {
      await SpecialPowers.pushPrefEnv({
        set: [
          ["privacy.partition.network_state", networkIsolation],
          ["privacy.dynamic_firstparty.use_site", partitionPerSite],
          ["security.mixed_content.upgrade_display_content", false],
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

      let finalURL = waitFor(
        "example.com",
        Ci.nsIContentPolicy.TYPE_INTERNAL_IFRAME
      );

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

add_task(async function test_subresource() {
  for (let networkIsolation of [true, false]) {
    for (let partitionPerSite of [true, false]) {
      await SpecialPowers.pushPrefEnv({
        set: [
          ["privacy.partition.network_state", networkIsolation],
          ["privacy.dynamic_firstparty.use_site", partitionPerSite],
          ["security.mixed_content.upgrade_display_content", false],
        ],
      });

      let tab = (gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser));

      // Load a secure page as first party.
      await promiseTabLoadEvent(tab, secureEmptyURL, secureEmptyURL);

      let loadPromise = waitFor(
        "example.com",
        Ci.nsIContentPolicy.TYPE_INTERNAL_IMAGE
      );

      // Load a secure subresource. HSTS won't be activated, since third
      // parties can't set HSTS.
      await SpecialPowers.spawn(
        tab.linkedBrowser,
        [secureImgURL],
        async url => {
          let ifr = content.document.createElement("img");
          content.document.body.appendChild(ifr);
          ifr.src = url;
        }
      );

      // Ensure the subresource is loaded.
      await loadPromise;

      // Reload the secure page as first party.
      await promiseTabLoadEvent(tab, secureEmptyURL, secureEmptyURL);

      let finalURL = waitFor(
        "example.com",
        Ci.nsIContentPolicy.TYPE_INTERNAL_IMAGE
      );

      // Load an unsecure subresource. It should not be upgraded to https.
      await SpecialPowers.spawn(
        tab.linkedBrowser,
        [unsecureImgURL],
        async url => {
          let ifr = content.document.createElement("img");
          content.document.body.appendChild(ifr);
          ifr.src = url;
        }
      );

      is(await finalURL, unsecureImgURL, "HSTS isn't set for 3rd parties");

      // Load the secure page with a different origin as first party.
      await promiseTabLoadEvent(
        tab,
        secureAnotherEmptyURL,
        secureAnotherEmptyURL
      );

      finalURL = waitFor(
        "example.com",
        Ci.nsIContentPolicy.TYPE_INTERNAL_IMAGE
      );

      // Load a unsecure subresource
      await SpecialPowers.spawn(
        tab.linkedBrowser,
        [unsecureImgURL],
        async url => {
          let ifr = content.document.createElement("img");
          content.document.body.appendChild(ifr);
          ifr.src = url;
        }
      );

      is(await finalURL, unsecureImgURL, "HSTS isn't set for 3rd parties");

      gBrowser.removeCurrentTab();
      cleanupHSTS(networkIsolation, partitionPerSite);
    }
  }
});

add_task(async function test_includeSubDomains() {
  for (let networkIsolation of [true, false]) {
    for (let partitionPerSite of [true, false]) {
      await SpecialPowers.pushPrefEnv({
        set: [
          ["privacy.partition.network_state", networkIsolation],
          ["privacy.dynamic_firstparty.use_site", partitionPerSite],
          ["security.mixed_content.upgrade_display_content", false],
        ],
      });

      let tab = (gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser));

      // Load a secure page as first party to activate HSTS.
      await promiseTabLoadEvent(tab, secureIncludeSubURL, secureIncludeSubURL);

      // Load a unsecure sub-domain page as first party to see if it's upgraded.
      await promiseTabLoadEvent(tab, unsecureSubEmptyURL, secureSubEmptyURL);

      // Load a sub domain page which will trigger the cert error page.
      let certErrorLoaded = BrowserTestUtils.waitForErrorPage(
        tab.linkedBrowser
      );
      BrowserTestUtils.startLoadingURIString(
        tab.linkedBrowser,
        unsecureNoCertSubEmptyURL
      );
      await certErrorLoaded;

      // Verify the error page has the 'badStsCert' in its query string
      await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
        let searchParams = new content.URLSearchParams(
          content.document.documentURI
        );

        is(
          searchParams.get("s"),
          "badStsCert",
          "The cert error page has 'badStsCert' set"
        );
      });

      gBrowser.removeCurrentTab();
      cleanupHSTS(networkIsolation, partitionPerSite);
    }
  }
});
