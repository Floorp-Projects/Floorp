/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */

"use strict";

Services.prefs.setBoolPref("extensions.blocklist.enabled", false);

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "1",
  "43"
);

const { AddonManager } = ChromeUtils.importESModule(
  "resource://gre/modules/AddonManager.sys.mjs"
);

const LEAVE_UUID_PREF = "extensions.webextensions.keepUuidOnUninstall";

const BASE64_R_PIXEL =
  "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVQIW2P4z8DwHwAFAAH/F1FwBgAAAABJRU5ErkJggg==";
const BASE64_G_PIXEL =
  "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVQIW2Ng+M/wHwAEAQH/7yMK/gAAAABJRU5ErkJggg==";
const BASE64_B_PIXEL =
  "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVQIW2NgYPj/HwADAgH/eL9GtQAAAABJRU5ErkJggg==";

const toArrayBuffer = b64data =>
  Uint8Array.from(atob(b64data), c => c.charCodeAt(0));
const IMAGE_RED = toArrayBuffer(BASE64_R_PIXEL).buffer;
const IMAGE_GREEN = toArrayBuffer(BASE64_G_PIXEL).buffer;
const IMAGE_BLUE = toArrayBuffer(BASE64_B_PIXEL).buffer;

const RGB_RED = "rgb(255, 0, 0)";
const RGB_GREEN = "rgb(0, 255, 0)";
const RGB_BLUE = "rgb(0, 0, 255)";

const CSS_RED_BG = `body { background-color: ${RGB_RED}; }`;
const CSS_GREEN_BG = `body { background-color: ${RGB_GREEN}; }`;
const CSS_BLUE_BG = `body { background-color: ${RGB_BLUE}; }`;

const ADDON_ID = "test-cached-resources@test";

const manifest = {
  version: "1",
  browser_specific_settings: { gecko: { id: ADDON_ID } },
};

const files = {
  "extpage.html": `<!DOCTYPE html>
     <html>
       <head>
         <link rel="stylesheet" href="extpage.css">
       </head>
       <body>
         <img id="test-image" src="image.png">
       </body>
     </html>
  `,
  "other_extpage.html": `<!DOCTYPE html>
     <html>
       <body>
       </body>
     </html>
  `,
  "extpage.css": CSS_RED_BG,
  "image.png": IMAGE_RED,
};

const getBackgroundColor = () => {
  return this.content.getComputedStyle(this.content.document.body)
    .backgroundColor;
};

const hasCachedImage = imgUrl => {
  const { document } = this.content;

  const imageCache = Cc["@mozilla.org/image/tools;1"]
    .getService(Ci.imgITools)
    .getImgCacheForDocument(document);

  const imgCacheProps = imageCache.findEntryProperties(
    Services.io.newURI(imgUrl),
    document
  );

  // return true if the image was in the cache.
  return !!imgCacheProps;
};

const getImageColor = () => {
  const { document } = this.content;
  const img = document.querySelector("img#test-image");
  const canvas = document.createElement("canvas");
  canvas.width = 1;
  canvas.height = 1;
  const ctx = canvas.getContext("2d");
  ctx.drawImage(img, 0, 0); // Draw without scaling.
  const [r, g, b, a] = ctx.getImageData(0, 0, 1, 1).data;
  if (a < 1) {
    return `rgba(${r}, ${g}, ${b}, ${a})`;
  }
  return `rgb(${r}, ${g}, ${b})`;
};

async function assertBackgroundColor(page, color, message) {
  equal(
    await page.spawn([], getBackgroundColor),
    color,
    `Got the expected ${message}`
  );
}

async function assertImageColor(page, color, message) {
  equal(await page.spawn([], getImageColor), color, message);
}

async function assertImageCached(page, imageUrl, message) {
  ok(await page.spawn([imageUrl], hasCachedImage), message);
}

// This test verifies that cached css are cleared across addon upgrades and downgrades
// for permanently installed addon (See Bug 1746841).
add_task(async function test_cached_resources_cleared_across_addon_updates() {
  await AddonTestUtils.promiseStartupManager();

  const extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest,
    files,
  });

  await extension.startup();
  equal(
    extension.version,
    "1",
    "Got the expected version for the initial extension"
  );

  const url = extension.extension.baseURI.resolve("extpage.html");
  let page = await ExtensionTestUtils.loadContentPage(url);
  await assertBackgroundColor(
    page,
    RGB_RED,
    "background color (initial extension version)"
  );
  await assertImageColor(page, RGB_RED, "image (initial extension version)");

  info("Verify extension page css and image after addon upgrade");

  await extension.upgrade({
    useAddonManager: "permanent",
    manifest: {
      ...manifest,
      version: "2",
    },
    files: {
      ...files,
      "extpage.css": CSS_GREEN_BG,
      "image.png": IMAGE_GREEN,
    },
  });
  equal(
    extension.version,
    "2",
    "Got the expected version for the upgraded extension"
  );

  await page.loadURL(url);

  await assertBackgroundColor(
    page,
    RGB_GREEN,
    "background color (upgraded extension version)"
  );
  await assertImageColor(page, RGB_GREEN, "image (upgraded extension version)");

  info("Verify extension page css and image after addon downgrade");

  await extension.upgrade({
    useAddonManager: "permanent",
    manifest,
    files,
  });
  equal(
    extension.version,
    "1",
    "Got the expected version for the downgraded extension"
  );

  await page.loadURL(url);

  await assertBackgroundColor(
    page,
    RGB_RED,
    "background color (downgraded extension version)"
  );
  await assertImageColor(
    page,
    RGB_RED,
    "image color (downgraded extension version)"
  );

  await page.close();
  await extension.unload();
  await AddonTestUtils.promiseShutdownManager();
});

// This test verifies that cached css are cleared if we are installing a new
// extension and we did not clear the cache for a previous one with the same uuid
// when it was uninstalled (See Bug 1746841).
add_task(async function test_cached_resources_cleared_on_addon_install() {
  // Make sure the test addon installed without an AddonManager addon wrapper
  // and the ones installed right after that using the AddonManager will share
  // the same uuid (and so also the same moz-extension resource urls).
  Services.prefs.setBoolPref(LEAVE_UUID_PREF, true);
  registerCleanupFunction(() => Services.prefs.clearUserPref(LEAVE_UUID_PREF));

  await AddonTestUtils.promiseStartupManager();

  const nonAOMExtension = ExtensionTestUtils.loadExtension({
    manifest,
    files: {
      ...files,
      // Override css with a different color from the one expected
      // later in this test case.
      "extpage.css": CSS_BLUE_BG,
      "image.png": IMAGE_BLUE,
    },
  });

  await nonAOMExtension.startup();
  equal(
    await AddonManager.getAddonByID(ADDON_ID),
    null,
    "No AOM addon wrapper found as expected"
  );
  let url = nonAOMExtension.extension.baseURI.resolve("extpage.html");
  let page = await ExtensionTestUtils.loadContentPage(url);
  await assertBackgroundColor(
    page,
    RGB_BLUE,
    "background color (addon installed without uninstall observer)"
  );
  await assertImageColor(
    page,
    RGB_BLUE,
    "image (addon uninstalled without clearing cache)"
  );

  // NOTE: unloading a test extension that does not have an AddonManager addon wrapper
  // does not trigger the uninstall observer, and this is what this test needs to make
  // sure that if the cached resources were not cleared on uninstall, then we will still
  // clear it when a newly installed addon is installed even if the two extensions
  // are sharing the same addon uuid (and so also the same moz-extension resource urls).
  await nonAOMExtension.unload();

  const extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest,
    files,
  });

  await extension.startup();
  await page.loadURL(url);

  await assertBackgroundColor(
    page,
    RGB_RED,
    "background color (newly installed addon, same addon id)"
  );
  await assertImageColor(
    page,
    RGB_RED,
    "image (newly installed addon, same addon id)"
  );

  await page.close();
  await extension.unload();
  await AddonTestUtils.promiseShutdownManager();
});

// This test verifies that reloading a temporarily installed addon after
// changing a css file cached in a previous run clears the previously
// cached css and uses the new one changed on disk (See Bug 1746841).
add_task(
  async function test_cached_resources_cleared_on_temporary_addon_reload() {
    await AddonTestUtils.promiseStartupManager();

    const xpi = AddonTestUtils.createTempWebExtensionFile({
      manifest,
      files,
    });

    // This temporary directory is going to be removed from the
    // cleanup function, but also make it unique as we do for the
    // other temporary files (e.g. like getTemporaryFile as defined
    // in XPInstall.jsm).
    const random = Math.round(Math.random() * 36 ** 3).toString(36);
    const tmpDirName = `xpcshelltest_unpacked_addons_${random}`;
    let tmpExtPath = FileUtils.getDir("TmpD", [tmpDirName]);
    tmpExtPath.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
    registerCleanupFunction(() => {
      tmpExtPath.remove(true);
    });

    // Unpacking the xpi file into the temporary directory.
    const extDir = await AddonTestUtils.manuallyInstall(
      xpi,
      tmpExtPath,
      null,
      /* unpacked */ true
    );

    let extension = ExtensionTestUtils.expectExtension(ADDON_ID);
    await AddonManager.installTemporaryAddon(extDir);
    await extension.awaitStartup();

    equal(
      extension.version,
      "1",
      "Got the expected version for the initial extension"
    );

    const url = extension.extension.baseURI.resolve("extpage.html");
    let page = await ExtensionTestUtils.loadContentPage(url);
    await assertBackgroundColor(
      page,
      RGB_RED,
      "background color (initial extension version)"
    );
    await assertImageColor(page, RGB_RED, "image (initial extension version)");

    info("Verify updated extension page css and image after addon reload");

    const targetCSSFile = extDir.clone();
    targetCSSFile.append("extpage.css");
    ok(
      targetCSSFile.exists(),
      `Found the ${targetCSSFile.path} target file on disk`
    );
    await IOUtils.writeUTF8(targetCSSFile.path, CSS_GREEN_BG);

    const targetPNGFile = extDir.clone();
    targetPNGFile.append("image.png");
    ok(
      targetPNGFile.exists(),
      `Found the ${targetPNGFile.path} target file on disk`
    );
    await IOUtils.write(targetPNGFile.path, toArrayBuffer(BASE64_G_PIXEL));

    const addon = await AddonManager.getAddonByID(ADDON_ID);
    ok(addon, "Got an AddonWrapper for the test extension");
    await addon.reload();

    await page.loadURL(url);

    await assertBackgroundColor(
      page,
      RGB_GREEN,
      "background (updated files on disk)"
    );
    await assertImageColor(page, RGB_GREEN, "image (updated files on disk)");

    await page.close();
    await addon.uninstall();
    await AddonTestUtils.promiseShutdownManager();
  }
);

// This test verifies that cached images are not cleared between
// permanently installed addon reloads.
add_task(async function test_cached_image_kept_on_permanent_addon_restarts() {
  await AddonTestUtils.promiseStartupManager();
  const extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest,
    files,
  });

  await extension.startup();

  equal(
    extension.version,
    "1",
    "Got the expected version for the initial extension"
  );

  const imageUrl = extension.extension.baseURI.resolve("image.png");
  const url = extension.extension.baseURI.resolve("extpage.html");

  let page = await ExtensionTestUtils.loadContentPage(url);
  await assertBackgroundColor(
    page,
    RGB_RED,
    "background color (first startup)"
  );
  await assertImageColor(page, RGB_RED, "image (first startup)");
  await assertImageCached(page, imageUrl, "image cached (first startup)");

  info("Reload the AddonManager to simulate browser restart");
  extension.setRestarting();
  await AddonTestUtils.promiseRestartManager();
  await extension.awaitStartup();

  await page.loadURL(extension.extension.baseURI.resolve("other_extpage.html"));
  await assertImageCached(
    page,
    imageUrl,
    "image still cached after AddonManager restart"
  );

  await page.close();
  await extension.unload();
  await AddonTestUtils.promiseShutdownManager();
});
