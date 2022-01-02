/*
 * This test checks that the popupshowing event for input fields, which do not
 * have a dedicated contextmenu event, but use the global one (added by
 * editMenuOverlay.js, see bug 1693577) include a triggerNode.
 *
 * The search-input field of the browser-sidebar is one of the rare cases in
 * mozilla-central, which can be used to test this. There are a few more in
 * comm-central, which need the triggerNode information.
 */

add_task(async function test_search_input_popupshowing() {
  let sidebar = document.getElementById("sidebar");

  let loadPromise = BrowserTestUtils.waitForEvent(sidebar, "load", true);
  SidebarUI.toggle("viewBookmarksSidebar");
  await loadPromise;

  let inputField = sidebar.contentDocument.getElementById("search-box")
    .inputField;
  const popupshowing = BrowserTestUtils.waitForEvent(
    sidebar.contentWindow,
    "popupshowing"
  );

  EventUtils.synthesizeMouseAtCenter(
    inputField,
    {
      type: "contextmenu",
      button: 2,
    },
    sidebar.contentWindow
  );
  let popupshowingEvent = await popupshowing;

  Assert.equal(
    popupshowingEvent.target.triggerNode?.id,
    "search-box",
    "Popupshowing event for the search input includes triggernode."
  );

  const popup = popupshowingEvent.target;
  await BrowserTestUtils.waitForEvent(popup, "popupshown");

  const popuphidden = BrowserTestUtils.waitForEvent(popup, "popuphidden");
  popup.hidePopup();
  await popuphidden;

  SidebarUI.toggle("viewBookmarksSidebar");
});
