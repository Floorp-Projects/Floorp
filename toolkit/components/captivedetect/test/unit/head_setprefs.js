/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const kServerURL = 'http://localhost:4444';
const kCanonicalSitePath = '/canonicalSite.html';
const kCanonicalSiteContent = 'true';
const kPrefsCanonicalURL = 'captivedetect.canonicalURL';
const kPrefsCanonicalContent = 'captivedetect.canonicalContent';
const kPrefsMaxWaitingTime = 'captivedetect.maxWaitingTime';
const kPrefsPollingTime = 'captivedetect.pollingTime';

function setupPrefs() {
  let prefs = Components.classes["@mozilla.org/preferences-service;1"]
                .getService(Components.interfaces.nsIPrefService)
                .QueryInterface(Components.interfaces.nsIPrefBranch);
  prefs.setCharPref(kPrefsCanonicalURL, kServerURL + kCanonicalSitePath);
  prefs.setCharPref(kPrefsCanonicalContent, kCanonicalSiteContent);
  prefs.setIntPref(kPrefsMaxWaitingTime, 0);
  prefs.setIntPref(kPrefsPollingTime, 1);
}

setupPrefs();
