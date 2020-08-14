/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This is needed for extensions to load content scripts */

var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

// Send this notification from the content process so that the
// ExtensionPolicyService can register this content frame and be ready to load
// content scripts
Services.obs.notifyObservers(this, "tab-content-frameloader-created");
