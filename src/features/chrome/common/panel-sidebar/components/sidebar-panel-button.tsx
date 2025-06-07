/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Show, createResource, Suspense } from "solid-js";
import { getFaviconURLForPanel } from "../utils/favicon-getter";
import { CPanelSidebar } from "./panel-sidebar";
import { selectedPanelId, panelSidebarData, setPanelSidebarData } from "../data/data";
import type { Panel } from "../utils/type";
import { isExtensionExist } from "../extension-panels";
import { getUserContextColor } from "../utils/userContextColor-getter";


export function PanelSidebarButton(props: { panel: Panel, ctx:CPanelSidebar}) {
  const gPanelSidebar = props.ctx;

  const [faviconURL] = createResource(()=>props.panel,getFaviconURLForPanel);

  const handleDragStart = (e: DragEvent) => {
    e.dataTransfer?.setData("text/plain", props.panel.id);
    (e.target as HTMLElement).classList.add("dragging");
  };

  const handleDragEnd = (e: DragEvent) => {
    (e.target as HTMLElement).classList.remove("dragging");
  };

  const handleDragOver = (e: DragEvent) => {
    e.preventDefault();
    (e.target as HTMLElement).classList.add("drag-over");
  };

  const handleDragLeave = (e: DragEvent) => {
    (e.target as HTMLElement).classList.remove("drag-over");
  };

  const handleDrop = (e: DragEvent) => {
    e.preventDefault();
    (e.target as HTMLElement).classList.remove("drag-over");

    const sourceId = e.dataTransfer?.getData("text/plain");
    const targetId = props.panel.id;

    if (sourceId === targetId) return;

    const panels = [...panelSidebarData()];
    const sourceIndex = panels.findIndex((p) => p.id === sourceId);
    const targetIndex = panels.findIndex((p) => p.id === targetId);

    const [movedPanel] = panels.splice(sourceIndex, 1);
    panels.splice(targetIndex, 0, movedPanel);

    setPanelSidebarData(panels);
  };

  if (
    props.panel.type === "extension" &&
    !isExtensionExist(props.panel.extensionId as string)
  ) {
    return null;
  }

  return (
    <div
      draggable={true}
      onDragStart={handleDragStart}
      onDragEnd={handleDragEnd}
      onDragOver={handleDragOver}
      onDragLeave={handleDragLeave}
      onDrop={handleDrop}
    >
      <div
        id={props.panel.id}
        class={`${props.panel.type} panel-sidebar-panel`}
        data-checked={selectedPanelId() === props.panel.id}
        data-panel-id={props.panel.id}
        onClick={() => {
          gPanelSidebar.changePanel(props.panel.id);
        }}
        style={{display:"flex","align-items":"center","justify-content":"center"}}
      >
      <Suspense>
        <img src={faviconURL()} width="16" height="16" />
        </Suspense>
      </div>
      <Show
        when={
          props.panel.userContextId !== 0 &&
          props.panel.userContextId !== null &&
          props.panel.type === "web"
        }
      >
        <xul:box
          class="panel-sidebar-user-context-border"
          style={{
            "background-color": getUserContextColor(props.panel.userContextId ?? 0),
          }}
        />
      </Show>
    </div>
  );
}
