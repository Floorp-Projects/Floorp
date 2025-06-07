import React from "react";
import { FormProvider, useForm, useWatch } from "react-hook-form";
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
      document.documentElement.removeEventListener(
        "onfocus",
        fetchDefaultValues,
      );
    };
  }, [setValue]);

  React.useEffect(() => {
    savePwaSettings(watchAll as TProgressiveWebAppFormData);
  }, [watchAll]);

  return (
    <div className="p-6 space-y-6 container mx-auto max-w-3xl">
      <header className="mb-8">
        <h1 className="text-3xl font-bold">{t("progressiveWebApp.title")}</h1>
        <p className="text-lg text-muted-foreground">
          {t("progressiveWebApp.description")}
        </p>
      </header>
      <FormProvider {...methods}>
        <form className="space-y-6">
          <Preferences />
          <InstalledApps />
        </form>
      </FormProvider>
    </div>
  );
}
