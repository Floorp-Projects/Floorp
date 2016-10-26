/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(function* test_no_slash() {
  do_print("Searching for host match without slash should match host");
  yield PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://file.org/test/"),
    transition: TRANSITION_TYPED
  }, {
    uri: NetUtil.newURI("file:///c:/test.html"),
    transition: TRANSITION_TYPED
  });
  yield check_autocomplete({
    search: "file",
    autofilled: "file.org/",
    completed: "file.org/"
  });
  yield cleanup();
});

add_task(function* test_w_slash() {
  do_print("Searching match with slash at the end should do nothing");
  yield PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://file.org/test/"),
    transition: TRANSITION_TYPED
  }, {
    uri: NetUtil.newURI("file:///c:/test.html"),
    transition: TRANSITION_TYPED
  });
  yield check_autocomplete({
    search: "file.org/",
    autofilled: "file.org/",
    completed: "file.org/"
  });
  yield cleanup();
});

add_task(function* test_middle() {
  do_print("Searching match with slash in the middle should match url");
  yield PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://file.org/test/"),
    transition: TRANSITION_TYPED
  }, {
    uri: NetUtil.newURI("file:///c:/test.html"),
    transition: TRANSITION_TYPED
  });
  yield check_autocomplete({
    search: "file.org/t",
    autofilled: "file.org/test/",
    completed: "http://file.org/test/"
  });
  yield cleanup();
});

add_task(function* test_nonhost() {
  do_print("Searching for non-host match without slash should not match url");
  yield PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("file:///c:/test.html"),
    transition: TRANSITION_TYPED
  });
  yield check_autocomplete({
    search: "file",
    autofilled: "file",
    completed: "file"
  });
  yield cleanup();
});
