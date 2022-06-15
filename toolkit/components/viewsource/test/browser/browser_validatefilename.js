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
      // Some versions of Windows will crop the extension to three characters so allow both forms.
      ok(
        fl.leafName == test.basename + ".htm" ||
          fl.leafName == test.basename + ".html",
        "filename title for " + test.basename + " html"
      );

      doc.contentType = "application/xhtml+xml";
      fl = gViewSourceUtils.getTemporaryFile(
        browser.currentURI,
        doc,
        "application/xhtml+xml"
      );
      ok(
        fl.leafName == test.basename + ".xht" ||
          fl.leafName == test.basename + ".xhtml",
        "filename title for " + test.basename + " xhtml"
      );
    });
  }

  let fl = gViewSourceUtils.getTemporaryFile(
    Services.io.newURI("http://www.example.com/simple"),
    null,
    "text/html"
  );
  ok(
    fl.leafName == "simple.htm" || fl.leafName == "simple.html",
    "filename title for simple"
  );

  fl = gViewSourceUtils.getTemporaryFile(
    Services.io.newURI("http://www.example.com/samplefile.txt"),
    null,
    "text/html"
  );
  is(fl.leafName, "samplefile.txt", "filename title for samplefile");
});
