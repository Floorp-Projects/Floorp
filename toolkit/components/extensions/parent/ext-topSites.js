/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

XPCOMUtils.defineLazyModuleGetters(this, {
  NewTabUtils: "resource://gre/modules/NewTabUtils.jsm",
  shortURL: "resource://activity-stream/lib/ShortURL.jsm",
  getSearchProvider: "resource://activity-stream/lib/SearchShortcuts.jsm",
});

const SHORTCUTS_PREF =
  "browser.newtabpage.activity-stream.improvesearch.topSiteSearchShortcuts";

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

          if (options.includePinned) {
            let pinnedLinks = NewTabUtils.pinnedLinks.links;
            if (options.includeFavicon) {
              pinnedLinks = NewTabUtils.activityStreamProvider._faviconBytesToDataURI(
                await NewTabUtils.activityStreamProvider._addFavicons(pinnedLinks));
            }
            pinnedLinks.forEach((pinnedLink, index) => {
              if (pinnedLink &&
                  (!pinnedLink.searchTopSite || options.includeSearchShortcuts)) {
                // Remove any dupes from history.
                links = links.filter(link => link.url != pinnedLink.url &&
                  (!options.onePerDomain ||
                   NewTabUtils.extractSite(link.url) != pinnedLink.baseDomain));
                links.splice(index, 0, pinnedLink);
              }
            });
            // Because we may have addded links, we must crop again.
            if (options.limit) {
              links = links.slice(0, options.limit);
            }
          }

          // Convert links to search shortcuts, if necessary.
          if (options.includeSearchShortcuts &&
            Services.prefs.getBoolPref(SHORTCUTS_PREF, false)) {
            // Pinned shortcuts are already returned as searchTopSite links,
            // with a proper label and url. But certain non-pinned links may
            // also be promoted to search shortcuts; here we convert them.
            links = links.map(link => {
              let searchProvider = getSearchProvider(shortURL(link));
              if (searchProvider) {
                link.searchTopSite = true;
                link.label = searchProvider.keyword;
                link.url = searchProvider.url;
              }
              return link;
            });
          }

          return links.map(link => ({
            url: link.url,
            title: link.searchTopSite ? link.label : link.title,
            favicon: link.favicon,
            type: link.searchTopSite ? "search" : "url",
          }));
        },
      },
    };
  }
};
