/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { useState } from "react";
import { useTranslation } from "react-i18next";
import type { KeyboardShortcutConfig, ShortcutConfig } from "../../../types/pref.ts";
import { useAvailableActions } from "../../gesture/useAvailableActions.ts";
import { ShortcutEditor } from "./ShortcutEditor.tsx";
import {
    Card,
    CardContent,
    CardDescription,
    CardHeader,
    CardTitle,
} from "@/components/common/card.tsx";
import { Keyboard } from "lucide-react";
import { InfoTip } from "@/components/common/infotip.tsx";

interface ShortcutsSettingsProps {
    config: KeyboardShortcutConfig;
    addShortcut: (action: string, shortcut: ShortcutConfig) => void;
    updateShortcut: (action: string, shortcut: ShortcutConfig) => void;
    deleteShortcut: (action: string) => void;
}

export const ShortcutsSettings = ({
    config,
    addShortcut,
    updateShortcut,
    deleteShortcut,
}: ShortcutsSettingsProps) => {
    const { t } = useTranslation();
    const actions = useAvailableActions();
    const [editingAction, setEditingAction] = useState<string | null>(null);
    const [isEditorOpen, setIsEditorOpen] = useState(false);
    const [editingShortcut, setEditingShortcut] = useState<ShortcutConfig | null>(null);

    const handleSaveShortcut = (shortcut: ShortcutConfig) => {
        if (editingAction) {
            if (config.shortcuts[editingAction]) {
                updateShortcut(editingAction, shortcut);
            } else {
                addShortcut(editingAction, shortcut);
            }
        }
        setEditingShortcut(null);
        setEditingAction(null);
    };

    return (
        <Card>
            <CardHeader>
                <CardTitle className="flex items-center gap-2">
                    <Keyboard className="size-5" />
                    {t("keyboardShortcut.shortcuts")}
                </CardTitle>
                <CardDescription>
                    <div className="inline-flex items-center gap-2">
                        {t("keyboardShortcut.shortcutsDescription")}
                        <InfoTip
                            description={t("keyboardShortcut.shortcutsTip")}
                        />
                    </div>
                </CardDescription>
            </CardHeader>
            <CardContent>
                <div className="overflow-x-auto">
                    <table className="table">
                        <thead>
                            <tr>
                                <th>{t("keyboardShortcut.action")}</th>
                                <th>{t("keyboardShortcut.shortcut")}</th>
                                <th>{t("keyboardShortcut.actions")}</th>
                            </tr>
                        </thead>
                        <tbody>
                            {actions.map((action) => {
                                const shortcut = config.shortcuts[action.id];
                                return (
                                    <tr key={action.id}>
                                        <td>{action.name}</td>
                                        <td>
                                            {shortcut ? (
                                                <div className="flex items-center space-x-2">
                                                    {shortcut.modifiers.alt && <span>Alt</span>}
                                                    {shortcut.modifiers.ctrl && <span>Ctrl</span>}
                                                    {shortcut.modifiers.meta && <span>Meta</span>}
                                                    {shortcut.modifiers.shift && <span>Shift</span>}
                                                    <span>{shortcut.key.toUpperCase()}</span>
                                                </div>
                                            ) : (
                                                <span className="text-base-content/50">
                                                    {t("keyboardShortcut.notSet")}
                                                </span>
                                            )}
                                        </td>
                                        <td>
                                            <div className="flex gap-8">
                                                <button
                                                    type="button"
                                                    className="btn btn-sm btn-primary"
                                                    onClick={() => {
                                                        setEditingShortcut(shortcut);
                                                        setEditingAction(action.id);
                                                        setIsEditorOpen(true);
                                                    }}
                                                >
                                                    {shortcut ? t("keyboardShortcut.edit") : t("keyboardShortcut.add")}
                                                </button>
                                                {shortcut && (
                                                    <button
                                                        type="button"
                                                        className="btn btn-sm btn-error"
                                                        onClick={() => deleteShortcut(action.id)}
                                                    >
                                                        {t("keyboardShortcut.delete")}
                                                    </button>
                                                )}
                                            </div>
                                        </td>
                                    </tr>
                                );
                            })}
                        </tbody>
                    </table>
                </div>

                {isEditorOpen && (
                    <ShortcutEditor
                        isOpen={isEditorOpen}
                        onClose={() => setIsEditorOpen(false)}
                        onSave={handleSaveShortcut}
                        initialShortcut={editingShortcut}
                        existingShortcuts={Object.values(config.shortcuts)}
                        actionId={editingAction || ""}
                    />
                )}
            </CardContent>
        </Card>
    );
};