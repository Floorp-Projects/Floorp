/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Bug 593535 - Failure to download extension causes about:addons to list the
// addon with no way to restart the download

const PREF_GETADDONS_GETSEARCHRESULTS = "extensions.getAddons.search.url";
const SEARCH_URL = TESTROOT + "browser_bug593535.xml";
const QUERY = "NOTFOUND";

var gManagerWindow;

function test() {
  waitForExplicitFinish();

  // Turn on searching for this test
  Services.prefs.setIntPref(PREF_SEARCH_MAXRESULTS, 15);

  open_manager("addons://list/extension", function(aWindow) {
    gManagerWindow = aWindow;
    run_next_test();
  });
}

function end_test() {
  close_manager(gManagerWindow, function() {
    AddonManager.getAllInstalls(function(aInstallsList) {
      for (var install of aInstallsList) {
        var sourceURI = install.sourceURI.spec;
        if (sourceURI.match(/^http:\/\/example\.com\/(.+)\.xpi$/) != null)
          install.cancel();
      }

      finish();
    });
  });
}

function search(aQuery, aCallback) {
  // Point search to the correct xml test file
  Services.prefs.setCharPref(PREF_GETADDONS_GETSEARCHRESULTS, SEARCH_URL);

  var searchBox = gManagerWindow.document.getElementById("header-search");
  searchBox.value = aQuery;

  EventUtils.synthesizeMouseAtCenter(searchBox, { }, gManagerWindow);
  EventUtils.synthesizeKey("VK_RETURN", { }, gManagerWindow);

  wait_for_view_load(gManagerWindow, function() {
    var remoteFilter = gManagerWindow.document.getElementById("search-filter-remote");
    EventUtils.synthesizeMouseAtCenter(remoteFilter, { }, gManagerWindow);

    aCallback();
  });
}

function get_addon_item(aName) {
  var id = aName + "@tests.mozilla.org";
  var list = gManagerWindow.document.getElementById("search-list");
  var rows = list.getElementsByTagName("richlistitem");
  for (let row of rows) {
    if (row.mAddon && row.mAddon.id == id)
      return row;
  }

  return null;
}

function get_install_button(aItem) {
  isnot(aItem, null, "Item should not be null when checking state of install button");
  var installStatus = getAnonymousElementByAttribute(aItem, "anonid", "install-status");
  return getAnonymousElementByAttribute(installStatus, "anonid", "install-remote-btn");
}


function getAnonymousElementByAttribute(aElement, aName, aValue) {
  return gManagerWindow.document.getAnonymousElementByAttribute(aElement,
                                                                aName,
                                                                aValue);
}



// Tests that a failed install for a remote add-on will ask to retry the install
add_test(function() {
  var remoteItem;

  var listener = {
    onDownloadFailed(aInstall) {
      aInstall.removeListener(this);
      ok(true, "Install failed as expected");

      executeSoon(function() {
        is(remoteItem.getAttribute("notification"), "warning", "Item should have notification attribute set to 'warning'");
        is_element_visible(remoteItem._warning, "Warning text should be visible");
        is(remoteItem._warning.textContent, "There was an error downloading NOTFOUND.", "Warning should show correct message");
        is_element_visible(remoteItem._warningLink, "Retry button should be visible");
        run_next_test();
      });
    },

    onInstallEnded() {
      ok(false, "Install should have failed");
    }
  }

  search(QUERY, function() {
    var list = gManagerWindow.document.getElementById("search-list");
    remoteItem = get_addon_item("notfound1");
    list.ensureElementIsVisible(remoteItem);

    remoteItem.mAddon.install.addListener(listener);

    var installBtn = get_install_button(remoteItem);
    EventUtils.synthesizeMouseAtCenter(installBtn, { }, gManagerWindow);
  });
});
