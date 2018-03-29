/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

ChromeUtils.defineModuleGetter(this, "NewTabUtils",
                               "resource://gre/modules/NewTabUtils.jsm");

this.topSites = class extends ExtensionAPI {
  getAPI(context) {
    return {
      topSites: {
        get: function(options) {
          return new Promise((resolve) => {
            NewTabUtils.links.populateCache(async () => {
              let urls;

              // The placesProvider is a superset of activityStream if sites are blocked, etc.,
              // there is no need to attempt a merge of multiple provider lists.
              if (options.providers.includes("places")) {
                urls = NewTabUtils.getProviderLinks(NewTabUtils.placesProvider).slice();
              } else {
                urls = await NewTabUtils.activityStreamLinks.getTopSites();
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
