/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "NewTabUtils",
                                  "resource://gre/modules/NewTabUtils.jsm");

this.topSites = class extends ExtensionAPI {
  getAPI(context) {
    return {
      topSites: {
        get: function() {
          let urls = NewTabUtils.links.getLinks()
                                .filter(link => !!link)
                                .map(link => {
                                  return {
                                    url: link.url,
                                    title: link.title,
                                  };
                                });
          return Promise.resolve(urls);
        },
      },
    };
  }
};
