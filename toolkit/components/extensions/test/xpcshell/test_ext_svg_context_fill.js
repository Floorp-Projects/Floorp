/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "1",
  "42"
);

function createReferencePage(color) {
  return `<!DOCTYPE html>
  <div id="screenshot-target" style="width: 100px; height: 100px; background: ${color}">
  </div>
    `;
}

function screenshotPage(page) {
  return page.spawn([], async () => {
    const { TestUtils } = ChromeUtils.import(
      "resource://testing-common/TestUtils.jsm"
    );
    const el = this.content.document.querySelector("#screenshot-target");
    return TestUtils.screenshotArea(el, this.content);
  });
}

async function test_moz_extension_svg_context_fill({
  addonId,
  isPrivileged,
  expectAllowed,
}) {
  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: { gecko: { id: addonId } },
    },
    isPrivileged,
    files: {
      "context-fill-fallback-red.svg": `
        <svg xmlns="http://www.w3.org/2000/svg" version="1.1"
             xmlns:xlink="http://www.w3.org/1999/xlink">
          <rect height="100%" width="100%" fill="context-fill red" />
        </svg>
      `,
      "page.html": `<!DOCTYPE html>
        <style>
          img {
            -moz-context-properties: fill;
            fill: green;
          }
        </style>
        <img id="screenshot-target" src="context-fill-fallback-red.svg"
             style="width: 100px; height: 100px;">
      `,
      "red-reference.html": createReferencePage("red"),
      "green-reference.html": createReferencePage("green"),
    },
  });

  await extension.startup();

  const extURL = extension.extension.baseURI.resolve("page.html");
  const extPage = await ExtensionTestUtils.loadContentPage(extURL, {
    extension,
  });
  const resultScreenshot = await screenshotPage(extPage);

  const expectedColor = expectAllowed ? "green" : "red";
  const extReferencePageURL = extension.extension.baseURI.resolve(
    `${expectedColor}-reference.html`
  );
  const extReferencePage = await ExtensionTestUtils.loadContentPage(
    extReferencePageURL,
    {
      extension,
    }
  );
  const referenceScreenshot = await screenshotPage(extReferencePage);

  equal(
    resultScreenshot,
    referenceScreenshot,
    `Context-fill should be ${
      expectAllowed ? "allowed" : "disallowed"
    } (resulting in ${expectedColor}) on "${addonId}" extension`
  );

  await extPage.close();
  await extReferencePage.close();
  await extension.unload();
}

add_task(async function setup() {
  // Make sure test extension with an id that starts with privileged is
  // recognized as signed with the privileged certificate.
  await AddonTestUtils.promiseStartupManager();
});

add_task(async function test_allowed_on_privileged_ext() {
  await test_moz_extension_svg_context_fill({
    addonId: "privileged-addon@mochi.test",
    isPrivileged: true,
    expectAllowed: true,
  });
});

add_task(async function test_disallowed_on_non_privileged_ext() {
  await test_moz_extension_svg_context_fill({
    addonId: "non-privileged-arbitrary-addon-id@mochi.test",
    isPrivileged: false,
    expectAllowed: false,
  });
});

add_task(async function test_allowed_on_privileged_ext_with_mozilla_id() {
  await test_moz_extension_svg_context_fill({
    addonId: "privileged-addon@mozilla.org",
    isPrivileged: true,
    expectAllowed: true,
  });

  await test_moz_extension_svg_context_fill({
    addonId: "privileged-addon@mozilla.com",
    isPrivileged: true,
    expectAllowed: true,
  });
});

// Bug 1710917 TODO: this test should be reversed (changing allowed to disallowed)
// along with removing the special case based on the addon id suffixes.
add_task(async function test_allowed_on_non_privileged_ext_with_mozilla_id() {
  await test_moz_extension_svg_context_fill({
    addonId: "non-privileged-addon@mozilla.org",
    isPrivileged: false,
    expectAllowed: true,
  });

  await test_moz_extension_svg_context_fill({
    addonId: "non-privileged-addon@mozilla.com",
    isPrivileged: false,
    expectAllowed: true,
  });
});
