/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Zip files can be opened by Windows explorer, so we should always be able to
// determine a description and default handler for them. However, things can
// get messy if they are sent to us with a mime type other than what Windows
// considers the "right" mimetype (application/x-zip-compressed), like
// application/zip, which is what most places (IANA, macOS, probably all linux
// distros, Apache, etc.) think is the "right" mimetype.
add_task(async function test_check_unknown_mime_type() {
  const mimeService = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);
  let zipType = mimeService.getTypeFromExtension("zip");
  Assert.equal(zipType, "application/x-zip-compressed");
  try {
    let extension = mimeService.getPrimaryExtension("application/zip", "");
    Assert.ok(!extension, "Expect no known extension for zip files.");
  } catch (ex) {
    Assert.ok(
      ex,
      "We'll take an exception instead of an empty string, I guess."
    );
  }
  let found = {};
  let mimeInfo = mimeService.getMIMEInfoFromOS("application/zip", "zip", found);
  Assert.ok(
    found.value,
    "Should be able to find mime info even for the unknown mimetype."
  );
  Assert.ok(mimeInfo.hasDefaultHandler, "Should have a default app");
});
