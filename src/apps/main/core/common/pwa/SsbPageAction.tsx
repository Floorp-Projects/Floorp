/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createSignal, Show } from "solid-js";
import type { JSX } from "solid-js";
import { SiteSpecificBrowserManager } from "./ssbManager";
import { ManifestProcesser } from "./manifestProcesser";
import { render } from "@nora/solid-xul";

const [isInstalling, setIsInstalling] = createSignal(false);
const [icon, setIcon] = createSignal("");
const [title, setTitle] = createSignal("");
const [description, setDescription] = createSignal("");
const [canBeInstallAsPwa, setCanBeInstallAsPwa] = createSignal(false);
const [isInstalled, setIsInstalled] = createSignal(false);

export class SsbPageAction {
  private static instance: SsbPageAction;
  public static getInstance(): SsbPageAction {
    if (!SsbPageAction.instance) {
      SsbPageAction.instance = new SsbPageAction();
    }
    return SsbPageAction.instance;
  }

  constructor() {
    const starButtonBox = document?.getElementById("star-button-box");
    const ssbPageAction = document?.getElementById("page-action-buttons");
    if (!starButtonBox || !ssbPageAction) return;

    render(() => <this.Render />, ssbPageAction, {
      // biome-ignore lint/suspicious/noExplicitAny: <explanation>
      marker: starButtonBox,
    });

    Services.obs.addObserver(
      () => this.onCheckPageHasManifest(),
      "nora-pwa-check-page-has-manifest",
    );
    window.gBrowser.tabContainer.addEventListener(
      "TabSelect",
      this.onCheckPageHasManifest,
    );
  }

  private async onCheckPageHasManifest() {
    const browser = window.gBrowser.selectedBrowser;
    const canBeInstallAsPwa =
      await ManifestProcesser.getInstance().checkBrowserCanBeInstallAsPwa(
        browser,
      );
    setCanBeInstallAsPwa(canBeInstallAsPwa);

    const isInstalled =
      await SiteSpecificBrowserManager.getInstance().checkCurrentPageIsInstalled(
        browser,
      );
    setIsInstalled(isInstalled);
    SiteSpecificBrowserManager.getInstance().updateUIElements(isInstalled);
  }

  private static onCommand() {
    SiteSpecificBrowserManager.getInstance().installOrRunCurrentPageAsSsb(
      window.gBrowser.selectedBrowser,
      true,
    );
    setIsInstalling(true);
  }

  private static async onPopupShowing() {
    const icon = await SiteSpecificBrowserManager.getInstance().getIcon(
      window.gBrowser.selectedBrowser,
    );
    setIcon(icon);

    const manifest = await SiteSpecificBrowserManager.getInstance().getManifest(
      window.gBrowser.selectedBrowser,
    );
    setTitle(manifest.name ?? window.gBrowser.selectedBrowser.currentURI.spec);
    setDescription(window.gBrowser.selectedBrowser.currentURI.host);
  }

  private static onPopupHiding() {
    setIsInstalling(false);
    setIcon("");
    setTitle("");
    setDescription("");
  }

  private static closePopup() {
    const panel = document?.getElementById("ssb-panel") as XULElement & {
      hidePopup: () => void;
    };
    if (panel) {
      panel.hidePopup();
    }
    setIsInstalling(false);
  }

  public Render(): JSX.Element {
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
            onPopupShowing={SsbPageAction.onPopupShowing}
            onPopupHiding={SsbPageAction.onPopupHiding}
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
                    {/* biome-ignore lint/a11y/noLabelWithoutControl: <explanation> */}
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
                  onClick={SsbPageAction.onCommand}
                  label={
                    isInstalled() ? "アプリケーションを開く" : "インストール"
                  }
                />
                <xul:button
                  id="ssb-app-cancel-button"
                  class="panel-button ssb-install-buttons footer-button"
                  data-l10n-id="ssb-app-cancel-button"
                  hidden={isInstalling()}
                  onClick={SsbPageAction.closePopup}
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
