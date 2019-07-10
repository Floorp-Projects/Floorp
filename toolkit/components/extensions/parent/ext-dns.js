/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const dnssFlags = {
  allow_name_collisions: Ci.nsIDNSService.RESOLVE_ALLOW_NAME_COLLISION,
  bypass_cache: Ci.nsIDNSService.RESOLVE_BYPASS_CACHE,
  canonical_name: Ci.nsIDNSService.RESOLVE_CANONICAL_NAME,
  disable_ipv4: Ci.nsIDNSService.RESOLVE_DISABLE_IPV4,
  disable_ipv6: Ci.nsIDNSService.RESOLVE_DISABLE_IPV6,
  disable_trr: Ci.nsIDNSService.RESOLVE_DISABLE_TRR,
  offline: Ci.nsIDNSService.RESOLVE_OFFLINE,
  priority_low: Ci.nsIDNSService.RESOLVE_PRIORITY_LOW,
  priority_medium: Ci.nsIDNSService.RESOLVE_PRIORITY_MEDIUM,
  speculate: Ci.nsIDNSService.RESOLVE_SPECULATE,
};

function getErrorString(nsresult) {
  let e = new Components.Exception("", nsresult);
  return e.name;
}

this.dns = class extends ExtensionAPI {
  getAPI(context) {
    const dnss = Cc["@mozilla.org/network/dns-service;1"].getService(
      Ci.nsIDNSService
    );
    return {
      dns: {
        resolve: function(hostname, flags) {
          let dnsFlags = flags.reduce(
            (mask, flag) => mask | dnssFlags[flag],
            0
          );

          return new Promise((resolve, reject) => {
            let request;
            let response = {
              addresses: [],
            };
            let listener = {
              onLookupComplete: function(inRequest, inRecord, inStatus) {
                if (inRequest === request) {
                  if (!Components.isSuccessCode(inStatus)) {
                    return reject({ message: getErrorString(inStatus) });
                  }
                  if (dnsFlags & Ci.nsIDNSService.RESOLVE_CANONICAL_NAME) {
                    try {
                      response.canonicalName = inRecord.canonicalName;
                    } catch (e) {
                      // no canonicalName
                    }
                  }
                  response.isTRR = inRecord.IsTRR();
                  while (inRecord.hasMore()) {
                    let addr = inRecord.getNextAddrAsString();
                    // Sometimes there are duplicate records with the same ip.
                    if (!response.addresses.includes(addr)) {
                      response.addresses.push(addr);
                    }
                  }
                  return resolve(response);
                }
              },
            };
            try {
              request = dnss.asyncResolve(
                hostname,
                dnsFlags,
                listener,
                null,
                {} /* defaultOriginAttributes */
              );
            } catch (e) {
              // handle exceptions such as offline mode.
              return reject({ message: e.name });
            }
          });
        },
      },
    };
  }
};
