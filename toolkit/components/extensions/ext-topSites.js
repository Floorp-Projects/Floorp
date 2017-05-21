/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "NewTabUtils",
                                  "resource://gre/modules/NewTabUtils.jsm");

this.topSites = class extends ExtensionAPI {
  getAPI(context) {
    return {
      topSites: {
        get: function(options) {
          return new Promise(function(resolve) {
            NewTabUtils.links.populateCache(function() {
              let urls;

              if (options && options.providers && options.providers.length > 0) {
                let urlLists = options.providers.map(function(p) {
                  let provider = NewTabUtils[`${p}Provider`];
                  return provider ? NewTabUtils.getProviderLinks(provider).slice() : [];
                });
                urls = NewTabUtils.links.mergeLinkLists(urlLists);
              } else {
                urls = NewTabUtils.links.getLinks();
              }

              resolve(urls.filter(link => !!link)
                          .map(link => {
                            return {
                              url: link.url,
                              title: link.title,
                            };
                          }));
            }, false);
          });
        },
      },
    };
  }
};
