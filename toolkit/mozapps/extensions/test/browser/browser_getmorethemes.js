const MAIN_URL = "https://example.com/" + RELATIVE_DIR + "discovery.html";
const PREF_GETTHEMESURL = "extensions.getAddons.themes.browseURL";

add_task(function* getthemes_link_visible_on_appearance_when_pref_set() {
  yield SpecialPowers.pushPrefEnv({set: [[PREF_GETTHEMESURL, MAIN_URL]]});

  let aManager = yield open_manager("addons://list/extension");
  info("Testing theme discovery information");
  var button = aManager.document.querySelector("#getthemes-learnmore-link");
  is_element_hidden(button, "Thume discovery button should be hidden");

  info("Changing view to appearance");
  EventUtils.synthesizeMouseAtCenter(aManager.document.getElementById("category-theme"), { }, aManager);

  yield new Promise(resolve => wait_for_view_load(aManager, resolve));
  button = aManager.document.querySelector("#getthemes-learnmore-link");
  is_element_visible(button, "Theme discovery message should be visible");

  info("Clicking button to get more themes");
  let awaitNewTab = BrowserTestUtils.waitForNewTab(gBrowser, MAIN_URL);
  EventUtils.synthesizeMouseAtCenter(button, { }, aManager);
  yield awaitNewTab;

  is(gBrowser.currentURI.spec, Services.urlFormatter.formatURLPref(PREF_GETTHEMESURL), "Theme discovery URL should match");

  yield BrowserTestUtils.removeTab(gBrowser.selectedTab);
  yield close_manager(aManager);
});

add_task(function* getthemes_link_hidden_on_appearance_when_pref_cleared() {
  yield SpecialPowers.pushPrefEnv({set: [[PREF_GETTHEMESURL, ""]]});

  let aManager = yield open_manager("addons://list/extension");
  info("Testing theme discovery information");
  var button = aManager.document.querySelector("#getthemes-learnmore-link");
  is_element_hidden(button, "Thume discovery button should be hidden");

  info("Changing view to appearance");
  EventUtils.synthesizeMouseAtCenter(aManager.document.getElementById("category-theme"), { }, aManager);

  yield new Promise(resolve => wait_for_view_load(aManager, resolve));
  button = aManager.document.querySelector("#getthemes-learnmore-link");
  is_element_hidden(button, "Theme discovery message should be hidden");

  yield close_manager(aManager);
});
