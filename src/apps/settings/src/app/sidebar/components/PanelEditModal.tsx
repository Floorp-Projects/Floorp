import type React from "react";
import { useCallback, useEffect, useState } from "react";
import { useTranslation } from "react-i18next";
import type { Panel } from "../../../../../main/core/common/panel-sidebar/utils/type";
import { X } from "lucide-react";
import {
  getContainers,
  getExtensionPanels,
  getStaticPanels,
} from "../dataManager";

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
  id: string;
  title: string;
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

  // データの取得処理をuseCallbackでメモ化
  const fetchPanelData = useCallback(async () => {
    setIsLoading(true);
    try {
      const [containersData, staticPanelsData, extensionPanelsData] =
        await Promise.all([
          getContainers(),
          getStaticPanels(),
          getExtensionPanels(),
        ]);

      console.log("Loaded containers:", containersData?.length);
      console.log("Loaded static panels:", staticPanelsData?.length);
      console.log("Loaded extension panels:", extensionPanelsData?.length);

      // コンテナーのデータ形式を検証
      if (containersData && Array.isArray(containersData)) {
        console.log("Container sample:", containersData[0]);
        setContainers(containersData);
      } else {
        console.error("Invalid containers data", containersData);
        setContainers([]);
      }

      setStaticPanels(staticPanelsData || []);
      setExtensionPanels(extensionPanelsData || []);
    } catch (error) {
      console.error("Failed to fetch panel data:", error);
    } finally {
      setIsLoading(false);
    }
  }, []);

  // 初回マウント時にのみデータ取得を実行
  useEffect(() => {
    fetchPanelData();

    // クリーンアップ関数
    return () => {
      // キャンセル処理があれば実装
    };
  }, [fetchPanelData]);

  // パネルのタイプに応じて適切なコンポーネントの表示を制御
  const shouldShowWebFields = editedPanel.type === "web";
  const shouldShowStaticFields = editedPanel.type === "static";
  const shouldShowExtensionFields = editedPanel.type === "extension";

  // フォームの入力値が変更された時の処理
  const handleChange = (
    e: React.ChangeEvent<HTMLInputElement | HTMLSelectElement>,
  ) => {
    const { name, value } = e.target;

    if (name === "type") {
      // パネルタイプが変更された場合、不要なフィールドをリセットする
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
      }

      setEditedPanel(newPanel);
    } else if (name === "width") {
      setEditedPanel({ ...editedPanel, [name]: Number(value) });
    } else if (name === "zoomLevel") {
      setEditedPanel({ ...editedPanel, [name]: value ? Number(value) : null });
    } else if (name === "userContextId") {
      // コンテナーID変換のデバッグ出力
      console.log(
        `Setting userContextId: Raw value = "${value}", type = ${typeof value}`,
      );
      const numericValue = value ? Number(value) : null;
      console.log(
        `Converted userContextId = ${numericValue}, type = ${typeof numericValue}`,
      );

      setEditedPanel({ ...editedPanel, [name]: numericValue });
    } else if (name === "userAgent") {
      setEditedPanel({ ...editedPanel, [name]: value === "true" });
    } else {
      setEditedPanel({ ...editedPanel, [name]: value });
    }
  };

  // チェックボックスの変更処理
  const handleCheckboxChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const { name, checked } = e.target;
    setEditedPanel({ ...editedPanel, [name]: checked });
  };

  // フォームの検証
  const validateForm = (): boolean => {
    const newErrors: Record<string, string> = {};

    if (!editedPanel.type) {
      newErrors.type = t("panelSidebar.errors.typeRequired");
    }

    if (editedPanel.type === "web" && !editedPanel.url) {
      newErrors.url = t("panelSidebar.errors.urlRequired");
    }

    if (editedPanel.type === "extension" && !editedPanel.extensionId) {
      newErrors.extensionId = t("panelSidebar.errors.extensionIdRequired");
    }

    if (!editedPanel.width || editedPanel.width < 100) {
      newErrors.width = t("panelSidebar.errors.invalidWidth");
    }

    setErrors(newErrors);
    return Object.keys(newErrors).length === 0;
  };

  // 保存処理
  const handleSubmit = (e: React.FormEvent) => {
    e.preventDefault();

    if (validateForm()) {
      // 保存前にパネルデータの整合性を確認
      console.log("Saving panel:", editedPanel);
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
              <input
                type="text"
                name="url"
                value={editedPanel.url || ""}
                onChange={handleChange}
                placeholder="https://example.com"
                className="input input-bordered w-full"
              />
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
                {staticPanels.length === 0
                  ? <option value="">{t("panelSidebar.noStaticPanels")}</option>
                  : (
                    staticPanels.map((panel) => (
                      <option key={panel.id} value={panel.id}>
                        {panel.title}
                      </option>
                    ))
                  )}
              </select>
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
                {extensionPanels.length === 0
                  ? (
                    <option value="">
                      {t("panelSidebar.noExtensionPanels")}
                    </option>
                  )
                  : (
                    extensionPanels.map((panel) => (
                      <option key={panel.extensionId} value={panel.extensionId}>
                        {panel.title}
                      </option>
                    ))
                  )}
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
              min="100"
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
              {t("common.cancel")}
            </button>
            <button type="submit" className="btn btn-primary">
              {t("common.save")}
            </button>
          </div>
        </form>
      </div>
      <div className="modal-backdrop" onClick={onClose}></div>
    </div>
  );
};
