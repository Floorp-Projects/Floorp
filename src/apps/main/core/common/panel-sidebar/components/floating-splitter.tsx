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
  const onHorizontalMouseDown = (e: MouseEvent) => {
    setIsFloatingDragging(true);
    const sidebarBox = document?.getElementById(
      "panel-sidebar-box",
    ) as XULElement;

    if (!sidebarBox) {
      return;
    }

    const startX = e.clientX;
    const startWidth = sidebarBox.getBoundingClientRect().width;
    const startLeft = parseInt(sidebarBox.style.getPropertyValue("left") || "0", 10) ||
      sidebarBox.getBoundingClientRect().left;
    const isLeftSide = (e.target as HTMLElement).classList.contains("floating-splitter-left");

    const onMouseMove = (e: MouseEvent) => {
      const deltaX = e.clientX - startX;
      let newWidth: number;

      if (isLeftSide) {
        newWidth = Math.max(
          225,
          Math.min(startWidth - deltaX, window.innerWidth * 0.8),
        );
        const newLeft = Math.max(0, Math.min(window.innerWidth - newWidth, startLeft + deltaX));
        sidebarBox.style.setProperty("left", `${newLeft}px`);
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

  const onVerticalMouseDown = (e: MouseEvent) => {
    setIsFloatingDragging(true);
    const sidebarBox = document?.getElementById(
      "panel-sidebar-box",
    ) as XULElement;

    if (!sidebarBox) {
      return;
    }

    const startY = e.clientY;
    const startHeight = sidebarBox.getBoundingClientRect().height;
    const startTop = parseInt(sidebarBox.style.getPropertyValue("top") || "0", 10) ||
      sidebarBox.getBoundingClientRect().top;
    const isTopSide = (e.target as HTMLElement).classList.contains("floating-splitter-top");

    const onMouseMove = (e: MouseEvent) => {
      const deltaY = e.clientY - startY;
      let newHeight: number;

      if (isTopSide) {
        newHeight = Math.max(
          200,
          Math.min(startHeight - deltaY, window.innerHeight * 0.9),
        );
        const newTop = Math.max(0, Math.min(window.innerHeight - newHeight, startTop + deltaY));
        sidebarBox.style.setProperty("top", `${newTop}px`);
      } else {
        newHeight = Math.max(
          200,
          Math.min(startHeight + deltaY, window.innerHeight * 0.9),
        );
      }

      sidebarBox.style.setProperty("height", `${newHeight}px`);
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

  const onDiagonalMouseDown = (e: MouseEvent) => {
    setIsFloatingDragging(true);
    const sidebarBox = document?.getElementById(
      "panel-sidebar-box",
    ) as XULElement;

    if (!sidebarBox) {
      return;
    }

    const startX = e.clientX;
    const startY = e.clientY;
    const startWidth = sidebarBox.getBoundingClientRect().width;
    const startHeight = sidebarBox.getBoundingClientRect().height;
    const startLeft = parseInt(sidebarBox.style.getPropertyValue("left") || "0", 10) ||
      sidebarBox.getBoundingClientRect().left;
    const startTop = parseInt(sidebarBox.style.getPropertyValue("top") || "0", 10) ||
      sidebarBox.getBoundingClientRect().top;
    const target = e.target as HTMLElement;

    const isTopLeft = target.classList.contains("floating-splitter-corner-topleft");
    const isTopRight = target.classList.contains("floating-splitter-corner-topright");
    const isBottomLeft = target.classList.contains("floating-splitter-corner-bottomleft");

    const onMouseMove = (e: MouseEvent) => {
      const deltaX = e.clientX - startX;
      const deltaY = e.clientY - startY;

      let newWidth: number;
      if (isTopLeft || isBottomLeft) {
        newWidth = Math.max(
          225,
          Math.min(startWidth - deltaX, window.innerWidth * 0.8),
        );
        const newLeft = Math.max(0, Math.min(window.innerWidth - newWidth, startLeft + deltaX));
        sidebarBox.style.setProperty("left", `${newLeft}px`);
      } else {
        newWidth = Math.max(
          225,
          Math.min(startWidth + deltaX, window.innerWidth * 0.8),
        );
      }

      let newHeight: number;
      if (isTopLeft || isTopRight) {
        newHeight = Math.max(
          200,
          Math.min(startHeight - deltaY, window.innerHeight * 0.9),
        );
        const newTop = Math.max(0, Math.min(window.innerHeight - newHeight, startTop + deltaY));
        sidebarBox.style.setProperty("top", `${newTop}px`);
      } else {
        newHeight = Math.max(
          200,
          Math.min(startHeight + deltaY, window.innerHeight * 0.9),
        );
      }

      sidebarBox.style.setProperty("width", `${newWidth}px`);
      sidebarBox.style.setProperty("height", `${newHeight}px`);
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
        ?.getElementById("floating-splitter-right")
        ?.setAttribute("data-floating-splitter-side", "start");
      document
        ?.getElementById("floating-splitter-corner-bottomright")
        ?.setAttribute("data-floating-splitter-corner", "start");
    } else {
      document
        ?.getElementById("floating-splitter-right")
        ?.setAttribute("data-floating-splitter-side", "end");
      document
        ?.getElementById("floating-splitter-corner-bottomright")
        ?.setAttribute("data-floating-splitter-corner", "end");
    }
  });

  return (
    <>
      <xul:box
        id="floating-splitter-left"
        class="floating-splitter-side floating-splitter-left"
        part="floating-splitter-side"
        onMouseDown={onHorizontalMouseDown}
      />
      <xul:box
        id="floating-splitter-right"
        class="floating-splitter-side floating-splitter-right"
        part="floating-splitter-side"
        onMouseDown={onHorizontalMouseDown}
      />

      <xul:box
        id="floating-splitter-top"
        class="floating-splitter-vertical floating-splitter-top"
        part="floating-splitter-vertical"
        onMouseDown={onVerticalMouseDown}
      />
      <xul:box
        id="floating-splitter-bottom"
        class="floating-splitter-vertical floating-splitter-bottom"
        part="floating-splitter-vertical"
        onMouseDown={onVerticalMouseDown}
      />

      <xul:box
        id="floating-splitter-corner-topleft"
        class="floating-splitter-corner floating-splitter-corner-topleft"
        part="floating-splitter-corner"
        onMouseDown={onDiagonalMouseDown}
      />
      <xul:box
        id="floating-splitter-corner-topright"
        class="floating-splitter-corner floating-splitter-corner-topright"
        part="floating-splitter-corner"
        onMouseDown={onDiagonalMouseDown}
      />
      <xul:box
        id="floating-splitter-corner-bottomleft"
        class="floating-splitter-corner floating-splitter-corner-bottomleft"
        part="floating-splitter-corner"
        onMouseDown={onDiagonalMouseDown}
      />
      <xul:box
        id="floating-splitter-corner-bottomright"
        class="floating-splitter-corner floating-splitter-corner-bottomright"
        part="floating-splitter-corner"
        onMouseDown={onDiagonalMouseDown}
      />
    </>
  );
}
