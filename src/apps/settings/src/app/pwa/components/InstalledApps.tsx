import {
  Card,
  CardContent,
  CardHeader,
  CardTitle,
} from "@/components/common/card.tsx";
import { useTranslation } from "react-i18next";
import { type ChangeEvent, useEffect, useRef, useState } from "react";
import type { InstalledApp, TProgressiveWebAppObject } from "@/types/pref.ts";
import {
  getInstalledApps,
  renamePwaApp,
  uninstallPwaApp,
} from "../dataManager.ts";

export function InstalledApps() {
  const { t } = useTranslation();
  const [installedApps, setInstalledApps] = useState<TProgressiveWebAppObject>(
    {},
  );
  const [selectedApp, setSelectedApp] = useState<InstalledApp | null>(null);
  const [newName, setNewName] = useState("");
  const [error, setError] = useState<string>("");
  const renameDialogRef = useRef<HTMLDialogElement>(null);
  const uninstallDialogRef = useRef<HTMLDialogElement>(null);

  const fetchApps = async () => {
    try {
      const apps = await getInstalledApps();
      setInstalledApps(apps);
      setError("");
    } catch (e) {
      setError(t("progressiveWebApp.errorFetchingApps"));
      console.error("Error fetching apps:", e);
    }
  };

  useEffect(() => {
    fetchApps();
    document.documentElement.addEventListener("onfocus", fetchApps);
    return () => {
      document.documentElement.removeEventListener("onfocus", fetchApps);
    };
  }, []);

  const handleRename = (app: InstalledApp) => {
    setSelectedApp(app);
    setNewName(app.name);
    setError("");
    renameDialogRef.current?.showModal();
  };

  const handleUninstall = (app: InstalledApp) => {
    setSelectedApp(app);
    setError("");
    uninstallDialogRef.current?.showModal();
  };

  const executeRename = async () => {
    if (!selectedApp || !newName) return;

    try {
      await renamePwaApp(selectedApp.id, newName);
      renameDialogRef.current?.close();
      await fetchApps();
      setError("");
    } catch (e) {
      setError(t("progressiveWebApp.errorRenaming"));
      console.error("Error renaming app:", e);
    }
  };

  const executeUninstall = async () => {
    if (!selectedApp) return;

    try {
      await uninstallPwaApp(selectedApp.id);
      uninstallDialogRef.current?.close();
      await fetchApps();
      setError("");
    } catch (e) {
      setError(t("progressiveWebApp.errorUninstalling"));
      console.error("Error uninstalling app:", e);
    }
  };

  const handleClose = (dialogRef: React.RefObject<HTMLDialogElement>) => {
    setError("");
    dialogRef.current?.close();
  };

  return (
    <>
      <Card>
        <CardHeader>
          <CardTitle>{t("progressiveWebApp.installedApps")}</CardTitle>
        </CardHeader>
        <CardContent>
          {error && <p className="text-destructive mb-4">{error}</p>}
          {Object.keys(installedApps).length === 0
            ? (
              <p className="text-muted-foreground">
                {t("progressiveWebApp.noInstalledApps")}
              </p>
            )
            : (
              <div className="space-y-4">
                {(Object.values(installedApps) as InstalledApp[]).map((app) => (
                  <div
                    key={app.id}
                    className="flex items-center p-3 border rounded-lg"
                  >
                    <img
                      src={app.icon}
                      alt={app.name}
                      className="w-8 h-8 rounded mr-3"
                    />
                    <div className="flex-1 min-w-0">
                      <p className="font-medium truncate">{app.name}</p>
                      <p className="text-sm text-muted-foreground truncate">
                        {app.start_url}
                      </p>
                    </div>
                    <div className="flex gap-2 ml-4">
                      <button
                        type="button"
                        onClick={() => handleRename(app)}
                      >
                        {t("progressiveWebApp.renameApp")}
                      </button>
                      <button
                        type="button"
                        onClick={() => handleUninstall(app)}
                        className="text-destructive"
                      >
                        {t("progressiveWebApp.uninstallApp")}
                      </button>
                    </div>
                  </div>
                ))}
              </div>
            )}
        </CardContent>
      </Card>

      <dialog
        ref={renameDialogRef}
        className="fixed top-1/2 left-1/2 -translate-x-1/2 -translate-y-1/2 p-6 rounded-lg shadow-lg backdrop:bg-black/50"
        onClick={(e) => {
          if (e.target === renameDialogRef.current) {
            handleClose(renameDialogRef);
          }
        }}
      >
        <div className="min-w-[300px]">
          <h2 className="text-lg font-semibold mb-4">
            {t("progressiveWebApp.renameApp")}
          </h2>
          {error && <p className="text-destructive mb-4">{error}</p>}
          <p className="text-sm text-muted-foreground mb-4">
            {t("progressiveWebApp.enterNewName")}
          </p>
          <input
            value={newName}
            onChange={(e: ChangeEvent<HTMLinputElement>) =>
              setNewName(e.target.value)}
            className="mb-4"
          />
          <div className="flex justify-end gap-2">
            <button
              type="button"
              onClick={() => handleClose(renameDialogRef)}
            >
              {t("progressiveWebApp.cancel")}
            </button>
            <button
              type="button"
              onClick={executeRename}
              disabled={!newName.trim() || newName === selectedApp?.name}
            >
              {t("progressiveWebApp.rename")}
            </button>
          </div>
        </div>
      </dialog>

      <dialog
        ref={uninstallDialogRef}
        className="fixed top-1/2 left-1/2 -translate-x-1/2 -translate-y-1/2 p-6 rounded-lg shadow-lg backdrop:bg-black/50"
        onClick={(e) => {
          if (e.target === uninstallDialogRef.current) {
            handleClose(uninstallDialogRef);
          }
        }}
      >
        <div className="min-w-[300px]">
          <h2 className="text-lg font-semibold mb-4">
            {t("progressiveWebApp.uninstallConfirmation")}
          </h2>
          {error && <p className="text-destructive mb-4">{error}</p>}
          <p className="text-sm text-muted-foreground mb-4">
            {t("progressiveWebApp.uninstallWarning", {
              name: selectedApp?.name,
            })}
          </p>
          <div className="flex justify-end gap-2">
            <button
              type="button"
              onClick={() => handleClose(uninstallDialogRef)}
            >
              {t("progressiveWebApp.cancel")}
            </button>
            <button type="button" onClick={executeUninstall}>
              {t("progressiveWebApp.uninstall")}
            </button>
          </div>
        </div>
      </dialog>
    </>
  );
}
