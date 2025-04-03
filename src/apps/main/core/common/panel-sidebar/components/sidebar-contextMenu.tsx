/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { CPanelSidebar } from "./panel-sidebar";
import { createSignal, Show } from "solid-js";
import type { Panel } from "../utils/type";
import { ContextMenuUtils } from "@core/utils/context-menu.tsx";

export const [contextPanel, setContextPanel] = createSignal<Panel | null>(null);

export class SidebarContextMenuElem {

  ctx: CPanelSidebar
  constructor(ctx: CPanelSidebar) {
    this.ctx = ctx;
    ContextMenuUtils.addToolbarContentMenuPopupSet(() => this.sidebarContextMenu());
  }

  public contextPanelId: string | null = null;

  private getPanelByOriginalTarget(target: XULElement | null) {
    if (!target) {
      return;
    }

    let currentElement: Element | null = target as unknown as Element;
    let panelId: string | undefined;

    for (let i = 0; i < 10 && currentElement && !panelId; i++) {
      panelId = currentElement.getAttribute('data-panel-id') || undefined;

      if (!panelId && currentElement.parentElement) {
        currentElement = currentElement.parentElement;
      }
    }

    if (!panelId) {
      return;
    }

    const gPanelSidebar = this.ctx;
    return gPanelSidebar.getPanelData(panelId);
  }

  private handlePopupShowing(e: Event) {
    if (!e.explicitOriginalTarget) {
      return;
    }

    const panel = this.getPanelByOriginalTarget(
      e.explicitOriginalTarget as XULElement,
    );

    if (!panel) {
      return;
    }

    setContextPanel(panel);
  }

  private handlePopupHiding() {
    setTimeout(() => {
      setContextPanel(null);
    }, 0);
  }

  private safeExecuteCommand(callback: () => void) {
    try {
      callback();
    } catch (error) {
      console.error("コマンド実行エラー:", error);
    } finally {
      if (typeof document !== 'undefined' && document) {
        const contextMenu = document.getElementById("webpanel-context");
        if (contextMenu) {
          // @ts-ignore - Firefox特有のXUL APIなのでTypeScriptの型定義にない
          contextMenu.hidePopup();
        }
      }
    }
  }

  private handleUnloadCommand() {
    const gPanelSidebar = this.ctx;
    const panelId = contextPanel()?.id;
    if (panelId) {
      this.safeExecuteCommand(() => {
        gPanelSidebar.unloadPanel(panelId);
      });
    }
  }

  private handleDeleteCommand() {
    const gPanelSidebar = this.ctx;
    const panelId = contextPanel()?.id;
    if (panelId) {
      this.safeExecuteCommand(() => {
        gPanelSidebar.deletePanel(panelId);
      });
    }
  }

  private handleMuteCommand() {
    const gPanelSidebar = this.ctx;
    const panelId = contextPanel()?.id;
    if (panelId) {
      this.safeExecuteCommand(() => {
        gPanelSidebar.mutePanel(panelId);
      });
    }
  }

  private handleChangeZoomLevelCommand(type: "in" | "out" | "reset") {
    const gPanelSidebar = this.ctx;
    const panelId = contextPanel()?.id;
    if (panelId) {
      this.safeExecuteCommand(() => {
        gPanelSidebar.changeZoomLevel(panelId, type);
      });
    }
  }

  private handleChangeUserAgentCommand() {
    const gPanelSidebar = this.ctx;
    const panelId = contextPanel()?.id;
    if (panelId) {
      this.safeExecuteCommand(() => {
        // UAの変更処理を実装する必要がある
        // gPanelSidebar.changeUserAgent(panelId);
      });
    }
  }

  private sidebarContextMenu() {
    return (
      <xul:popupset>
        <xul:menupopup
          id="webpanel-context"
          onPopupShowing={(e) => this.handlePopupShowing(e)}
          onPopupHiding={() => this.handlePopupHiding()}
        >
          <xul:menuitem
            id="unloadWebpanelMenu"
            class="needLoadedWebpanel"
            data-l10n-id="sidebar2-unload-panel"
            label="Unload this webpanel"
            accesskey="U"
            onCommand={() => this.handleUnloadCommand()}
          />
          <Show when={contextPanel()?.type === "web"}>
            <xul:menuseparator class="context-webpanel-separator" />
            <xul:menuitem
              id="muteMenu"
              class="needLoadedWebpanel"
              data-l10n-id="sidebar2-mute-and-unmute"
              label="Mute/Unmute this webpanel"
              accesskey="M"
              onCommand={() => this.handleMuteCommand()}
            />
            <xul:menu
              id="changeZoomLevelMenu"
              class="needLoadedWebpanel needRunningExtensionsPanel"
              data-l10n-id="sidebar2-change-zoom-level"
              label="Change zoom level"
              accesskey="Z"
            >
              <xul:menupopup id="changeZoomLevelPopup">
                <xul:menuitem
                  id="zoomInMenu"
                  data-l10n-id="sidebar2-zoom-in"
                  label="Zoom in"
                  accesskey="I"
                  onCommand={() => this.handleChangeZoomLevelCommand("in")}
                />
                <xul:menuitem
                  id="zoomOutMenu"
                  data-l10n-id="sidebar2-zoom-out"
                  label="Zoom out"
                  accesskey="O"
                  onCommand={() => this.handleChangeZoomLevelCommand("out")}
                />
                <xul:menuitem
                  id="resetZoomMenu"
                  data-l10n-id="sidebar2-reset-zoom"
                  label="Reset zoom"
                  accesskey="R"
                  onCommand={() => this.handleChangeZoomLevelCommand("reset")}
                />
              </xul:menupopup>
            </xul:menu>
            <xul:menuitem
              id="changeUAWebpanelMenu"
              data-l10n-id="sidebar2-change-ua-panel"
              label="Switch User agent to Mobile/Desktop Version at this Webpanel"
              accesskey="R"
              onCommand={() => this.handleChangeUserAgentCommand()}
            />
          </Show>
          <xul:menuseparator class="context-webpanel-separator" />
          <xul:menuitem
            id="deleteWebpanelMenu"
            data-l10n-id="sidebar2-delete-panel"
            label="Delete this panel"
            accesskey="D"
            onCommand={() => this.handleDeleteCommand()}
          />
        </xul:menupopup>
      </xul:popupset>
    );
  }
}
