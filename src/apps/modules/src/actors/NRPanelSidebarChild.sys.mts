/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class NRPanelSidebarChild extends JSWindowActorChild {
  actorCreated() {
    console.debug("NRPanelSidebarChild created!");
    const window = this.contentWindow;
    if (
      window?.location.port === "5183" ||
      window?.location.href.startsWith("chrome://noraneko-settings") ||
      window?.location.href.startsWith("about:")
    ) {
      console.debug("NRPanelSidebarChild activated for settings page!");
      Cu.exportFunction(this.GetContainerContexts.bind(this), window, {
        defineAs: "NRGetContainerContexts",
      });
      Cu.exportFunction(this.GetStaticPanels.bind(this), window, {
        defineAs: "NRGetStaticPanels",
      });
      Cu.exportFunction(this.GetExtensionPanels.bind(this), window, {
        defineAs: "NRGetExtensionPanels",
      });
    }
  }

  // コンテナー情報を取得
  GetContainerContexts(callback: (containers: string) => void = () => {}) {
    console.log("GetContainerContexts called");
    const promise = new Promise<string>((resolve, _reject) => {
      this.resolveGetContainerContexts = resolve;
    });
    this.sendAsyncMessage("NRPanelSidebar:GetContainerContexts");
    promise.then((containers) => {
      console.log(
        "Received container data:",
        containers.substring(0, 100) + "...",
      );
      callback(containers);
    });
  }

  resolveGetContainerContexts: ((containers: string) => void) | null = null;

  // 静的パネル情報を取得（手打ちデータ）
  GetStaticPanels(callback: (panels: string) => void = () => {}) {
    console.log("GetStaticPanels called");
    // 静的パネルの情報を手動で定義
    const staticPanels = [
      {
        id: "sidebar",
        title: "サイドバー",
        icon: "chrome://browser/skin/sidebars.svg",
      },
      {
        id: "bookmarks",
        title: "ブックマーク",
        icon: "chrome://browser/skin/bookmark.svg",
      },
      {
        id: "history",
        title: "履歴",
        icon: "chrome://browser/skin/history.svg",
      },
      {
        id: "synced-tabs",
        title: "同期したタブ",
        icon: "chrome://browser/skin/tab.svg",
      },
    ];
    // 直接コールバックを呼び出し
    const panelsStr = JSON.stringify(staticPanels);
    console.log("Static panels data:", panelsStr.substring(0, 100) + "...");
    callback(panelsStr);
  }

  // 拡張機能パネル情報を取得
  GetExtensionPanels(callback: (extensions: string) => void = () => {}) {
    console.log("GetExtensionPanels called");
    const promise = new Promise<string>((resolve, _reject) => {
      this.resolveGetExtensionPanels = resolve;
    });
    this.sendAsyncMessage("NRPanelSidebar:GetExtensionPanels");
    promise.then((extensions) => {
      console.log(
        "Received extensions data:",
        extensions.substring(0, 100) + "...",
      );
      callback(extensions);
    });
  }

  resolveGetExtensionPanels: ((extensions: string) => void) | null = null;

  receiveMessage(message: { name: string; data: any }) {
    console.log("NRPanelSidebarChild receiveMessage:", message.name);

    switch (message.name) {
      case "NRPanelSidebar:GetContainerContexts": {
        console.log("Resolving container contexts with data");
        this.resolveGetContainerContexts?.(message.data);
        this.resolveGetContainerContexts = null;
        break;
      }
      case "NRPanelSidebar:GetExtensionPanels": {
        console.log("Resolving extension panels with data");
        this.resolveGetExtensionPanels?.(message.data);
        this.resolveGetExtensionPanels = null;
        break;
      }
    }
  }
}
