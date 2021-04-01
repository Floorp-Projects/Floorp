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
  let popupShownPromise = new Promise(resolve => {
    sidebar.contentWindow.addEventListener(
      "popupshowing",
      e => {
        Assert.equal(
          e.target.triggerNode?.id,
          "search-box",
          "Popupshowing event for the search input includes triggernode."
        );
        resolve();
      },
      { once: true }
    );
  });

  EventUtils.synthesizeMouseAtCenter(
    inputField,
    {
      type: "contextmenu",
      button: 2,
    },
    sidebar.contentWindow
  );
  await popupShownPromise;
  SidebarUI.toggle("viewBookmarksSidebar");
});
