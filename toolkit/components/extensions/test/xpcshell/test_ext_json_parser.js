/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function test_json_parser() {
  const ID = "json@test.web.extension";

  let xpi = AddonTestUtils.createTempWebExtensionFile({
    files: {
      "manifest.json": String.raw`{
        // This is a manifest.
        "manifest_version": 2,
        "applications": {"gecko": {"id": "${ID}"}},
        "name": "This \" is // not a comment",
        "version": "0.1\\" // , "description": "This is not a description"
      }`,
    },
  });

  let expectedManifest = {
    manifest_version: 2,
    applications: { gecko: { id: ID } },
    name: 'This " is // not a comment',
    version: "0.1\\",
  };

  let fileURI = Services.io.newFileURI(xpi);
  let uri = NetUtil.newURI(`jar:${fileURI.spec}!/`);

  let extension = new ExtensionData(uri, false);

  await extension.parseManifest();

  Assert.deepEqual(
    extension.rawManifest,
    expectedManifest,
    "Manifest with correctly-filtered comments"
  );

  Services.obs.notifyObservers(xpi, "flush-cache-entry");
});

add_task(async function test_getExtensionVersionWithoutValidation() {
  let xpi = AddonTestUtils.createTempWebExtensionFile({
    files: {
      "manifest.json": String.raw`{
        // This is valid JSON but not a valid manifest.
        "version": ["This is not a valid version"]
      }`,
    },
  });
  let fileURI = Services.io.newFileURI(xpi);
  let uri = NetUtil.newURI(`jar:${fileURI.spec}!/`);
  let extension = new ExtensionData(uri, false);

  let rawVersion = await extension.getExtensionVersionWithoutValidation();
  Assert.deepEqual(
    rawVersion,
    ["This is not a valid version"],
    "Got the raw value of the 'version' key from an (invalid) manifest file"
  );

  // The manifest lacks several required properties and manifest_version is
  // invalid. The exact error here doesn't matter, as long as it shows that the
  // manifest is invalid.
  await Assert.rejects(
    extension.parseManifest(),
    /Unexpected params.manifestVersion value: undefined/,
    "parseManifest() should reject an invalid manifest"
  );

  Services.obs.notifyObservers(xpi, "flush-cache-entry");
});
