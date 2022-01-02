/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const handlerSvc = Cc["@mozilla.org/uriloader/handler-service;1"].getService(
  Ci.nsIHandlerService
);

const mimeSvc = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);

const { Integration } = ChromeUtils.import(
  "resource://gre/modules/Integration.jsm"
);

/* global DownloadIntegration */
Integration.downloads.defineModuleGetter(
  this,
  "DownloadIntegration",
  "resource://gre/modules/DownloadIntegration.jsm"
);

/**
 * Tests that the migration does not run if the pref
 * browser.download.improvements_to_download_panel is disabled.
 */
add_task(async function test_migration_pref_disabled() {
  registerCleanupFunction(async function() {
    Services.prefs.clearUserPref(
      "browser.download.improvements_to_download_panel"
    );
  });

  Services.prefs.setBoolPref(
    "browser.download.improvements_to_download_panel",
    false
  );

  // Plain text file
  let txtHandlerInfo = mimeSvc.getFromTypeAndExtension("text/plain", "txt");
  txtHandlerInfo.preferredAction = Ci.nsIHandlerInfo.alwaysAsk;
  txtHandlerInfo.alwaysAskBeforeHandling = true;
  // PDF file
  let pdfHandlerInfo = mimeSvc.getFromTypeAndExtension(
    "application/pdf",
    "pdf"
  );
  // When the downloads pref is disabled, HandlerService.store defaults the
  // preferredAction from alwaysAsk to useHelperApp for compatibility reasons.
  pdfHandlerInfo.preferredAction = Ci.nsIHandlerInfo.useHelperApp;
  pdfHandlerInfo.alwaysAskBeforeHandling = true;
  handlerSvc.store(txtHandlerInfo);
  handlerSvc.store(pdfHandlerInfo);

  gHandlerService.wrappedJSObject._migrateDownloadsImprovementsIfNeeded();

  txtHandlerInfo = mimeSvc.getFromTypeAndExtension("text/plain", "txt");
  pdfHandlerInfo = mimeSvc.getFromTypeAndExtension("application/pdf", "pdf");
  let data = gHandlerService.wrappedJSObject._store.data;

  Assert.equal(
    data.isDownloadsImprovementsAlreadyMigrated,
    false,
    "isDownloadsImprovementsAlreadyMigrated should be set to false"
  );
  Assert.notEqual(
    pdfHandlerInfo.preferredAction,
    Ci.nsIHandlerInfo.saveToDisk,
    "application/pdf - preferredAction should not be saveToDisk"
  );
  Assert.equal(
    txtHandlerInfo.preferredAction,
    Ci.nsIHandlerInfo.useHelperApp,
    "text/plain - preferredAction should be useHelperApp"
  );
  Assert.equal(
    txtHandlerInfo.alwaysAskBeforeHandling,
    true,
    "text/plain - alwaysAskBeforeHandling should be true"
  );
});

/**
 * Tests that the migration runs if the pref
 * browser.download.improvements_to_download_panel is enabled and
 * that only files with preferredAction alwaysAsk are updated.
 */
add_task(async function test_migration_pref_enabled() {
  // Create mock implementation of shouldDownloadInternally for test case
  let oldShouldViewDownloadInternally =
    DownloadIntegration.shouldViewDownloadInternally;
  DownloadIntegration.shouldViewDownloadInternally = (mimeType, extension) => {
    let downloadTypesViewableInternally = [
      {
        extension: "pdf",
        mimeTypes: ["application/pdf"],
      },
      {
        extension: "svg",
        mimeTypes: ["image/svg+xml"],
      },
    ];

    for (const mockHandler of downloadTypesViewableInternally) {
      if (mockHandler.mimeTypes.includes(mimeType)) {
        return true;
      }
    }

    return false;
  };

  registerCleanupFunction(async function() {
    Services.prefs.clearUserPref(
      "browser.download.improvements_to_download_panel"
    );
    DownloadIntegration.shouldViewDownloadInternally = oldShouldViewDownloadInternally;
  });

  // For setup, set pref to false. Will be enabled later.
  Services.prefs.setBoolPref(
    "browser.download.improvements_to_download_panel",
    false
  );

  // Plain text file
  let txtHandlerInfo = mimeSvc.getFromTypeAndExtension("text/plain", "txt");
  txtHandlerInfo.preferredAction = Ci.nsIHandlerInfo.alwaysAsk;
  txtHandlerInfo.alwaysAskBeforeHandling = true;
  // PDF file
  let pdfHandlerInfo = mimeSvc.getFromTypeAndExtension(
    "application/pdf",
    "pdf"
  );
  pdfHandlerInfo.preferredAction = Ci.nsIHandlerInfo.alwaysAsk;
  pdfHandlerInfo.alwaysAskBeforeHandling = true;
  // SVG file
  let svgHandlerInfo = mimeSvc.getFromTypeAndExtension("image/svg+xml", "svg");
  svgHandlerInfo.preferredAction = Ci.nsIHandlerInfo.useSystemDefault;
  svgHandlerInfo.alwaysAskBeforeHandling = false;

  handlerSvc.store(txtHandlerInfo);
  handlerSvc.store(pdfHandlerInfo);
  handlerSvc.store(svgHandlerInfo);

  Services.prefs.setBoolPref(
    "browser.download.improvements_to_download_panel",
    true
  );
  gHandlerService.wrappedJSObject._migrateDownloadsImprovementsIfNeeded();

  txtHandlerInfo = mimeSvc.getFromTypeAndExtension("text/plain", "txt");
  pdfHandlerInfo = mimeSvc.getFromTypeAndExtension("application/pdf", "pdf");
  svgHandlerInfo = mimeSvc.getFromTypeAndExtension("image/svg+xml", "svg");
  let data = gHandlerService.wrappedJSObject._store.data;
  Assert.equal(
    data.isDownloadsImprovementsAlreadyMigrated,
    true,
    "isDownloadsImprovementsAlreadyMigrated should be set to true"
  );
  Assert.equal(
    pdfHandlerInfo.preferredAction,
    Ci.nsIHandlerInfo.handleInternally,
    "application/pdf - preferredAction should be handleInternally"
  );
  Assert.equal(
    pdfHandlerInfo.alwaysAskBeforeHandling,
    false,
    "application/pdf - alwaysAskBeforeHandling should be false"
  );
  Assert.equal(
    svgHandlerInfo.preferredAction,
    Ci.nsIHandlerInfo.useSystemDefault,
    "image/svg+xml - preferredAction should be useSystemDefault"
  );
  Assert.equal(
    pdfHandlerInfo.alwaysAskBeforeHandling,
    false,
    "image/svg+xml - alwaysAskBeforeHandling should be false"
  );
  Assert.equal(
    txtHandlerInfo.preferredAction,
    Ci.nsIHandlerInfo.saveToDisk,
    "text/plain - preferredAction should be saveToDisk"
  );
  Assert.equal(
    txtHandlerInfo.alwaysAskBeforeHandling,
    false,
    "text/plain - alwaysAskBeforeHandling should be false"
  );
});

/**
 * Tests that the migration does not run if the pref
 * browser.download.improvements_to_download_panel is enabled but
 * the migration was already run.
 */
add_task(async function test_migration_pref_enabled_already_run() {
  registerCleanupFunction(async function() {
    Services.prefs.clearUserPref(
      "browser.download.improvements_to_download_panel"
    );
  });

  Services.prefs.setBoolPref(
    "browser.download.improvements_to_download_panel",
    true
  );
  let data = gHandlerService.wrappedJSObject._store.data;
  data.isDownloadsImprovementsAlreadyMigrated = true;

  // Plain text file
  let txtHandlerInfo = mimeSvc.getFromTypeAndExtension("text/plain", "txt");
  txtHandlerInfo.preferredAction = Ci.nsIHandlerInfo.alwaysAsk;
  txtHandlerInfo.alwaysAskBeforeHandling = true;
  // PDF file
  let pdfHandlerInfo = mimeSvc.getFromTypeAndExtension(
    "application/pdf",
    "pdf"
  );
  pdfHandlerInfo.preferredAction = Ci.nsIHandlerInfo.alwaysAsk;
  pdfHandlerInfo.alwaysAskBeforeHandling = true;

  handlerSvc.store(txtHandlerInfo);
  handlerSvc.store(pdfHandlerInfo);

  gHandlerService.wrappedJSObject._migrateDownloadsImprovementsIfNeeded();

  txtHandlerInfo = mimeSvc.getFromTypeAndExtension("text/plain", "txt");
  pdfHandlerInfo = mimeSvc.getFromTypeAndExtension("application/pdf", "pdf");
  data = gHandlerService.wrappedJSObject._store.data;
  Assert.equal(
    pdfHandlerInfo.preferredAction,
    Ci.nsIHandlerInfo.alwaysAsk,
    "application/pdf - preferredAction should be alwaysAsk"
  );
  Assert.equal(
    pdfHandlerInfo.alwaysAskBeforeHandling,
    true,
    "application/pdf - alwaysAskBeforeHandling should be true"
  );
  Assert.equal(
    txtHandlerInfo.preferredAction,
    Ci.nsIHandlerInfo.alwaysAsk,
    "text/plain - preferredAction should be alwaysAsk"
  );
  Assert.equal(
    txtHandlerInfo.alwaysAskBeforeHandling,
    true,
    "text/plain - alwaysAskBeforeHandling should be true"
  );
});
