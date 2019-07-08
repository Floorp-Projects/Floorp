/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Preferences } = ChromeUtils.import(
  "resource://gre/modules/Preferences.jsm"
);
const { UpdateUtils } = ChromeUtils.import(
  "resource://gre/modules/UpdateUtils.jsm"
);

const PREF_APP_UPDATE_CHANNEL = "app.update.channel";
const TEST_CHANNEL = "TestChannel";
const PREF_PARTNER_A = "app.partner.test_partner_a";
const TEST_PARTNER_A = "TestPartnerA";
const PREF_PARTNER_B = "app.partner.test_partner_b";
const TEST_PARTNER_B = "TestPartnerB";

add_task(async function test_updatechannel() {
  let defaultPrefs = new Preferences({ defaultBranch: true });
  let currentChannel = defaultPrefs.get(PREF_APP_UPDATE_CHANNEL);

  Assert.equal(UpdateUtils.UpdateChannel, currentChannel);
  Assert.equal(UpdateUtils.getUpdateChannel(true), currentChannel);
  Assert.equal(UpdateUtils.getUpdateChannel(false), currentChannel);

  defaultPrefs.set(PREF_APP_UPDATE_CHANNEL, TEST_CHANNEL);
  Assert.equal(UpdateUtils.UpdateChannel, TEST_CHANNEL);
  Assert.equal(UpdateUtils.getUpdateChannel(true), TEST_CHANNEL);
  Assert.equal(UpdateUtils.getUpdateChannel(false), TEST_CHANNEL);

  defaultPrefs.set(PREF_PARTNER_A, TEST_PARTNER_A);
  defaultPrefs.set(PREF_PARTNER_B, TEST_PARTNER_B);
  Assert.equal(
    UpdateUtils.UpdateChannel,
    TEST_CHANNEL + "-cck-" + TEST_PARTNER_A + "-" + TEST_PARTNER_B
  );
  Assert.equal(
    UpdateUtils.getUpdateChannel(true),
    TEST_CHANNEL + "-cck-" + TEST_PARTNER_A + "-" + TEST_PARTNER_B
  );
  Assert.equal(UpdateUtils.getUpdateChannel(false), TEST_CHANNEL);
});
