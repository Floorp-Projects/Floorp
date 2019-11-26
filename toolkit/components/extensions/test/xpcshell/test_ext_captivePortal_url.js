/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function test_url_get_without_set() {
  async function background() {
    browser.captivePortal.canonicalURL.onChange.addListener(details => {
      browser.test.sendMessage("url", details);
    });
    let url = await browser.captivePortal.canonicalURL.get({});
    browser.test.sendMessage("url", url);
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      permissions: ["captivePortal"],
    },
  });

  let defaultURL = Services.prefs.getStringPref("captivedetect.canonicalURL");

  await extension.startup();
  let url = await extension.awaitMessage("url");
  equal(
    url.value,
    defaultURL,
    "The canonicalURL setting has the expected value."
  );
  equal(
    url.levelOfControl,
    "not_controllable",
    "The canonicalURL setting has the expected levelOfControl."
  );

  Services.prefs.setStringPref(
    "captivedetect.canonicalURL",
    "http://example.com"
  );
  url = await extension.awaitMessage("url");
  equal(
    url.value,
    "http://example.com",
    "The canonicalURL setting has the expected value."
  );
  equal(
    url.levelOfControl,
    "not_controllable",
    "The canonicalURL setting has the expected levelOfControl."
  );

  await extension.unload();
});
