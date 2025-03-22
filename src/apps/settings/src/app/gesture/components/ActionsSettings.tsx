/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { useState } from "react";
import { useTranslation } from "react-i18next";
import { MousePointer, PlusCircle, Edit, Copy, Trash2 } from "lucide-react";
import type { GestureAction, MouseGestureConfig } from "@/types/pref.ts";
import { patternToString } from "../dataManager.ts";
import { useAvailableActions } from "../useAvailableActions.ts";
import { ActionEditModal } from "./ActionEditModal.tsx";

interface ActionsSettingsProps {
    config: MouseGestureConfig;
    addAction: (action: GestureAction) => Promise<boolean>;
    updateAction: (index: number, action: GestureAction) => Promise<boolean>;
    deleteAction: (index: number) => Promise<boolean>;
}

export function ActionsSettings({
    config,
    addAction,
    updateAction,
    deleteAction,
}: ActionsSettingsProps) {
    const { t } = useTranslation();
    const [editingAction, setEditingAction] = useState<GestureAction | null>(null);
    const [isDialogOpen, setIsDialogOpen] = useState(false);
    const [editingMode, setEditingMode] = useState<"new" | "edit" | "duplicate">("new");
    const [editingIndex, setEditingIndex] = useState<number | null>(null);
    const availableActions = useAvailableActions();

    const editAction = (action: GestureAction, index: number) => {
        setEditingAction({ ...action });
        setEditingIndex(index);
        setEditingMode("edit");
        setIsDialogOpen(true);
    };

    const duplicateAction = (action: GestureAction) => {
        setEditingAction({
            ...action,
            name: `${action.name} ${t("mouseGesture.copy")}`,
        });
        setEditingMode("duplicate");
        setIsDialogOpen(true);
    };

    const newAction = () => {
        setEditingAction({
            name: "",
            pattern: [],
            action: availableActions[0]?.id || "",
            description: "",
        });
        setEditingIndex(null);
        setEditingMode("new");
        setIsDialogOpen(true);
    };

    const handleDeleteAction = async (actionIndex: number) => {
        await deleteAction(actionIndex);
    };

    const handleSaveAction = async (action: GestureAction) => {
        if (editingMode === "edit" && editingIndex !== null) {
            await updateAction(editingIndex, action);
        } else {
            await addAction(action);
        }
        setIsDialogOpen(false);
        setEditingAction(null);
        setEditingIndex(null);
    };

    const handleCloseModal = () => {
        setIsDialogOpen(false);
        setEditingAction(null);
        setEditingIndex(null);
    };

    return (
        <div className="card w-full bg-base-100 shadow-md border border-base-300/20 dark:bg-base-300/40 mb-6">
            <div className="card-body">
                <h2 className="card-title text-base-content/90 font-medium flex items-center gap-2">
                    <MousePointer className="h-5 w-5 text-purple-500" />
                    {t("mouseGesture.actionsSettings")}
                </h2>
                <p className="text-base-content/60 text-sm mb-4">
                    {t("mouseGesture.actionsSettingsDescription")}
                </p>

                <div>
                    <table className="table w-full">
                        <thead>
                            <tr>
                                <th className="text-base-content/70">{t("mouseGesture.name")}</th>
                                <th className="text-base-content/70">{t("mouseGesture.pattern")}</th>
                                <th className="text-base-content/70">{t("mouseGesture.action")}</th>
                                <th className="text-base-content/70 w-[140px]" />
                            </tr>
                        </thead>
                        <tbody>
                            {config.actions.map((action, index) => (
                                <tr key={index} className="hover:bg-base-200/50">
                                    <td className="font-medium">{action.name}</td>
                                    <td className="font-mono">
                                        <div className="flex flex-wrap gap-1">
                                            {action.pattern.map((direction, i) => (
                                                <span
                                                    key={i}
                                                    className="badge badge-primary badge-sm"
                                                    title={direction}
                                                >
                                                    {patternToString([direction])}
                                                </span>
                                            ))}
                                        </div>
                                    </td>
                                    <td>
                                        {availableActions.find((a) => a.id === action.action)
                                            ?.name || action.action}
                                    </td>
                                    <td>
                                        <div className="flex space-x-1">
                                            <button
                                                type="button"
                                                className="btn btn-ghost btn-sm"
                                                onClick={() => editAction(action, index)}
                                                title={t("mouseGesture.edit")}
                                            >
                                                <Edit className="h-4 w-4" />
                                            </button>
                                            <button
                                                type="button"
                                                className="btn btn-ghost btn-sm"
                                                onClick={() => duplicateAction(action)}
                                                title={t("mouseGesture.duplicate")}
                                            >
                                                <Copy className="h-4 w-4" />
                                            </button>
                                            <button
                                                type="button"
                                                className="btn btn-ghost btn-sm"
                                                onClick={() => handleDeleteAction(index)}
                                                title={t("mouseGesture.delete")}
                                            >
                                                <Trash2 className="h-4 w-4" />
                                            </button>
                                        </div>
                                    </td>
                                </tr>
                            ))}
                            {config.actions.length === 0 && (
                                <tr>
                                    <td colSpan={4} className="text-center py-4 text-base-content/60">
                                        {t("mouseGesture.noActions")}
                                    </td>
                                </tr>
                            )}
                        </tbody>
                    </table>
                </div>

                <div className="flex justify-start mt-4">
                    <button
                        type="button"
                        className="btn btn-primary flex items-center gap-2"
                        onClick={newAction}
                        disabled={!config.enabled}
                    >
                        <PlusCircle className="h-4 w-4" />
                        {t("mouseGesture.addAction")}
                    </button>
                </div>
            </div>

            {isDialogOpen && editingAction && (
                <ActionEditModal
                    action={editingAction}
                    mode={editingMode}
                    onSave={handleSaveAction}
                    onClose={handleCloseModal}
                />
            )}
        </div>
    );
} 