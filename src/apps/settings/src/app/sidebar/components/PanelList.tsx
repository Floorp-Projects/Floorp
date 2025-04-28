import type React from "react";
import { useEffect, useState } from "react";
import { useTranslation } from "react-i18next";
import { DragDropContext, Draggable, Droppable } from "react-beautiful-dnd";
import {
  addPanel,
  deletePanel,
  getPanelsList,
  savePanelsList,
  updatePanel,
} from "../dataManager.ts";
import type {
  Panel,
  Panels,
} from "../../../../../../apps/main/core/common/panel-sidebar/utils/type.ts";
import { PanelEditModal } from "./PanelEditModal.tsx";
import { Edit, GripVertical, Plus, Trash2 } from "lucide-react";
import { Card } from "../../../../../newtab/src/components/common/card";

export const PanelList: React.FC = () => {
  const { t } = useTranslation();
  const [panels, setPanels] = useState<Panels>([]);
  const [isModalOpen, setIsModalOpen] = useState(false);
  const [currentPanel, setCurrentPanel] = useState<Panel | null>(null);
  const [isLoading, setIsLoading] = useState(true);

  // パネル一覧データの取得
  useEffect(() => {
    const fetchPanels = async () => {
      setIsLoading(true);
      const data = await getPanelsList();
      if (data) {
        setPanels(data);
      }
      setIsLoading(false);
    };

    fetchPanels();
  }, []);

  // 新しいパネル追加モーダルを開く
  const handleAddPanel = () => {
    // 新しいパネルの初期値
    const newPanel: Panel = {
      id: `panel-${Date.now()}`,
      type: "web",
      width: 300,
      url: "",
      icon: "",
      userContextId: null,
      zoomLevel: null,
      userAgent: null,
      extensionId: null,
    };

    setCurrentPanel(newPanel);
    setIsModalOpen(true);
  };

  // パネル編集モーダルを開く
  const handleEditPanel = (panel: Panel) => {
    setCurrentPanel({ ...panel });
    setIsModalOpen(true);
  };

  // パネルの保存処理
  const handleSavePanel = async (panel: Panel) => {
    setIsModalOpen(false);

    const isNew = !panels.some((p) => p.id === panel.id);

    if (isNew) {
      await addPanel(panel);
    } else {
      await updatePanel(panel);
    }

    // リストを更新
    const updatedPanels = await getPanelsList();
    if (updatedPanels) {
      setPanels(updatedPanels);
    }
  };

  // パネルの削除
  const handleDeletePanel = async (panelId: string) => {
    if (window.confirm(t("panelSidebar.confirmDelete"))) {
      await deletePanel(panelId);
      // リストを更新
      const updatedPanels = await getPanelsList();
      if (updatedPanels) {
        setPanels(updatedPanels);
      }
    }
  };

  // D&Dによるパネルの並び替え
  const handleDragEnd = async (result: any) => {
    if (!result.destination) return;

    const items = Array.from(panels);
    const [reorderedItem] = items.splice(result.source.index, 1);
    items.splice(result.destination.index, 0, reorderedItem);

    setPanels(items);
    await savePanelsList(items);
  };

  // モーダルを閉じる
  const handleCloseModal = () => {
    setIsModalOpen(false);
    setCurrentPanel(null);
  };

  if (isLoading) {
    return (
      <div className="flex justify-center py-8">
        <span className="loading loading-spinner loading-md text-primary"></span>
      </div>
    );
  }

  return (
    <Card>
      <div className="flex justify-between items-center">
        <h3 className="text-xl font-semibold">{t("panelSidebar.panelList")}</h3>
        <button
          type="button"
          onClick={handleAddPanel}
          className="btn btn-primary btn-sm gap-2"
        >
          <Plus size={16} />
          {t("panelSidebar.addPanel")}
        </button>
      </div>

      <div className="card bg-base-100 shadow-sm">
        {panels.length === 0 ? (
          <div className="card-body items-center text-center text-base-content/70">
            <p>{t("panelSidebar.noPanels")}</p>
          </div>
        ) : (
          <DragDropContext onDragEnd={handleDragEnd}>
            <Droppable droppableId="panels">
              {(provided) => (
                <div
                  {...provided.droppableProps}
                  ref={provided.innerRef}
                  className="divide-y divide-base-300 rounded-box overflow-hidden"
                >
                  {panels.map((panel, index) => (
                    <Draggable
                      key={panel.id}
                      draggableId={panel.id}
                      index={index}
                    >
                      {(provided) => (
                        <div
                          ref={provided.innerRef}
                          {...provided.draggableProps}
                          className="bg-base-100 hover:bg-base-200 p-4 flex items-center justify-between transition-colors"
                        >
                          <div className="flex items-center gap-3">
                            <div
                              {...provided.dragHandleProps}
                              className="cursor-grab text-base-content/50 hover:text-base-content/70"
                              aria-label="Drag to reorder"
                            >
                              <GripVertical size={20} />
                            </div>
                            <div>
                              <div className="font-medium flex items-center gap-2">
                                {panel.icon && (
                                  <img
                                    src={panel.icon}
                                    alt=""
                                    className="w-5 h-5 rounded-full"
                                  />
                                )}
                                <span className="max-w-md truncate">
                                  {panel.url ||
                                    panel.extensionId ||
                                    t("panelSidebar.untitled")}
                                </span>
                              </div>
                              <div className="text-sm text-base-content/60 mt-1">
                                <div className="badge badge-sm">
                                  {t(`panelSidebar.type.${panel.type}`)}
                                </div>
                                {panel.width > 0 && (
                                  <span className="ml-2 text-xs">
                                    {panel.width}px
                                  </span>
                                )}
                              </div>
                            </div>
                          </div>
                          <div className="flex gap-2">
                            <button
                              type="button"
                              onClick={() => handleEditPanel(panel)}
                              className="btn btn-ghost btn-sm btn-square text-blue-600"
                              aria-label={t("common.edit")}
                            >
                              <Edit size={16} />
                            </button>
                            <button
                              onClick={() => handleDeletePanel(panel.id)}
                              className="btn btn-ghost btn-sm btn-square text-error"
                              aria-label={t("common.delete")}
                            >
                              <Trash2 size={16} />
                            </button>
                          </div>
                        </div>
                      )}
                    </Draggable>
                  ))}
                  {provided.placeholder}
                </div>
              )}
            </Droppable>
          </DragDropContext>
        )}
      </div>

      {isModalOpen && currentPanel && (
        <PanelEditModal
          panel={currentPanel}
          onSave={handleSavePanel}
          onClose={handleCloseModal}
        />
      )}
    </Card>
  );
};
