import React from "react";
import { useForm, FormProvider, useWatch } from "react-hook-form";
import { useTranslation } from "react-i18next";
import { Preferences } from "./components/Preferences.tsx";
import { InstalledApps } from "./components/InstalledApps.tsx";
import { getPwaSettings, savePwaSettings } from "./dataManager.ts";
import { TProgressiveWebAppFormData } from "@/types/pref.ts";

export default function Page() {
    const { t } = useTranslation();
    const methods = useForm<TProgressiveWebAppFormData>({ defaultValues: {} });
    const { control, setValue } = methods;
    const watchAll = useWatch({ control });

    React.useEffect(() => {
        const fetchDefaultValues = async () => {
            const values = await getPwaSettings();
            for (const key in values) {
                setValue(key as keyof TProgressiveWebAppFormData, values[key]);
            }
        };
        fetchDefaultValues();
        document.documentElement.addEventListener("onfocus", fetchDefaultValues);
        return () => {
            document.documentElement.removeEventListener("onfocus", fetchDefaultValues);
        };
    }, [setValue]);

    React.useEffect(() => {
        savePwaSettings(watchAll as TProgressiveWebAppFormData);
    }, [watchAll]);

    return (
        <div className="p-6 space-y-6">
            <div className="flex flex-col items-start pl-6">
                <h1 className="text-3xl font-bold mb-2">{t("progressiveWebApp.title")}</h1>
                <p className="text-sm mb-8">{t("progressiveWebApp.description")}</p>
            </div>

            <FormProvider {...methods}>
                <form className="space-y-6 pl-6">
                    <Preferences />
                    <InstalledApps />
                </form>
            </FormProvider>
        </div>
    );
}