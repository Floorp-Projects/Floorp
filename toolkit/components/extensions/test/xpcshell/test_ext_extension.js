/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(function* test_is_allowed_incognito_access() {
  async function background() {
    let allowed = await browser.extension.isAllowedIncognitoAccess();

    browser.test.assertEq(true, allowed, "isAllowedIncognitoAccess is true");
    browser.test.notifyPass("isAllowedIncognitoAccess");
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {},
  });

  yield extension.startup();
  yield extension.awaitFinish("isAllowedIncognitoAccess");
  yield extension.unload();
});

add_task(function* test_in_incognito_context_false() {
  function background() {
    browser.test.assertEq(false, browser.extension.inIncognitoContext, "inIncognitoContext returned false");
    browser.test.notifyPass("inIncognitoContext");
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {},
  });

  yield extension.startup();
  yield extension.awaitFinish("inIncognitoContext");
  yield extension.unload();
});

add_task(function* test_is_allowed_file_scheme_access() {
  async function background() {
    let allowed = await browser.extension.isAllowedFileSchemeAccess();

    browser.test.assertEq(false, allowed, "isAllowedFileSchemeAccess is false");
    browser.test.notifyPass("isAllowedFileSchemeAccess");
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {},
  });

  yield extension.startup();
  yield extension.awaitFinish("isAllowedFileSchemeAccess");
  yield extension.unload();
});
