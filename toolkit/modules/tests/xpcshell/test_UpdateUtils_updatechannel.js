/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var { utils: Cu } = Components;

Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/UpdateUtils.jsm");

const PREF_APP_UPDATE_CHANNEL = "app.update.channel";
const TEST_CHANNEL            = "TestChannel";
const PREF_PARTNER_A          = "app.partner.test_partner_a";
const TEST_PARTNER_A          = "TestPartnerA";
const PREF_PARTNER_B          = "app.partner.test_partner_b";
const TEST_PARTNER_B          = "TestPartnerB";

add_task(function* test_updatechannel() {
  let defaultPrefs = new Preferences({ defaultBranch: true });
  let currentChannel = defaultPrefs.get(PREF_APP_UPDATE_CHANNEL);

  do_check_eq(UpdateUtils.UpdateChannel, currentChannel);
  do_check_eq(UpdateUtils.getUpdateChannel(true), currentChannel);
  do_check_eq(UpdateUtils.getUpdateChannel(false), currentChannel);

  defaultPrefs.set(PREF_APP_UPDATE_CHANNEL, TEST_CHANNEL);
  do_check_eq(UpdateUtils.UpdateChannel, TEST_CHANNEL);
  do_check_eq(UpdateUtils.getUpdateChannel(true), TEST_CHANNEL);
  do_check_eq(UpdateUtils.getUpdateChannel(false), TEST_CHANNEL);

  defaultPrefs.set(PREF_PARTNER_A, TEST_PARTNER_A);
  defaultPrefs.set(PREF_PARTNER_B, TEST_PARTNER_B);
  do_check_eq(UpdateUtils.UpdateChannel,
              TEST_CHANNEL + "-cck-" + TEST_PARTNER_A + "-" + TEST_PARTNER_B);
  do_check_eq(UpdateUtils.getUpdateChannel(true),
              TEST_CHANNEL + "-cck-" + TEST_PARTNER_A + "-" + TEST_PARTNER_B);
  do_check_eq(UpdateUtils.getUpdateChannel(false), TEST_CHANNEL);
});
