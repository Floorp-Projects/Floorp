add_task(async function test_enabled() {
  // Test for bug 471903 to make sure searching in autocomplete can be turned on
  // and off. Also test bug 463535 for pref changing search.
  Services.prefs.setBoolPref("browser.urlbar.autoFill", false);

  let uri = NetUtil.newURI("http://url/0");
  await PlacesTestUtils.addVisits([ { uri, title: "title" } ]);

  info("plain search");
  await check_autocomplete({
    search: "url",
    matches: [ { uri, title: "title" } ]
  });

  info("search disabled");
  Services.prefs.setBoolPref("browser.urlbar.autocomplete.enabled", false);
  await check_autocomplete({
    search: "url",
    matches: [ ]
  });

  info("resume normal search");
  Services.prefs.setBoolPref("browser.urlbar.autocomplete.enabled", true);
  await check_autocomplete({
    search: "url",
    matches: [ { uri, title: "title" } ]
  });

  await cleanup();
});

add_task(async function test_linked_enabled_prefs() {
  // Initialize unified complete.
  Cc["@mozilla.org/autocomplete/search;1?name=unifiedcomplete"]
    .getService(Ci.mozIPlacesAutoComplete);

  let types = [ "history", "bookmark", "openpage", "searches" ];

  // Test the service keeps browser.urlbar.autocomplete.enabled synchronized
  // with browser.urlbar.suggest prefs.
  for (let type of types) {
    Services.prefs.setBoolPref("browser.urlbar.suggest." + type, true);
  }
  Assert.equal(Services.prefs.getBoolPref("browser.urlbar.autocomplete.enabled"), true);

  // Disable autocomplete and check all the suggest prefs are set to false.
  Services.prefs.setBoolPref("browser.urlbar.autocomplete.enabled", false);
  for (let type of types) {
    Assert.equal(Services.prefs.getBoolPref("browser.urlbar.suggest." + type), false);
  }

  // Setting even a single suggest pref to true should enable autocomplete.
  Services.prefs.setBoolPref("browser.urlbar.suggest.history", true);
  for (let type of types.filter(t => t != "history")) {
    Assert.equal(Services.prefs.getBoolPref("browser.urlbar.suggest." + type), false);
  }
  Assert.equal(Services.prefs.getBoolPref("browser.urlbar.autocomplete.enabled"), true);

  // Disable autocoplete again, then re-enable it and check suggest prefs
  // have been reset.
  Services.prefs.setBoolPref("browser.urlbar.autocomplete.enabled", false);
  Services.prefs.setBoolPref("browser.urlbar.autocomplete.enabled", true);
  for (let type of types.filter(t => t != "history")) {
    if (type == "searches") {
      Assert.equal(Services.prefs.getBoolPref("browser.urlbar.suggest." + type), false);
    } else {
      Assert.equal(Services.prefs.getBoolPref("browser.urlbar.suggest." + type), true);
    }
  }
});
