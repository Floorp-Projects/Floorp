/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { ExtensionError } = ExtensionUtils;

this.webRequest = class extends ExtensionAPI {
  STREAM_FILTER_INACTIVE_STATUSES = ["closed", "disconnected", "failed"];

  hasActiveStreamFilter(filtersWeakSet) {
    const iter = ChromeUtils.nondeterministicGetWeakSetKeys(filtersWeakSet);
    for (let filter of iter) {
      if (!this.STREAM_FILTER_INACTIVE_STATUSES.includes(filter.status)) {
        return true;
      }
    }
    return false;
  }

  watchStreamFilterSuspendCancel({
    context,
    filters,
    onSuspend,
    onSuspendCanceled,
  }) {
    if (
      !context.isBackgroundContext ||
      context.extension.persistentBackground !== false
    ) {
      return;
    }

    const { extension } = context;
    const cancelSuspendOnActiveStreamFilter = () =>
      this.hasActiveStreamFilter(filters);
    context.callOnClose({
      close() {
        extension.off(
          "internal:stream-filter-suspend-cancel",
          cancelSuspendOnActiveStreamFilter
        );
        extension.off("background-script-suspend", onSuspend);
        extension.off("background-script-suspend-canceled", onSuspend);
      },
    });
    extension.on(
      "internal:stream-filter-suspend-cancel",
      cancelSuspendOnActiveStreamFilter
    );
    extension.on("background-script-suspend", onSuspend);
    extension.on("background-script-suspend-canceled", onSuspendCanceled);
  }

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

    let isSuspending = false;
    this.watchStreamFilterSuspendCancel({
      context,
      filters,
      onSuspend: () => (isSuspending = true),
      onSuspendCanceled: () => (isSuspending = false),
    });

    function filterResponseData(requestId) {
      if (isSuspending) {
        throw new ExtensionError(
          "filterResponseData method calls forbidden while background extension global is suspending"
        );
      }
      requestId = parseInt(requestId, 10);

      let streamFilter = context.cloneScope.StreamFilter.create(
        requestId,
        context.extension.id
      );

      filters.add(streamFilter);
      return streamFilter;
    }

    const webRequest = {
      onAuthRequired: new EventManager({
        context,
        name: "webRequest.onAuthRequired",
        // Parent event already resets idle if needed, no need to do it here.
        resetIdleOnEvent: false,
        register: (fire, filter, extra) => {
          const listener = details => {
            // NOTE: asyncBlocking and blocking are mutually exclusive
            // (and an error raised synchronously from schema based
            // validation).
            if (!extra.includes("asyncBlocking")) {
              // NOTE: may return the result or a Promise.
              return fire.raw(details);
            }

            // Wrap the call into a promise resolved with what the extension
            // passed to the additional asyncCallback parameter.
            let asyncCallback;
            const promise = new Promise(resolve => {
              asyncCallback = Cu.exportFunction(value => {
                resolve(value);
              }, context.cloneScope);
            });
            // Return value is ignored on asyncBlocking listeners
            // (chrome compatible behavior).
            fire.raw(details, asyncCallback);
            return promise;
          };
          const parentEvent = context.childManager.getParentEvent(
            "webRequest.onAuthRequired"
          );
          parentEvent.addListener(listener, filter, extra);
          return () => {
            parentEvent.removeListener(listener);
          };
        },
      }).api(),
    };

    // For extensions with manifest_version >= 3, an additional webRequestFilterResponse permission
    // is required to get access to the webRequest.filterResponseData API method.
    if (
      context.extension.manifestVersion < 3 ||
      context.extension.hasPermission("webRequestFilterResponse")
    ) {
      webRequest.filterResponseData = filterResponseData;
    } else {
      webRequest.filterResponseData = () => {
        throw new ExtensionError(
          'Missing required "webRequestFilterResponse" permission'
        );
      };
    }

    return { webRequest };
  }
};
