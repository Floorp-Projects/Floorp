/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// First do searches with typed behavior forced to false, so later tests will
// ensure autocomplete is able to dinamically switch behavior.

const FAVICON_HREF = NetUtil.newURI(do_get_file("../favicons/favicon-normal16.png")).spec;

add_task(async function test_domain() {
  info("Searching for domain should autoFill it");
  Services.prefs.setBoolPref("browser.urlbar.autoFill.typed", false);
  await PlacesTestUtils.addVisits(NetUtil.newURI("http://mozilla.org/link/"));
  await setFaviconForPage("http://mozilla.org/link/", FAVICON_HREF);
  await check_autocomplete({
    search: "moz",
    autofilled: "mozilla.org/",
    completed: "mozilla.org/",
    icon: "moz-anno:favicon:" + FAVICON_HREF
  });
  await cleanup();
});

add_task(async function test_url() {
  info("Searching for url should autoFill it");
  Services.prefs.setBoolPref("browser.urlbar.autoFill.typed", false);
  await PlacesTestUtils.addVisits(NetUtil.newURI("http://mozilla.org/link/"));
  await setFaviconForPage("http://mozilla.org/link/", FAVICON_HREF);
  await check_autocomplete({
    search: "mozilla.org/li",
    autofilled: "mozilla.org/link/",
    completed: "http://mozilla.org/link/",
    icon: "moz-anno:favicon:" + FAVICON_HREF
  });
  await cleanup();
});

// Now do searches with typed behavior forced to true.

add_task(async function test_untyped_domain() {
  info("Searching for non-typed domain should not autoFill it");
  await PlacesTestUtils.addVisits(NetUtil.newURI("http://mozilla.org/link/"));
  await check_autocomplete({
    search: "moz",
    autofilled: "moz",
    completed: "moz"
  });
  await cleanup();
});

add_task(async function test_typed_domain() {
  info("Searching for typed domain should autoFill it");
  await PlacesTestUtils.addVisits({ uri: NetUtil.newURI("http://mozilla.org/typed/"),
                           transition: TRANSITION_TYPED });
  await check_autocomplete({
    search: "moz",
    autofilled: "mozilla.org/",
    completed: "mozilla.org/"
  });
  await cleanup();
});

add_task(async function test_untyped_url() {
  info("Searching for non-typed url should not autoFill it");
  await PlacesTestUtils.addVisits(NetUtil.newURI("http://mozilla.org/link/"));
  await check_autocomplete({
    search: "mozilla.org/li",
    autofilled: "mozilla.org/li",
    completed: "mozilla.org/li"
  });
  await cleanup();
});

add_task(async function test_typed_url() {
  info("Searching for typed url should autoFill it");
  await PlacesTestUtils.addVisits({ uri: NetUtil.newURI("http://mozilla.org/link/"),
                           transition: TRANSITION_TYPED });
  await check_autocomplete({
    search: "mozilla.org/li",
    autofilled: "mozilla.org/link/",
    completed: "http://mozilla.org/link/"
  });
  await cleanup();
});
