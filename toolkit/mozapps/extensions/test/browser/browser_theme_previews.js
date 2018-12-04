const {AddonTestUtils} = ChromeUtils.import("resource://testing-common/AddonTestUtils.jsm", {});

let gManagerWindow;
let gCategoryUtilities;

registerCleanupFunction(() => {
  // AddonTestUtils with open_manager cause this reference to be maintained and creates a leak.
  gManagerWindow = null;
});

function imageBufferFromDataURI(encodedImageData) {
  let decodedImageData = atob(encodedImageData);
  return Uint8Array.from(decodedImageData, byte => byte.charCodeAt(0)).buffer;
}
const img = "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVQImWNgYGBgAAAABQABh6FO1AAAAABJRU5ErkJggg==";
const imageBuffer = imageBufferFromDataURI(img);

const id = "theme@mochi.test";

AddonTestUtils.initMochitest(this);

function getThemeData(_id = id, manifest = {}, files = {}) {
  return {
    "manifest.json": {
      applications: {
        gecko: {id: _id},
      },
      manifest_version: 2,
      name: "atheme",
      description: "wow. such theme.",
      author: "Pixel Pusher",
      version: "1",
      theme: {},
      ...manifest,
    },
    "preview.png": imageBuffer,
    ...files,
  };
}

add_task(async function testThemePreviewShown() {
  gManagerWindow = await open_manager(null);
  gCategoryUtilities = new CategoryUtilities(gManagerWindow);

  await gCategoryUtilities.openType("theme");

  await AddonTestUtils.promiseInstallXPI(getThemeData());
  let addon = await AddonManager.getAddonByID(id);

  ok(addon.screenshots[0].url, "The add-on has a preview URL");
  let previewURL = addon.screenshots[0].url;

  let doc = gManagerWindow.document;
  let item = doc.querySelector(`richlistitem[value="${id}"]`);

  await BrowserTestUtils.waitForCondition(
    () => item.getAttribute("status") == "installed",
    "Wait for the item to update to installed");

  is(item.getAttribute("previewURL"), previewURL, "The previewURL is set on the item");
  let image = doc.getAnonymousElementByAttribute(item, "anonid", "theme-screenshot");
  is(image.src, previewURL, "The previewURL is set on the image src");

  item.click();
  await wait_for_view_load(gManagerWindow);

  image = doc.querySelector(".theme-screenshot");
  is(image.src, previewURL, "The previewURL is set on the detail image src");

  await close_manager(gManagerWindow);
  await addon.uninstall();
});
