/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// eslint-disable-next-line mozilla/use-chromeutils-import
Cu.import("resource://gre/modules/NetUtil.jsm");
// eslint-disable-next-line mozilla/reject-importGlobalProperties
Cu.importGlobalProperties(["URLSearchParams"]);

function loadHTMLFromFile(path) {
  // Load the HTML to return in the response from file.
  // Since it's relative to the cwd of the test runner, we start there and
  // append to get to the actual path of the file.
  const testHTMLFile =
    // eslint-disable-next-line mozilla/use-services
    Cc["@mozilla.org/file/directory_service;1"]
      .getService(Ci.nsIProperties)
      .get("CurWorkD", Ci.nsIFile);
  const dirs = path.split("/");
  for (let i = 0; i < dirs.length; i++) {
    testHTMLFile.append(dirs[i]);
  }

  const testHTMLFileStream = Cc[
    "@mozilla.org/network/file-input-stream;1"
  ].createInstance(Ci.nsIFileInputStream);
  testHTMLFileStream.init(testHTMLFile, -1, 0, 0);
  const testHTML = NetUtil.readInputStreamToString(
    testHTMLFileStream,
    testHTMLFileStream.available()
  );

  return testHTML;
}

// eslint-disable-next-line no-unused-vars
function handleRequest(request, response) {
  const queryString = new URLSearchParams(request.queryString);
  const html = queryString.get("html");

  response.setHeader("Cache-Control", "no-cache", false);
  if (html) {
    response.setHeader("Content-Type", "text/html", false);
    response.write(html);
    return;
  }

  const path = queryString.get("file");
  const doc = loadHTMLFromFile(path);
  response.setHeader(
    "Content-Type",
    path.endsWith(".xhtml") ? "application/xhtml+xml" : "text/html",
    false
  );
  // This is a hack to set the correct id for the content document that is to be
  // loaded in the iframe.
  response.write(doc.replace(`id="body"`, `id="default-iframe-body-id"`));
}
