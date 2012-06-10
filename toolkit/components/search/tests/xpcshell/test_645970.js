/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test nsSearchService with nested jar: uris
 */
function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "2");

  do_load_manifest("data/chrome.manifest");

  let url  = "chrome://testsearchplugin/locale/searchplugins/";
  Services.prefs.setCharPref("browser.search.jarURIs", url);

  Services.prefs.setBoolPref("browser.search.loadFromJars", true);

  // The search service needs to be started after the jarURIs pref has been
  // set in order to initiate it correctly
  let engine = Services.search.getEngineByName("bug645970");
  do_check_neq(engine, null);
  Services.obs.notifyObservers(null, "quit-application", null);
}

