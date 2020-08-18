/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

XPCOMUtils.defineLazyServiceGetter(
  this,
  "gMIMEService",
  "@mozilla.org/mime;1",
  "nsIMIMEService"
);
XPCOMUtils.defineLazyServiceGetter(
  this,
  "gBundleService",
  "@mozilla.org/intl/stringbundle;1",
  "nsIStringBundleService"
);

// PDF files should always have a generic description instead
// of relying on what is registered with the Operating System.
add_task(async function test_check_unknown_mime_type() {
  const mimeService = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);
  let pdfType = mimeService.getTypeFromExtension("pdf");
  Assert.equal(pdfType, "application/pdf");
  let extension = mimeService.getPrimaryExtension("application/pdf", "");
  Assert.equal(extension, "pdf", "Expect pdf extension when given mime");
  let mimeInfo = gMIMEService.getFromTypeAndExtension("", "pdf");
  let stringBundle = gBundleService.createBundle(
    "chrome://mozapps/locale/downloads/unknownContentType.properties"
  );
  Assert.equal(
    mimeInfo.description,
    stringBundle.GetStringFromName("pdfExtHandlerDescription"),
    "PDF has generic description"
  );
});
