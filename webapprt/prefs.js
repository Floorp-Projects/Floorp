/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

pref("browser.chromeURL", "chrome://webapprt/content/webapp.xul");

// We set this to the value of DEFAULT_HIDDENWINDOW_URL in nsAppShellService.cpp
// so our app is treated as not having an application-provided hidden window.
// Ideally, we could just leave it out, but because we are being distributed
// in a unified directory with Firefox, Firefox's preferences are being read
// before ours, which means this preference is being set by Firefox, and we need
// to set it here to override the Firefox-provided value.
pref("browser.hiddenWindowChromeURL", "resource://gre-resources/hiddenWindow.html");
