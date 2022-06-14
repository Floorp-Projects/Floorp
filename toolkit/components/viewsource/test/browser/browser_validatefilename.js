/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function() {
  let tests = [
    {
      uri: "data:text/html,Test",
      basename: "index",
    },
    {
      uri: "data:text/html,<title>Hello There</title>Test",
      basename: "Hello There",
    },
  ];

  let isWindows = AppConstants.platform == "win";

  for (let test of tests) {
    await BrowserTestUtils.withNewTab(test.uri, async browser => {
      let doc = {
        characterSet: browser.characterSet,
        contentType: browser.documentContentType,
        title: browser.contentTitle,
      };

      let fl = gViewSourceUtils.getTemporaryFile(
        browser.currentURI,
        doc,
        "text/html"
      );
      is(
        fl.leafName,
        test.basename + (isWindows ? ".htm" : ".html"),
        "filename title for " + test.basename + " html"
      );

      doc.contentType = "application/xhtml+xml";
      fl = gViewSourceUtils.getTemporaryFile(
        browser.currentURI,
        doc,
        "application/xhtml+xml"
      );
      is(
        fl.leafName,
        test.basename + (isWindows ? ".xht" : ".xhtml"),
        "filename title for " + test.basename + " xhtml"
      );
    });
  }

  let fl = gViewSourceUtils.getTemporaryFile(
    Services.io.newURI("http://www.example.com/simple"),
    null,
    "text/html"
  );
  is(
    fl.leafName,
    isWindows ? "simple.htm" : "simple.html",
    "filename title for simple"
  );

  fl = gViewSourceUtils.getTemporaryFile(
    Services.io.newURI("http://www.example.com/samplefile.txt"),
    null,
    "text/html"
  );
  is(fl.leafName, "samplefile.txt", "filename title for samplefile");
});
