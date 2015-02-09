/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Inline should never return matches shorter than the search string, since
// that largely confuses completeDefaultIndex

add_autocomplete_test([
  "Do not autofill whitespaced entry 1",
  "mozilla.org ",
  "mozilla.org ",
  function () {
    PlacesTestUtils.addVisits({
      uri: NetUtil.newURI("http://mozilla.org/link/"),
      transition: TRANSITION_TYPED
    });
  }
]);

add_autocomplete_test([
  "Do not autofill whitespaced entry 2",
  "mozilla.org/ ",
  "mozilla.org/ ",
  function () {
    PlacesTestUtils.addVisits({
      uri: NetUtil.newURI("http://mozilla.org/link/"),
      transition: TRANSITION_TYPED
    });
  }
]);

add_autocomplete_test([
  "Do not autofill whitespaced entry 3",
  "mozilla.org/link ",
  "mozilla.org/link ",
  function () {
    PlacesTestUtils.addVisits({
      uri: NetUtil.newURI("http://mozilla.org/link/"),
      transition: TRANSITION_TYPED
    });
  }
]);

add_autocomplete_test([
  "Do not autofill whitespaced entry 4",
  "mozilla.org/link/ ",
  "mozilla.org/link/ ",
  function () {
    PlacesTestUtils.addVisits({
      uri: NetUtil.newURI("http://mozilla.org/link/"),
      transition: TRANSITION_TYPED
    });
  }
]);


add_autocomplete_test([
  "Do not autofill whitespaced entry 5",
  "moz illa ",
  "moz illa ",
  function () {
    PlacesTestUtils.addVisits({
      uri: NetUtil.newURI("http://mozilla.org/link/"),
      transition: TRANSITION_TYPED
    });
  }
]);

add_autocomplete_test([
  "Do not autofill whitespaced entry 6",
  " mozilla",
  " mozilla",
  function () {
    PlacesTestUtils.addVisits({
      uri: NetUtil.newURI("http://mozilla.org/link/"),
      transition: TRANSITION_TYPED
    });
  }
]);
