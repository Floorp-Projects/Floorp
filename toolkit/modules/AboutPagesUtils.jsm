/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=2 ts=2 sts=2 expandtab
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["AboutPagesUtils"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const AboutPagesUtils = {};

XPCOMUtils.defineLazyGetter(AboutPagesUtils, "visibleAboutUrls", () => {
  const urls = [];
  const rx = /@mozilla.org\/network\/protocol\/about;1\?what\=(.*)$/;
  for (const cid in Cc) {
    const result = cid.match(rx);
    if (!result) {
      continue;
    }
    const [, aboutType] = result;
    try {
      const am = Cc[cid].getService(Ci.nsIAboutModule);
      const uri = Services.io.newURI(`about:${aboutType}`);
      const flags = am.getURIFlags(uri);
      if (!(flags & Ci.nsIAboutModule.HIDE_FROM_ABOUTABOUT)) {
        urls.push(`about:${aboutType}`);
      }
    } catch (e) {
      // getService() might have thrown if the component doesn't actually
      // implement nsIAboutModule
    }
  }
  urls.sort();
  return urls;
});
