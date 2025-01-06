/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { Manifest, Browser, Icon } from "./type.js";
import { IconProcesser } from "./iconProcesser";

const { ManifestObtainer } = ChromeUtils.importESModule(
  "resource://gre/modules/ManifestObtainer.sys.mjs"
);

const { ManifestProcessor } = ChromeUtils.importESModule(
  "resource://gre/modules/ManifestProcessor.sys.mjs"
);

export class ManifestProcesser {
  private iconProcesser: IconProcesser;

  constructor() {
    this.iconProcesser = new IconProcesser();
  }

  private get uuid() {
    return Services.uuid.generateUUID().toString();
  }

  /**
   * Generates an app manifest for Browser. It should be called if Browser has a manifest file.
   * If you want generate SSB Manifest file, You should call generateManifestForBrowser.
   *
   * @param {Browser} browser the browser element the site is loaded in.
   * @returns {Promise<Manifest>} an app manifest.
   */
  public async getManifestFromBrowser(
    browser: Browser,
    useWebManifest: boolean
  ): Promise<Manifest> {
    let manifest = null;
    try {
      if (useWebManifest) {
        manifest = await ManifestObtainer.browserObtainManifest(browser);
      }
    } catch (e) {
      throw new Error("Failed to obtain manifest from browser");
    }

    // YouTube returns monoChrome icon at default. Floorp(Noraneko) will reject monoChrome icon.
    if (manifest?.icons && manifest.icons.length !== 1) {
      const icons = manifest.icons;
      for (let i = 0; i < icons.length; i++) {
        if (icons[i].purpose.includes("monochrome")) {
          icons.splice(i, 1);
        }
      }
    }

    // Reject the manifest if its scope doesn't include the current document.
    // This follow to w3c document. We need to generate web manifest from scope.
    if (
      !manifest ||
      !this.scopeIncludes(
        Services.io.newURI(manifest.scope),
        browser.currentURI
      )
    ) {
      manifest = this.generateManifestForURI(browser.currentURI);
    }

    // Cache all the icons as data URIs since we can need access to them when
    // the website is not loaded.
    manifest.icons = (
      await Promise.all(
        manifest.icons.map(async (icon: Icon) => {
          if (icon.src.startsWith("data:")) {
            return icon;
          }

          const actor = browser.browsingContext.currentWindowGlobal.getActor(
            "NRProgressiveWebApp"
          );
          try {
            icon.src = await actor.sendQuery("LoadIcon", icon.src);
          } catch (e) {
            // Bad icon, drop it from the list.
            return null;
          }

          return icon;
        })
      )
    ).filter((icon) => icon);

    // If the site provided no icons then try to use the normal page icons.
    if (!manifest.icons.length) {
      const icon = await this.iconProcesser.getIconForBrowser(browser);
      manifest.icons.push({ src: icon });
    }

    if (!manifest.name) {
      manifest.name = browser.contentTitle;
    }

    manifest.id = this.uuid;
    manifest.icon = manifest.icons[0].src;
    return manifest;
  }

  public async checkBrowserCanBeInstallAsPwa(browser: Browser) {
    const manifest = await ManifestObtainer.browserObtainManifest(browser);
    return manifest !== null;
  }

  /**
   * Tests whether an app manifest's scope includes the given URI.
   * @param {nsIURI} scope the manifest's scope.
   * @param {nsIURI} uri the URI to test.
   */
  private scopeIncludes(scope: nsIURI, uri: nsIURI) {
    // https://w3c.github.io/manifest/#dfn-within-scope
    if (scope.prePath !== uri.prePath) {
      return false;
    }
    return uri.filePath.startsWith(scope.filePath);
  }

  /**
   * Generates a basic app manifest for a URI.
   * It should be called if user or Noraneko (Floorp) wants to generate manifest with not depending to Web Manifest.
   *
   * @param {nsIURI} uri the start URI for the site.
   * @returns {Manifest} an app manifest.
   */
  private generateManifestForURI(uri: nsIURI) {
    try {
      const manifestURI = Services.io.newURI("/manifest.json", "", uri);
      return ManifestProcessor.process({
        jsonText: "{}",
        manifestURL: manifestURI.spec,
        docURL: uri.spec,
      });
    } catch (e) {
      console.error(
        `Failed to generate a SSB (PWA) manifest for ${uri.spec}.`,
        e
      );
      throw e;
    }
  }
}
