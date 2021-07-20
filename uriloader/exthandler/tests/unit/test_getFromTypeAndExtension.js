/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_utf8_extension() {
  const mimeService = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);
  let someMIME = mimeService.getFromTypeAndExtension(
    "application/x-gobbledygook",
    ".тест"
  );
  Assert.stringContains(someMIME.description, "тест");
  // primary extension isn't set on macOS, see bug 1721181
  if (AppConstants.platform != "macosx") {
    Assert.equal(someMIME.primaryExtension, ".тест");
  }
});
