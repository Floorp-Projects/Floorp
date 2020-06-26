"use strict";

const { L10nRegistry, FileSource } = ChromeUtils.import(
  "resource://gre/modules/L10nRegistry.jsm"
);
const { FileUtils } = ChromeUtils.import(
  "resource://gre/modules/FileUtils.jsm"
);
const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");

add_task(async function setup() {
  // Add a test .ftl file
  // (Note: other tests do this by patching L10nRegistry.load() but in
  // this test L10nRegistry is also loaded in the extension process --
  // just adding a new resource is easier than trying to patch
  // L10nRegistry in all processes)
  let dir = FileUtils.getDir("TmpD", ["l10ntest"]);
  dir.createUnique(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

  await OS.File.writeAtomic(
    OS.Path.join(dir.path, "test.ftl"),
    "key = value\n"
  );

  let target = Services.io.newFileURI(dir);
  let resProto = Services.io
    .getProtocolHandler("resource")
    .QueryInterface(Ci.nsIResProtocolHandler);

  resProto.setSubstitution("l10ntest", target);

  const source = new FileSource(
    "test",
    Services.locale.requestedLocales,
    "resource://l10ntest/"
  );
  L10nRegistry.registerSources([source]);
});

// Test that privileged extensions can use fluent to get strings from
// language packs (and that unprivileged extensions cannot)
add_task(async function test_l10n_dom() {
  const PAGE = `<!DOCTYPE html>
                <html><head>
                  <meta charset="utf8">
                  <link rel="localization" href="test.ftl"/>
                  <script src="page.js"></script>
                </head></html>`;

  function SCRIPT() {
    window.addEventListener(
      "load",
      async () => {
        try {
          await document.l10n.ready;
          let result = await document.l10n.formatValue("key");
          browser.test.sendMessage("result", { success: true, result });
        } catch (err) {
          browser.test.sendMessage("result", {
            success: false,
            msg: err.message,
          });
        }
      },
      { once: true }
    );
  }

  async function runTest(isPrivileged) {
    let extension = ExtensionTestUtils.loadExtension({
      background() {
        browser.test.sendMessage("ready", browser.runtime.getURL("page.html"));
      },
      manifest: {
        web_accessible_resources: ["page.html"],
      },
      isPrivileged,
      files: {
        "page.html": PAGE,
        "page.js": SCRIPT,
      },
    });

    await extension.startup();
    let url = await extension.awaitMessage("ready");
    let page = await ExtensionTestUtils.loadContentPage(url, { extension });
    let results = await extension.awaitMessage("result");
    await page.close();
    await extension.unload();

    return results;
  }

  // Everything should work for a privileged extension
  let results = await runTest(true);
  equal(results.success, true, "Translation succeeded in privileged extension");
  equal(results.result, "value", "Translation got the right value");

  // In an unprivleged extension, document.l10n shouldn't show up
  results = await runTest(false);
  equal(results.success, false, "Translation failed in unprivileged extension");
  equal(
    results.msg.endsWith("document.l10n is undefined"),
    true,
    "Translation failed due to missing document.l10n"
  );
});

add_task(async function test_l10n_manifest() {
  // Fluent can't be used to localize properties that the AddonManager
  // reads (see comment inside ExtensionData.parseManifest for details)
  // so test by localizing a property that only the extension framework
  // cares about: page_action.  This means we can only do this test from
  // browser.
  if (AppConstants.MOZ_BUILD_APP != "browser") {
    return;
  }

  AddonTestUtils.initializeURLPreloader();

  async function runTest(isPrivileged) {
    let extension = ExtensionTestUtils.loadExtension({
      isPrivileged,
      manifest: {
        l10n_resources: ["test.ftl"],
        page_action: {
          default_title: "__MSG_key__",
        },
      },
    });

    await extension.startup();
    let title = extension.extension.manifest.page_action.default_title;
    await extension.unload();
    return title;
  }

  let title = await runTest(true);
  equal(
    title,
    "value",
    "Manifest key localized with fluent in privileged extension"
  );
  title = await runTest(false);
  equal(
    title,
    "__MSG_key__",
    "Manifest key not localized in unprivileged extension"
  );
});
