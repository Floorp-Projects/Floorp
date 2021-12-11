/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function test_is_allowed_incognito_access() {
  async function background() {
    let allowed = await browser.extension.isAllowedIncognitoAccess();

    browser.test.assertEq(true, allowed, "isAllowedIncognitoAccess is true");
    browser.test.notifyPass("isAllowedIncognitoAccess");
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    incognitoOverride: "spanning",
  });

  await extension.startup();
  await extension.awaitFinish("isAllowedIncognitoAccess");
  await extension.unload();
});

add_task(async function test_is_denied_incognito_access() {
  async function background() {
    let allowed = await browser.extension.isAllowedIncognitoAccess();

    browser.test.assertEq(false, allowed, "isAllowedIncognitoAccess is false");
    browser.test.notifyPass("isNotAllowedIncognitoAccess");
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
  });

  await extension.startup();
  await extension.awaitFinish("isNotAllowedIncognitoAccess");
  await extension.unload();
});

add_task(async function test_in_incognito_context_false() {
  function background() {
    browser.test.assertEq(
      false,
      browser.extension.inIncognitoContext,
      "inIncognitoContext returned false"
    );
    browser.test.notifyPass("inIncognitoContext");
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
  });

  await extension.startup();
  await extension.awaitFinish("inIncognitoContext");
  await extension.unload();
});

add_task(async function test_is_allowed_file_scheme_access() {
  async function background() {
    let allowed = await browser.extension.isAllowedFileSchemeAccess();

    browser.test.assertEq(false, allowed, "isAllowedFileSchemeAccess is false");
    browser.test.notifyPass("isAllowedFileSchemeAccess");
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
  });

  await extension.startup();
  await extension.awaitFinish("isAllowedFileSchemeAccess");
  await extension.unload();
});
