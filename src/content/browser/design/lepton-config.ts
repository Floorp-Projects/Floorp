/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export function setPhotonUI() {
  Services.prefs.setIntPref("floorp.lepton.interface", 1);
  Services.prefs.setBoolPref("userChrome.tab.connect_to_window", true);
  Services.prefs.setBoolPref("userChrome.tab.color_like_toolbar", true);

  Services.prefs.setBoolPref("userChrome.tab.lepton_like_padding", false);
  Services.prefs.setBoolPref("userChrome.tab.photon_like_padding", true);

  Services.prefs.setBoolPref("userChrome.tab.dynamic_separator", false);
  Services.prefs.setBoolPref("userChrome.tab.static_separator", true);
  Services.prefs.setBoolPref(
    "userChrome.tab.static_separator.selected_accent",
    false,
  );
  Services.prefs.setBoolPref("userChrome.tab.bar_separator", false);

  Services.prefs.setBoolPref("userChrome.tab.newtab_button_like_tab", false);
  Services.prefs.setBoolPref("userChrome.tab.newtab_button_smaller", true);
  Services.prefs.setBoolPref("userChrome.tab.newtab_button_proton", false);

  Services.prefs.setBoolPref("userChrome.icon.panel_full", false);
  Services.prefs.setBoolPref("userChrome.icon.panel_photon", true);

  Services.prefs.setBoolPref("userChrome.tab.box_shadow", false);
  Services.prefs.setBoolPref("userChrome.tab.bottom_rounded_corner", false);

  Services.prefs.setBoolPref("userChrome.tab.photon_like_contextline", true);
  Services.prefs.setBoolPref("userChrome.rounding.square_tab", true);
}

export function setLeptonUI() {
  Services.prefs.setIntPref("floorp.lepton.interface", 2);
  Services.prefs.setBoolPref("userChrome.tab.connect_to_window", true);
  Services.prefs.setBoolPref("userChrome.tab.color_like_toolbar", true);

  Services.prefs.setBoolPref("userChrome.tab.lepton_like_padding", true);
  Services.prefs.setBoolPref("userChrome.tab.photon_like_padding", false);

  Services.prefs.setBoolPref("userChrome.tab.dynamic_separator", true);
  Services.prefs.setBoolPref("userChrome.tab.static_separator", false);
  Services.prefs.setBoolPref(
    "userChrome.tab.static_separator.selected_accent",
    false,
  );
  Services.prefs.setBoolPref("userChrome.tab.bar_separator", false);

  Services.prefs.setBoolPref("userChrome.tab.newtab_button_like_tab", true);
  Services.prefs.setBoolPref("userChrome.tab.newtab_button_smaller", false);
  Services.prefs.setBoolPref("userChrome.tab.newtab_button_proton", false);

  Services.prefs.setBoolPref("userChrome.icon.panel_full", true);
  Services.prefs.setBoolPref("userChrome.icon.panel_photon", false);

  Services.prefs.setBoolPref("userChrome.tab.box_shadow", false);
  Services.prefs.setBoolPref("userChrome.tab.bottom_rounded_corner", true);

  Services.prefs.setBoolPref("userChrome.tab.photon_like_contextline", false);
  Services.prefs.setBoolPref("userChrome.rounding.square_tab", false);
}

export function setProtonFixUI() {
  Services.prefs.setIntPref("floorp.lepton.interface", 3);

  Services.prefs.setBoolPref("userChrome.tab.connect_to_window", false);
  Services.prefs.setBoolPref("userChrome.tab.color_like_toolbar", false);

  Services.prefs.setBoolPref("userChrome.tab.lepton_like_padding", false);
  Services.prefs.setBoolPref("userChrome.tab.photon_like_padding", false);

  Services.prefs.setBoolPref("userChrome.tab.dynamic_separator", true);
  Services.prefs.setBoolPref("userChrome.tab.static_separator", false);
  Services.prefs.setBoolPref(
    "userChrome.tab.static_separator.selected_accent",
    false,
  );
  Services.prefs.setBoolPref("userChrome.tab.bar_separator", false);

  Services.prefs.setBoolPref("userChrome.tab.newtab_button_like_tab", false);
  Services.prefs.setBoolPref("userChrome.tab.newtab_button_smaller", false);
  Services.prefs.setBoolPref("userChrome.tab.newtab_button_proton", true);

  Services.prefs.setBoolPref("userChrome.icon.panel_full", true);
  Services.prefs.setBoolPref("userChrome.icon.panel_photon", false);

  Services.prefs.setBoolPref("userChrome.tab.box_shadow", false);
  Services.prefs.setBoolPref("userChrome.tab.bottom_rounded_corner", false);

  Services.prefs.setBoolPref("userChrome.tab.photon_like_contextline", false);
  Services.prefs.setBoolPref("userChrome.rounding.square_tab", false);
}

// Add observers
Services.obs.addObserver(setPhotonUI, "set-photon-ui");
Services.obs.addObserver(setLeptonUI, "set-lepton-ui");
Services.obs.addObserver(setProtonFixUI, "set-protonfix-ui");
