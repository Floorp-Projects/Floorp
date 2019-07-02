/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

ChromeUtils.defineModuleGetter(this, "NewTabUtils",
                               "resource://gre/modules/NewTabUtils.jsm");

this.topSites = class extends ExtensionAPI {
  getAPI(context) {
    return {
      topSites: {
        get: async function(options) {
          let links = await NewTabUtils.activityStreamLinks.getTopSites({
            ignoreBlocked: options.includeBlocked,
            onePerDomain: options.onePerDomain,
            numItems: options.limit,
            includeFavicon: options.includeFavicon,
          });
          return links.map(link => {
            return {
              url: link.url,
              title: link.title,
              favicon: link.favicon,
            };
          });
        },
      },
    };
  }
};
