/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(function* test_json_parser() {
  const ID = "json@test.web.extension";

  let xpi = Extension.generateXPI({
    files: {
      "manifest.json": String.raw`{
        // This is a manifest.
        "applications": {"gecko": {"id": "${ID}"}},
        "name": "This \" is // not a comment",
        "version": "0.1\\" // , "description": "This is not a description"
      }`,
    },
  });

  let expectedManifest = {
    "applications": {"gecko": {"id": ID}},
    "name": "This \" is // not a comment",
    "version": "0.1\\",
  };

  let fileURI = Services.io.newFileURI(xpi);
  let uri = NetUtil.newURI(`jar:${fileURI.spec}!/`);

  let extension = new ExtensionData(uri);

  yield extension.readManifest();

  Assert.deepEqual(extension.rawManifest, expectedManifest,
                   "Manifest with correctly-filtered comments");

  Services.obs.notifyObservers(xpi, "flush-cache-entry", null);
  xpi.remove(false);
});
