/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

do_get_profile();

add_setup(_ => {
  // Init cookieBannerService and pretend we opened a profile.
  let cbs = Cc["@mozilla.org/cookie-banner-service;1"].getService(
    Ci.nsIObserver
  );
  cbs.observe(null, "profile-after-change", null);
});

add_task(async function test_delete_exception() {
  info("Enabling cookie banner service with MODE_REJECT");
  Services.prefs.setIntPref(
    "cookiebanners.service.mode",
    Ci.nsICookieBannerService.MODE_REJECT
  );

  // Test nsIClearDataService.deleteDataFromHost
  info("Adding an exception for example.com");
  let uri = Services.io.newURI("https://example.com");

  Services.cookieBanners.setDomainPref(
    uri,
    Ci.nsICookieBannerService.MODE_REJECT_OR_ACCEPT,
    false
  );

  info("Verify that the exception is properly added");
  Assert.equal(
    Services.cookieBanners.getDomainPref(uri, false),
    Ci.nsICookieBannerService.MODE_REJECT_OR_ACCEPT,
    "The exception is properly set."
  );

  info("Trigger the deleteDataFromHost");
  await new Promise(resolve => {
    Services.clearData.deleteDataFromHost(
      "example.com",
      true /* user request */,
      Ci.nsIClearDataService.CLEAR_COOKIE_BANNER_EXCEPTION,
      _ => {
        resolve();
      }
    );
  });

  info("Verify that the exception is deleted");
  Assert.equal(
    Services.cookieBanners.getDomainPref(uri, false),
    Ci.nsICookieBannerService.MODE_UNSET,
    "The exception is properly cleared."
  );

  // Test nsIClearDataService.deleteDataFromBaseDomain
  info("Adding an exception for example.com");
  Services.cookieBanners.setDomainPref(
    uri,
    Ci.nsICookieBannerService.MODE_REJECT_OR_ACCEPT,
    false
  );

  info("Verify that the exception is properly added");
  Assert.equal(
    Services.cookieBanners.getDomainPref(uri, false),
    Ci.nsICookieBannerService.MODE_REJECT_OR_ACCEPT,
    "The exception is properly set."
  );

  info("Trigger the deleteDataFromBaseDomain");
  await new Promise(resolve => {
    Services.clearData.deleteDataFromBaseDomain(
      "example.com",
      true /* user request */,
      Ci.nsIClearDataService.CLEAR_COOKIE_BANNER_EXCEPTION,
      _ => {
        resolve();
      }
    );
  });

  info("Verify that the exception is deleted");
  Assert.equal(
    Services.cookieBanners.getDomainPref(uri, false),
    Ci.nsICookieBannerService.MODE_UNSET,
    "The exception is properly cleared."
  );

  // Test nsIClearDataService.deleteDataFromPrincipal
  info("Adding an exception for example.com");
  Services.cookieBanners.setDomainPref(
    uri,
    Ci.nsICookieBannerService.MODE_REJECT_OR_ACCEPT,
    false
  );

  info("Verify that the exception is properly added");
  Assert.equal(
    Services.cookieBanners.getDomainPref(uri, false),
    Ci.nsICookieBannerService.MODE_REJECT_OR_ACCEPT,
    "The exception is properly set."
  );

  info("Trigger the deleteDataFromPrincipal");
  let principal = Services.scriptSecurityManager.createContentPrincipal(
    uri,
    {}
  );
  await new Promise(resolve => {
    Services.clearData.deleteDataFromPrincipal(
      principal,
      true /* user request */,
      Ci.nsIClearDataService.CLEAR_COOKIE_BANNER_EXCEPTION,
      _ => {
        resolve();
      }
    );
  });

  info("Verify that the exception is deleted");
  Assert.equal(
    Services.cookieBanners.getDomainPref(uri, false),
    Ci.nsICookieBannerService.MODE_UNSET,
    "The exception is properly cleared."
  );

  // Test nsIClearDataService.deleteData
  info("Adding an exception for example.com");
  Services.cookieBanners.setDomainPref(
    uri,
    Ci.nsICookieBannerService.MODE_REJECT_OR_ACCEPT,
    false
  );

  info("Verify that the exception is properly added");
  Assert.equal(
    Services.cookieBanners.getDomainPref(uri, false),
    Ci.nsICookieBannerService.MODE_REJECT_OR_ACCEPT,
    "The exception is properly set."
  );

  info("Trigger the deleteData");
  await new Promise(resolve => {
    Services.clearData.deleteData(
      Ci.nsIClearDataService.CLEAR_COOKIE_BANNER_EXCEPTION,
      _ => {
        resolve();
      }
    );
  });

  info("Verify that the exception is deleted");
  Assert.equal(
    Services.cookieBanners.getDomainPref(uri, false),
    Ci.nsICookieBannerService.MODE_UNSET,
    "The exception is properly cleared."
  );

  Services.prefs.clearUserPref("cookiebanners.service.mode");
});

add_task(async function test_delete_executed_record() {
  info("Enabling cookie banner service with MODE_REJECT");
  Services.prefs.setIntPref(
    "cookiebanners.service.mode",
    Ci.nsICookieBannerService.MODE_REJECT
  );
  Services.prefs.setIntPref(
    "cookiebanners.bannerClicking.maxTriesPerSiteAndSession",
    1
  );

  // Test nsIClearDataService.deleteDataFromHost
  info("Adding a record for example.com");
  Services.cookieBanners.markSiteExecuted("example.com", true, false);

  info("Verify that the record is properly added");
  Assert.ok(
    Services.cookieBanners.shouldStopBannerClickingForSite(
      "example.com",
      true,
      false
    ),
    "The record is properly set."
  );

  info("Trigger the deleteDataFromHost");
  await new Promise(resolve => {
    Services.clearData.deleteDataFromHost(
      "example.com",
      true /* user request */,
      Ci.nsIClearDataService.CLEAR_COOKIE_BANNER_EXECUTED_RECORD,
      _ => {
        resolve();
      }
    );
  });

  info("Verify that the exception is deleted");
  Assert.ok(
    !Services.cookieBanners.shouldStopBannerClickingForSite(
      "example.com",
      true,
      false
    ),
    "The record is properly cleared."
  );

  // Test nsIClearDataService.deleteDataFromBaseDomain
  info("Adding a record for example.com");
  Services.cookieBanners.markSiteExecuted("example.com", true, false);

  info("Verify that the record is properly added");
  Assert.ok(
    Services.cookieBanners.shouldStopBannerClickingForSite(
      "example.com",
      true,
      false
    ),
    "The record is properly set."
  );

  info("Trigger the deleteDataFromBaseDomain");
  await new Promise(resolve => {
    Services.clearData.deleteDataFromBaseDomain(
      "example.com",
      true /* user request */,
      Ci.nsIClearDataService.CLEAR_COOKIE_BANNER_EXECUTED_RECORD,
      _ => {
        resolve();
      }
    );
  });

  info("Verify that the exception is deleted");
  Assert.ok(
    !Services.cookieBanners.shouldStopBannerClickingForSite(
      "example.com",
      true,
      false
    ),
    "The record is properly cleared."
  );

  // Test nsIClearDataService.deleteDataFromPrincipal
  info("Adding a record for example.com");
  Services.cookieBanners.markSiteExecuted("example.com", true, false);

  info("Verify that the record is properly added");
  Assert.ok(
    Services.cookieBanners.shouldStopBannerClickingForSite(
      "example.com",
      true,
      false
    ),
    "The record is properly set."
  );

  info("Trigger the deleteDataFromPrincipal");
  let uri = Services.io.newURI("https://example.com");
  let principal = Services.scriptSecurityManager.createContentPrincipal(
    uri,
    {}
  );
  await new Promise(resolve => {
    Services.clearData.deleteDataFromPrincipal(
      principal,
      true /* user request */,
      Ci.nsIClearDataService.CLEAR_COOKIE_BANNER_EXECUTED_RECORD,
      _ => {
        resolve();
      }
    );
  });

  info("Verify that the exception is deleted");
  Assert.ok(
    !Services.cookieBanners.shouldStopBannerClickingForSite(
      "example.com",
      true,
      false
    ),
    "The record is properly cleared."
  );

  // Test nsIClearDataService.deleteData
  info("Adding a record for example.com");
  Services.cookieBanners.markSiteExecuted("example.com", true, false);

  info("Verify that the record is properly added");
  Assert.ok(
    Services.cookieBanners.shouldStopBannerClickingForSite(
      "example.com",
      true,
      false
    ),
    "The record is properly set."
  );

  info("Trigger the deleteData");
  await new Promise(resolve => {
    Services.clearData.deleteData(
      Ci.nsIClearDataService.CLEAR_COOKIE_BANNER_EXECUTED_RECORD,
      _ => {
        resolve();
      }
    );
  });

  info("Verify that the exception is deleted");
  Assert.ok(
    !Services.cookieBanners.shouldStopBannerClickingForSite(
      "example.com",
      true,
      false
    ),
    "The record is properly cleared."
  );

  Services.prefs.clearUserPref("cookiebanners.service.mode");
  Services.prefs.clearUserPref(
    "cookiebanners.bannerClicking.maxTriesPerSiteAndSession"
  );
});
