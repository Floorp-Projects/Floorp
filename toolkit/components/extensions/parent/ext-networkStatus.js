/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

XPCOMUtils.defineLazyServiceGetter(
  this,
  "gNetworkLinkService",
  "@mozilla.org/network/network-link-service;1",
  "nsINetworkLinkService"
);

function getLinkType() {
  switch (gNetworkLinkService.linkType) {
    case gNetworkLinkService.LINK_TYPE_UNKNOWN:
      return "unknown";
    case gNetworkLinkService.LINK_TYPE_ETHERNET:
      return "ethernet";
    case gNetworkLinkService.LINK_TYPE_USB:
      return "usb";
    case gNetworkLinkService.LINK_TYPE_WIFI:
      return "wifi";
    case gNetworkLinkService.LINK_TYPE_WIMAX:
      return "wimax";
    case gNetworkLinkService.LINK_TYPE_MOBILE:
      return "mobile";
    default:
      return "unknown";
  }
}

function getLinkStatus() {
  if (!gNetworkLinkService.linkStatusKnown) {
    return "unknown";
  }
  return gNetworkLinkService.isLinkUp ? "up" : "down";
}

function getLinkInfo() {
  return {
    id: gNetworkLinkService.networkID || undefined,
    status: getLinkStatus(),
    type: getLinkType(),
  };
}

this.networkStatus = class extends ExtensionAPI {
  getAPI(context) {
    return {
      networkStatus: {
        getLinkInfo,
        onConnectionChanged: new EventManager({
          context,
          name: "networkStatus.onConnectionChanged",
          register: fire => {
            let observerStatus = () => {
              fire.async(getLinkInfo());
            };

            Services.obs.addObserver(
              observerStatus,
              "network:link-status-changed"
            );
            Services.obs.addObserver(
              observerStatus,
              "network:link-type-changed"
            );
            return () => {
              Services.obs.removeObserver(
                observerStatus,
                "network:link-status-changed"
              );
              Services.obs.removeObserver(
                observerStatus,
                "network:link-type-changed"
              );
            };
          },
        }).api(),
      },
    };
  }
};
