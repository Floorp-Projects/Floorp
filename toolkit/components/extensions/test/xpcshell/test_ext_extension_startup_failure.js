/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const { ExtensionTestCommon } = ChromeUtils.import(
  "resource://testing-common/ExtensionTestCommon.jsm"
);

add_task(async function extension_startup_early_error() {
  const EXTENSION_ID = "@extension-with-package-error";
  let extension = ExtensionTestCommon.generate({
    manifest: {
      applications: { gecko: { id: EXTENSION_ID } },
    },
  });

  extension.initLocale = async function() {
    // Simulate error that happens during startup.
    extension.packagingError("dummy error");
  };

  let startupPromise = extension.startup();

  let policy = WebExtensionPolicy.getByID(EXTENSION_ID);
  ok(policy, "WebExtensionPolicy instantiated at startup");
  let readyPromise = policy.readyPromise;
  ok(readyPromise, "WebExtensionPolicy.readyPromise is set");

  await Assert.rejects(
    startupPromise,
    /dummy error/,
    "Extension with packaging error should fail to load"
  );

  Assert.equal(
    WebExtensionPolicy.getByID(EXTENSION_ID),
    null,
    "WebExtensionPolicy should be unregistered"
  );

  Assert.equal(
    await readyPromise,
    null,
    "policy.readyPromise should be resolved with null"
  );
});
