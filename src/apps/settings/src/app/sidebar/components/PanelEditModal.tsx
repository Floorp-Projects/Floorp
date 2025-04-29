import type React from "react";
import { useCallback, useEffect, useMemo, useState } from "react";
import { useTranslation } from "react-i18next";
import type { Panel } from "../../../../../main/core/common/panel-sidebar/utils/type.ts";
import { X } from "lucide-react";
import {
  getContainers,
  getExtensionPanels,
  getStaticPanelDisplayName,
  getStaticPanels,
} from "../dataManager.ts";

interface PanelEditModalProps {
  panel: Panel;
  onSave: (panel: Panel) => void;
  onClose: () => void;
}

type Container = {
  id: number;
  name: string;
  label: string;
  icon: string;
  color: string;
};

type StaticPanel = {
  value: string;
  label: string;
  icon: string;
};

type ExtensionPanel = {
  extensionId: string;
  title: string;
  iconUrl: string;
};

export const PanelEditModal: React.FC<PanelEditModalProps> = ({
  panel,
  onSave,
  onClose,
}) => {
  const { t } = useTranslation();
  const [editedPanel, setEditedPanel] = useState<Panel>({ ...panel });
  const [errors, setErrors] = useState<Record<string, string>>({});
  const [containers, setContainers] = useState<Container[]>([]);
  const [staticPanels, setStaticPanels] = useState<StaticPanel[]>([]);
  const [extensionPanels, setExtensionPanels] = useState<ExtensionPanel[]>([]);
  const [isLoading, setIsLoading] = useState(true);

  const fetchPanelData = useCallback(async () => {
    setIsLoading(true);
    try {
      const [containersData, staticPanelsData, extensionPanelsData] =
        await Promise.all([
          getContainers(),
          getStaticPanels(),
          getExtensionPanels(),
        ]);

      if (containersData && Array.isArray(containersData)) {
        setContainers(containersData as any);
      } else {
        console.error("Invalid containers data", containersData);
        setContainers([]);
      }

      if (staticPanelsData && Array.isArray(staticPanelsData)) {
        setStaticPanels(staticPanelsData);
      } else {
        setStaticPanels([]);
      }

      if (extensionPanelsData && Array.isArray(extensionPanelsData)) {
        setExtensionPanels(extensionPanelsData);
      } else {
        setExtensionPanels([]);
      }
    } catch (error) {
      console.error("Failed to fetch panel data:", error);
    } finally {
      setIsLoading(false);
    }
  }, []);

  useEffect(() => {
    fetchPanelData();

    return () => {
    };
  }, [fetchPanelData]);

  const shouldShowWebFields = editedPanel.type === "web";
  const shouldShowStaticFields = editedPanel.type === "static";
  const shouldShowExtensionFields = editedPanel.type === "extension";

  const validateAndFormatUrl = (url: string): string => {
    if (!url) return "";

    if (url.startsWith("http://") || url.startsWith("https://")) {
      return url;
    }

    return `https://${url}`;
  };

  const handleChange = (
    e: React.ChangeEvent<HTMLInputElement | HTMLSelectElement>,
  ) => {
    const { name, value } = e.target;

    if (name === "type") {
      const newPanel = {
        ...editedPanel,
        [name]: value as "web" | "static" | "extension",
      };

      if (value === "web") {
        newPanel.extensionId = null;
      } else if (value === "extension") {
        newPanel.url = "";
        newPanel.userAgent = null;
        newPanel.userContextId = null;
      } else if (value === "static") {
        newPanel.extensionId = null;
        newPanel.url = "";
        newPanel.userAgent = null;
        newPanel.userContextId = null;
      }

      setEditedPanel(newPanel);
    } else if (name === "url") {
      if (editedPanel.type === "web") {
        const formattedUrl = validateAndFormatUrl(value);
        setEditedPanel({ ...editedPanel, [name]: formattedUrl });
      } else {
        setEditedPanel({ ...editedPanel, [name]: value });
      }
    } else if (name === "width") {
      setEditedPanel({ ...editedPanel, [name]: Number(value) });
    } else if (name === "zoomLevel") {
      setEditedPanel({ ...editedPanel, [name]: value ? Number(value) : null });
    } else if (name === "userContextId") {
      const numericValue = value ? Number(value) : null;
      setEditedPanel({ ...editedPanel, [name]: numericValue });
    } else if (name === "userAgent") {
      setEditedPanel({ ...editedPanel, [name]: value === "true" });
    } else if (name === "extensionId") {
      setEditedPanel({ ...editedPanel, [name]: value || null });
    } else {
      setEditedPanel({ ...editedPanel, [name]: value });
    }
  };

  const handleCheckboxChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const { name, checked } = e.target;
    setEditedPanel({ ...editedPanel, [name]: checked });
  };

  const validateForm = (): boolean => {
    const newErrors: Record<string, string> = {};

    if (!editedPanel.type) {
      newErrors.type = t("panelSidebar.errors.typeRequired");
    }

    if (editedPanel.type === "web") {
      if (!editedPanel.url) {
        newErrors.url = t("panelSidebar.errors.urlRequired");
      } else {
        try {
          new URL(editedPanel.url);
        } catch (e) {
          console.error("Invalid URL:", e);
          newErrors.url = t("panelSidebar.errors.invalidUrl");
        }
      }
    }

    if (editedPanel.type === "static" && !editedPanel.url) {
      newErrors.url = t("panelSidebar.errors.staticPanelRequired");
    }

    if (editedPanel.type === "extension" && !editedPanel.extensionId) {
      newErrors.extensionId = t("panelSidebar.errors.extensionIdRequired");
    }

    setErrors(newErrors);
    return Object.keys(newErrors).length === 0;
  };

  const handleSubmit = (e: React.FormEvent) => {
    e.preventDefault();

    if (validateForm()) {
      onSave(editedPanel);
    }
  };

  if (isLoading) {
    return (
      <div className="modal modal-open">
        <div className="modal-box relative max-w-2xl">
          <div className="flex justify-center py-8">
            <span className="loading loading-spinner loading-md text-primary">
            </span>
          </div>
        </div>
        <div className="modal-backdrop"></div>
      </div>
    );
  }

  return (
    <div className="modal modal-open">
      <div className="modal-box relative max-w-2xl">
        <button
          onClick={onClose}
          className="btn btn-sm btn-circle absolute right-2 top-2"
        >
          <X className="h-4 w-4" />
        </button>

        <h3 className="font-bold text-lg mb-4">
          {editedPanel.id.startsWith("panel-")
            ? t("panelSidebar.addPanel")
            : t("panelSidebar.editPanel")}
        </h3>

        <form onSubmit={handleSubmit} className="space-y-4">
          <div className="form-control">
            <label className="label">
              <span className="label-text">{t("panelSidebar.panelType")}</span>
            </label>
            <select
              name="type"
              value={editedPanel.type}
              onChange={handleChange}
              className="select select-bordered w-full"
            >
              <option value="web">{t("panelSidebar.type.web")}</option>
              <option value="static">{t("panelSidebar.type.static")}</option>
              <option value="extension">
                {t("panelSidebar.type.extension")}
              </option>
            </select>
            {errors.type && (
              <label className="label">
                <span className="label-text-alt text-error">{errors.type}</span>
              </label>
            )}
          </div>

          {shouldShowWebFields && (
            <div className="form-control">
              <label className="label">
                <span className="label-text">{t("panelSidebar.url")}</span>
              </label>
              <div className="relative">
                <input
                  type="text"
                  name="url"
                  value={editedPanel.url || ""}
                  onChange={handleChange}
                  placeholder="example.com"
                  className="input input-bordered w-full pr-8"
                />
                <div className="absolute inset-y-0 right-0 flex items-center pr-3 pointer-events-none text-base-content/50">
                  {editedPanel.url && (
                    <span className="text-xs">
                      {editedPanel.url.startsWith("https://") ? "🔒" : "⚠️"}
                    </span>
                  )}
                </div>
              </div>
              {errors.url && (
                <label className="label">
                  <span className="label-text-alt text-error">
                    {errors.url}
                  </span>
                </label>
              )}
            </div>
          )}

          {shouldShowStaticFields && (
            <div className="form-control">
              <label className="label">
                <span className="label-text">
                  {t("panelSidebar.staticPanel")}
                </span>
              </label>
              <select
                name="url"
                value={editedPanel.url || ""}
                onChange={handleChange}
                className="select select-bordered w-full"
              >
                <option value="">{t("panelSidebar.selectStaticPanel")}</option>
                {staticPanels.map((panel) => (
                  <option key={panel.value} value={panel.value}>
                    {getStaticPanelDisplayName(panel.value, t)}
                  </option>
                ))}
              </select>
              {errors.url && (
                <label className="label">
                  <span className="label-text-alt text-error">
                    {errors.url}
                  </span>
                </label>
              )}
            </div>
          )}

          {shouldShowExtensionFields && (
            <div className="form-control">
              <label className="label">
                <span className="label-text">
                  {t("panelSidebar.extensionPanel")}
                </span>
              </label>
              <select
                name="extensionId"
                value={editedPanel.extensionId || ""}
                onChange={handleChange}
                className="select select-bordered w-full"
              >
                <option value="">{t("panelSidebar.selectExtension")}</option>
                {extensionPanels.map((panel) => (
                  <option key={panel.extensionId} value={panel.extensionId}>
                    {panel.title}
                  </option>
                ))}
              </select>
              {errors.extensionId && (
                <label className="label">
                  <span className="label-text-alt text-error">
                    {errors.extensionId}
                  </span>
                </label>
              )}
            </div>
          )}

          <div className="form-control">
            <label className="label">
              <span className="label-text">{t("panelSidebar.icon")}</span>
            </label>
            <input
              type="text"
              name="icon"
              value={editedPanel.icon || ""}
              onChange={handleChange}
              placeholder="chrome://browser/skin/preferences/icon.svg"
              className="input input-bordered w-full"
            />
          </div>

          <div className="form-control">
            <label className="label">
              <span className="label-text">{t("panelSidebar.width")} (px)</span>
            </label>
            <input
              type="number"
              name="width"
              value={editedPanel.width}
              onChange={handleChange}
              className="input input-bordered w-full"
            />
            {errors.width && (
              <label className="label">
                <span className="label-text-alt text-error">
                  {errors.width}
                </span>
              </label>
            )}
          </div>

          {shouldShowWebFields && (
            <>
              <div className="form-control">
                <label className="label">
                  <span className="label-text">
                    {t("panelSidebar.zoomLevel")}
                  </span>
                </label>
                <input
                  type="number"
                  name="zoomLevel"
                  step="0.1"
                  value={editedPanel.zoomLevel ?? ""}
                  onChange={handleChange}
                  placeholder="1.0"
                  className="input input-bordered w-full"
                />
              </div>

              <div className="form-control">
                <label className="label">
                  <span className="label-text">
                    {t("panelSidebar.container")}
                  </span>
                </label>
                <select
                  name="userContextId"
                  value={editedPanel.userContextId?.toString() || "0"}
                  onChange={handleChange}
                  className="select select-bordered w-full"
                >
                  <option value="0">{t("panelSidebar.noContainer")}</option>
                  {containers.map((container) => (
                    <option key={container.id} value={container.id.toString()}>
                      {container.label}
                    </option>
                  ))}
                </select>
              </div>

              <div className="form-control">
                <label className="cursor-pointer label justify-start gap-2">
                  <input
                    type="checkbox"
                    name="userAgent"
                    checked={editedPanel.userAgent === true}
                    onChange={handleCheckboxChange}
                    className="checkbox checkbox-primary"
                  />
                  <span className="label-text">
                    {t("panelSidebar.useUserAgent")}
                  </span>
                </label>
              </div>
            </>
          )}

          <div className="modal-action mt-6">
            <button type="button" onClick={onClose} className="btn btn-outline">
              {t("panelSidebar.cancel")}
            </button>
            <button type="submit" className="btn btn-primary">
              {t("panelSidebar.save")}
            </button>
          </div>
        </form>
      </div>
      <div className="modal-backdrop" onClick={onClose}></div>
    </div>
  );
};
