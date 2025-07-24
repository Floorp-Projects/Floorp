/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { Panel } from "./type.ts";
import { STATIC_PANEL_DATA } from "../data/static-panels.ts";
import { getSidebarIconFromSidebarController } from "../extension-panels.ts";

const { PlacesUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/PlacesUtils.sys.mjs",
);

const DEFAULT_FAVICON = "chrome://devtools/skin/images/globe.svg";

const gFavicons = PlacesUtils.favicons as {
  getFaviconForPage: (
    uri: nsIURI,
  ) => Promise<{ uri: nsIURI }>;
};

export async function getFaviconURLForPanel(panel: Panel): Promise<string> {
  try {
    await globalThis.SessionStore.promiseInitialized;
    const faviconURL = await getFaviconFromPlaces(panel.url ?? "");
    return faviconURL ?? getFallbackFavicon(panel);
  } catch (e) {
    console.error(e);
    return getFallbackFavicon(panel);
  }
}

async function getFaviconFromPlaces(url: string): Promise<string | undefined> {
  if (!url) {
    return undefined;
  }

  const result = await gFavicons.getFaviconForPage(
    Services.io.newURI(url),
  );
  return result.uri.spec ?? undefined;
}

function getFaviconFromExtension(extensionId: string): string {
  return getSidebarIconFromSidebarController(extensionId) ?? "";
}

function getFallbackFavicon(panel: Panel): string {
  switch (panel.type) {
    case "static":
      return (
        STATIC_PANEL_DATA[panel.url as keyof typeof STATIC_PANEL_DATA].icon ??
          DEFAULT_FAVICON
      );
    case "extension":
      return getFaviconFromExtension(panel.extensionId ?? "");
    default:
      if (
        panel.url?.startsWith("http://") ||
        panel.url?.startsWith("https://")
      ) {
        return `https://www.google.com/s2/favicons?domain=${panel.url}&sz=32`;
      }
      return DEFAULT_FAVICON;
  }
}
