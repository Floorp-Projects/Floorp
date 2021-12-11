/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.webRequest = class extends ExtensionAPI {
  getAPI(context) {
    let filters = new WeakSet();

    context.callOnClose({
      close() {
        for (let filter of ChromeUtils.nondeterministicGetWeakSetKeys(
          filters
        )) {
          try {
            filter.disconnect();
          } catch (e) {
            // Ignore.
          }
        }
      },
    });

    return {
      webRequest: {
        filterResponseData(requestId) {
          requestId = parseInt(requestId, 10);

          let streamFilter = context.cloneScope.StreamFilter.create(
            requestId,
            context.extension.id
          );

          filters.add(streamFilter);
          return streamFilter;
        },
      },
    };
  }
};
