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

async function init(startPage) {
  gManagerWindow = await open_manager(null);
  gCategoryUtilities = new CategoryUtilities(gManagerWindow);
  return gCategoryUtilities.openType(startPage);
}

add_task(async function testThemePreviewShown() {
  await init("theme");

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

add_task(async function testThemeOrdering() {
  // Install themes before loading the manager, if it's open they'll sort by install date.
  let themeId = id => id + "@mochi.test";
  let themeIds = [themeId(5), themeId(6), themeId(7), themeId(8)];
  await AddonTestUtils.promiseInstallXPI(getThemeData(themeId(6), {name: "BBB"}));
  await AddonTestUtils.promiseInstallXPI(getThemeData(themeId(7), {name: "CCC"}));
  await AddonTestUtils.promiseInstallXPI(getThemeData(themeId(5), {name: "AAA"}, {previewURL: ""}));
  await AddonTestUtils.promiseInstallXPI(getThemeData(themeId(8), {name: "DDD"}));

  // Enable a theme to make sure it's first.
  let addon = await AddonManager.getAddonByID(themeId(8));
  addon.enable();

  // Load themes now that the extensions are setup.
  await init("theme");

  // Find the order of ids for the ones we installed.
  let list = gManagerWindow.document.getElementById("addon-list");
  let idOrder = list.itemChildren
    .map(row => row.getAttribute("value"))
    .filter(id => themeIds.includes(id));

  // Check the order.
  Assert.deepEqual(
    idOrder,
    [
      themeId(8), // The active theme first.
      themeId(6), themeId(7), // With previews, ordered by name.
      themeId(5), // The theme without a preview last.
    ],
    "Themes are ordered by enabled, previews, then name");

  await close_manager(gManagerWindow);
  for (let addon of await promiseAddonsByIDs(themeIds)) {
    await addon.uninstall();
  }
});
