/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1761774 - Shim INFOnline IAM tracker
 *
 * Sites using IAM can break if it is blocked. This shim mitigates that
 * breakage by loading a stand-in module.
 */

if (!window.iom?.c) {
  window.iom = {
    c: () => {},
    consent: () => {},
    count: () => {},
    ct: () => {},
    deloptout: () => {},
    doo: () => {},
    e: () => {},
    event: () => {},
    getInvitation: () => {},
    getPlus: () => {},
    gi: () => {},
    gp: () => {},
    h: () => {},
    hybrid: () => {},
    i: () => {},
    init: () => {},
    oi: () => {},
    optin: () => {},
    setMultiIdentifier: () => {},
    setoptout: () => {},
    smi: () => {},
    soo: () => {},
  };
}
