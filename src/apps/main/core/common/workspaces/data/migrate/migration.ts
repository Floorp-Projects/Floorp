import { zFloorp11WorkspacesSchema } from "./old_type.ts";
import type { Floorp11Workspaces, WorkspaceDetail } from "./old_type.ts";
import type { TWorkspace, TWorkspaceID } from "../../utils/type.ts";
import { zWorkspaceID } from "../../utils/type.ts";
import { setSelectedWorkspaceID, setWorkspacesDataStore } from "../data.ts";

export function migrateWorkspacesData(): Promise<void> {
  const { SessionStore } = ChromeUtils.importESModule(
    "resource:///modules/sessionstore/SessionStore.sys.mjs",
  );
  const { NetUtil } = ChromeUtils.importESModule(
    "resource://gre/modules/NetUtil.sys.mjs",
  );

  return SessionStore.promiseAllWindowsRestored.then(() => {
    return new Promise<void>((resolve, reject) => {
      try {
        const profDir = Services.dirsvc.get("ProfD", Ci.nsIFile);
        profDir.append("Workspaces");
        profDir.append("Workspaces.json");
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
          (inputStream: any, status: any) => {
            try {
              if (!Components.isSuccessCode(status)) {
                console.error(
                  "Workspaces migration: failed to read file",
                  status,
                );
                resolve();
                return;
              }
              const raw = NetUtil.readInputStreamToString(
                inputStream,
                inputStream.available(),
              );
              let text: string = raw;
              try {
                const converter = (Components.classes as any)[
                  "@mozilla.org/intl/scriptableunicodeconverter"
                ].createInstance(Ci.nsIScriptableUnicodeConverter);
                converter.charset = "UTF-8";
                text = converter.ConvertToUnicode(raw);
                text += converter.Finish();
              } catch (e: unknown) {
                console.warn(
                  "Workspaces migration: unicode conversion failed",
                  e,
                );
              }
              let oldData: Floorp11Workspaces;
              try {
                oldData = zFloorp11WorkspacesSchema.parse(JSON.parse(text));
              } catch (e) {
                console.error("Workspaces migration: invalid schema", e);
                resolve();
                return;
              }

              const dataMap = new Map<TWorkspaceID, TWorkspace>();
              const orderList: TWorkspaceID[] = [];
              let defaultID: TWorkspaceID | null = null;

              for (const winObj of Object.values(oldData.windows)) {
                for (const [key, entry] of Object.entries(winObj)) {
                  if (key === "preferences") continue;
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
                  if (!dataMap.has(idParsed)) {
                    dataMap.set(idParsed, {
                      name: detail.name,
                      icon: detail.icon ?? undefined,
                      userContextId: 0,
                      isDefault: detail.defaultWorkspace,
                    });
                    orderList.push(idParsed);
                    if (detail.defaultWorkspace) {
                      defaultID = idParsed;
                    }
                  }
                }
              }

              for (
                const win of Services.wm.getEnumerator(
                  "navigator:browser",
                ) as any
              ) {
                try {
                  const tabs = (win as any).gBrowser.tabs as any[];
                  for (const tab of tabs) {
                    const tabEl = tab as {
                      getAttribute(name: string): string | null;
                    };
                    const wsId: string =
                      tabEl.getAttribute("floorpWorkspaceId") ??
                        "";
                    const rawWsId = wsId.replace(/[{}]/g, "");
                    const wsParse = zWorkspaceID.safeParse(rawWsId);
                    if (!wsParse.success) {
                      console.warn(
                        "Workspaces migration: invalid selected workspace id",
                        wsId,
                      );
                      continue;
                    }
                    const wsParsed = wsParse.data;
                    if (dataMap.has(wsParsed)) {
                      setSelectedWorkspaceID(wsParsed);
                      break;
                    }
                  }
                } catch (e: unknown) {
                  console.error(
                    "Workspaces migration: error iterating windows",
                    e,
                  );
                }
              }

              if (!defaultID && orderList.length > 0) {
                defaultID = orderList[0];
              }
              if (defaultID) {
                setWorkspacesDataStore("defaultID", defaultID);
              }
              setWorkspacesDataStore("data", new Map(dataMap));
              setWorkspacesDataStore("order", orderList);

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
