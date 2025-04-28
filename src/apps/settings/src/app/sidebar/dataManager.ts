import { rpc } from "@/lib/rpc/rpc.ts";
import type { PanelSidebarFormData } from "@/types/pref";
import type {
  Panel,
  Panels,
} from "../../../../main/core/common/panel-sidebar/utils/type";

export async function savePanelSidebarSettings(
  data: PanelSidebarFormData,
): Promise<void> {
  const { enabled, ...configData } = data;

  await Promise.all([
    rpc.setBoolPref("floorp.panelSidebar.enabled", enabled),
    rpc.setStringPref("floorp.panelSidebar.config", JSON.stringify(configData)),
  ]);
}

export async function getPanelSidebarSettings(): Promise<
  PanelSidebarFormData | null
> {
  const [enabled, configResult] = await Promise.all([
    rpc.getBoolPref("floorp.panelSidebar.enabled"),
    rpc.getStringPref("floorp.panelSidebar.config"),
  ]);

  if (!configResult) {
    return null;
  }

  const config = JSON.parse(configResult);
  return {
    enabled: enabled ?? true,
    autoUnload: config.autoUnload,
    position_start: config.position_start,
    globalWidth: config.globalWidth,
    displayed: config.displayed ?? false,
    webExtensionRunningEnabled: config.webExtensionRunningEnabled ?? false,
  };
}

/**
 * パネルの一覧データを取得します
 * @returns パネルの配列
 */
export async function getPanelsList(): Promise<Panels | null> {
  const result = await rpc.getStringPref("floorp.panelSidebar.data");
  if (!result) {
    return null;
  }

  try {
    const data = JSON.parse(result);
    return data.data || [];
  } catch (e) {
    console.error("Failed to parse panel sidebar data:", e);
    return null;
  }
}

/**
 * パネルの一覧を保存します
 * @param panels パネルの配列
 */
export async function savePanelsList(panels: Panels): Promise<void> {
  await rpc.setStringPref(
    "floorp.panelSidebar.data",
    JSON.stringify({ data: panels }),
  );
}

/**
 * 新しいパネルを追加します
 * @param panel 追加するパネル
 */
export async function addPanel(panel: Panel): Promise<void> {
  const panels = await getPanelsList();
  if (!panels) {
    return;
  }

  await savePanelsList([...panels, panel]);
}

/**
 * 既存のパネルを更新します
 * @param updatedPanel 更新するパネル
 */
export async function updatePanel(updatedPanel: Panel): Promise<void> {
  const panels = await getPanelsList();
  if (!panels) {
    return;
  }

  const updatedPanels = panels.map((panel) =>
    panel.id === updatedPanel.id ? updatedPanel : panel
  );

  await savePanelsList(updatedPanels);
}

/**
 * パネルを削除します
 * @param panelId 削除するパネルのID
 */
export async function deletePanel(panelId: string): Promise<void> {
  const panels = await getPanelsList();
  if (!panels) {
    return;
  }

  const filteredPanels = panels.filter((panel) => panel.id !== panelId);
  await savePanelsList(filteredPanels);
}

// データ取得の重複防止用フラグ
let isGettingContainers = false;
let isGettingStaticPanels = false;
let isGettingExtensionPanels = false;

// キャッシュ用変数
let containersCache: any[] = [];
let staticPanelsCache: any[] = [];
let extensionPanelsCache: any[] = [];

/**
 * コンテナーの一覧を取得します
 * @returns コンテナーのリスト
 */
export async function getContainers() {
  console.log("getContainers called");

  // すでにキャッシュがあれば返す
  if (containersCache.length > 0) {
    console.log("Using containers cache", containersCache.length);
    return containersCache;
  }

  // 重複実行防止
  if (isGettingContainers) {
    console.log("Already getting containers, waiting...");
    return new Promise((resolve) => {
      const checkInterval = setInterval(() => {
        if (!isGettingContainers && containersCache.length > 0) {
          clearInterval(checkInterval);
          resolve(containersCache);
        }
      }, 100);
    });
  }

  isGettingContainers = true;

  // windowオブジェクトのNRGetContainerContextsが存在するか確認
  if (typeof (window as any).NRGetContainerContexts !== "function") {
    console.error("NRGetContainerContexts is not available");
    isGettingContainers = false;
    return [];
  }

  try {
    return new Promise((resolve) => {
      // タイムアウト処理を追加
      const timeoutId = setTimeout(() => {
        console.warn("getContainers timed out");
        isGettingContainers = false;
        resolve([]);
      }, 5000);

      const callback = (containersStr: string) => {
        clearTimeout(timeoutId);
        console.log("Container callback received data");
        try {
          const containers = JSON.parse(containersStr);
          console.log(`Parsed ${containers.length} containers`, containers);

          // 形式変換: {id, label, icon} → {id, name, label, icon, color}
          const formattedContainers = containers.map((container: any) => {
            // IDの変換処理を修正
            let containerId = 0;
            if (container.id === "0") {
              containerId = 0;
            } else {
              try {
                containerId = Number.parseInt(container.id, 10);
                // 数値に変換できない場合は元の文字列を保持
                if (isNaN(containerId)) {
                  console.warn(
                    `Invalid container ID: ${container.id}, using original string`,
                  );
                  containerId = container.id;
                }
              } catch (error) {
                console.error(
                  `Error parsing container ID: ${container.id}`,
                  error,
                );
                containerId = container.id;
              }
            }

            return {
              id: containerId,
              name: container.label,
              label: container.label,
              icon: container.icon || "",
              color: container.color || "",
            };
          });

          // キャッシュに保存
          containersCache = formattedContainers;
          isGettingContainers = false;

          resolve(formattedContainers);
        } catch (error) {
          console.error("Failed to parse containers:", error);
          isGettingContainers = false;
          resolve([]);
        }
      };

      console.log("Calling NRGetContainerContexts");
      (window as any).NRGetContainerContexts(callback);
    });
  } catch (error) {
    console.error("Failed to get containers:", error);
    isGettingContainers = false;
    return [];
  }
}

/**
 * 静的パネルの一覧を取得します
 * @returns 静的パネルのリスト
 */
export async function getStaticPanels() {
  console.log("getStaticPanels called");

  // すでにキャッシュがあれば返す
  if (staticPanelsCache.length > 0) {
    console.log("Using static panels cache", staticPanelsCache.length);
    return staticPanelsCache;
  }

  // 重複実行防止
  if (isGettingStaticPanels) {
    console.log("Already getting static panels, waiting...");
    return new Promise((resolve) => {
      const checkInterval = setInterval(() => {
        if (!isGettingStaticPanels && staticPanelsCache.length > 0) {
          clearInterval(checkInterval);
          resolve(staticPanelsCache);
        }
      }, 100);
    });
  }

  isGettingStaticPanels = true;

  // windowオブジェクトのNRGetStaticPanelsが存在するか確認
  if (typeof (window as any).NRGetStaticPanels !== "function") {
    console.error("NRGetStaticPanels is not available");
    isGettingStaticPanels = false;
    return [];
  }

  try {
    return new Promise((resolve) => {
      // タイムアウト処理を追加
      const timeoutId = setTimeout(() => {
        console.warn("getStaticPanels timed out");
        isGettingStaticPanels = false;
        resolve([]);
      }, 5000);

      const callback = (panelsStr: string) => {
        clearTimeout(timeoutId);
        console.log("Static panels callback received data");
        try {
          const panels = JSON.parse(panelsStr);
          console.log(`Parsed ${panels.length} static panels`);

          const formattedPanels = panels.map((panel: any) => ({
            value: panel.id,
            label: panel.title,
            icon: panel.icon || "",
          }));

          staticPanelsCache = formattedPanels;
          isGettingStaticPanels = false;

          resolve(formattedPanels);
        } catch (error) {
          console.error("Failed to parse static panels:", error);
          isGettingStaticPanels = false;
          resolve([]);
        }
      };

      console.log("Calling NRGetStaticPanels");
      (window as any).NRGetStaticPanels(callback);
    });
  } catch (error) {
    console.error("Failed to get static panels:", error);
    isGettingStaticPanels = false;
    return [];
  }
}

/**
 * 拡張機能パネルの一覧を取得します
 * @returns 拡張機能パネルのリスト
 */
export async function getExtensionPanels() {
  console.log("getExtensionPanels called");

  if (extensionPanelsCache.length > 0) {
    console.log("Using extension panels cache", extensionPanelsCache.length);
    return extensionPanelsCache;
  }

  if (isGettingExtensionPanels) {
    return new Promise((resolve) => {
      const checkInterval = setInterval(() => {
        if (!isGettingExtensionPanels && extensionPanelsCache.length > 0) {
          clearInterval(checkInterval);
          resolve(extensionPanelsCache);
        }
      }, 100);
    });
  }

  isGettingExtensionPanels = true;

  // windowオブジェクトのNRGetExtensionPanelsが存在するか確認
  if (typeof (window as any).NRGetExtensionPanels !== "function") {
    console.error("NRGetExtensionPanels is not available");
    isGettingExtensionPanels = false;
    return [];
  }

  try {
    return new Promise((resolve) => {
      const timeoutId = setTimeout(() => {
        console.warn("getExtensionPanels timed out");
        isGettingExtensionPanels = false;
        resolve([]);
      }, 5000);

      const callback = (extensionsStr: string) => {
        clearTimeout(timeoutId);
        try {
          const extensions = JSON.parse(extensionsStr);

          const formattedExtensions = extensions.map((ext: any) => ({
            extensionId: ext.value,
            title: ext.label,
            iconUrl: ext.icon || "",
          }));

          extensionPanelsCache = formattedExtensions;
          isGettingExtensionPanels = false;

          resolve(formattedExtensions);
        } catch (error) {
          console.error("Failed to parse extension panels:", error);
          isGettingExtensionPanels = false;
          resolve([]);
        }
      };

      console.log("Calling NRGetExtensionPanels");
      (window as any).NRGetExtensionPanels(callback);
    });
  } catch (error) {
    console.error("Failed to get extension panels:", error);
    isGettingExtensionPanels = false;
    return [];
  }
}
