/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { useState } from "react";
import { useTranslation } from "react-i18next";
import type { GestureAction, GestureDirection } from "@/types/pref.ts";
import { patternToString } from "../dataManager.ts";
import { useAvailableActions } from "../useAvailableActions.ts";
import { getActionDisplayName } from "../../../../../main/core/common/mouse-gesture/utils/gestures.ts";

interface ActionEditModalProps {
    action: GestureAction;
    mode: "new" | "edit" | "duplicate";
    onSave: (action: GestureAction) => Promise<void>;
    onClose: () => void;
}

export function ActionEditModal({
    action: initialAction,
    mode,
    onSave,
    onClose,
}: ActionEditModalProps) {
    const { t } = useTranslation();
    const [action, setAction] = useState<GestureAction>(initialAction);
    const availableActions = useAvailableActions();

    const updatePattern = (direction: GestureDirection) => {
        setAction({
            ...action,
            pattern: [...action.pattern, direction],
        });
    };

    const resetPattern = () => {
        setAction({
            ...action,
            pattern: [],
        });
    };

    const removeDirection = (index: number) => {
        const newPattern = [...action.pattern];
        newPattern.splice(index, 1);
        setAction({
            ...action,
            pattern: newPattern,
        });
    };

    const handleSave = async () => {
        const finalAction = { ...action };

        if (finalAction.pattern.length === 0) {
            alert(t("mouseGesture.emptyPatternError"));
            return;
        }

        await onSave(finalAction);
    };

    const getModalTitle = () => {
        switch (mode) {
            case "edit":
                return t("mouseGesture.editAction");
            case "duplicate":
                return t("mouseGesture.duplicateAction");
            default:
                return t("mouseGesture.addAction");
        }
    };

    const getDisplayName = () => {
        return getActionDisplayName(action.action);
    };

    return (
        <div className="modal modal-open">
            <div className="modal-box max-w-md">
                <h3 className="font-bold text-lg mb-4">{getModalTitle()}</h3>

                <div className="form-control w-full">
                    <label className="mb-2">
                        <span className="text-base-content/90">
                            {t("mouseGesture.action")}
                        </span>
                    </label>
                    <select
                        className="select select-bordered w-full"
                        value={action.action}
                        onChange={(e) =>
                            setAction({
                                ...action,
                                action: e.target.value,
                            })
                        }
                    >
                        <option disabled value="">
                            {t("mouseGesture.selectAction")}
                        </option>
                        {availableActions.map((availableAction) => (
                            <option key={availableAction.id} value={availableAction.id}>
                                {availableAction.name}
                            </option>
                        ))}
                    </select>
                </div>

                <div className="py-4 space-y-4">
                    <div className="form-control w-full">
                        <label className="mb-2">
                            <span className="text-base-content/90">
                                {t("mouseGesture.pattern")}
                            </span>
                        </label>
                        <div className="flex flex-wrap gap-2 mb-3 min-h-8">
                            {action.pattern.map((direction, index) => (
                                <div
                                    key={index}
                                    className="badge badge-primary badge-lg font-mono gap-1"
                                >
                                    {patternToString([direction])}
                                    <button
                                        type="button"
                                        className="btn btn-ghost btn-xs btn-circle"
                                        onClick={() => removeDirection(index)}
                                    >
                                        ×
                                    </button>
                                </div>
                            ))}
                            {action.pattern.length === 0 && (
                                <div className="text-sm text-base-content/60">
                                    {t("mouseGesture.noPattern")}
                                </div>
                            )}
                        </div>
                        <div className="grid grid-cols-5 gap-2">
                            <button
                                type="button"
                                className="btn btn-outline btn-sm col-span-1"
                                onClick={() => updatePattern("up")}
                            >
                                ↑
                            </button>
                            <button
                                type="button"
                                className="btn btn-outline btn-sm col-span-1"
                                onClick={() => updatePattern("down")}
                            >
                                ↓
                            </button>
                            <button
                                type="button"
                                className="btn btn-outline btn-sm col-span-1"
                                onClick={() => updatePattern("left")}
                            >
                                ←
                            </button>
                            <button
                                type="button"
                                className="btn btn-outline btn-sm col-span-1"
                                onClick={() => updatePattern("right")}
                            >
                                →
                            </button>
                            <button
                                type="button"
                                className="btn btn-outline btn-sm col-span-1"
                                onClick={resetPattern}
                            >
                                {t("mouseGesture.reset")}
                            </button>
                        </div>
                    </div>
                </div>

                <div className="modal-action">
                    <button type="button" className="btn btn-outline" onClick={onClose}>
                        {t("mouseGesture.cancel")}
                    </button>
                    <button
                        type="button"
                        className="btn btn-primary"
                        onClick={handleSave}
                        disabled={action.pattern.length === 0}
                    >
                        {t("mouseGesture.save")}
                    </button>
                </div>
            </div>
            <div className="modal-backdrop" onClick={onClose}></div>
        </div>
    );
}
