/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

this.webRequest = class extends ExtensionAPI {
  getAPI(context) {
    return {
      webRequest: {
        filterResponseData(requestId) {
          requestId = parseInt(requestId, 10);

          return context.cloneScope.StreamFilter.create(
            requestId, context.extension.id);
        },
      },
    };
  }
};
