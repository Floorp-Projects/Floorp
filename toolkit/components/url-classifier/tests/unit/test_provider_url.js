ChromeUtils.import("resource://testing-common/AppInfo.jsm", this);

function updateVersion(version) {
  updateAppInfo({ version });
}

add_test(function test_provider_url() {
  let urls = [
    "browser.safebrowsing.provider.google.updateURL",
    "browser.safebrowsing.provider.google.gethashURL",
    "browser.safebrowsing.provider.mozilla.updateURL",
    "browser.safebrowsing.provider.mozilla.gethashURL",
  ];

  let versions = ["49.0", "49.0.1", "49.0a1", "49.0b1", "49.0esr", "49.0.1esr"];

  for (let version of versions) {
    for (let url of urls) {
      updateVersion(version);
      let value = Services.urlFormatter.formatURLPref(url);
      Assert.notEqual(value.indexOf("&appver=49.0&"), -1);
    }
  }

  run_next_test();
});
