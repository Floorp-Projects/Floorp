/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createSignal } from "solid-js";
import i18next from "i18next";

export class QRCodeManager {
  _showPanel = createSignal(false);
  showPanel = this._showPanel[0];
  setShowPanel = this._showPanel[1];

  _currentUrl = createSignal("");
  currentUrl = this._currentUrl[0];
  setCurrentUrl = this._currentUrl[1];

  private qrCode: any = null;

  constructor() {
    if (!window.gFloorp) {
      window.gFloorp = {};
    }

    window.gFloorp.qrCode = {
      show: () => this.showQRPanel(),
      hide: () => this.hideQRPanel(),
      generateForUrl: (url: string) => this.generateQRCode(url),
    };

    // gFloorpPageAction互換性
    if (!window.gFloorpPageAction) {
      window.gFloorpPageAction = {};
    }

    window.gFloorpPageAction.qrCode = {
      onPopupShowing: () => {
        this.updateCurrentTabUrl();
        const url = this.currentUrl();
        if (url) {
          setTimeout(async () => {
            const container = document?.getElementById(
              "qrcode-img-vbox",
            ) as HTMLElement;
            if (container) {
              await this.generateQRCode(url, container);
            }
          }, 100);
        }
      },
    };
  }

  init() {
    this.updateCurrentTabUrl();

    window.gBrowser?.tabContainer.addEventListener("TabSelect", () => {
      this.updateCurrentTabUrl();
    });
  }

  showQRPanel() {
    this.updateCurrentTabUrl();
    this.setShowPanel(true);
  }

  hideQRPanel() {
    this.setShowPanel(false);
  }

  updateCurrentTabUrl() {
    try {
      const currentTab = window.gBrowser?.selectedTab;
      if (currentTab) {
        const currentTabURL = currentTab.linkedBrowser?.currentURI?.spec;
        if (currentTabURL) {
          this.setCurrentUrl(currentTabURL);
        }
      }
    } catch (error) {
      console.error("Failed to get current tab URL:", error);
    }
  }

  async generateQRCode(url: string, container?: HTMLElement): Promise<any> {
    if (!url) return null;

    try {
      const QRCodeStyling = await import("qr-code-styling");
      const QRCodeStylingClass = QRCodeStyling.default || QRCodeStyling;

      const options = {
        width: 250,
        height: 250,
        type: "svg",
        data: url,
        image: "chrome://branding/content/about-logo.png",
        dotsOptions: {
          color: "#4267b2",
          type: "rounded",
        },
        cornersSquareOptions: {
          type: "extra-rounded",
          color: "#4267b2",
        },
        cornersDotOptions: {
          type: "dot",
          color: "#4267b2",
        },
        backgroundOptions: {
          color: "#e9ebee",
        },
        imageOptions: {
          crossOrigin: "anonymous",
          margin: 10,
        },
      };

      this.qrCode = new (QRCodeStylingClass as any)(options);

      if (container) {
        // Clear previous QR code
        while (container.firstChild) {
          container.removeChild(container.firstChild);
        }
        this.qrCode.append(container);
      }

      return this.qrCode;
    } catch (error) {
      console.error("Failed to load QRCodeStyling:", error);
      return null;
    }
  }

  downloadQRCode(format: "png" | "jpeg" | "webp" | "svg") {
    if (this.qrCode) {
      this.qrCode.download({
        name: "qr-code",
        extension: format,
      });
    }
  }
}
