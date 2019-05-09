/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {SiteDataTestUtils} = ChromeUtils.import("resource://testing-common/SiteDataTestUtils.jsm");

function addServiceWorker(origin) {
  let swURL = getRootDirectory(gTestPath).replace("chrome://mochitests/content", origin) + "worker.js";
  return SiteDataTestUtils.addServiceWorker(swURL);
}

add_task(async function test_deleteFromHost() {
  await addServiceWorker("https://example.com");
  await addServiceWorker("https://example.org");

  await TestUtils.waitForCondition(() => SiteDataTestUtils.hasServiceWorkers("https://example.com"), "example.com has a service worker");
  ok(true, "example.com has a service worker");

  await TestUtils.waitForCondition(() => SiteDataTestUtils.hasServiceWorkers("https://example.org"), "example.org has a service worker");
  ok(true, "example.org has a service worker");

  await new Promise(aResolve => {
    Services.clearData.deleteDataFromHost("example.com", true, Ci.nsIClearDataService.CLEAR_DOM_QUOTA, value => {
      Assert.equal(value, 0);
      aResolve();
    });
  });

  ok(!SiteDataTestUtils.hasServiceWorkers("https://example.com"), "example.com has no service worker");
  ok(SiteDataTestUtils.hasServiceWorkers("https://example.org"), "example.org has a service worker");

  await new Promise(aResolve => {
    Services.clearData.deleteDataFromHost("example.org", true, Ci.nsIClearDataService.CLEAR_DOM_QUOTA, value => {
      Assert.equal(value, 0);
      aResolve();
    });
  });

  ok(!SiteDataTestUtils.hasServiceWorkers("https://example.org"), "example.org has no service worker");
});

add_task(async function test_deleteFromPrincipal() {
  await addServiceWorker("https://example.com");
  await addServiceWorker("https://example.org");

  await TestUtils.waitForCondition(() => SiteDataTestUtils.hasServiceWorkers("https://example.com"), "example.com has a service worker");
  ok(true, "example.com has a service worker");

  await TestUtils.waitForCondition(() => SiteDataTestUtils.hasServiceWorkers("https://example.org"), "example.org has a service worker");
  ok(true, "example.org has a service worker");

  let principal = Services.scriptSecurityManager.createCodebasePrincipalFromOrigin("https://example.com/");
  await new Promise(aResolve => {
    Services.clearData.deleteDataFromPrincipal(principal, true, Ci.nsIClearDataService.CLEAR_DOM_QUOTA, value => {
      Assert.equal(value, 0);
      aResolve();
    });
  });

  ok(!SiteDataTestUtils.hasServiceWorkers("https://example.com"), "example.com has no service worker");
  ok(SiteDataTestUtils.hasServiceWorkers("https://example.org"), "example.org has a service worker");

  principal = Services.scriptSecurityManager.createCodebasePrincipalFromOrigin("https://example.org/");
  await new Promise(aResolve => {
    Services.clearData.deleteDataFromPrincipal(principal, true, Ci.nsIClearDataService.CLEAR_DOM_QUOTA, value => {
      Assert.equal(value, 0);
      aResolve();
    });
  });

  ok(!SiteDataTestUtils.hasServiceWorkers("https://example.org"), "example.org has no service worker");
});

add_task(async function test_deleteAll() {
  await addServiceWorker("https://example.com");
  await addServiceWorker("https://example.org");

  await TestUtils.waitForCondition(() => SiteDataTestUtils.hasServiceWorkers("https://example.com"), "example.com has a service worker");
  ok(true, "example.com has a service worker");

  await TestUtils.waitForCondition(() => SiteDataTestUtils.hasServiceWorkers("https://example.org"), "example.org has a service worker");
  ok(true, "example.org has a service worker");

  await new Promise(aResolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_DOM_QUOTA, value => {
      Assert.equal(value, 0);
      aResolve();
    });
  });

  ok(!SiteDataTestUtils.hasServiceWorkers("https://example.com"), "example.com has no service worker");
  ok(!SiteDataTestUtils.hasServiceWorkers("https://example.org"), "example.org has no service worker");

  await SiteDataTestUtils.clear();
});
