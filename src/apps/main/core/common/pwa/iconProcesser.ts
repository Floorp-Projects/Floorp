/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { Browser, Icon, IconData } from "./type.js";
import { ImageTools } from "./imageTools";

const { PlacesUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/PlacesUtils.sys.mjs"
);

export class IconProcesser {
  private static instance: IconProcesser;
  public static getInstance() {
    if (!IconProcesser.instance) {
      IconProcesser.instance = new IconProcesser();
    }
    return IconProcesser.instance;
  }

  buildIconList(icons: Icon[]) {
    const iconList = [];

    for (const icon of icons) {
      for (const sizeSpec of icon.sizes) {
        const size =
          sizeSpec === "any"
            ? Number.MAX_SAFE_INTEGER
            : Number.parseInt(sizeSpec);

        iconList.push({
          icon,
          size,
        });
      }
    }

    iconList.sort((a, b) => {
      // Given that we're using MAX_SAFE_INTEGER adding a value to that would
      // overflow and give odd behaviour. And we're using numbers supplied by a
      // website so just compare for safety.
      if (a.size < b.size) {
        return -1;
      }

      if (a.size > b.size) {
        return 1;
      }

      return 0;
    });
    return iconList;
  }

  /**
   * Creates an IconResource from the LinkHandler data.
   *
   * @param {IconData} iconData the data from the LinkHandler actor.
   * @returns {Promise<IconResource>} an icon resource.
   */
  async getIconResource(iconData: IconData): Promise<Icon> {
    // This should be a data url so no network traffic.
    const imageData = await ImageTools.loadImage(
      Services.io.newURI(iconData.iconURL)
    );
    if (imageData.container.type === Ci.imgIContainer.TYPE_VECTOR) {
      return {
        src: iconData.iconURL,
        purpose: ["any"],
        type: imageData.type,
        sizes: ["any"],
      };
    }

    return {
      src: iconData.iconURL,
      purpose: ["any"],
      type: imageData.type,
      sizes: [`${imageData.container.width}x${imageData.container.height}`],
    };
  }

  public async getIconForBrowser(browser: Browser): Promise<string> {
    const gFavicons = PlacesUtils.favicons as nsIFaviconService;
    const iconUrl = await new Promise<string>((resolve) => {
      gFavicons.getFaviconURLForPage(
        Services.io.newURI(browser.currentURI.spec),
        (uri: nsIURI) => resolve(uri?.spec || "")
      );
    });

    if (!iconUrl) {
      return "";
    }

    try {
      const response = await fetch(iconUrl);
      const blob = await response.blob();
      const reader = new FileReader();
      const old_base64 = await new Promise<string>((resolve) => {
        reader.onloadend = () => resolve(reader.result as string);
        reader.readAsDataURL(blob);
      });
      const { container } = await ImageTools.loadImage(
        Services.io.newURI(old_base64)
      );

      const blobPng = (await ImageTools.scaleImage(container, 64, 64)) as Blob;
      const pngBase64 = await new Promise<string>((resolve) => {
        reader.onloadend = () => resolve(reader.result as string);
        reader.readAsDataURL(blobPng);
      });

      return pngBase64;
    } catch (e) {
      console.error("Failed to fetch icon:", e);
      return "";
    }
  }
}
