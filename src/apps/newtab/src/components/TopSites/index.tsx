import { type MouseEvent, useEffect, useRef, useState } from "react";
import {
  getNewTabSettings,
  type NewTabSettings,
  type PinnedSite,
  saveNewTabSettings,
} from "@/utils/dataManager.ts";
import { useTranslation } from "react-i18next";
import { PinIcon } from "lucide-react";
import { callNRWithRetry } from "@/utils/nrRetry.ts";

interface TopSite {
  url: string;
  title: string;
  label?: string;
  favicon?: string;
  smallFavicon?: string | null;
}

declare global {
  interface Window {
    NRGetCurrentTopSites: (callback: (data: string) => void) => void;
  }
}

async function getTopSites(): Promise<TopSite[]> {
  try {
    console.log("🔍 [TopSites Debug] Starting to get top sites...");
    const nrFunction = (globalThis as unknown as Window).NRGetCurrentTopSites;
    console.log(
      "🔍 [TopSites Debug] NRGetCurrentTopSites function exists:",
      !!nrFunction,
    );
    console.log(
      "🔍 [TopSites Debug] NRGetCurrentTopSites type:",
      typeof nrFunction,
    );

    const parsed = await callNRWithRetry<{ topsites?: TopSite[] | undefined }>(
      (cb) => (globalThis as unknown as Window).NRGetCurrentTopSites(cb),
      (sites) => {
        console.log("🔍 [TopSites Debug] Raw sites data received:", sites);
        try {
          const parsed = JSON.parse(sites);
          console.log("🔍 [TopSites Debug] Parsed sites data:", parsed);
          return parsed;
        } catch (e) {
          console.error("🔍 [TopSites Debug] Failed to parse sites data:", e);
          throw e;
        }
      },
      {
        retries: 3,
        timeoutMs: 1200,
        delayMs: 300,
        shouldRetry: (res) => {
          const shouldRetry = !res || !Array.isArray(res.topsites);
          console.log(
            "🔍 [TopSites Debug] Should retry:",
            shouldRetry,
            "Response:",
            res,
          );
          return shouldRetry;
        },
      },
    );

    console.log("🔍 [TopSites Debug] Final parsed data:", parsed);
    console.log("🔍 [TopSites Debug] Topsites array:", parsed.topsites);
    console.log(
      "🔍 [TopSites Debug] Topsites length:",
      parsed.topsites?.length ?? 0,
    );

    const list = (parsed.topsites ?? []).map((site: TopSite) => ({
      ...site,
      title: site.title || site.label || site.url,
      smallFavicon: site.smallFavicon || "",
    }));

    console.log("🔍 [TopSites Debug] Processed sites list:", list);
    console.log("🔍 [TopSites Debug] Processed sites count:", list.length);

    return list;
  } catch (e) {
    console.error(
      "🔍 [TopSites Debug] Failed to get top sites after retries:",
      e,
    );
    return [];
  }
}

function BlockModal({
  site,
  onConfirm,
  onCancel,
}: {
  site: TopSite | PinnedSite;
  onConfirm: () => void;
  onCancel: () => void;
}) {
  const { t } = useTranslation();
  return (
    <dialog className="modal modal-open">
      <div className="modal-box">
        <h3 className="font-bold text-lg">
          {t("topSites.blockTitle", {
            defaultValue: `Block "${site.title}"?`,
          })}
        </h3>
        <p className="py-4">
          {t(
            "topSites.blockConfirm",
            "This site will be removed from your Top Sites and won't be suggested again. You can manage blocked sites in settings.",
          )}
        </p>
        <div className="modal-action">
          <button type="button" onClick={onConfirm} className="btn btn-primary">
            {t("topSites.block")}
          </button>
          <button type="button" onClick={onCancel} className="btn">
            {t("topSites.cancel")}
          </button>
        </div>
      </div>
    </dialog>
  );
}

function AddSiteModal({
  onAdd,
  onCancel,
}: {
  onAdd: (url: string, label: string) => void;
  onCancel: () => void;
}) {
  const { t } = useTranslation();
  const [url, setUrl] = useState("");
  const [label, setLabel] = useState("");
  const [error, setError] = useState<string | null>(null);
  const urlInputRef = useRef<HTMLInputElement>(null);

  useEffect(() => {
    urlInputRef.current?.focus();
  }, []);

  const normalizeUrl = (raw: string): string | null => {
    let value = raw.trim();
    if (!value) return null;
    if (!/^https?:\/\//i.test(value)) {
      value = `https://${value}`;
    }
    try {
      const u = new URL(value);
      // 不要な末尾スラッシュを除去（ルートのみ維持）
      if (u.pathname !== "/") {
        u.pathname = u.pathname.replace(/\/+$/, "");
      }
      return u.href;
    } catch {
      return null;
    }
  };

  const handleSubmit = (e: React.FormEvent) => {
    e.preventDefault();
    const normalized = normalizeUrl(url);
    if (!normalized) {
      setError(t("topSites.invalidUrl", { defaultValue: "Invalid URL" }));
      return;
    }
    onAdd(normalized, label.trim() || normalized);
  };

  return (
    <dialog className="modal modal-open">
      <div className="modal-box">
        <h3 className="font-bold text-lg mb-2">{t("topSites.addNewSite")}</h3>
        <form onSubmit={handleSubmit} className="space-y-4">
          <div className="form-control">
            <label className="label">
              <span className="label-text">{t("topSites.url")}</span>
            </label>
            <input
              ref={urlInputRef}
              type="text"
              className="input input-bordered w-full"
              placeholder="https://example.com"
              value={url}
              onChange={(e) => setUrl(e.target.value)}
              required
            />
          </div>
          <div className="form-control">
            <label className="label">
              <span className="label-text">{t("topSites.label")}</span>
            </label>
            <input
              type="text"
              className="input input-bordered w-full"
              placeholder={t("topSites.labelPlaceholder", {
                defaultValue: "Website name",
              })}
              value={label}
              onChange={(e) => setLabel(e.target.value)}
            />
          </div>
          {error && <p className="text-error text-sm">{error}</p>}
          <div className="modal-action">
            <button type="button" className="btn" onClick={onCancel}>
              {t("topSites.cancel")}
            </button>
            <button type="submit" className="btn btn-primary">
              {t("topSites.add")}
            </button>
          </div>
        </form>
      </div>
    </dialog>
  );
}

export function TopSites() {
  const { t } = useTranslation();
  const [sites, setSites] = useState<TopSite[]>([]);
  const [pinnedSites, setPinnedSites] = useState<PinnedSite[]>([]);
  const [blockedSites, setBlockedSites] = useState<string[]>([]);
  const [contextMenu, setContextMenu] = useState<
    {
      x: number;
      y: number;
      site: TopSite | PinnedSite;
    } | null
  >(null);
  const [siteToBlock, setSiteToBlock] = useState<TopSite | PinnedSite | null>(
    null,
  );
  const [showAddModal, setShowAddModal] = useState(false);

  // デバッグ用：状態の変化を監視
  useEffect(() => {
    console.log("🔍 [TopSites Debug] Sites state updated:", sites);
    console.log("🔍 [TopSites Debug] Sites count:", sites.length);
  }, [sites]);

  useEffect(() => {
    console.log("🔍 [TopSites Debug] Pinned sites state updated:", pinnedSites);
    console.log("🔍 [TopSites Debug] Pinned sites count:", pinnedSites.length);
  }, [pinnedSites]);

  useEffect(() => {
    console.log(
      "🔍 [TopSites Debug] Blocked sites state updated:",
      blockedSites,
    );
    console.log(
      "🔍 [TopSites Debug] Blocked sites count:",
      blockedSites.length,
    );
  }, [blockedSites]);

  const contextMenuRef = useRef<HTMLDivElement>(null);

  const loadSettings = async () => {
    console.log("🔍 [TopSites Debug] Loading settings...");

    const settings = await getNewTabSettings();
    console.log("🔍 [TopSites Debug] Settings loaded:", settings);
    console.log("🔍 [TopSites Debug] Pinned sites:", settings.topSites.pinned);
    console.log(
      "🔍 [TopSites Debug] Blocked sites:",
      settings.topSites.blocked,
    );

    setPinnedSites(settings.topSites.pinned);
    setBlockedSites(settings.topSites.blocked);

    const topSites = await getTopSites();
    console.log("🔍 [TopSites Debug] Top sites before filtering:", topSites);

    const filteredSites = topSites.filter(
      (site) => {
        const isBlocked = settings.topSites.blocked.includes(site.url);
        const isPinned = settings.topSites.pinned.some((p) =>
          p.url === site.url
        );
        const shouldInclude = !isBlocked && !isPinned;

        console.log(
          `🔍 [TopSites Debug] Site "${site.title}" (${site.url}): blocked=${isBlocked}, pinned=${isPinned}, include=${shouldInclude}`,
        );

        return shouldInclude;
      },
    );

    console.log("🔍 [TopSites Debug] Filtered sites:", filteredSites);
    console.log(
      "🔍 [TopSites Debug] Final site count (regular):",
      filteredSites.length,
    );
    console.log(
      "🔍 [TopSites Debug] Final site count (pinned):",
      settings.topSites.pinned.length,
    );
    console.log(
      "🔍 [TopSites Debug] Total visible sites:",
      filteredSites.length + settings.topSites.pinned.length,
    );

    setSites(filteredSites);
  };

  useEffect(() => {
    loadSettings();

    // デバッグツールをwindowオブジェクトに追加
    interface DebugTools {
      testNRFunction: () => void;
      reloadSettings: () => void;
      getCurrentState: () => void;
      globalThis: typeof globalThis;
      window: typeof window;
    }

    (window as typeof window & { debugTopSites: DebugTools }).debugTopSites = {
      testNRFunction: () => {
        console.log("🔍 [Debug Tool] Testing NRGetCurrentTopSites function...");
        const nrFunction =
          (globalThis as unknown as Window).NRGetCurrentTopSites;
        console.log("🔍 [Debug Tool] Function exists:", !!nrFunction);
        console.log("🔍 [Debug Tool] Function type:", typeof nrFunction);

        if (nrFunction) {
          const startTime = Date.now();
          try {
            nrFunction((data) => {
              const endTime = Date.now();
              const duration = endTime - startTime;

              console.log("🔍 [Debug Tool] Response time:", duration + "ms");
              console.log(
                "🔍 [Debug Tool] Raw callback data type:",
                typeof data,
              );
              console.log(
                "🔍 [Debug Tool] Raw callback data length:",
                data?.length || 0,
              );
              console.log(
                "🔍 [Debug Tool] Raw callback data preview:",
                data?.substring(0, 200) + "...",
              );

              try {
                const parsed = JSON.parse(data);
                console.log(
                  "🔍 [Debug Tool] Parsed data keys:",
                  Object.keys(parsed),
                );
                console.log(
                  "🔍 [Debug Tool] Has topsites:",
                  "topsites" in parsed,
                );
                console.log(
                  "🔍 [Debug Tool] Topsites is array:",
                  Array.isArray(parsed.topsites),
                );
                console.log(
                  "🔍 [Debug Tool] Topsites count:",
                  parsed.topsites?.length || 0,
                );

                if (parsed.topsites && parsed.topsites.length > 0) {
                  console.log(
                    "🔍 [Debug Tool] Sample topsite:",
                    parsed.topsites[0],
                  );
                  console.log("🔍 [Debug Tool] All topsites:", parsed.topsites);
                }

                console.log("🔍 [Debug Tool] Full parsed data:", parsed);
              } catch (e) {
                console.error("🔍 [Debug Tool] Parse error:", e);
                console.log(
                  "🔍 [Debug Tool] Raw data causing parse error:",
                  data,
                );
              }
            });
          } catch (e) {
            console.error("🔍 [Debug Tool] Function call error:", e);
          }
        } else {
          console.error(
            "🔍 [Debug Tool] NRGetCurrentTopSites function not found!",
          );
        }
      },
      reloadSettings: () => {
        console.log("🔍 [Debug Tool] Reloading settings...");
        loadSettings();
      },
      getCurrentState: () => {
        console.log("🔍 [Debug Tool] Current state:");
        console.log("  - Sites:", sites);
        console.log("  - Pinned Sites:", pinnedSites);
        console.log("  - Blocked Sites:", blockedSites);
      },
      globalThis: globalThis,
      window: window,
    };

    console.log(
      "🔍 [Debug Tool] Debug tools available via window.debugTopSites",
    );
    console.log(
      "🔍 [Debug Tool] Use: window.debugTopSites.testNRFunction() to test data retrieval",
    );
    console.log(
      "🔍 [Debug Tool] Use: window.debugTopSites.reloadSettings() to reload settings",
    );
    console.log(
      "🔍 [Debug Tool] Use: window.debugTopSites.getCurrentState() to view current state",
    );
  }, []);

  useEffect(() => {
    const handleClickOutside = (event: globalThis.MouseEvent) => {
      if (
        contextMenuRef.current &&
        !contextMenuRef.current.contains(event.target as Node)
      ) {
        setContextMenu(null);
      }
    };
    document.addEventListener("mousedown", handleClickOutside);
    return () => {
      document.removeEventListener("mousedown", handleClickOutside);
    };
  }, [contextMenuRef]);

  const handleContextMenu = (
    e: MouseEvent<HTMLAnchorElement>,
    site: TopSite | PinnedSite,
  ) => {
    e.preventDefault();
    setContextMenu({ x: e.pageX, y: e.pageY, site });
  };

  const handleSaveSettings = async (settings: Partial<NewTabSettings>) => {
    await saveNewTabSettings(settings);
    await loadSettings();
    setContextMenu(null);
  };

  const pinSite = async (site: TopSite | PinnedSite) => {
    const title = "title" in site && site.title ? site.title : site.url;
    const newPinnedSites = [...pinnedSites, { url: site.url, title }];
    await handleSaveSettings({
      topSites: { pinned: newPinnedSites, blocked: blockedSites },
    });
  };

  const addCustomSite = async (url: string, title: string) => {
    // 重複確認
    if (
      pinnedSites.some((p) => p.url === url) ||
      sites.some((s) => s.url === url) ||
      blockedSites.includes(url)
    ) {
      // 既に存在する場合はピン済み扱いとして閉じる
      setShowAddModal(false);
      return;
    }
    const newPinnedSites = [...pinnedSites, { url, title }];
    await handleSaveSettings({
      topSites: { pinned: newPinnedSites, blocked: blockedSites },
    });
    setShowAddModal(false);
  };

  const unpinSite = async (site: PinnedSite) => {
    const newPinnedSites = pinnedSites.filter((p) => p.url !== site.url);
    await handleSaveSettings({
      topSites: { pinned: newPinnedSites, blocked: blockedSites },
    });
  };

  const handleBlockRequest = (site: TopSite | PinnedSite) => {
    setSiteToBlock(site);
    setContextMenu(null);
  };

  const confirmBlockSite = async () => {
    if (!siteToBlock) return;
    const newBlockedSites = [...blockedSites, siteToBlock.url];
    let newPinnedSites = pinnedSites;
    if (isPinned(siteToBlock)) {
      newPinnedSites = pinnedSites.filter((p) => p.url !== siteToBlock.url);
    }
    await handleSaveSettings({
      topSites: { pinned: newPinnedSites, blocked: newBlockedSites },
    });
    setSiteToBlock(null);
  };

  const cancelBlockSite = () => {
    setSiteToBlock(null);
  };

  const isPinned = (site: TopSite | PinnedSite) => {
    return pinnedSites.some((p) => p.url === site.url);
  };

  // レンダリング前のデバッグ情報
  console.log("🔍 [TopSites Debug] Rendering TopSites component");
  console.log("🔍 [TopSites Debug] Render state - Sites:", sites.length, sites);
  console.log(
    "🔍 [TopSites Debug] Render state - Pinned sites:",
    pinnedSites.length,
    pinnedSites,
  );
  console.log(
    "🔍 [TopSites Debug] Render state - Total sites to render:",
    pinnedSites.length + sites.length,
  );

  return (
    <>
      {siteToBlock && (
        <BlockModal
          site={siteToBlock}
          onConfirm={confirmBlockSite}
          onCancel={cancelBlockSite}
        />
      )}
      {showAddModal && (
        <AddSiteModal
          onAdd={addCustomSite}
          onCancel={() => setShowAddModal(false)}
        />
      )}
      <div className="bg-gray-800/50 rounded-lg shadow-sm p-3 inline-block">
        <div className="flex flex-wrap gap-x-0.5">
          {/* Debug: Rendering pinned sites */}
          {(() => {
            console.log(
              "🔍 [TopSites Debug] Rendering pinned sites:",
              pinnedSites.length,
            );
            return null;
          })()}
          {pinnedSites.map((site) => (
            <a
              key={site.url}
              href={site.url}
              className="group flex flex-col items-center w-16 p-2 rounded-lg transition-all duration-200 hover:bg-white/50 dark:hover:bg-gray-700/50"
              onContextMenu={(e) => handleContextMenu(e, site)}
            >
              <div className="relative w-8 h-8">
                <div className="w-8 h-8 rounded-lg overflow-hidden bg-gray-700/80 flex items-center justify-center transform transition-transform group-hover:scale-110 mb-1">
                  <img
                    src={`https://www.google.com/s2/favicons?domain=${
                      new URL(site.url).hostname
                    }&sz=32`}
                    alt={site.title}
                    className="w-6 h-6 object-contain"
                  />
                </div>
                <div className="absolute top-0 right-0 transform translate-x-1/2 -translate-y-1/2">
                  <PinIcon className="w-4 h-4 text-white" />
                </div>
              </div>
              <span className="text-xs text-center text-gray-300 line-clamp-2 leading-tight">
                {site.title}
              </span>
            </a>
          ))}
          {/* Debug: Rendering regular sites */}
          {(() => {
            console.log(
              "🔍 [TopSites Debug] Rendering regular sites:",
              sites.length,
            );
            return null;
          })()}
          {sites.map((site) => (
            <a
              key={site.url}
              href={site.url}
              className="group flex flex-col items-center w-16 p-2 rounded-lg transition-all duration-200 hover:bg-white/50 dark:hover:bg-gray-700/50"
              onContextMenu={(e) => handleContextMenu(e, site)}
            >
              <div className="w-8 h-8 rounded-lg overflow-hidden bg-gray-700/80 flex items-center justify-center transform transition-transform group-hover:scale-110 mb-1">
                {site.favicon || site.smallFavicon
                  ? (
                    <img
                      src={site.favicon || site.smallFavicon!}
                      alt={site.title}
                      className="w-6 h-6 object-contain"
                    />
                  )
                  : (
                    <span className="text-lg font-semibold text-gray-300">
                      {(site.title || "?")[0]}
                    </span>
                  )}
              </div>
              <span className="text-xs text-center text-gray-300 line-clamp-2 leading-tight">
                {site.title}
              </span>
            </a>
          ))}
          {/* Add new site tile */}
          <button
            type="button"
            onClick={() => setShowAddModal(true)}
            className="group flex flex-col items-center w-16 p-2 rounded-lg border border-dashed border-gray-500/50 text-gray-400 hover:text-white hover:bg-white/10 dark:hover:bg-gray-700/50 transition-colors"
            title={t("topSites.addSite")}
          >
            <div className="w-8 h-8 rounded-lg flex items-center justify-center bg-gray-700/40 group-hover:scale-110 transform transition-transform mb-1">
              <span className="text-2xl leading-none">＋</span>
            </div>
            <span className="text-[10px] text-center leading-tight line-clamp-2">
              {t("topSites.addSite")}
            </span>
          </button>
        </div>
      </div>
      {contextMenu && (
        <div
          ref={contextMenuRef}
          className="absolute z-10 bg-base-200 rounded-md shadow-lg"
          style={{
            top: `${contextMenu.y}px`,
            left: `${contextMenu.x}px`,
          }}
        >
          <ul className="menu p-2">
            {isPinned(contextMenu.site)
              ? (
                <li onClick={() => unpinSite(contextMenu.site as PinnedSite)}>
                  <a>{t("topSites.unpin")}</a>
                </li>
              )
              : (
                <li onClick={() => pinSite(contextMenu.site)}>
                  <a>{t("topSites.pin")}</a>
                </li>
              )}
            <li onClick={() => handleBlockRequest(contextMenu.site)}>
              <a>{t("topSites.block")}</a>
            </li>
          </ul>
        </div>
      )}
    </>
  );
}
