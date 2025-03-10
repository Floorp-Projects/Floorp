/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createEffect, createSignal } from "solid-js";
import { panelSidebarConfig, setIsFloatingDragging } from "../data/data.ts";

export const [isResizeCooldown, setIsResizeCooldown] = createSignal<boolean>(
  false,
);
let resizeCooldownTimer: number | null = null;

export function FloatingSplitter() {
  const onMouseDown = (e: MouseEvent) => {
    setIsFloatingDragging(true);
    const sidebarBox = document?.getElementById(
      "panel-sidebar-box",
    ) as XULElement;

    if (!sidebarBox) {
      return;
    }

    const startX = e.clientX;
    const startWidth = sidebarBox.getBoundingClientRect().width;
    const position = panelSidebarConfig().position_start;

    const onMouseMove = (e: MouseEvent) => {
      const deltaX = e.clientX - startX;
      let newWidth: number;
      if (position) {
        newWidth = Math.max(
          225,
          Math.min(startWidth - deltaX, window.innerWidth * 0.8),
        );
      } else {
        newWidth = Math.max(
          225,
          Math.min(startWidth + deltaX, window.innerWidth * 0.8),
        );
      }
      sidebarBox.style.setProperty("width", `${newWidth}px`);
    };

    const onMouseUp = () => {
      setIsFloatingDragging(false);
      document?.removeEventListener("mousemove", onMouseMove);
      document?.removeEventListener("mouseup", onMouseUp);

      setIsResizeCooldown(true);

      if (resizeCooldownTimer !== null) {
        clearTimeout(resizeCooldownTimer);
      }

      resizeCooldownTimer = globalThis.setTimeout(() => {
        setIsResizeCooldown(false);
        resizeCooldownTimer = null;
      }, 1000);
    };

    document?.addEventListener("mousemove", onMouseMove);
    document?.addEventListener("mouseup", onMouseUp);
  };

  createEffect(() => {
    const position = panelSidebarConfig().position_start;
    if (position) {
      document
        ?.getElementById("floating-splitter-side")
        ?.setAttribute("data-floating-splitter-side", "start");
    } else {
      document
        ?.getElementById("floating-splitter-side")
        ?.setAttribute("data-floating-splitter-side", "end");
    }
  });

  return (
    <>
      <xul:box
        id="floating-splitter-side"
        class="floating-splitter-side"
        part="floating-splitter-side"
        onMouseDown={onMouseDown}
      />
    </>
  );
}
