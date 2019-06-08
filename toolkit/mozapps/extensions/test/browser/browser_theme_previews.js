const {AddonTestUtils} = ChromeUtils.import("resource://testing-common/AddonTestUtils.jsm");

let gManagerWindow;
let gCategoryUtilities;

// This test is testing the theme list at XUL about:addons. HTML about:addons's
// theme list is already tested in browser_html_list_view.js.
// The testThemeOrdering part of this test should be adapted when bug 1557768
// is fixed.
SpecialPowers.pushPrefEnv({
  set: [["extensions.htmlaboutaddons.enabled", false]],
});

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
  let theme = await AddonManager.getAddonByID(id);

  ok(theme.screenshots[0].url, "The add-on has a preview URL");
  let previewURL = theme.screenshots[0].url;

  let doc = gManagerWindow.document;
  let item = doc.querySelector(`richlistitem[value="${id}"]`);

  await BrowserTestUtils.waitForCondition(
    () => item.getAttribute("status") == "installed" && item.getAttribute("previewURL"),
    "Wait for the item to update to installed");

  is(item.getAttribute("previewURL"), previewURL, "The previewURL is set on the item");
  let image = doc.getAnonymousElementByAttribute(item, "anonid", "theme-screenshot");
  is(image.src, previewURL, "The previewURL is set on the image src");

  item.click();
  await wait_for_view_load(gManagerWindow);

  image = doc.querySelector(".theme-screenshot");
  is(image.src, previewURL, "The previewURL is set on the detail image src");

  // Now check that an add-on doesn't have a preview (bug 1519616).
  let extensionId = "extension@mochi.test";
  await AddonTestUtils.promiseInstallXPI({
    "manifest.json": {
      applications: {
        gecko: {id: extensionId},
      },
      manifest_version: 2,
      name: "anextension",
      description: "wow. such extension.",
      author: "Woof",
      version: "1",
    },
  });

  await gCategoryUtilities.openType("extension");

  // Go to the detail page.
  item = doc.querySelector(`richlistitem[value="${extensionId}"]`);
  item.click();
  await wait_for_view_load(gManagerWindow);

  // Check that the image has no src attribute.
  image = doc.querySelector(".theme-screenshot");
  ok(!image.src, "There is no preview for extensions");

  await close_manager(gManagerWindow);
  await theme.uninstall();

  let extension = await AddonManager.getAddonByID(extensionId);
  await extension.uninstall();
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

  // Ensure allow in private mode badge is hidden for themes.
  for (let item of list.itemChildren) {
    let badge = gManagerWindow.document.getAnonymousElementByAttribute(item, "anonid", "privateBrowsing");
    is_element_hidden(badge, `private browsing badge is hidden`);
  }

  await close_manager(gManagerWindow);
  for (let addon of await promiseAddonsByIDs(themeIds)) {
    await addon.uninstall();
  }
});
