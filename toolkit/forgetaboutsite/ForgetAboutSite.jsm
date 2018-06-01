/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/NetUtil.jsm");

var EXPORTED_SYMBOLS = ["ForgetAboutSite"];

var ForgetAboutSite = {
  async removeDataFromDomain(aDomain) {
    let promises = [];
    let errorCount = 0;

    ["http://", "https://"].forEach(scheme => {
      promises.push(new Promise(resolve => {
        Services.clearData.deleteDataFromHost(aDomain, true /* user request */,
                                              Ci.nsIClearDataService.CLEAR_FORGET_ABOUT_SITE,
                                              value => {
          errorCount += bitCounting(value);
          resolve();
        });
      }));
    });

    await Promise.all(promises);

    if (errorCount !== 0) {
      throw new Error(`There were a total of ${errorCount} errors during removal`);
    }
  }
};

function bitCounting(value) {
  // To know more about how to count bits set to 1 in a numeric value, see this
  // interesting article:
  // https://blogs.msdn.microsoft.com/jeuge/2005/06/08/bit-fiddling-3/
  const count = value - ((value >> 1) & 0o33333333333) - ((value >> 2) & 0o11111111111);
  return ((count + (count >> 3)) & 0o30707070707) % 63;
}
