/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

export const ProcessType = Object.freeze({
  /**
   * Converts a key string to a fluent ID defined in processTypes.ftl.
   */
  kProcessTypeMap: {
    // Keys defined in xpcom/build/GeckoProcessTypes.h
    default: "process-type-default",
    gpu: "process-type-gpu",
    tab: "process-type-tab",
    rdd: "process-type-rdd",
    socket: "process-type-socket",
    utility: "process-type-utility",

    // Keys defined in dom/ipc/RemoteType.h
    extension: "process-type-extension",
    file: "process-type-file",
    prealloc: "process-type-prealloc",
    privilegedabout: "process-type-privilegedabout",
    privilegedmozilla: "process-type-privilegedmozilla",
    web: "process-type-web",
    webIsolated: "process-type-webisolated",
    webServiceWorker: "process-type-webserviceworker",
  },

  kFallback: "process-type-unknown",

  fluentNameFromProcessTypeString(type) {
    return this.kProcessTypeMap[type] || this.kFallback;
  },
});
