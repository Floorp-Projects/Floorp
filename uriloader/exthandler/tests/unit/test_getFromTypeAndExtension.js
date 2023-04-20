/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_utf8_extension() {
  const mimeService = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);
  let someMIME = mimeService.getFromTypeAndExtension(
    "application/x-nonsense",
    ".тест"
  );
  Assert.stringContains(someMIME.description, "тест");
  // primary extension isn't set on macOS or android, see bug 1721181
  if (AppConstants.platform != "macosx" && AppConstants.platform != "android") {
    Assert.equal(someMIME.primaryExtension, ".тест");
  }
});
