import { zFloorp11WorkspacesSchema } from "./old_type.ts";
import type { Floorp11Workspaces, WorkspaceDetail } from "./old_type.ts";
import type { TWorkspace, TWorkspaceID } from "../../utils/type.ts";
import { zWorkspaceID } from "../../utils/type.ts";
import {
  setSelectedWorkspaceID as _setSelectedWorkspaceID,
  setWorkspacesDataStore,
} from "../data.ts";

export function migrateWorkspacesData(): Promise<void> {
  const { SessionStore } = ChromeUtils.importESModule(
    "resource:///modules/sessionstore/SessionStore.sys.mjs",
  );
  const { NetUtil } = ChromeUtils.importESModule(
    "resource://gre/modules/NetUtil.sys.mjs",
  );

  return SessionStore.promiseInitialized.then(() => {
    return new Promise<void>((resolve, reject) => {
      try {
        const profDir = Services.dirsvc.get("ProfD", Ci.nsIFile);
        profDir.append("Workspaces");
        profDir.append("Workspaces.json");
        console.debug("Workspaces migration: start");
        if (!profDir.exists()) {
          console.debug(
            "Workspaces migration: Workspaces.json not found, skipping migration",
          );
          resolve();
          return;
        }

        const fileURI = Services.io.newFileURI(profDir);

        NetUtil.asyncFetch(
          { uri: fileURI, loadUsingSystemPrincipal: true },
          (inputStream: unknown, status: unknown) => {
            try {
              if (!Components.isSuccessCode(status as number)) {
                console.error(
                  "Workspaces migration: failed to read file",
                  status,
                );
                resolve();
                return;
              }
              const raw = NetUtil.readInputStreamToString(
                inputStream as unknown as object,
                (inputStream as { available: () => number }).available(),
              );
              console.debug(
                "Workspaces migration: file read OK, bytes=",
                raw.length,
              );
              let text: string = raw;
              try {
                type MinimalUnicodeConverter = {
                  ConvertToUnicode: (s: string) => string;
                  Finish: () => string;
                  charset: string;
                };
                const converterFactory =
                  (Components.classes as unknown as Record<
                    string,
                    { createInstance: (iface: unknown) => unknown }
                  >)[
                    "@mozilla.org/intl/scriptableunicodeconverter"
                  ];
                const converter = converterFactory.createInstance(
                  Ci.nsIScriptableUnicodeConverter,
                ) as MinimalUnicodeConverter;
                converter.charset = "UTF-8";
                text = converter.ConvertToUnicode(raw);
                text += converter.Finish();
                console.debug("Workspaces migration: unicode conversion OK");
              } catch (e: unknown) {
                console.warn(
                  "Workspaces migration: unicode conversion failed",
                  e,
                );
              }
              let oldData: Floorp11Workspaces;
              try {
                oldData = zFloorp11WorkspacesSchema.parse(
                  JSON.parse(text),
                ) as unknown as Floorp11Workspaces;
                console.debug(
                  "Workspaces migration: schema parsed OK, windows=",
                  Object.keys(oldData.windows ?? {}).length,
                );
              } catch (e) {
                console.error("Workspaces migration: invalid schema", e);
                resolve();
                return;
              }

              const usedWorkspaceIDs = new Set<TWorkspaceID>();
              for (
                const win of Services.wm.getEnumerator(
                  "navigator:browser",
                ) as unknown as Iterable<Window>
              ) {
                try {
                  const tabs =
                    (win as unknown as { gBrowser: { tabs: unknown[] } })
                      .gBrowser.tabs as unknown[];
                  for (const tab of tabs) {
                    const tabEl = tab as {
                      getAttribute(name: string): string | null;
                    };
                    const wsId: string =
                      tabEl.getAttribute("floorpWorkspaceId") ?? "";
                    const rawWsId = wsId.replace(/[{}]/g, "");
                    const wsParse = zWorkspaceID.safeParse(rawWsId);
                    if (wsParse.success) {
                      usedWorkspaceIDs.add(wsParse.data);
                    }
                  }
                } catch (e: unknown) {
                  console.error(
                    "Workspaces migration: error iterating windows",
                    e,
                  );
                }
              }
              console.debug(
                "Workspaces migration: usedWorkspaceIDs size=",
                usedWorkspaceIDs.size,
              );

              const dataMap = new Map<TWorkspaceID, TWorkspace>();
              const orderList: TWorkspaceID[] = [];
              let defaultID: TWorkspaceID | null = null;
              let preferredDefaultFromPreferences: TWorkspaceID | null = null;
              let defaultPickedFromDetail = false;

              for (const winObj of Object.values(oldData.windows)) {
                for (const [key, entry] of Object.entries(winObj)) {
                  if (key === "preferences") {
                    try {
                      const pref = entry as {
                        selectedWorkspaceId?: string;
                        defaultWorkspace?: string;
                      };
                      const candidateRaw =
                        (pref.selectedWorkspaceId ?? pref.defaultWorkspace ??
                          "").replace(/[{}]/g, "");
                      if (candidateRaw) {
                        const idParse = zWorkspaceID.safeParse(candidateRaw);
                        if (
                          idParse.success && usedWorkspaceIDs.has(idParse.data)
                        ) {
                          preferredDefaultFromPreferences = idParse.data;
                          console.debug(
                            "Workspaces migration: preferred default from preferences=",
                            preferredDefaultFromPreferences,
                          );
                        }
                      }
                    } catch (e: unknown) {
                      console.warn(
                        "Workspaces migration: invalid preferences entry",
                        e,
                      );
                    }
                    continue;
                  }
                  if (!("id" in entry)) {
                    console.warn(
                      "Workspaces migration: unexpected non-detail entry",
                      key,
                      entry,
                    );
                    continue;
                  }
                  const detail = entry as WorkspaceDetail;
                  const rawId = detail.id.replace(/[{}]/g, "");
                  const idParse = zWorkspaceID.safeParse(rawId);
                  if (!idParse.success) {
                    console.warn(
                      "Workspaces migration: invalid workspace id",
                      detail.id,
                    );
                    continue;
                  }
                  const idParsed = idParse.data;
                  if (!usedWorkspaceIDs.has(idParsed)) continue;
                  if (!dataMap.has(idParsed)) {
                    dataMap.set(idParsed, {
                      name: detail.name,
                      icon: detail.icon ?? undefined,
                      userContextId: typeof detail.userContextId === "number"
                        ? detail.userContextId
                        : 0,
                      isDefault: detail.defaultWorkspace,
                    });
                    orderList.push(idParsed);
                    if (detail.defaultWorkspace && !defaultID) {
                      defaultID = idParsed;
                      defaultPickedFromDetail = true;
                    }
                  }
                }
              }
              console.debug(
                "Workspaces migration: migrated entries=",
                dataMap.size,
                "order size=",
                orderList.length,
              );

              if (
                !defaultID && preferredDefaultFromPreferences &&
                dataMap.has(preferredDefaultFromPreferences)
              ) {
                defaultID = preferredDefaultFromPreferences;
                console.debug(
                  "Workspaces migration: default selected from preferences",
                );
              }

              if (!defaultID && orderList.length > 0) {
                defaultID = orderList[0];
                console.debug(
                  "Workspaces migration: default selected by fallback (first order)",
                );
              }
              if (defaultID) {
                setWorkspacesDataStore("defaultID", defaultID);
              }
              setWorkspacesDataStore("data", new Map(dataMap));
              setWorkspacesDataStore("order", orderList);
              console.debug(
                "Workspaces migration: store updated (default from detail=",
                defaultPickedFromDetail,
                ") defaultID=",
                defaultID,
                "",
              );

              try {
                profDir.remove(false);
                console.debug(
                  "Workspaces migration: removed old Workspaces.json",
                );
              } catch (e: unknown) {
                console.warn(
                  "Workspaces migration: failed to remove old Workspaces.json",
                  e,
                );
              }
              resolve();
            } catch (e: unknown) {
              console.error(
                "Workspaces migration: unexpected error in asyncFetch callback",
                e,
              );
              resolve();
            }
          },
        );
      } catch (e: unknown) {
        console.error("Workspaces migration: unexpected error", e);
        reject(e);
      }
    });
  })
    .catch((e: unknown) => {
      console.error("Workspaces migration promise rejected", e);
    });
}
