/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Default value of browser.contentblocking.enabled.
// Please note that privacy protections provided by Gecko may depend on this preference.
// Turning this off may disable some protections.  Please do not turn this pref off without
// realizing the implications of what you're doing.
#define CONTENTBLOCKING_ENABLED true

// Default value of browser.contentblocking.ui.enabled.
// Enable the new Content Blocking UI only on Nightly.
#ifdef NIGHTLY_BUILD
# define CONTENTBLOCKING_UI_ENABLED true
#else
# define CONTENTBLOCKING_UI_ENABLED false
#endif

// Default value of browser.contentblocking.rejecttrackers.ui.enabled.
#define CONTENTBLOCKING_REJECTTRACKERS_UI_ENABLED true
