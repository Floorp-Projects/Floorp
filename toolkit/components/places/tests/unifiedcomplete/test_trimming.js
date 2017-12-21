/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function test_untrimmed_secure_www() {
  info("Searching for untrimmed https://www entry");
  await PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("https://www.mozilla.org/test/"),
    transition: TRANSITION_TYPED
  });
  await check_autocomplete({
    search: "mo",
    autofilled: "mozilla.org/",
    completed: "https://www.mozilla.org/"
  });
  await cleanup();
});

add_task(async function test_untrimmed_secure_www_path() {
  info("Searching for untrimmed https://www entry with path");
  await PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("https://www.mozilla.org/test/"),
    transition: TRANSITION_TYPED
  });
  await check_autocomplete({
    search: "mozilla.org/t",
    autofilled: "mozilla.org/test/",
    completed: "https://www.mozilla.org/test/"
  });
  await cleanup();
});

add_task(async function test_untrimmed_secure() {
  info("Searching for untrimmed https:// entry");
  await PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("https://mozilla.org/test/"),
    transition: TRANSITION_TYPED
  });
  await check_autocomplete({
    search: "mo",
    autofilled: "mozilla.org/",
    completed: "https://mozilla.org/"
  });
  await cleanup();
});

add_task(async function test_untrimmed_secure_path() {
  info("Searching for untrimmed https:// entry with path");
  await PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("https://mozilla.org/test/"),
    transition: TRANSITION_TYPED
  });
  await check_autocomplete({
    search: "mozilla.org/t",
    autofilled: "mozilla.org/test/",
    completed: "https://mozilla.org/test/"
  });
  await cleanup();
});

add_task(async function test_untrimmed_www() {
  info("Searching for untrimmed http://www entry");
  await PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://www.mozilla.org/test/"),
    transition: TRANSITION_TYPED
  });
  await check_autocomplete({
    search: "mo",
    autofilled: "mozilla.org/",
    completed: "www.mozilla.org/"
  });
  await cleanup();
});

add_task(async function test_untrimmed_www_path() {
  info("Searching for untrimmed http://www entry with path");
  await PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://www.mozilla.org/test/"),
    transition: TRANSITION_TYPED
  });
  await check_autocomplete({
    search: "mozilla.org/t",
    autofilled: "mozilla.org/test/",
    completed: "http://www.mozilla.org/test/"
  });
  await cleanup();
});

add_task(async function test_untrimmed_ftp() {
  info("Searching for untrimmed ftp:// entry");
  await PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("ftp://mozilla.org/test/"),
    transition: TRANSITION_TYPED
  });
  await check_autocomplete({
    search: "mo",
    autofilled: "mozilla.org/",
    completed: "ftp://mozilla.org/"
  });
  await cleanup();
});

add_task(async function test_untrimmed_ftp_path() {
  info("Searching for untrimmed ftp:// entry with path");
  await PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("ftp://mozilla.org/test/"),
    transition: TRANSITION_TYPED
  });
  await check_autocomplete({
    search: "mozilla.org/t",
    autofilled: "mozilla.org/test/",
    completed: "ftp://mozilla.org/test/"
  });
  await cleanup();
});

add_task(async function test_priority_1() {
  info("Ensuring correct priority 1");
  await PlacesTestUtils.addVisits([
    { uri: NetUtil.newURI("https://www.mozilla.org/test/"), transition: TRANSITION_TYPED },
    { uri: NetUtil.newURI("https://mozilla.org/test/"), transition: TRANSITION_TYPED },
    { uri: NetUtil.newURI("ftp://mozilla.org/test/"), transition: TRANSITION_TYPED },
    { uri: NetUtil.newURI("http://www.mozilla.org/test/"), transition: TRANSITION_TYPED },
    { uri: NetUtil.newURI("http://mozilla.org/test/"), transition: TRANSITION_TYPED }
  ]);
  await check_autocomplete({
    search: "mo",
    autofilled: "mozilla.org/",
    completed: "mozilla.org/"
  });
  await cleanup();
});

add_task(async function test_priority_2() {
  info( "Ensuring correct priority 2");
  await PlacesTestUtils.addVisits([
    { uri: NetUtil.newURI("https://mozilla.org/test/"), transition: TRANSITION_TYPED },
    { uri: NetUtil.newURI("ftp://mozilla.org/test/"), transition: TRANSITION_TYPED },
    { uri: NetUtil.newURI("http://www.mozilla.org/test/"), transition: TRANSITION_TYPED },
    { uri: NetUtil.newURI("http://mozilla.org/test/"), transition: TRANSITION_TYPED }
  ]);
  await check_autocomplete({
    search: "mo",
    autofilled: "mozilla.org/",
    completed: "mozilla.org/"
  });
  await cleanup();
});

add_task(async function test_priority_3() {
  info("Ensuring correct priority 3");
  await PlacesTestUtils.addVisits([
    { uri: NetUtil.newURI("ftp://mozilla.org/test/"), transition: TRANSITION_TYPED },
    { uri: NetUtil.newURI("http://www.mozilla.org/test/"), transition: TRANSITION_TYPED },
    { uri: NetUtil.newURI("http://mozilla.org/test/"), transition: TRANSITION_TYPED }
  ]);
  await check_autocomplete({
    search: "mo",
    autofilled: "mozilla.org/",
    completed: "mozilla.org/"
  });
  await cleanup();
});

add_task(async function test_priority_4() {
  info("Ensuring correct priority 4");
  await PlacesTestUtils.addVisits([
    { uri: NetUtil.newURI("http://www.mozilla.org/test/"), transition: TRANSITION_TYPED },
    { uri: NetUtil.newURI("http://mozilla.org/test/"), transition: TRANSITION_TYPED }
  ]);
  await check_autocomplete({
    search: "mo",
    autofilled: "mozilla.org/",
    completed: "www.mozilla.org/"
  });
  await cleanup();
});

add_task(async function test_priority_5() {
  info("Ensuring correct priority 5");
  await PlacesTestUtils.addVisits([
    { uri: NetUtil.newURI("ftp://mozilla.org/test/"), transition: TRANSITION_TYPED },
    { uri: NetUtil.newURI("ftp://www.mozilla.org/test/"), transition: TRANSITION_TYPED }
  ]);
  await check_autocomplete({
    search: "mo",
    autofilled: "mozilla.org/",
    completed: "ftp://mozilla.org/"
  });
  await cleanup();
});

add_task(async function test_priority_6() {
  info("Ensuring correct priority 6");
  await PlacesTestUtils.addVisits([
    { uri: NetUtil.newURI("http://www.mozilla.org/test1/"), transition: TRANSITION_TYPED },
    { uri: NetUtil.newURI("http://www.mozilla.org/test2/"), transition: TRANSITION_TYPED }
  ]);
  await check_autocomplete({
    search: "mo",
    autofilled: "mozilla.org/",
    completed: "www.mozilla.org/"
  });
  await cleanup();
});

add_task(async function test_longer_domain() {
  info("Ensuring longer domain can't match");
  // The .co should be preferred, but should not get the https from the .com.
  // The .co domain must be added later to activate the trigger bug.
  await PlacesTestUtils.addVisits([
    { uri: NetUtil.newURI("https://mozilla.com/"), transition: TRANSITION_TYPED },
    { uri: NetUtil.newURI("http://mozilla.co/"), transition: TRANSITION_TYPED },
    { uri: NetUtil.newURI("http://mozilla.co/"), transition: TRANSITION_TYPED }
  ]);
  await check_autocomplete({
    search: "mo",
    autofilled: "mozilla.co/",
    completed: "mozilla.co/"
  });

  await cleanup();
});

add_task(async function test_escaped_chars() {
  info("Searching for URL with characters that are normally escaped");
  await PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("https://www.mozilla.org/啊-test"),
    transition: TRANSITION_TYPED
  });
  await check_autocomplete({
    search: "https://www.mozilla.org/啊-test",
    autofilled: "https://www.mozilla.org/啊-test",
    completed: "https://www.mozilla.org/啊-test"
  });
  await cleanup();
});

add_task(async function test_unsecure_secure() {
  info("Don't return unsecure URL when searching for secure ones");
  await PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://test.moz.org/test/"),
    transition: TRANSITION_TYPED
  });
  await check_autocomplete({
    search: "https://test.moz.org/t",
    autofilled: "https://test.moz.org/test/",
    completed: "https://test.moz.org/test/"
  });
  await cleanup();
});

add_task(async function test_unsecure_secure_domain() {
  info("Don't return unsecure domain when searching for secure ones");
  await PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://test.moz.org/test/"),
    transition: TRANSITION_TYPED
  });
  await check_autocomplete({
    search: "https://test.moz",
    autofilled: "https://test.moz.org/",
    completed: "https://test.moz.org/"
  });
  await cleanup();
});

add_task(async function test_untyped_www() {
  info("Untyped is not accounted for www");
  Services.prefs.setBoolPref("browser.urlbar.autoFill.typed", false);
  await PlacesTestUtils.addVisits({ uri: NetUtil.newURI("http://www.moz.org/test/") });
  await check_autocomplete({
    search: "mo",
    autofilled: "moz.org/",
    completed: "moz.org/"
  });
  await cleanup();
});

add_task(async function test_untyped_ftp() {
  info("Untyped is not accounted for ftp");
  Services.prefs.setBoolPref("browser.urlbar.autoFill.typed", false);
  await PlacesTestUtils.addVisits({ uri: NetUtil.newURI("ftp://moz.org/test/") });
  await check_autocomplete({
    search: "mo",
    autofilled: "moz.org/",
    completed: "moz.org/"
  });
  await cleanup();
});

add_task(async function test_untyped_secure() {
  info("Untyped is not accounted for https");
  Services.prefs.setBoolPref("browser.urlbar.autoFill.typed", false);
  await PlacesTestUtils.addVisits({ uri: NetUtil.newURI("https://moz.org/test/") });
  await check_autocomplete({
    search: "mo",
    autofilled: "moz.org/",
    completed: "moz.org/"
  });
  await cleanup();
});

add_task(async function test_untyped_secure_www() {
  info("Untyped is not accounted for https://www");
  Services.prefs.setBoolPref("browser.urlbar.autoFill.typed", false);
  await PlacesTestUtils.addVisits({ uri: NetUtil.newURI("https://www.moz.org/test/") });
  await check_autocomplete({
    search: "mo",
    autofilled: "moz.org/",
    completed: "moz.org/"
  });
  await cleanup();
});
