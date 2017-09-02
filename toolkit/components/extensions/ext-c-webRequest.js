/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

var {
  ExtensionError,
} = ExtensionCommon;

this.webRequest = class extends ExtensionAPI {
  getAPI(context) {
    return {
      webRequest: {
        filterResponseData(requestId) {
          if (AppConstants.RELEASE_OR_BETA) {
            throw new ExtensionError("filterResponseData() unsupported in release builds");
          }
          requestId = parseInt(requestId, 10);

          return context.cloneScope.StreamFilter.create(
            requestId, context.extension.id);
        },
      },
    };
  }
};
