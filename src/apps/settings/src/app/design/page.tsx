import React from "react";
import { FormProvider, useForm, useWatch } from "react-hook-form";
import { useTranslation } from "react-i18next";
import {
  getDesignSettings,
  saveDesignSettings,
} from "@/app/design/dataManager.ts";
import { Interface } from "@/app/design/components/Interface.tsx";
import { Tabbar } from "@/app/design/components/Tabbar.tsx";
import { Tab } from "@/app/design/components/Tab.tsx";
import type { DesignFormData } from "@/types/pref.ts";

export default function Page() {
  const { t } = useTranslation();
  const methods = useForm({ defaultValues: {} });
  const { control, setValue } = methods;
  const watchAll = useWatch({ control });

  React.useEffect(() => {
    const fetchDefaultValues = async () => {
      const values = await getDesignSettings();
      for (const key in values) {
        setValue(key as keyof DesignFormData, values[key]);
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
    saveDesignSettings(watchAll);
  }, [watchAll]);

  return (
    <div className="p-6 space-y-3">
      <div className="flex flex-col items-start pl-6">
        <h1 className="text-3xl font-bold mb-2">
          {t("design.tabAndAppearance")}
        </h1>
        <p className="text-sm mb-8">
          {t("design.customizePositionOfToolbars")}
        </p>
      </div>

      <FormProvider {...methods}>
        <form className="space-y-3 pl-6">
          <Interface />
          <Tabbar />
          <Tab />
        </form>
      </FormProvider>
    </div>
  );
}
