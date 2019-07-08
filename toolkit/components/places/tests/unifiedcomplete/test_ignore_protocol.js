/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test bug 424509 to make sure searching for "h" doesn't match "http" of urls.
 */

add_task(async function test_escape() {
  Services.prefs.setBoolPref("browser.urlbar.autoFill", false);

  let uri1 = NetUtil.newURI("http://site/");
  let uri2 = NetUtil.newURI("http://happytimes/");
  await PlacesTestUtils.addVisits([
    { uri: uri1, title: "title" },
    { uri: uri2, title: "title" },
  ]);

  info("Searching for h matches site and not http://");
  await check_autocomplete({
    search: "h",
    matches: [{ uri: uri2, title: "title" }],
  });

  await cleanup();
});
