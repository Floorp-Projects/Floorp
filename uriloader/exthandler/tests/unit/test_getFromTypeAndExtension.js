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

add_task(async function test_pretty_name_for_edge() {
  if (AppConstants.platform == "win" && !AppConstants.IS_ESR) {
    const mimeService = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);

    let mimeInfo = mimeService.getFromTypeAndExtension("text/html", "html");
    let appList = mimeInfo?.possibleLocalHandlers || [];

    for (let index = 0; index < appList.length; index++) {
      let app = appList.queryElementAt(index, Ci.nsILocalHandlerApp);
      let executableName = app.executable.displayName;
      let prettyName = await app.prettyNameAsync();

      // Hardcode Edge, as an extra test, when it's installed
      if (executableName == "msedge.exe") {
        Assert.equal(
          prettyName,
          "Microsoft Edge",
          "The generated pretty name for MS Edge should match the expectation."
        );
      }

      // The pretty name should always be something nicer than the executable name.
      // This isn't testing that's nice, but should be good enough to validate that
      // something other than the executable is found.
      Assert.notEqual(executableName, prettyName);
    }
  }
});

add_task(async function test_pretty_names_match_names_on_non_windows() {
  if (AppConstants.platform != "win") {
    const mimeService = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);

    let mimeInfo = mimeService.getFromTypeAndExtension("text/html", "html");
    let appList = mimeInfo?.possibleLocalHandlers || [];

    for (let index = 0; index < appList.length; index++) {
      let app = appList.queryElementAt(index, Ci.nsILocalHandlerApp);
      let name = app.executable.name;
      let prettyName = await app.prettyNameAsync();

      Assert.equal(
        prettyName,
        name,
        "On platforms other than windows, the prettyName and the name of file handlers should be the same."
      );
    }
  }
});
