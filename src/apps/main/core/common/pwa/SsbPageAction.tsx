/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createSignal, Show } from "solid-js";
import type { JSX } from "solid-js";
import { render } from "@nora/solid-xul";
import type { PwaService } from "./pwaService";

export class SsbPageAction {
  private isInstalling = createSignal(false);
  private icon = createSignal("");
  private title = createSignal("");
  private description = createSignal("");
  private canBeInstallAsPwa = createSignal(false);
  private isInstalled = createSignal(false);

  constructor(private pwaService: PwaService) {
    const starButtonBox = document?.getElementById("star-button-box");
    const ssbPageAction = document?.getElementById("page-action-buttons");
    if (!starButtonBox || !ssbPageAction) return;

    render(() => this.Render(), ssbPageAction, {
      marker: starButtonBox,
    });

    Services.obs.addObserver(
      () => this.onCheckPageHasManifest(),
      "nora-pwa-check-page-has-manifest"
    );
    window.gBrowser.tabContainer.addEventListener(
      "TabSelect",
      () => this.onCheckPageHasManifest()
    );

    this.onCheckPageHasManifest();
  }

  private async onCheckPageHasManifest() {
    const browser = window.gBrowser.selectedBrowser;

    const canBeInstallAsPwa = await this.pwaService.checkBrowserCanBeInstallAsPwa(browser);
    this.canBeInstallAsPwa[1](canBeInstallAsPwa);

    const isInstalled = await this.pwaService.checkCurrentPageIsInstalled(browser);
    this.isInstalled[1](isInstalled);
    this.pwaService.updateUIElements(isInstalled);
  }

  private onCommand = () => {
    this.pwaService.installOrRunCurrentPageAsSsb(
      window.gBrowser.selectedBrowser,
      true
    );
    this.isInstalling[1](true);
  };

  private onPopupShowing = async () => {
    const icon = await this.pwaService.getIcon(window.gBrowser.selectedBrowser);
    this.icon[1](icon);

    const manifest = await this.pwaService.getManifest(
      window.gBrowser.selectedBrowser
    );
    this.title[1](manifest.name ?? window.gBrowser.selectedBrowser.currentURI.spec);
    this.description[1](window.gBrowser.selectedBrowser.currentURI.host);
  };

  private onPopupHiding = () => {
    this.isInstalling[1](false);
    this.icon[1]("");
    this.title[1]("");
    this.description[1]("");
  };

  private closePopup = () => {
    const panel = document?.getElementById("ssb-panel") as XULElement & {
      hidePopup: () => void;
    };
    if (panel) {
      panel.hidePopup();
    }
    this.isInstalling[1](false);
  };

  private Render(): JSX.Element {
    const [isInstalling] = this.isInstalling;
    const [icon] = this.icon;
    const [title] = this.title;
    const [description] = this.description;
    const [canBeInstallAsPwa] = this.canBeInstallAsPwa;
    const [isInstalled] = this.isInstalled;

    return (
      <Show when={canBeInstallAsPwa() || isInstalled()}>
        <xul:hbox
          id="ssbPageAction"
          data-l10n-id="ssb-page-action"
          class="urlbar-page-action"
          popup="ssb-panel"
        >
          <xul:image
            id="ssbPageAction-image"
            class={`urlbar-icon ${isInstalled() ? "open-ssb" : ""}`}
          />
          <xul:panel
            id="ssb-panel"
            type="arrow"
            position="bottomright topright"
            onPopupShowing={this.onPopupShowing}
            onPopupHiding={this.onPopupHiding}
          >
            <xul:vbox id="ssb-box">
              <xul:vbox class="panel-header">
                <h1>
                  <span data-l10n-id="ssb-page-action-title" />
                  {isInstalled()
                    ? "アプリケーションを開く"
                    : "アプリケーションをインストール"}
                </h1>
              </xul:vbox>
              <xul:toolbarseparator />
              <xul:hbox id="ssb-content-hbox">
                <xul:vbox id="ssb-content-icon-vbox">
                  <img
                    id="ssb-content-icon"
                    width="48"
                    height="48"
                    alt="Site icon"
                    src={icon()}
                  />
                </xul:vbox>
                <xul:vbox id="ssb-content-label-vbox">
                  <h2>
                    <xul:label id="ssb-content-label" />
                    {title()}
                  </h2>
                  <xul:description id="ssb-content-description">
                    {description()}
                  </xul:description>
                </xul:vbox>
              </xul:hbox>
              <xul:hbox id="ssb-button-hbox">
                <xul:vbox id="ssb-installing-vbox">
                  <img
                    id="ssb-installing-icon"
                    hidden={!isInstalling()}
                    src="chrome://floorp/skin/icons/installing.gif"
                    width="48"
                    height="48"
                    alt="Installing indicator"
                  />
                </xul:vbox>
                <xul:button
                  id="ssb-app-install-button"
                  class="panel-button ssb-install-buttons footer-button primary"
                  hidden={isInstalling()}
                  onClick={this.onCommand}
                  label={
                    isInstalled() ? "アプリケーションを開く" : "インストール"
                  }
                />
                <xul:button
                  id="ssb-app-cancel-button"
                  class="panel-button ssb-install-buttons footer-button"
                  data-l10n-id="ssb-app-cancel-button"
                  hidden={isInstalling()}
                  onClick={this.closePopup}
                  label="キャンセル"
                />
              </xul:hbox>
            </xul:vbox>
          </xul:panel>
        </xul:hbox>
      </Show>
    );
  }
}
