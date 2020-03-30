/* eslint max-len: ["error", 80] */
let gProvider;

function dateHoursAgo(hours) {
  let date = new Date();
  date.setTime(date.getTime() - hours * 3600000);
  return date;
}

add_task(async function enableHtmlViews() {
  gProvider = new MockProvider();
  gProvider.createAddons([
    {
      id: "addon-today-2@mochi.test",
      name: "Updated today two",
      creator: { name: "The creator" },
      version: "3.3",
      type: "extension",
      updateDate: dateHoursAgo(6),
    },
    {
      id: "addon-today-3@mochi.test",
      name: "Updated today three",
      creator: { name: "The creator" },
      version: "3.3",
      type: "extension",
      updateDate: dateHoursAgo(9),
    },
    {
      id: "addon-today-1@mochi.test",
      name: "Updated today",
      creator: { name: "The creator" },
      version: "3.1",
      type: "extension",
      releaseNotesURI: "http://example.com/notes.txt",
      updateDate: dateHoursAgo(1),
    },
    {
      id: "addon-yesterday-1@mochi.test",
      name: "Updated yesterday one",
      creator: { name: "The creator" },
      version: "3.3",
      type: "extension",
      updateDate: dateHoursAgo(15),
    },
    {
      id: "addon-earlier@mochi.test",
      name: "Updated earlier",
      creator: { name: "The creator" },
      version: "3.3",
      type: "extension",
      updateDate: dateHoursAgo(49),
    },
    {
      id: "addon-yesterday-2@mochi.test",
      name: "Updated yesterday",
      creator: { name: "The creator" },
      version: "3.3",
      type: "extension",
      updateDate: dateHoursAgo(24),
    },
    {
      id: "addon-lastweek@mochi.test",
      name: "Updated last week",
      creator: { name: "The creator" },
      version: "3.3",
      type: "extension",
      updateDate: dateHoursAgo(192),
    },
  ]);
});

add_task(async function testRecentUpdatesList() {
  // Load extension view first so we can mock the startOfDay property.
  let win = await loadInitialView("extension");
  let doc = win.document;
  let categoryUtils = new CategoryUtilities(win.managerWindow);
  const RECENT_URL = "addons://updates/recent";
  let recentCat = categoryUtils.get("recent-updates");

  ok(recentCat.hidden, "Recent updates category is initially hidden");

  // Load the recent updates view.
  let loaded = waitForViewLoad(win);
  doc.querySelector('#page-options [action="view-recent-updates"]').click();
  await loaded;

  is(
    categoryUtils.getSelectedViewId(),
    RECENT_URL,
    "Recent updates is selected"
  );
  ok(!recentCat.hidden, "Recent updates category is now shown");

  // Find all the add-on ids.
  let list = doc.querySelector("addon-list");
  let addonsInOrder = () =>
    Array.from(list.querySelectorAll("addon-card"))
      .map(card => card.addon.id)
      .filter(id => id.endsWith("@mochi.test"));

  // Verify that the add-ons are in the right order.
  Assert.deepEqual(
    addonsInOrder(),
    [
      "addon-today-1@mochi.test",
      "addon-today-2@mochi.test",
      "addon-today-3@mochi.test",
      "addon-yesterday-1@mochi.test",
      "addon-yesterday-2@mochi.test",
    ],
    "The add-ons are in the right order"
  );

  info("Check that release notes are shown on the details page");
  let card = list.querySelector(
    'addon-card[addon-id="addon-today-1@mochi.test"]'
  );
  loaded = waitForViewLoad(win);
  card.querySelector('[action="expand"]').click();
  await loaded;

  card = doc.querySelector("addon-card");
  ok(card.expanded, "The card is expanded");
  ok(!card.details.tabGroup.hidden, "The tabs are shown");
  ok(
    !card.details.tabGroup.querySelector('[name="release-notes"]').hidden,
    "The release notes button is shown"
  );

  info("Go back to the recent updates view");
  loaded = waitForViewLoad(win);
  doc.querySelector('#page-options [action="view-recent-updates"]').click();
  await loaded;

  // Find the list again.
  list = doc.querySelector("addon-list");

  info("Install a new add-on, it should be first in the list");
  // Install a new extension.
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "New extension",
      applications: { gecko: { id: "new@mochi.test" } },
    },
    useAddonManager: "temporary",
  });
  let added = BrowserTestUtils.waitForEvent(list, "add");
  await extension.startup();
  await added;

  // The new extension should now be at the top of the list.
  Assert.deepEqual(
    addonsInOrder(),
    [
      "new@mochi.test",
      "addon-today-1@mochi.test",
      "addon-today-2@mochi.test",
      "addon-today-3@mochi.test",
      "addon-yesterday-1@mochi.test",
      "addon-yesterday-2@mochi.test",
    ],
    "The new add-on went to the top"
  );

  // Open the detail view for the new add-on.
  card = list.querySelector('addon-card[addon-id="new@mochi.test"]');
  loaded = waitForViewLoad(win);
  card.querySelector('[action="expand"]').click();
  await loaded;

  is(
    categoryUtils.getSelectedViewId(),
    "addons://list/extension",
    "The extensions category is selected"
  );

  await closeView(win);
  await extension.unload();
});
