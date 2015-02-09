/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

add_autocomplete_test([
  "Searching for host match without slash should match host",
  "file",
  "file.org/",
  function () {
    PlacesTestUtils.addVisits({
      uri: NetUtil.newURI("http://file.org/test/"),
      transition: TRANSITION_TYPED
    }, {
      uri: NetUtil.newURI("file:///c:/test.html"),
      transition: TRANSITION_TYPED
    });
  },
]);

add_autocomplete_test([
  "Searching match with slash at the end should do nothing",
  "file.org/",
  "file.org/",
  function () {
    PlacesTestUtils.addVisits({
      uri: NetUtil.newURI("http://file.org/test/"),
      transition: TRANSITION_TYPED
    }, {
      uri: NetUtil.newURI("file:///c:/test.html"),
      transition: TRANSITION_TYPED
    });
  },
]);

add_autocomplete_test([
  "Searching match with slash in the middle should match url",
  "file.org/t",
  "file.org/test/",
  function () {
    PlacesTestUtils.addVisits({
      uri: NetUtil.newURI("http://file.org/test/"),
      transition: TRANSITION_TYPED
    }, {
      uri: NetUtil.newURI("file:///c:/test.html"),
      transition: TRANSITION_TYPED
    });
  },
]);

add_autocomplete_test([
  "Searching for non-host match without slash should not match url",
  "file",
  "file",
  function () {
    PlacesTestUtils.addVisits({
      uri: NetUtil.newURI("file:///c:/test.html"),
      transition: TRANSITION_TYPED
    });
  },
]);
