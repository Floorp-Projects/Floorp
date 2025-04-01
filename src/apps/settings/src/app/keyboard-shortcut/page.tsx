/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { useTranslation } from "react-i18next";
import { useKeyboardShortcutConfig } from "./dataManager.ts";
import { GeneralSettings } from "./components/GeneralSettings.tsx";
import { ShortcutsSettings } from "./components/ShortcutsSettings.tsx";

export default function Page() {
    const { t } = useTranslation();
    const {
        config,
        loading,
        updateConfig,
        toggleEnabled,
        addShortcut,
        updateShortcut,
        deleteShortcut,
    } = useKeyboardShortcutConfig();

    if (loading) {
        return <div className="py-6 text-center">{t("loading")}...</div>;
    }

    return (
        <div className="p-6 space-y-3">
            <div className="flex flex-col items-start pl-6">
                <h1 className="text-3xl font-bold mb-2">
                    {t("pages.keyboardShortcut")}
                </h1>
                <p className="text-sm mb-8">
                    {t("keyboardShortcut.description")}
                </p>
            </div>

            <div className="space-y-3 pl-6">
                <GeneralSettings
                    config={config}
                    toggleEnabled={toggleEnabled}
                    updateConfig={updateConfig}
                />

                <ShortcutsSettings
                    config={config}
                    addShortcut={addShortcut}
                    updateShortcut={updateShortcut}
                    deleteShortcut={deleteShortcut}
                />
            </div>
        </div>
    );
}