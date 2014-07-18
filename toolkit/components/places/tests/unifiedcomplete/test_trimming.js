/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(function* test_untrimmed_secure_www() {
  do_log_info("Searching for untrimmed https://www entry");
  yield promiseAddVisits({ uri: NetUtil.newURI("https://www.mozilla.org/test/"),
                           transition: TRANSITION_TYPED });
  yield check_autocomplete({
    search: "mo",
    autofilled: "mozilla.org/",
    completed: "https://www.mozilla.org/"
  });
  yield cleanup();
});

add_task(function* test_untrimmed_secure_www_path() {
  do_log_info("Searching for untrimmed https://www entry with path");
  yield promiseAddVisits({ uri: NetUtil.newURI("https://www.mozilla.org/test/"),
                           transition: TRANSITION_TYPED });
  yield check_autocomplete({
    search: "mozilla.org/t",
    autofilled: "mozilla.org/test/",
    completed: "https://www.mozilla.org/test/"
  });
  yield cleanup();
});

add_task(function* test_untrimmed_secure() {
  do_log_info("Searching for untrimmed https:// entry");
  yield promiseAddVisits({ uri: NetUtil.newURI("https://mozilla.org/test/"),
                           transition: TRANSITION_TYPED });
  yield check_autocomplete({
    search: "mo",
    autofilled: "mozilla.org/",
    completed: "https://mozilla.org/"
  });
  yield cleanup();
});

add_task(function* test_untrimmed_secure_path() {
  do_log_info("Searching for untrimmed https:// entry with path");
  yield promiseAddVisits({ uri: NetUtil.newURI("https://mozilla.org/test/"),
                           transition: TRANSITION_TYPED });
  yield check_autocomplete({
    search: "mozilla.org/t",
    autofilled: "mozilla.org/test/",
    completed: "https://mozilla.org/test/"
  });
  yield cleanup();
});

add_task(function* test_untrimmed_www() {
  do_log_info("Searching for untrimmed http://www entry");
  yield promiseAddVisits({ uri: NetUtil.newURI("http://www.mozilla.org/test/"),
                           transition: TRANSITION_TYPED });
  yield check_autocomplete({
    search: "mo",
    autofilled: "mozilla.org/",
    completed: "www.mozilla.org/"
  });
  yield cleanup();
});

add_task(function* test_untrimmed_www_path() {
  do_log_info("Searching for untrimmed http://www entry with path");
  yield promiseAddVisits({ uri: NetUtil.newURI("http://www.mozilla.org/test/"),
                           transition: TRANSITION_TYPED });
  yield check_autocomplete({
    search: "mozilla.org/t",
    autofilled: "mozilla.org/test/",
    completed: "http://www.mozilla.org/test/"
  });
  yield cleanup();
});

add_task(function* test_untrimmed_ftp() {
  do_log_info("Searching for untrimmed ftp:// entry");
  yield promiseAddVisits({ uri: NetUtil.newURI("ftp://mozilla.org/test/"),
                           transition: TRANSITION_TYPED });
  yield check_autocomplete({
    search: "mo",
    autofilled: "mozilla.org/",
    completed: "ftp://mozilla.org/"
  });
  yield cleanup();
});

add_task(function* test_untrimmed_ftp_path() {
  do_log_info("Searching for untrimmed ftp:// entry with path");
  yield promiseAddVisits({ uri: NetUtil.newURI("ftp://mozilla.org/test/"),
                           transition: TRANSITION_TYPED });
  yield check_autocomplete({
    search: "mozilla.org/t",
    autofilled: "mozilla.org/test/",
    completed: "ftp://mozilla.org/test/"
  });
  yield cleanup();
});

add_task(function* test_priority_1() {
  do_log_info("Ensuring correct priority 1");
  yield promiseAddVisits([{ uri: NetUtil.newURI("https://www.mozilla.org/test/"),
                            transition: TRANSITION_TYPED },
                          { uri: NetUtil.newURI("https://mozilla.org/test/"),
                            transition: TRANSITION_TYPED },
                          { uri: NetUtil.newURI("ftp://mozilla.org/test/"),
                            transition: TRANSITION_TYPED },
                          { uri: NetUtil.newURI("http://www.mozilla.org/test/"),
                            transition: TRANSITION_TYPED },
                          { uri: NetUtil.newURI("http://mozilla.org/test/"),
                            transition: TRANSITION_TYPED }]);
  yield check_autocomplete({
    search: "mo",
    autofilled: "mozilla.org/",
    completed: "mozilla.org/"
  });
  yield cleanup();
});

add_task(function* test_periority_2() {
  do_log_info( "Ensuring correct priority 2");
  yield promiseAddVisits([{ uri: NetUtil.newURI("https://mozilla.org/test/"),
                            transition: TRANSITION_TYPED },
                          { uri: NetUtil.newURI("ftp://mozilla.org/test/"),
                            transition: TRANSITION_TYPED },
                          { uri: NetUtil.newURI("http://www.mozilla.org/test/"),
                            transition: TRANSITION_TYPED },
                          { uri: NetUtil.newURI("http://mozilla.org/test/"),
                            transition: TRANSITION_TYPED }]);
  yield check_autocomplete({
    search: "mo",
    autofilled: "mozilla.org/",
    completed: "mozilla.org/"
  });
  yield cleanup();
});

add_task(function* test_periority_3() {
  do_log_info("Ensuring correct priority 3");
  yield promiseAddVisits([{ uri: NetUtil.newURI("ftp://mozilla.org/test/"),
                            transition: TRANSITION_TYPED },
                          { uri: NetUtil.newURI("http://www.mozilla.org/test/"),
                            transition: TRANSITION_TYPED },
                          { uri: NetUtil.newURI("http://mozilla.org/test/"),
                            transition: TRANSITION_TYPED }]);
  yield check_autocomplete({
    search: "mo",
    autofilled: "mozilla.org/",
    completed: "mozilla.org/"
  });
  yield cleanup();
});

add_task(function* test_periority_4() {
  do_log_info("Ensuring correct priority 4");
  yield promiseAddVisits([{ uri: NetUtil.newURI("http://www.mozilla.org/test/"),
                            transition: TRANSITION_TYPED },
                          { uri: NetUtil.newURI("http://mozilla.org/test/"),
                            transition: TRANSITION_TYPED }]);
  yield check_autocomplete({
    search: "mo",
    autofilled: "mozilla.org/",
    completed: "mozilla.org/"
  });
  yield cleanup();
});

add_task(function* test_priority_5() {
  do_log_info("Ensuring correct priority 5");
  yield promiseAddVisits([{ uri: NetUtil.newURI("ftp://mozilla.org/test/"),
                            transition: TRANSITION_TYPED },
                          { uri: NetUtil.newURI("ftp://www.mozilla.org/test/"),
                            transition: TRANSITION_TYPED }]);
  yield check_autocomplete({
    search: "mo",
    autofilled: "mozilla.org/",
    completed: "ftp://mozilla.org/"
  });
  yield cleanup();
});

add_task(function* test_priority_6() {
  do_log_info("Ensuring correct priority 6");
  yield promiseAddVisits([{ uri: NetUtil.newURI("http://www.mozilla.org/test1/"),
                            transition: TRANSITION_TYPED },
                          { uri: NetUtil.newURI("http://www.mozilla.org/test2/"),
                            transition: TRANSITION_TYPED }]);
  yield check_autocomplete({
    search: "mo",
    autofilled: "mozilla.org/",
    completed: "www.mozilla.org/"
  });
  yield cleanup();
});

add_task(function* test_longer_domain() {
  do_log_info("Ensuring longer domain can't match");
  // The .co should be preferred, but should not get the https from the .com.
  // The .co domain must be added later to activate the trigger bug.
  yield promiseAddVisits([{ uri: NetUtil.newURI("https://mozilla.com/"),
                            transition: TRANSITION_TYPED },
                          { uri: NetUtil.newURI("http://mozilla.co/"),
                            transition: TRANSITION_TYPED },
                          { uri: NetUtil.newURI("http://mozilla.co/"),
                            transition: TRANSITION_TYPED }]);
  yield check_autocomplete({
    search: "mo",
    autofilled: "mozilla.co/",
    completed: "mozilla.co/"
  });

  yield cleanup();
});

add_task(function* test_escaped_chars() {
  do_log_info("Searching for URL with characters that are normally escaped");
  yield promiseAddVisits({ uri: NetUtil.newURI("https://www.mozilla.org/啊-test"),
                           transition: TRANSITION_TYPED });
  yield check_autocomplete({
    search: "https://www.mozilla.org/啊-test",
    autofilled: "https://www.mozilla.org/啊-test",
    completed: "https://www.mozilla.org/啊-test"
  });
  yield cleanup();
});

add_task(function* test_unsecure_secure() {
  do_log_info("Don't return unsecure URL when searching for secure ones");
  yield promiseAddVisits({ uri: NetUtil.newURI("http://test.moz.org/test/"),
                           transition: TRANSITION_TYPED });
  yield check_autocomplete({
    search: "https://test.moz.org/t",
    autofilled: "https://test.moz.org/test/",
    completed: "https://test.moz.org/test/"
  });
  yield cleanup();
});

add_task(function* test_unsecure_secure_domain() {
  do_log_info("Don't return unsecure domain when searching for secure ones");
  yield promiseAddVisits({ uri: NetUtil.newURI("http://test.moz.org/test/"),
                           transition: TRANSITION_TYPED });
  yield check_autocomplete({
    search: "https://test.moz",
    autofilled: "https://test.moz.org/",
    completed: "https://test.moz.org/"
  });
  yield cleanup();
});

add_task(function* test_untyped_www() {
  do_log_info("Untyped is not accounted for www");
  Services.prefs.setBoolPref("browser.urlbar.autoFill.typed", false);
  yield promiseAddVisits({ uri: NetUtil.newURI("http://www.moz.org/test/") });
  yield check_autocomplete({
    search: "mo",
    autofilled: "moz.org/",
    completed: "moz.org/"
  });
  yield cleanup();
});

add_task(function* test_untyped_ftp() {
  do_log_info("Untyped is not accounted for ftp");
  Services.prefs.setBoolPref("browser.urlbar.autoFill.typed", false);
  yield promiseAddVisits({ uri: NetUtil.newURI("ftp://moz.org/test/") });
  yield check_autocomplete({
    search: "mo",
    autofilled: "moz.org/",
    completed: "moz.org/"
  });
  yield cleanup();
});

add_task(function* test_untyped_secure() {
  do_log_info("Untyped is not accounted for https");
  Services.prefs.setBoolPref("browser.urlbar.autoFill.typed", false);
  yield promiseAddVisits({ uri: NetUtil.newURI("https://moz.org/test/") });
  yield check_autocomplete({
    search: "mo",
    autofilled: "moz.org/",
    completed: "moz.org/"
  });
  yield cleanup();
});

add_task(function* test_untyped_secure_www() {
  do_log_info("Untyped is not accounted for https://www");
  Services.prefs.setBoolPref("browser.urlbar.autoFill.typed", false);
  yield promiseAddVisits({ uri: NetUtil.newURI("https://www.moz.org/test/") });
  yield check_autocomplete({
    search: "mo",
    autofilled: "moz.org/",
    completed: "moz.org/"
  });
  yield cleanup();
});
