import { useEffect } from "react"
import { useTranslation } from "react-i18next"
import { useForm } from "react-hook-form"
import { FormProvider } from "react-hook-form"
import type { PanelSidebarFormData } from "../../../types/sidebar"
import { getPanelSidebarSettings, savePanelSidebarSettings } from "./data-manager.ts"
import Preferences from "./preferences.tsx"

declare global {
    interface Window {
        NRSPrefGet: (options: { prefName: string; prefType: string }) => Promise<string>;
        NRSPrefSet: (options: { prefName: string; prefValue: string | boolean | number; prefType: string }) => Promise<void>;
        addEventListener: typeof window.addEventListener;
        removeEventListener: typeof window.removeEventListener;
    }
}

export default function SidebarSettingsPage() {
    const { t } = useTranslation()
    const methods = useForm<PanelSidebarFormData>()
    const { setValue, watch } = methods
    const watchAll = watch()

    useEffect(() => {
        const fetchDefaultValues = async () => {
            const values = await getPanelSidebarSettings()
            Object.entries(values).forEach(([key, value]) => {
                setValue(key as keyof PanelSidebarFormData, value)
            })
        }

        fetchDefaultValues()
        globalThis.addEventListener("focus", fetchDefaultValues)
        return () => {
            globalThis.removeEventListener("focus", fetchDefaultValues)
        }
    }, [setValue])

    useEffect(() => {
        if (Object.keys(watchAll).length > 0) {
            savePanelSidebarSettings(watchAll as PanelSidebarFormData)
        }
    }, [watchAll])

    return (
        <div className="p-6 space-y-6">
            <div className="flex flex-col items-start pl-6">
                <h1 className="text-3xl font-bold mb-2">{t("panelSidebar.title")}</h1>
                <p className="text-sm mb-8">{t("panelSidebar.description")}</p>
            </div>

            <FormProvider {...methods}>
                <form className="space-y-6 pl-6">
                    <Preferences />
                </form>
            </FormProvider>
        </div>
    )
}