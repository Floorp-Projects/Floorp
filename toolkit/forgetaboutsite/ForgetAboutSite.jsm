/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

var EXPORTED_SYMBOLS = ["ForgetAboutSite"];

var ForgetAboutSite = {
  async removeDataFromDomain(aDomain) {
    let promises = [
      new Promise(resolve =>
        Services.clearData.deleteDataFromHost(
          aDomain,
          true /* user request */,
          Ci.nsIClearDataService.CLEAR_FORGET_ABOUT_SITE,
          errorCode => resolve(bitCounting(errorCode))
        )
      ),
    ];

    try {
      let baseDomain = Services.eTLD.getBaseDomainFromHost(aDomain);

      let cookies = Services.cookies.cookies;
      let hosts = new Set();
      for (let cookie of cookies) {
        if (Services.eTLD.hasRootDomain(cookie.rawHost, baseDomain)) {
          hosts.add(cookie.rawHost);
        }
      }

      for (let host of hosts) {
        promises.push(
          new Promise(resolve =>
            Services.clearData.deleteDataFromHost(
              host,
              true /* user request */,
              Ci.nsIClearDataService.CLEAR_COOKIES,
              errorCode => resolve(bitCounting(errorCode))
            )
          )
        );
      }
    } catch (e) {
      // - NS_ERROR_HOST_IS_IP_ADDRESS: the host is in ipv4/ipv6.
      // - NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS: not enough domain parts to extract,
      //   i.e. the host is on the PSL.
      // In both these cases we should probably not try to use the host as a base
      // domain to remove more data, but we can still (try to) continue deleting the host.
      if (
        e.result != Cr.NS_ERROR_HOST_IS_IP_ADDRESS &&
        e.result != Cr.NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS
      ) {
        throw e;
      }
    }

    let errorCount = (await Promise.all(promises)).reduce((a, b) => a + b);

    if (errorCount !== 0) {
      throw new Error(
        `There were a total of ${errorCount} errors during removal`
      );
    }
  },
};

function bitCounting(value) {
  // To know more about how to count bits set to 1 in a numeric value, see this
  // interesting article:
  // https://blogs.msdn.microsoft.com/jeuge/2005/06/08/bit-fiddling-3/
  const count =
    value - ((value >> 1) & 0o33333333333) - ((value >> 2) & 0o11111111111);
  return ((count + (count >> 3)) & 0o30707070707) % 63;
}
