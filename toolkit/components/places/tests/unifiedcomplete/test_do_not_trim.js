/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Inline should never return matches shorter than the search string, since
// that largely confuses completeDefaultIndex

add_task(async function test_not_autofill_ws_1() {
  info("Do not autofill whitespaced entry 1");
  await PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://mozilla.org/link/"),
    transition: TRANSITION_TYPED
  });
  await check_autocomplete({
    search: "mozilla.org ",
    autofilled: "mozilla.org ",
    completed: "mozilla.org "
  });
  await cleanup();
});

add_task(async function test_not_autofill_ws_2() {
  info("Do not autofill whitespaced entry 2");
  await PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://mozilla.org/link/"),
    transition: TRANSITION_TYPED
  });
  await check_autocomplete({
    search: "mozilla.org/ ",
    autofilled: "mozilla.org/ ",
    completed: "mozilla.org/ "
  });
  await cleanup();
});

add_task(async function test_not_autofill_ws_3() {
  info("Do not autofill whitespaced entry 3");
  await PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://mozilla.org/link/"),
    transition: TRANSITION_TYPED
  });
  await check_autocomplete({
    search: "mozilla.org/link ",
    autofilled: "mozilla.org/link ",
    completed: "mozilla.org/link "
  });
  await cleanup();
});

add_task(async function test_not_autofill_ws_4() {
  info("Do not autofill whitespaced entry 4");
  await PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://mozilla.org/link/"),
    transition: TRANSITION_TYPED
  });
  await check_autocomplete({
    search: "mozilla.org/link/ ",
    autofilled: "mozilla.org/link/ ",
    completed: "mozilla.org/link/ "
  });
  await cleanup();
});


add_task(async function test_not_autofill_ws_5() {
  info("Do not autofill whitespaced entry 5");
  await PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://mozilla.org/link/"),
    transition: TRANSITION_TYPED
  });
  await check_autocomplete({
    search: "moz illa ",
    autofilled: "moz illa ",
    completed: "moz illa "
  });
  await cleanup();
});

add_task(async function test_not_autofill_ws_6() {
  info("Do not autofill whitespaced entry 6");
  await PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://mozilla.org/link/"),
    transition: TRANSITION_TYPED
  });
  await check_autocomplete({
    search: " mozilla",
    autofilled: " mozilla",
    completed: " mozilla"
  });
  await cleanup();
});
