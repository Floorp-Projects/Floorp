/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cm = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);

const classID = Components.ID("{97bf9550-2a7b-11e9-b56e-0800200c9a66}");

if (!Cm.isCIDRegistered(classID)) {
  const { XPCOMUtils } = ChromeUtils.import(
    "resource://gre/modules/XPCOMUtils.jsm"
  );

  const factory = XPCOMUtils.generateSingletonFactory(function() {
    const { AboutCompat } = ChromeUtils.import(
      "resource://webcompat/AboutCompat.jsm"
    );
    return new AboutCompat();
  });

  Cm.registerFactory(
    classID,
    "about:compat",
    "@mozilla.org/network/protocol/about;1?what=compat",
    factory
  );
}
