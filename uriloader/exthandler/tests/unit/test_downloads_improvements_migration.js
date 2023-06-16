/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const handlerSvc = Cc["@mozilla.org/uriloader/handler-service;1"].getService(
  Ci.nsIHandlerService
);

const mimeSvc = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);

const { Integration } = ChromeUtils.importESModule(
  "resource://gre/modules/Integration.sys.mjs"
);

/* global DownloadIntegration */
Integration.downloads.defineESModuleGetter(
  this,
  "DownloadIntegration",
  "resource://gre/modules/DownloadIntegration.sys.mjs"
);

/**
 * Tests that the migration runs and that only
 * files with preferredAction alwaysAsk are updated.
 */
add_task(async function test_migration() {
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
        extension: "webp",
        mimeTypes: ["image/webp"],
      },
    ];

    for (const mockHandler of downloadTypesViewableInternally) {
      if (mockHandler.mimeTypes.includes(mimeType)) {
        return true;
      }
    }

    return false;
  };

  registerCleanupFunction(async function () {
    Services.prefs.clearUserPref(
      "browser.download.improvements_to_download_panel"
    );
    DownloadIntegration.shouldViewDownloadInternally =
      oldShouldViewDownloadInternally;
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
  // WebP file
  let webpHandlerInfo = mimeSvc.getFromTypeAndExtension("image/webp", "webp");
  webpHandlerInfo.preferredAction = Ci.nsIHandlerInfo.useSystemDefault;
  webpHandlerInfo.alwaysAskBeforeHandling = false;

  handlerSvc.store(txtHandlerInfo);
  handlerSvc.store(pdfHandlerInfo);
  handlerSvc.store(webpHandlerInfo);

  Services.prefs.setBoolPref(
    "browser.download.improvements_to_download_panel",
    true
  );
  gHandlerService.wrappedJSObject._migrateDownloadsImprovementsIfNeeded();

  txtHandlerInfo = mimeSvc.getFromTypeAndExtension("text/plain", "txt");
  pdfHandlerInfo = mimeSvc.getFromTypeAndExtension("application/pdf", "pdf");
  webpHandlerInfo = mimeSvc.getFromTypeAndExtension("image/webp", "webp");
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
    webpHandlerInfo.preferredAction,
    Ci.nsIHandlerInfo.useSystemDefault,
    "image/webp - preferredAction should be useSystemDefault"
  );
  Assert.equal(
    webpHandlerInfo.alwaysAskBeforeHandling,
    false,
    "image/webp - alwaysAskBeforeHandling should be false"
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
 * Tests that the migration does not run if the migration was already run.
 */
add_task(async function test_migration_already_run() {
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

/**
 * Test migration of SVG and XML info.
 */
add_task(async function test_migration_xml_svg() {
  let data = gHandlerService.wrappedJSObject._store.data;
  // Plain text file
  let txtHandlerInfo = mimeSvc.getFromTypeAndExtension("text/plain", "txt");
  txtHandlerInfo.preferredAction = Ci.nsIHandlerInfo.alwaysAsk;
  txtHandlerInfo.alwaysAskBeforeHandling = true;
  // SVG file
  let svgHandlerInfo = mimeSvc.getFromTypeAndExtension("image/svg+xml", "svg");
  svgHandlerInfo.preferredAction = Ci.nsIHandlerInfo.handleInternally;
  svgHandlerInfo.alwaysAskBeforeHandling = false;
  // XML file
  let xmlHandlerInfo = mimeSvc.getFromTypeAndExtension("text/xml", "xml");
  xmlHandlerInfo.preferredAction = Ci.nsIHandlerInfo.handleInternally;
  xmlHandlerInfo.alwaysAskBeforeHandling = false;

  handlerSvc.store(txtHandlerInfo);
  handlerSvc.store(svgHandlerInfo);
  handlerSvc.store(xmlHandlerInfo);

  gHandlerService.wrappedJSObject._migrateSVGXMLIfNeeded();

  txtHandlerInfo = mimeSvc.getFromTypeAndExtension("text/plain", "txt");
  svgHandlerInfo = mimeSvc.getFromTypeAndExtension("image/svg+xml", "svg");
  xmlHandlerInfo = mimeSvc.getFromTypeAndExtension("text/xml", "xml");
  data = gHandlerService.wrappedJSObject._store.data;
  Assert.equal(
    svgHandlerInfo.preferredAction,
    Ci.nsIHandlerInfo.saveToDisk,
    "image/svg+xml - preferredAction should be saveToDisk"
  );
  Assert.equal(
    svgHandlerInfo.alwaysAskBeforeHandling,
    false,
    "image/svg+xml - alwaysAskBeforeHandling should be false"
  );
  Assert.equal(
    xmlHandlerInfo.preferredAction,
    Ci.nsIHandlerInfo.saveToDisk,
    "text/xml - preferredAction should be saveToDisk"
  );
  Assert.equal(
    xmlHandlerInfo.alwaysAskBeforeHandling,
    false,
    "text/xml - alwaysAskBeforeHandling should be false"
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

  ok(
    data.isSVGXMLAlreadyMigrated,
    "Should have stored migration state on the data object."
  );
});
