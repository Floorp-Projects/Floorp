/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createSignal, For, onCleanup } from "solid-js";
import type { JSX } from "solid-js";
import { render, createRootHMR } from "@nora/solid-xul";
import type { Manifest } from "./type";
import type { PwaService } from "./pwaService";

export class SsbPanelView {
  private static installedApps = createSignal<Manifest[]>([]);
  private static pwaService: PwaService;
  private isOpen = createSignal<boolean>(false);
  private isRendered = false;

  constructor(pwaService: PwaService) {
    SsbPanelView.pwaService = pwaService;
    if (!this.panelUIButton) return;

    createRootHMR(() => {
      const [, setIsOpen] = this.isOpen;
      const observer = new MutationObserver((mutations) => {
        mutations.forEach((mutation) => {
          if (
            mutation.type === "attributes" &&
            mutation.attributeName === "open"
          ) {
            const isOpened = this.panelUIButton?.getAttribute("open") === "true";
            setIsOpen(isOpened);

            if (isOpened && !this.isRendered) {
              this.renderPanel();
            }
          }
        });
      });

      observer.observe(this.panelUIButton!, {
        attributes: true,
      });

      onCleanup(() => observer.disconnect());
    }, import.meta.hot);
  }

  private get parentElement(): HTMLElement | null {
    return document?.querySelector(
      "#appMenu-mainView > .panel-subview-body"
    ) as HTMLElement | null;
  }

  private get beforeElement(): HTMLElement | null {
    return document?.getElementById(
      "appMenu-bookmarks-button"
    ) as HTMLElement | null;
  }

  private get panelUIButton(): HTMLElement | null {
    return document?.getElementById(
      "PanelUI-menu-button"
    ) as HTMLElement | null;
  }

  private renderPanel(): void {
    if (!this.parentElement || !this.beforeElement) return;

    this.isRendered = true;
    render(() => <SsbPanelView.Render />, this.parentElement, {
      marker: this.beforeElement,
    });
  }

  private static async showSsbPanelSubView() {
    await window.PanelUI.showSubView(
      "PanelUI-ssb",
      document?.getElementById("appMenu-ssb-button")
    );

    SsbPanelView.updateInstalledApps();
  }

  private static async updateInstalledApps() {
    const [, setInstalledApps] = SsbPanelView.installedApps;
    const apps = await SsbPanelView.pwaService.getInstalledApps();
    setInstalledApps(
      Object.values(apps).map((value) => ({ ...(value as Manifest) }))
    );
  }

  private static handleInstallOrRunCurrentPageAsSsb() {
    SsbPanelView.pwaService.installOrRunCurrentPageAsSsb(
      window.gBrowser.selectedBrowser,
      false
    );
  }

  private static InstalledAppsList(): JSX.Element {
    const [apps] = SsbPanelView.installedApps;
    return (
      <For each={apps()}>
        {(app) => (
          <xul:toolbarbutton
            id={`ssb-${app.id}`}
            class="subviewbutton ssb-app-info-button"
            label={app.name}
            image={app.icon}
            data-ssbId={app.id}
            onCommand={() => {
              SsbPanelView.pwaService.runSsbByUrl(app.start_url);
            }}
          />
        )}
      </For>
    );
  }

  public static Render(): JSX.Element {
    return (
      <>
        <xul:toolbarbutton
          id="appMenu-ssb-button"
          class="subviewbutton subviewbutton-nav"
          data-l10n-id="appmenuitem-webapps"
          label="Web Apps"
          closemenu="none"
          onCommand={() => SsbPanelView.showSsbPanelSubView()}
        />
        <xul:panelview id="PanelUI-ssb">
          <xul:vbox id="ssb-subview-body" class="panel-subview-body">
            <xul:toolbarbutton
              id="appMenu-install-or-open-ssb-current-page-button"
              class="subviewbutton"
              data-l10n-id="appmenuitem-install-current-page"
              label="Install Current Page as Progressive Web App"
              onCommand={() => SsbPanelView.handleInstallOrRunCurrentPageAsSsb()}
            />
            <xul:toolbarseparator />
            <h2
              id="panelMenu_openInstalledApps"
              data-l10n-id="appmenu-open-installed-apps-subheader"
              class="subview-subheader"
              aria-label="Open Installed Apps"
            >
              Installed Apps
            </h2>
            <xul:toolbaritem
              id="panelMenu_installedSsbMenu"
              orient="vertical"
              smoothscroll={false}
              flatList={true}
              tooltip="bhTooltip"
              context="ssbInstalledAppMenu-context"
              aria-labelledby="panelMenu_openInstalledApps"
            >
              <SsbPanelView.InstalledAppsList />
            </xul:toolbaritem>
          </xul:vbox>
          <xul:toolbarseparator hidden={true} />
          <xul:toolbarbutton
            id="PanelUI-openManageSsbPage"
            class="subviewbutton panel-subview-footer-button"
            data-l10n-id="floorp-open-manage-ssb-page"
            hidden={true}
          />
        </xul:panelview>
      </>
    );
  }
}
