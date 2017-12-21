/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Ensure inline autocomplete doesn't return zero frecency pages.

add_task(async function test_zzero_frec_domain() {
  info("Searching for zero frecency domain should not autoFill it");
  Services.prefs.setBoolPref("browser.urlbar.autoFill.typed", false);
  await PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://mozilla.org/framed_link/"),
    transition: TRANSITION_FRAMED_LINK
  });
  await check_autocomplete({
    search: "moz",
    autofilled: "moz",
    completed:  "moz"
  });
  await cleanup();
});

add_task(async function test_zzero_frec_url() {
  info("Searching for zero frecency url should not autoFill it");
  Services.prefs.setBoolPref("browser.urlbar.autoFill.typed", false);
  await PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://mozilla.org/framed_link/"),
    transition: TRANSITION_FRAMED_LINK
  });
  await check_autocomplete({
    search: "mozilla.org/f",
    autofilled: "mozilla.org/f",
    completed:  "mozilla.org/f"
  });
  await cleanup();
});
