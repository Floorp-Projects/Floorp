/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { Download, HistoryItem, Tab } from "../type.ts";

/**
 * browserInfo - Provides browser information as potentially relevant context when the LLM generates workflows
 *
 * This module provides functionality to:
 * - Collect recent browser history
 * - Collect recent browser tabs
 * - Collect recent browser downloads
 */
export class browserInfo {
  /**
   * Gets recent browser history items
   *
   * @param limit - Maximum number of history items to return (default: 10)
   * @returns Promise<HistoryItem[]> - Array of recent history items
   */
  getRecentHistory(limit = 10): Promise<HistoryItem[]> {
    return new Promise((resolve) => {
      try {
        // Get browser history using Places API
        const { PlacesUtils } = ChromeUtils.importESModule(
          "resource://gre/modules/PlacesUtils.sys.mjs",
        );

        const query = PlacesUtils.history.getNewQuery();
        query.beginTime = Date.now() - 7 * 24 * 60 * 60 * 1000; // Last 7 days
        query.endTime = Date.now();

        const options = PlacesUtils.history.getNewQueryOptions();
        options.sortingMode =
          Ci.nsINavHistoryQueryOptions.SORT_BY_LASTMODIFIED_DESCENDING;
        options.maxResults = limit;

        const result = PlacesUtils.history.executeQuery(query, options);
        const history: HistoryItem[] = [];

        for (let i = 0; i < result.root.childCount; i++) {
          const node = result.root.getChild(i);
          if (node.uri) {
            history.push({
              url: node.uri,
              title: node.title || node.uri,
              lastVisitDate: node.lastModified,
              visitCount: node.accessCount || 1,
            });
          }
        }

        resolve(history);
      } catch (error) {
        console.error("Error getting recent history:", error);
        resolve([]);
      }
    });
  }

  /**
   * Gets current browser tabs
   *
   * @returns Promise<Tab[]> - Array of current tabs
   */
  getRecentTabs(): Promise<Tab[]> {
    return new Promise((resolve) => {
      try {
        const tabs: Tab[] = [];
        const windows = Services.wm.getEnumerator("navigator:browser");

        while (windows.hasMoreElements()) {
          const window = windows.getNext() as Window;
          const tabbrowser = window.gBrowser;

          for (let i = 0; i < tabbrowser.tabs.length; i++) {
            const tab = tabbrowser.tabs[i];
            const browser = tabbrowser.getBrowserForTab(tab);

            if (
              browser.currentURI && browser.currentURI.spec !== "about:blank"
            ) {
              tabs.push({
                id: tab.linkedPanel ? Number.parseInt(tab.linkedPanel) : i,
                url: browser.currentURI.spec,
                title: tab.label || browser.currentURI.spec,
                isActive: tab === tabbrowser.selectedTab,
                isPinned: tab.pinned,
              });
            }
          }
        }

        resolve(tabs);
      } catch (error) {
        console.error("Error getting recent tabs:", error);
        resolve([]);
      }
    });
  }

  /**
   * Gets recent browser downloads
   *
   * @param limit - Maximum number of downloads to return (default: 10)
   * @returns Promise<Download[]> - Array of recent downloads
   */
  getRecentDownloads(limit = 10): Promise<Download[]> {
    return new Promise((resolve) => {
      try {
        const { Downloads } = ChromeUtils.importESModule(
          "resource://gre/modules/Downloads.sys.mjs",
        );

        const downloads: Download[] = [];

        // Use async/await within the Promise
        (async () => {
          try {
            const list = await Downloads.getList(Downloads.ALL);
            const items = await list.getAll();

            // Sort by start time (most recent first) and limit results
            const sortedItems = items
              .sort((a, b) => (b.startTime || 0) - (a.startTime || 0))
              .slice(0, limit);

            for (const item of sortedItems) {
              downloads.push({
                id: item.id,
                url: item.source.url,
                filename: item.target.path.split(/[/\\]/).pop() || "unknown",
                status: item.succeeded
                  ? "completed"
                  : item.error
                  ? "failed"
                  : "in-progress",
                startTime: item.startTime || 0,
              });
            }

            resolve(downloads);
          } catch (error) {
            console.error("Error getting recent downloads:", error);
            resolve([]);
          }
        })();
      } catch (error) {
        console.error("Error importing Downloads module:", error);
        resolve([]);
      }
    });
  }

  /**
   * Gets all browser context data as a single object
   *
   * @param historyLimit - Maximum number of history items to include
   * @param downloadLimit - Maximum number of downloads to include
   * @returns Promise<{
   *   history: HistoryItem[];
   *   tabs: Tab[];
   *   downloads: Download[];
   * }> - Complete browser context data
   */
  getAllContextData(
    historyLimit = 10,
    downloadLimit = 10,
  ): Promise<{
    history: HistoryItem[];
    tabs: Tab[];
    downloads: Download[];
  }> {
    return new Promise((resolve) => {
      Promise.all([
        this.getRecentHistory(historyLimit),
        this.getRecentTabs(),
        this.getRecentDownloads(downloadLimit),
      ]).then(([history, tabs, downloads]) => {
        resolve({
          history,
          tabs,
          downloads,
        });
      }).catch((error) => {
        console.error("Error getting all context data:", error);
        resolve({
          history: [],
          tabs: [],
          downloads: [],
        });
      });
    });
  }
}

export const BrowserInfo = new browserInfo();
