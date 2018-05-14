/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function test_untrimmed_secure_www() {
  info("Searching for untrimmed https://www entry");
  await PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("https://www.mozilla.org/test/"),
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
  });
  await check_autocomplete({
    search: "mo",
    autofilled: "mozilla.org/",
    completed: "http://www.mozilla.org/"
  });
  await cleanup();
});

add_task(async function test_untrimmed_www_path() {
  info("Searching for untrimmed http://www entry with path");
  await PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://www.mozilla.org/test/"),
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
  });
  await check_autocomplete({
    search: "mozilla.org/t",
    autofilled: "mozilla.org/test/",
    completed: "ftp://mozilla.org/test/"
  });
  await cleanup();
});

add_task(async function test_escaped_chars() {
  info("Searching for URL with characters that are normally escaped");
  await PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("https://www.mozilla.org/啊-test"),
  });
  await check_autocomplete({
    search: "https://www.mozilla.org/啊-test",
    autofilled: "https://www.mozilla.org/啊-test",
    completed: "https://www.mozilla.org/啊-test"
  });
  await cleanup();
});
